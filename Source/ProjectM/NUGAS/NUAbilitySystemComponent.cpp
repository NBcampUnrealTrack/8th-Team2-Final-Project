// Fill out your copyright notice in the Description page of Project Settings.


#include "NUAbilitySystemComponent.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "NUGameplayTags.h"
#include "Ability/NUBattleGameplayAbility.h"
#include "Attribute/NUVitalAttributeSet.h"
#include "Attribute/NUCombatAttributeSet.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUJobResourceViews.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "NUGAS/Attribute/NUStatAttributeSet.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayEffect/NUStatusGameplayEffect.h"
#include "GameplayEffect/NUSkillGameplayEffect.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUActiveSkillContext.h"
#include "Battle/Data/NUPassiveDefinition.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// 다음 턴에는 2턴
	// 거는 턴이 끝날 때 AdvanceTurnBasedEffects 가 한 번 깎으므로, 1로 걸면 다음 턴에 사라짐
	constexpr int32 BonusResourceTurns = 2;
	
	// 전이가 서로에게 튕겨 무한히 오가는 것을 막는 용도
	bool GTransferringStatusEffect = false;
	
	// 반사가 서로에게 튕겨 무한히 오가는 것을 막는 용도
	bool GResolvingReflect = false;


	/* 직업 스킬 상시 발동 */

	// 구간표를 행으로 편 조건들, 같은 스킬 안에서 여럿이 동시에 참이 된다
	// 중첩 수도 구간임 (3202 원소 지배 — 약점 1 · 3 · 5중첩 +6 · +18 · +30%), 빠져 있을 때는 약점 5 에서 셋이 다 더해져 +54% 였음
	bool IsBandCondition(ENUEffectCondition Condition)
	{
		return Condition == ENUEffectCondition::HPBelow
			|| Condition == ENUEffectCondition::HPAbove
			|| Condition == ENUEffectCondition::SpeedDiffAtLeast
			|| Condition == ENUEffectCondition::SpeedDiffAtMost
			|| Condition == ENUEffectCondition::StackAtCount;
	}

	// 어느 구간표인지, 서로 다른 표를 한 묶음에서 견주면 한쪽이 조용히 빠짐
	// 좁기의 부호도 표마다 달라서(HP 이하는 음수, 중첩은 양수) 견줄 수 없음
	uint8 BandKind(ENUEffectCondition Condition)
	{
		switch (Condition)
		{
		case ENUEffectCondition::HPBelow:
		case ENUEffectCondition::HPAbove:
			return 1;

		case ENUEffectCondition::SpeedDiffAtLeast:
		case ENUEffectCondition::SpeedDiffAtMost:
			return 2;

		case ENUEffectCondition::StackAtCount:
			return 3;

		default:
			return 0;
		}
	}

	// 좁은 구간일수록 큰 값, "이하" 는 기준이 작을수록, "이상"(중첩 수 포함) 은 클수록 좁음
	float BandNarrowness(const FNUJobSkillEffectRow& Row)
	{
		const float Threshold = Row.GetConditionNumber();

		return Row.ConditionType == ENUEffectCondition::HPBelow 
			|| Row.ConditionType == ENUEffectCondition::SpeedDiffAtMost
				? -Threshold 
				: Threshold;
	}

	// 자원표를 찾는 길, 여러 군데에서 쓰므로 한 곳으로 모음
	const FNUJobResourceDefinition* FindResourceDef(const UWorld* World, FName ResourceName)
	{
		if (ResourceName.IsNone() || !IsValid(World))
		{
			return nullptr;
		}
		
		const UGameInstance* GameInstance = World->GetGameInstance();
		const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
			? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
			: nullptr;
		
		return IsValid(Data) ? Data->FindJobResource(ResourceName) : nullptr;
	}
	
	// 자원표를 보고 자원이 누구 칸에 있는지 정함
	// 대상에게 붙는 자원도 자기 편 줄이 셀 때는 자원표의 SelfSideCountFrom 을 따름 (문양 — 살아 있는 상대 중 가장 많이 쌓인 쪽)
	// 적 대상 줄은 늘 그 대상의 칸을 봄
	const UNUAbilitySystemComponent* ResourceOwnerASC(FName ResourceName,
		const UNUAbilitySystemComponent* SelfASC, const UNUAbilitySystemComponent* TargetASC,
		ENUJobTargetScope RowScope)
	{
		const FNUJobResourceDefinition* Def = FindResourceDef(
			IsValid(SelfASC) ? SelfASC->GetWorld() : nullptr, ResourceName);
		
		if (!Def)
		{
			return nullptr;
		}
		
		if (Def->OwnerScope != ENUJobResourceOwner::Target)
		{
			return SelfASC;
		}
		
		if (RowScope != ENUJobTargetScope::Enemy && Def->SelfSideCountFrom == ENUJobSelfSideCount::MostStacked)
		{
			return IsValid(SelfASC) ? SelfASC->FindMostStackedOpponent(ResourceName) : nullptr;
		}
		
		return TargetASC;
	}
	
	// 이 줄의 조건이 지금 참인지, 모르는 조건은 거짓이라 그 줄만 빠짐
	bool IsJobConditionMet(const FNUJobSkillEffectRow& Row,
		const UNUAbilitySystemComponent* SelfASC, const UNUAbilitySystemComponent* TargetASC)
	{
		if (Row.ConditionType == ENUEffectCondition::None)
		{
			return true;
		}

		if (!IsValid(SelfASC))
		{
			return false;
		}

		const float Threshold = Row.GetConditionNumber();

		switch (Row.ConditionType)
		{
		case ENUEffectCondition::HPBelow:
		case ENUEffectCondition::HPAbove:
		{
			const float MaxHealth = SelfASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
			if (MaxHealth <= 0.f)
			{
				return false;
			}

			// 표가 백분율로 적음 (70 = 70%)
			const float Percent = SelfASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute())
				/ MaxHealth * 100.f;

			return Row.ConditionType == ENUEffectCondition::HPBelow
				? Percent <= Threshold : Percent > Threshold;
		}

		case ENUEffectCondition::SpeedDiffAtLeast:
		case ENUEffectCondition::SpeedDiffAtMost:
		{
			// 대상이 없으면 속도 차이를 셀 수 없다, 지어내지 않고 그 줄을 뺀다
			if (!IsValid(TargetASC))
			{
				return false;
			}

			const float Diff = SelfASC->GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute())
				- TargetASC->GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute());

			return Row.ConditionType == ENUEffectCondition::SpeedDiffAtLeast
				? Diff >= Threshold : Diff <= Threshold;
		}

		case ENUEffectCondition::SpeedAbove:
			// 자기 속도만 봄, 기준선을 넘어야 켜짐
			return SelfASC->GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute()) > Threshold;

		case ENUEffectCondition::ResourceAtMax:
		{
			// 조건 칸에 이름만 옴 ("영력")
			const FName Name = Row.GetConditionResourceName();
			const UNUAbilitySystemComponent* Owner = ResourceOwnerASC(Name, SelfASC, TargetASC, Row.TargetScope);
			
			return IsValid(Owner) && Owner->IsJobResourceAtMax(Name);
		}

		case ENUEffectCondition::ResourceAbove:
		{
			// "자원:50%" — 상한 대비 비율이라 상한을 모르면 셀 수 없음
			const FName Name = Row.GetConditionResourceName();
			const UNUAbilitySystemComponent* Owner = ResourceOwnerASC(Name, SelfASC, TargetASC, Row.TargetScope);
			
			int32 Amount = 0;
			if (!IsValid(Owner) || !Owner->TryGetJobResource(Name, Amount))
			{
				return false;
			}
			
			const int32 Max = Owner->GetJobResourceMax(Name);
			
			return Max > 0 && static_cast<float>(Amount) / Max * 100.f > Threshold;
		}

		case ENUEffectCondition::StackAtCount:
		{
			// "약점:5" — 몇 중첩 이상인지. 중첩은 맞는 쪽에 붙어 있음
			const FName Name = Row.GetConditionResourceName();
			const UNUAbilitySystemComponent* Owner = ResourceOwnerASC(Name, SelfASC, TargetASC, Row.TargetScope);
			
			int32 Amount = 0;
			
			return IsValid(Owner) && Owner->TryGetJobResource(Name, Amount)
				&& Amount >= FMath::RoundToInt(Threshold);
		}

		case ENUEffectCondition::ResourceSumAbove:
		{
			// "성배+검:8" — 둘을 더해서 확인함
			TArray<FString> Names;
			Row.GetConditionResourceName().ToString().ParseIntoArray(Names, TEXT("+"), true);
			
			int32 Sum = 0;
			for (const FString& Part : Names)
			{
				const FName Name(*Part.TrimStartAndEnd());
				const UNUAbilitySystemComponent* Owner = ResourceOwnerASC(Name, SelfASC, TargetASC, Row.TargetScope);
				
				int32 Amount = 0;
				if (!IsValid(Owner) || !Owner->TryGetJobResource(Name, Amount))
				{
					return false;
				}
				
				Sum += Amount;
			}
			
			return Names.Num() > 0 && Sum > Threshold;
		}

		case ENUEffectCondition::SingleHit:
			// 지금 행동의 타수가 하나일 때, 행동 밖에서 들어온 피해(도트 · 반사)는 타수가 0 이라 거짓
			return SelfASC->GetCurrentActionHitCount() == 1;

		case ENUEffectCondition::HitCountAtLeast:
			// 이번 턴 이미 공격한 횟수, 지금 타격은 끝난 뒤에 셈 — "6회 이상 공격하면 7회차부터" 가 6 >= 6
			return SelfASC->GetHitsDealtThisTurn() >= FMath::RoundToInt(Threshold);

		case ENUEffectCondition::OnCardDrawn:
			// 이번 턴에 뽑은 카드가 ConditionValue 와 같을 때 (3301 운명 읽기 · 3303 계시)
			return !SelfASC->GetCurrentJobCard().IsNone() && SelfASC->GetCurrentJobCard() == Row.GetConditionName();

		case ENUEffectCondition::DuringState:
			return SelfASC->IsJobStateActive(Row.GetConditionName());

		case ENUEffectCondition::AfterState:
			// "번아웃:1턴" 은 상태가 켜져 있는 동안으로 읽음, 상태 자체가 다음 턴 1턴짜리라 턴 수가 이미 들어 있음
			// 턴 제한이 없는 상태(임계)는 끝난 뒤 N턴째 (이름 뒤의 턴 수가 N), 변환기는 기간이 있는 상태만 자원표 기간과 맞춰 봄
			return SelfASC->IsJobStateAfterEnd(Row.GetConditionName(), FMath::RoundToInt(Row.GetConditionNumber()));

		default:
			// 카드 · 행동 순서 조건은 아직 없음
			return false;
		}
	}

	// 무엇 1당 을 몇 번 곱할지, 비어 있으면 1, 셀 수 없는 것이면 음수
	// 대상에게 붙는 자원(마크 · 문양)이 기준이면 TargetASC 쪽을 읽음, 대상이 없으면 셀 수 없음
	float JobValueSteps(const FNUJobSkillEffectRow& Row, const UNUAbilitySystemComponent* SelfASC,
		const UNUAbilitySystemComponent* TargetASC = nullptr)
	{
		if (Row.ValuePer.IsNone())
		{
			return 1.f;
		}

		if (!IsValid(SelfASC))
		{
			return -1.f;
		}

		const float MaxHealth = SelfASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
		if (MaxHealth <= 0.f)
		{
			return -1.f;
		}

		static const FName LostHealthName(TEXT("잃은HP"));
		static const FName MaxHealthName(TEXT("최대HP"));
		static const FName SpeedOverName(TEXT("속도초과분"));

		// 잃은 HP 20%마다, 비율
		if (Row.ValuePer == LostHealthName)
		{
			const float LostPercent = (MaxHealth
				- SelfASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute()))
				/ MaxHealth * 100.f;

			return FMath::FloorToFloat(LostPercent / Row.GetPerUnit());
		}

		// 최대 HP 250당, 절댓값
		if (Row.ValuePer == MaxHealthName)
		{
			return FMath::FloorToFloat(MaxHealth / Row.GetPerUnit());
		}

		// 속도가 기준선을 초과한 만큼, 기준선은 조건 칸에 있음 (3146 초속 = 20)
		// 자원표에 없는 축이지만 쌓이는 것이 아니라 매번 세는 값이라 자원 칸이 필요 없음
		if (Row.ValuePer == SpeedOverName)
		{
			const float Over = SelfASC->GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute())
				- Row.GetConditionNumber();

			return FMath::Max(0.f, FMath::FloorToFloat(Over / Row.GetPerUnit()));
		}

		// 자원표가 아는 이름이면 그 자원의 현재 수치로 셈
		// 쌓이지 않는 자원(속도차이 같은 파생값, 전직 노드가 정하는 공용 `자원`)은 여기서
		// 거짓이 되어 그 줄이 통째로 빠짐
		// 누구 칸을 읽을지는 자원표가 정함 — 자기에게서 마크를 세면 늘 없어서 줄이 빠졌음 (3207 문양 1당)
		int32 Amount = 0;
		const UNUAbilitySystemComponent* Owner = ResourceOwnerASC(Row.ValuePer, SelfASC, TargetASC, Row.TargetScope);
		if (IsValid(Owner) && Owner->TryGetJobResource(Row.ValuePer, Amount))
		{
			return FMath::FloorToFloat(Amount / Row.GetPerUnit());
		}

		return -1.f;
	}

	// 합연산 줄이 내놓는 값, 상한까지 잘라서 돌려줌
	float JobAdditiveAmount(const FNUJobSkillEffectRow& Row, float Steps)
	{
		const float Amount = Row.GetScaledValue() * Steps;
		const float Cap = Row.GetScaledCap();

		if (FMath::IsNearlyZero(Cap))
		{
			return Amount;
		}

		// 상한이 음수인 줄이 있음 (마크 1중첩당 속도 -5, 최대 -25)
		return Cap >= 0.f ? FMath::Min(Amount, Cap) : FMath::Max(Amount, Cap);
	}

	// 곱연산 줄이 내놓는 배수, 무엇 1당 이 붙으면 그만큼 거듭제곱 처리 (3014 균열)
	float JobMultiplierAmount(const FNUJobSkillEffectRow& Row, float Steps)
	{
		// 곱연산 줄의 값은 배수 그 자체
		const float Base = Row.GetScaledValue();
		const float Amount = Row.ValuePer.IsNone() ? Base : FMath::Pow(Base, FMath::Max(0.f, Steps));
		const float Cap = Row.GetScaledCap();

		if (FMath::IsNearlyZero(Cap))
		{
			return Amount;
		}

		// 1보다 큰 배수는 위로, 작은 배수(피해 감소)는 아래로 잘린다
		return Base >= 1.f ? FMath::Min(Amount, Cap) : FMath::Max(Amount, Cap);
	}

	// 상시 층이 읽을 줄인지
	bool IsJobAlwaysRow(const FNUJobSkillEffectRow& Row)
	{
		// Pending 은 표가 조건을 아직 안 적은 줄이라 그대로 읽으면 상시 효과가 되어버림
		// Code* 는 전용 구현이 붙어야 도는 줄
		return Row.ImplementBy == ENUEffectImplementBy::Data
			&& Row.DurationScope == ENUEffectDurationScope::Permanent
			&& Row.TargetScope == ENUJobTargetScope::Self;
	}

	// 전투당 1회 상태를 이루는 상시 줄 (3027 · 3044 — HP 25% 이하에서 주는 피해 +10% · 받는 피해 x0.9)
	// 피해 계산 때 고르는 종류만, 죽음 방지 줄은 따로 셈(TryConsumeOncePerBattle)
	// 스탯 줄은 상시 스탯 GE 가 상태가 바뀔 때 다시 안 걸려서 넣지 않음 — 조건 달린 스탯 줄은 거기서 경고하고 빠짐
	bool IsJobOnceStateRow(const FNUJobSkillEffectRow& Row)
	{
		return Row.MaxPerBattle > 0
			&& IsJobAlwaysRow(Row)
			&& (Row.EffectType == ENUJobEffectType::DamageDealtMod
				|| Row.EffectType == ENUJobEffectType::DamageTakenMod);
	}

	// 공격마다 값이 갈리는 치명타 줄 — 조건(3114 단발)이나 자원 · 잃은 HP 1당(3015 · 3039)이 붙은 것
	// 상시 스탯 GE 에 실으면 GE 를 걸 때 값으로 굳으므로 실행기가 공격 순간 더함
	bool IsPerAttackCriticalRow(const FNUJobSkillEffectRow& Row)
	{
		static const FName MaxHealthName(TEXT("최대HP"));

		return NUStatusEffect::IsRatioStat(Row.AffectedStat)
			&& (Row.ConditionType != ENUEffectCondition::None
				|| (!Row.ValuePer.IsNone() && Row.ValuePer != MaxHealthName));
	}

	// 자원 수치에 비례하는 상시 스탯 줄 (3104 템포 3당 SP 상한 +1), 치명타 두 축은 위에서 갈림
	// 자원이 바뀔 때마다 값을 고쳐야 해서 상시 스탯 GE 와 따로 걸어 둠
	bool IsResourceScaledStatRow(const FNUJobSkillEffectRow& Row, const UWorld* World)
	{
		return Row.EffectType == ENUJobEffectType::StatModifier
			&& IsJobAlwaysRow(Row)
			&& Row.ConditionType == ENUEffectCondition::None
			&& !NUStatusEffect::IsRatioStat(Row.AffectedStat)
			&& FindResourceDef(World, Row.ValuePer) != nullptr;
	}

	// 상태가 켜져 있는 동안만 붙는 상시 스탯 줄 (3104 번아웃 — SP 상한 -70%), 공격마다 값이 갈리는 치명타 줄은 실행기가 맡음
	// 상시 스탯 GE 에 실으면 상태가 바뀔 때 다시 안 걸려서 따로 걸어 둠
	bool IsJobStateStatRow(const FNUJobSkillEffectRow& Row)
	{
		return Row.EffectType == ENUJobEffectType::StatModifier
			&& IsJobAlwaysRow(Row)
			&& (Row.ConditionType == ENUEffectCondition::DuringState
				|| Row.ConditionType == ENUEffectCondition::AfterState)
			&& !IsPerAttackCriticalRow(Row);
	}

	// 상대가 가진 줄 중 이 전투원에게 붙은 자원에 비례하는 상시 스탯 줄 (3107 마크 1중첩당 대상 속도 -5)
	// 대상이 적인 상시 줄은 줄 주인에게 걸면 안 되고, 자원을 든 쪽이 상대의 줄을 모아 자기에게 걸음
	bool IsOpponentScaledStatRow(const FNUJobSkillEffectRow& Row, const UWorld* World)
	{
		if (Row.EffectType != ENUJobEffectType::StatModifier
			|| Row.ImplementBy != ENUEffectImplementBy::Data
			|| Row.DurationScope != ENUEffectDurationScope::Permanent
			|| Row.TargetScope != ENUJobTargetScope::Enemy
			|| Row.ConditionType != ENUEffectCondition::None)
		{
			return false;
		}

		const FNUJobResourceDefinition* Def = FindResourceDef(World, Row.ValuePer);

		return Def && Def->OwnerScope == ENUJobResourceOwner::Target;
	}

	// 3107 호라이즌 "속도 5 미만 불가" — 표에 칸이 없어 설명의 값을 둠, 칸이 오면 표에서 읽을 것 (7차 요청서 5번)
	constexpr float MarkSpeedFloor = 5.f;

	// 계수 가산 중 대상에게 붙는 자원(3140 마크 · 3241 문양)이 기준인 줄, 대상을 알아야 셀 수 있음
	bool IsTargetCoefficientRow(const FNUJobSkillEffectRow& Row, const UWorld* World)
	{
		if (Row.EffectType != ENUJobEffectType::CoefficientAdd
			|| Row.ImplementBy == ENUEffectImplementBy::Pending)
		{
			return false;
		}

		const FNUJobResourceDefinition* Def = FindResourceDef(World, Row.ValuePer);

		return Def && Def->OwnerScope == ENUJobResourceOwner::Target;
	}

	// 턴이 끝날 때 대상의 자원 수만큼 피해를 주는 줄인지 (3207 문양 지속 피해 — 문양 1중첩당 마법 공격력 x52%)
	// 전용 구현 줄이라 상시 층이 읽지 않고, 턴이 닫힐 때 ApplyJobResourceTickDamage 가 따로 돌림
	bool IsJobTickDamageRow(const FNUJobSkillEffectRow& Row)
	{
		return Row.EffectType == ENUJobEffectType::Damage
			&& Row.ConditionType == ENUEffectCondition::OnTurnEnd
			&& Row.TargetScope == ENUJobTargetScope::Enemy
			&& Row.DurationScope == ENUEffectDurationScope::Permanent
			&& !Row.ValuePer.IsNone()
			&& Row.ImplementBy != ENUEffectImplementBy::Pending;
	}

	// 받는 쪽까지 아는 상시 줄의 문, IsJobAlwaysRow 는 줄 주인 전용이라 따로 둠
	// 자기 줄은 줄 주인이 곧 받는 쪽일 때만, 아군 범위 줄은 줄 주인의 범위가 받는 쪽에 닿을 때 (3201 파티 치명타 · 3423 나이아 받는 피해)
	bool IsJobAlwaysRowFor(const FNUJobSkillEffectRow& Row, const UNUAbilitySystemComponent* Owner,
		const UNUAbilitySystemComponent* Receiver)
	{
		if (Row.ImplementBy != ENUEffectImplementBy::Data
			|| Row.DurationScope != ENUEffectDurationScope::Permanent
			|| !IsValid(Owner))
		{
			return false;
		}

		switch (Row.TargetScope)
		{
		case ENUJobTargetScope::Self:
			return Owner == Receiver;

		case ENUJobTargetScope::Ally:
		case ENUJobTargetScope::AllyAndNaia:
		case ENUJobTargetScope::Party:
			return Owner->DoesJobScopeReach(Row.TargetScope, Receiver);

		default:
			return false;
		}
	}

	// 쓸 줄만 true 로 채운 마스크를 생성함, 구간 조건은 가장 좁은 것 하나만 남김
	// ReceiverASC 를 주면 그 전투원에게 닿는 줄로 고름 (SelfASC 는 줄 주인), 안 주면 줄 주인 자신의 줄만
	void SelectJobRows(const TArray<FNUOwnedJobEffect>& Effects,
		ENUJobEffectType WantedType,
		const UNUAbilitySystemComponent* SelfASC, const UNUAbilitySystemComponent* TargetASC,
		TArray<int32>& OutIndices, const UNUAbilitySystemComponent* ReceiverASC = nullptr)
	{
		OutIndices.Reset();

		// 묶음마다 이긴 줄의 자리와 그 좁기, 중첩 구간은 자원 이름까지 열쇠에 넣음
		TMap<TPair<uint64, FName>, TPair<int32, float>> BandWinners;

		for (int32 Index = 0; Index < Effects.Num(); ++Index)
		{
			const FNUJobSkillEffectRow& Row = Effects[Index].Row;

			const bool bReaches = IsValid(ReceiverASC)
				? IsJobAlwaysRowFor(Row, SelfASC, ReceiverASC)
				: IsJobAlwaysRow(Row);

			if (Row.EffectType != WantedType || !bReaches)
			{
				continue;
			}

			if (JobValueSteps(Row, SelfASC, TargetASC) < 0.f || !IsJobConditionMet(Row, SelfASC, TargetASC))
			{
				continue;
			}

			// 전투당 1회 상태의 줄은 그 상태가 켜져 있을 때만 (3027 — HP 25% 이하에 들어가 켜지고, 회복하면 끝나 다시 안 켜짐)
			// 조건만 보면 HP 가 25% 밑으로 내려갈 때마다 다시 켜져 설계보다 강했음
			if (IsJobOnceStateRow(Row) && (!IsValid(SelfASC) || !SelfASC->IsJobOnceStateActive(Effects[Index].JobSkillId)))
			{
				continue;
			}

			if (!IsBandCondition(Row.ConditionType))
			{
				OutIndices.Add(Index);
				continue;
			}

			// 같은 스킬의 같은 축, 같은 구간표끼리만 비교함
			const uint64 Group = static_cast<uint64>(Effects[Index].JobSkillId) << 24
				| static_cast<uint64>(BandKind(Row.ConditionType)) << 16
				| static_cast<uint64>(Row.StackType) << 8
				| static_cast<uint64>(Row.AffectedStat);

			// 중첩은 자원까지 같아야 같은 표 — 약점 · 문양 구간이 한 스킬에 같이 오면 따로 셈
			const FName StackName = Row.ConditionType == ENUEffectCondition::StackAtCount
				? Row.GetConditionResourceName()
				: NAME_None;

			TPair<int32, float>& Winner = BandWinners.FindOrAdd(TPair<uint64, FName>(Group, StackName),
				TPair<int32, float>(INDEX_NONE, -MAX_flt));

			if (const float Narrowness = BandNarrowness(Row); Narrowness > Winner.Value)
			{
				Winner = TPair<int32, float>(Index, Narrowness);
			}
		}

		for (const TPair<TPair<uint64, FName>, TPair<int32, float>>& Pair : BandWinners)
		{
			if (Pair.Value.Key != INDEX_NONE)
			{
				OutIndices.Add(Pair.Value.Key);
			}
		}
	}

	// 전용 구현이 붙은 상시 줄(Code_*)의 배수, 줄이 없으면 1
	// IsJobAlwaysRow 는 Data 줄만 받으므로 효과 종류마다 여기서 따로 고름
	// 대상 범위는 표가 적은 그대로만 받음 - 자기 줄을 아군 줄로 읽거나 그 반대로 읽지 않음
	float JobCodeRowMultiplier(const TArray<FNUOwnedJobEffect>& Effects, ENUJobEffectType WantedType,
		ENUJobTargetScope WantedScope, const UNUAbilitySystemComponent* SelfASC)
	{
		float Multiplier = 1.f;

		for (const FNUOwnedJobEffect& Owned : Effects)
		{
			const FNUJobSkillEffectRow& Row = Owned.Row;

			if (Row.EffectType != WantedType
				|| Row.DurationScope != ENUEffectDurationScope::Permanent
				|| Row.TargetScope != WantedScope)
			{
				continue;
			}

			const float Steps = JobValueSteps(Row, SelfASC);
			if (Steps < 0.f || !IsJobConditionMet(Row, SelfASC, nullptr))
			{
				continue;
			}

			// 곱연산 줄의 값은 배수 그대로(x2), 합연산 줄은 1에 더할 비율(-0.3)
			Multiplier *= Row.StackType == ENUEffectStackType::Multiplicative
				? FMath::Max(0.f, JobMultiplierAmount(Row, Steps))
				: FMath::Max(0.f, 1.f + JobAdditiveAmount(Row, Steps));
		}

		return Multiplier;
	}
}

UNUAbilitySystemComponent::UNUAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicated(true);
}

void UNUAbilitySystemComponent::GiveStartupAbilities()
{
	for (const TSubclassOf<UGameplayAbility>& Ability : StartupAbilities)
	{
		GiveAbility(FGameplayAbilitySpec(Ability));
	}
}

void UNUAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, CurrentAvatar);
	DOREPLIFETIME(ThisClass, Encounter);
}

bool UNUAbilitySystemComponent::ApplyInitialVitals(const FNUAbilitySystemInitVitalData& InitData,
	TSubclassOf<UGameplayEffect> InitEffectClass, int Level)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return false;
	
	if (!InitEffectClass)
	{
		UE_LOG(LogTemp, Error, TEXT("초기화 GameplayEffect가 설정되지 않았습니다!"));
		return false;
	}
	
	FGameplayEffectContextHandle ContextHandle = MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(InitEffectClass, static_cast<float>(Level), ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Gameplay SpecHandle 이 유효하지 않습니다"));
		return false;
	}
	
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::MaxHealth, InitData.MaxHealth);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::Health, InitData.Health);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::MaxSP, InitData.MaxSP);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::SP, InitData.SP);
	
	ApplyGameplayEffectSpecToSelf(*Spec);
	
	return true;
}

bool UNUAbilitySystemComponent::ApplyInitialCombatStats(const FNUAbilitySystemInitCombatData& InitData,
	TSubclassOf<UGameplayEffect> InitEffectClass, int Level)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return false;
	
	if (!InitEffectClass)
	{
		UE_LOG(LogTemp, Error, TEXT("초기화 GameplayEffect가 설정되지 않았습니다!"));
		return false;
	}
	
	FGameplayEffectContextHandle ContextHandle = MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(InitEffectClass, static_cast<float>(Level), ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Gameplay SpecHandle 이 유효하지 않습니다"));
		return false;
	}
	
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::AttackPower, InitData.AttackPower);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::MagicAttackPower, InitData.MagicAttackPower);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::Defense, InitData.Defense);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::Speed, InitData.Speed);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::CriticalRate, InitData.CriticalRate);
	Spec->SetSetByCallerMagnitude(NU::Data::Initial::CriticalDamage, InitData.CriticalDamage);
	
	ApplyGameplayEffectSpecToSelf(*Spec);
	
	return true;
}

AActor* UNUAbilitySystemComponent::GetCurrentAvatar() const
{
	return CurrentAvatar;
}

void UNUAbilitySystemComponent::SetCurrentAvatar(AActor* NewAvatar)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	
	CurrentAvatar = NewAvatar;
	RefreshAbilityActorInfo();
	Owner->ForceNetUpdate();
}

ANUBattleEncounter* UNUAbilitySystemComponent::GetBattleEncounter() const
{
	return Encounter;
}

void UNUAbilitySystemComponent::SetBattleEncounter(ANUBattleEncounter* NewEncounter)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	
	if (Encounter == NewEncounter) return;
	
	Encounter = NewEncounter;
	
	// 전투가 바뀌면 전투당 1회 표시를 비운다
	// 전투 종료 정리(ClearTurnBasedEffects)에 얹지 않는 이유는 그쪽을 안 타는 전투원이 생기면
	// 다음 전투에서 조용히 한 번을 손해 보기 때문, 여기는 전투에 들어가면 반드시 지나간다
	UsedOncePerBattlePassives.Reset();
	
	// 전투 범위로 걸어둔 GE 도 같이 제거함
	// 안 걷으면 만료가 없는 효과라 다음 전투까지 남아있음
	for (const FActiveGameplayEffectHandle& Handle : BattleScopedEffects)
	{
		RemoveActiveGameplayEffect(Handle);
	}
	
	BattleScopedEffects.Reset();
	
	// 보호막·반사·동기화 목록처럼 이 전투에만 유지되는 효과를 되돌림
	ClearBattleScopedState();
	
	// 체력이 이미 낮은 채로 들어오면 첫 피해 전부터 켜져 있어야 함 (3027 불굴)
	RefreshJobOnceStates();
	
	OnBattleEncounterChanged.Broadcast(NewEncounter);	// 리슨서버일 경우
	
	Owner->ForceNetUpdate();
}

const UNUVitalAttributeSet* UNUAbilitySystemComponent::GetVitalAttributeSet() const
{
	return GetSet<UNUVitalAttributeSet>();
}

const UNUCombatAttributeSet* UNUAbilitySystemComponent::GetCombatAttributeSet() const
{
	return GetSet<UNUCombatAttributeSet>();
}

FGameplayAbilitySpecHandle UNUAbilitySystemComponent::FindBattleAbilityHandle(const FGameplayTag& InAbilityId)
{
	ABILITYLIST_SCOPE_LOCK();	// 어빌리티 순회 중 변동 방지
	
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UNUBattleGameplayAbility* BattleAbility = 
			Cast<UNUBattleGameplayAbility>(Spec.Ability.Get());
		
		if (!BattleAbility) continue;
		
		if (BattleAbility->GetAbilityId().MatchesTagExact(InAbilityId))
		{
			return Spec.Handle;
		}
	}
	
	return FGameplayAbilitySpecHandle();
}

bool UNUAbilitySystemComponent::TriggerBattleAbility(const FGameplayAbilitySpecHandle AbilityHandle,
	const FGameplayTag& AbilityId, const FGameplayEventData& EventData)
{
	if (!AbilityHandle.IsValid() || !AbilityActorInfo.IsValid()) return false;
	
	return TriggerAbilityFromGameplayEvent(
		AbilityHandle,
		AbilityActorInfo.Get(),
		AbilityId,
		&EventData,
		*this
		);
}

bool UNUAbilitySystemComponent::IsAlive() const
{
	const UNUVitalAttributeSet* Attributes = GetVitalAttributeSet();
	if (!Attributes) return false;
	
	return Attributes->GetHealth() > KINDA_SMALL_NUMBER
		&& !HasMatchingGameplayTag(NU::State::Dead);
}

void UNUAbilitySystemComponent::ReviveIfDead(float ReviveHealth)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	if (IsAlive()) return;
	
	// State.Dead 부여하는 GE 제거
	RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(NU::State::Dead));
	
	if (const UNUVitalAttributeSet* VitalAttributes = GetVitalAttributeSet())
	{
		SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(),
			FMath::Clamp(ReviveHealth, 1.f, VitalAttributes->GetMaxHealth()));
	}
}

FActiveGameplayEffectHandle UNUAbilitySystemComponent::ApplyTurnBasedEffect(const FGameplayEffectSpecHandle& SpecHandle,
	UNUAbilitySystemComponent* TargetASC, int32 Turns, int32 StatusEffectId)
{
	// Turns가 0이면 전투가 끝날때까지 유지해야하므로 음수만 막기
	if (!SpecHandle.IsValid() || !IsValid(TargetASC) || Turns < 0)
	{
		return FActiveGameplayEffectHandle();
	}
	
	// Infinite GE를 받아서 직접 턴 계산할 것
	FActiveGameplayEffectHandle Handle = ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	
	if (Handle.IsValid())
	{
		TargetASC->GrantTurnBasedEffects(Handle, Turns, StatusEffectId, this);
	}
	return Handle;
}

void UNUAbilitySystemComponent::ApplyReplacingStatModifier(const FGameplayEffectSpecHandle& SpecHandle,
	UNUAbilitySystemComponent* TargetASC, int32 Turns, int32 SkillId, ENUStatusAffectedStat Stat)
{
	if (!SpecHandle.IsValid() || !IsValid(TargetASC) || Turns <= 0 || SkillId <= 0
		|| Stat == ENUStatusAffectedStat::None)
	{
		return;
	}

	// 같은 시전자 · 같은 스킬 · 같은 축의 이전 것, 지우면 목록이 바뀌므로 핸들만 모아 둠
	TArray<FActiveGameplayEffectHandle> Previous;
	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TargetASC->TurnBasedEffects)
	{
		if (Pair.Value.SkillId == SkillId
			&& Pair.Value.ModifiedStat == Stat
			&& Pair.Value.SourceASC.Get() == this)
		{
			Previous.Add(Pair.Key);
		}
	}

	for (const FActiveGameplayEffectHandle& Old : Previous)
	{
		TargetASC->RemoveActiveGameplayEffect(Old);
		TargetASC->TurnBasedEffects.Remove(Old);
	}

	// StatusEffectId 는 상태이상이 아니라 0, 다음에 다시 걸 때 찾을 키만 채움
	const FActiveGameplayEffectHandle Handle = ApplyTurnBasedEffect(SpecHandle, TargetASC, Turns, 0);
	if (FNUTurnBasedEffect* Effect = TargetASC->TurnBasedEffects.Find(Handle))
	{
		Effect->SkillId = SkillId;
		Effect->ModifiedStat = Stat;
	}
}

void UNUAbilitySystemComponent::AdvanceTurnBasedEffects()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	
	// 도트 피해가 대상을 처치하면 사망 처리 시 GE를 건드릴 수 있음
	// 그러면 순회중 TMap이 변경될 수 있으므로 미리 복사해두기 
	TArray<FActiveGameplayEffectHandle> Handles;
	TurnBasedEffects.GetKeys(Handles);
	
	TArray<FActiveGameplayEffectHandle> Expired;
	
	// 모든 턴 기반 Effect 턴 감소 및 만료된 Effect 수거
	for (const  FActiveGameplayEffectHandle& Handle : Handles)
	{
		FNUTurnBasedEffect* Effect = TurnBasedEffects.Find(Handle);
		if (!Effect)
		{
			// 도중에 사라졌으면 넘어가기
			continue;
		}
		
		// 이미 다른 방식으로 제거된 효과라면 만료 목록에 추가
		// FActiveGameplayEffectHandle::IsValid() 는 제거해도 true라서 사용 불가능
		if (!GetActiveGameplayEffect(Handle))
		{
			Expired.Add(Handle);
			continue;
		}
		
		// 도트데미지로 대상이 사망하고 Encounter가 종료되면 턴기반 효과를 정리할 수 있음
		// 이 경우엔 Effect 포인터가 무효화 되므로 값을 복사해서 사용해야 함
		const FNUTurnBasedEffect SnapShot = *Effect;
		ApplyStatusTickDamage(SnapShot);
		
		// 틱 데미지를 적용한뒤에 다시 찾아보고, 없을경우 사망한것이므로 만료 목록에 추가할 필요 X
		Effect = TurnBasedEffects.Find(Handle);
		if (!Effect)
		{
			continue;
		}
		
		// 회복은 목록이 살아남은 뒤에 적용함, 앞선 항목의 도트가 대상을 죽였다면 건너뛰게 됨
		ApplyTurnBasedHealTick(SnapShot);
		
		// 남은 턴이 0보다 큰 턴 효과, 감소시켰을때 0이하로 떨어지면 만료됨
		// 남은턴이 0인 경우는 무한지속이므로 감소 X
		if (Effect->RemainingTurns > 0 && --Effect->RemainingTurns <= 0)
		{
			Expired.Add(Handle);
		}
	}
	
	// 만료된 Effect 제거
	for (const FActiveGameplayEffectHandle& Handle : Expired)
	{
		RemoveActiveGameplayEffect(Handle);
		TurnBasedEffects.Remove(Handle);
	}

	// 보호막·반사도 같은 턴 경계에서 줄어듦, 여기 말고 턴이 끝나는 자리가 따로 없음
	AdvanceBattleAxes();
	
	// 만료가 없어도 남은 턴이 줄어들어 표시가 바뀌므로 루프 밖에서 한 번만 알림
	NotifyStatusEffectsChanged();
}

void UNUAbilitySystemComponent::ClearTurnBasedEffects()
{
	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		RemoveActiveGameplayEffect(Pair.Key);
	}
	TurnBasedEffects.Reset();
	
	// 여기를 안 타는 전투원이 있어 SetBattleEncounter 에도 같은 정리가 있음
	// 양쪽에 두는 이유는 전투가 끝난 직후 필드에 보호막이 남아 보이면 안 되기 때문
	ClearBattleScopedState();

	NotifyStatusEffectsChanged();
}

FActiveGameplayEffectHandle UNUAbilitySystemComponent::TryApplyStatusEffect(UNUAbilitySystemComponent* TargetASC,
	int32 StatusEffectId, float Chance, int32 DurationOverride)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(TargetASC))
	{
		return FActiveGameplayEffectHandle();
	}
	
	// 타겟이 회피에 성공했다면 상태이상을 부여하면 안됨
	if (TargetASC->HasMatchingGameplayTag(NU::State::Dodging))
	{
		return FActiveGameplayEffectHandle();
	}
	
	const FNUStatusEffectDefinition* Definition = FindStatusEffectDefinition(StatusEffectId);
	if (!Definition || !Definition->StatusEffectTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("상태이상 정의를 찾을 수 없습니다 : %d"), StatusEffectId);
		return FActiveGameplayEffectHandle(); 
	}
	
	// 면역인지 확인
	if (TargetASC->IsImmuneStatusEffect(*Definition))
	{
		return FActiveGameplayEffectHandle();
	}
	
	
	// 상태이상 부여 확률 판정
	if (Chance < 1.f && FMath::FRand() >= Chance)
	{
		return FActiveGameplayEffectHandle();
	}
	
	
	// 지속 턴수 확인
	int32 Turns = DurationOverride > 0 ? DurationOverride : Definition->DurationTurns;
	
	// 2820 잔향 - 거는 쪽이 들고 있으면 지속 턴이 늘어남
	// 지속이 0인 행은 건드리지 않음, 0은 전투가 끝날 때까지라는 뜻이라 1을 더하면 오히려 사라지게 됨
	// DurationOverride 가 온 경우도 제외함, 부르는 쪽이 이미 턴을 정한 것이고,
	// 2832 전이가 그렇게 부르므로 넣으면 한 번 늘어난 값을 또 늘리게 됨
	if (DurationOverride <= 0 && Turns > 0)
	{
		const int32 DurationBonus = SumStatusDurationBonus();
		if (DurationBonus != 0)
		{
			const int32 Extended = FMath::Max(1, Turns + DurationBonus);
			
			UE_LOG(LogTemp, Warning, TEXT("잔향 발동 : 상태이상 %d 지속 %d -> %d턴"),
				StatusEffectId, Turns, Extended);
			
			Turns = Extended;
		}
	}
	
	
	// 이미 걸려있다면 스택 수 확인
	const FActiveGameplayEffectHandle ExistingHandle = 
		TargetASC->FindStatusEffectHandle(StatusEffectId);
	
	if (ExistingHandle.IsValid())
	{
		TargetASC->AddStatusEffectStack(ExistingHandle, Definition->MaxStack, Turns, this);
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::Status);
		return ExistingHandle;
	}
	
	
	// 처음 걸리는 경우만 GE 생성
	// 상태이상은 태그만 얹는 마커로 붙임, 각 수치는 정의를 읽는 쪽에서 해석함
	// 어트리뷰트를 직접 건드릴 경우가 생기면 그때 모디파이어 GE 를 사용함
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
		UNUGE_StatusEffect_Marker::StaticClass(),
		1.f,
		MakeEffectContext()
		);
	
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	
	// 정의에서 읽은 상태이상 태그를 스펙에 실어서 보냄
	SpecHandle.Data->DynamicGrantedTags.AddTag(Definition->StatusEffectTag);
	
	const FActiveGameplayEffectHandle AppliedHandle =
		ApplyTurnBasedEffect(SpecHandle, TargetASC, Turns, StatusEffectId);
	
	if (AppliedHandle.IsValid())
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::Status);
	}

	// 전이 - 걸린 쪽이 같은 상태이상을 부여자에게 되돌림 (2832)
	// 처음 걸릴 때만 처리함, 스택이 오르는 경우는 위에서 이미 반환함
	// 면역·회피 판정은 되돌리는 호출 안에서 다시 도므로 여기서 따로 보지 않음
	int32 TransferPassiveId = 0;
	const bool bHasTransfer = TargetASC->FindOwnedPassiveEffect(
		ENUPassiveEffectType::ReflectStatus, TransferPassiveId) != nullptr;
	
	if (AppliedHandle.IsValid()
		&& !GTransferringStatusEffect
		&& bHasTransfer)
	{
		TGuardValue<bool> TransferGuard(GTransferringStatusEffect, true);
		TargetASC->TryApplyStatusEffect(this, StatusEffectId, 1.f, Turns);
	}
	
	return AppliedHandle;
}

int32 UNUAbilitySystemComponent::CureStatusEffects(int32 Count)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || Count <= 0)
	{
		return 0;
	}
	
	// 해제할 후보를 모음
	TArray<TPair<int32, FActiveGameplayEffectHandle>> Candidates;
	
	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		// 다른 방식으로 제거된 효과가 목록에 남아 있을 수 있음
		if (!GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}
		
		const FNUStatusEffectDefinition* Definition =
			FindStatusEffectDefinition(Pair.Value.StatusEffectId);
		
		// Ailment 태그인것만 찾아야 함
		if (!Definition || !Definition->StatusEffectTag.MatchesTag(NU::State::Ailment::Root))
		{
			continue;
		}
		
		Candidates.Emplace(Pair.Value.StatusEffectId, Pair.Key);
	}
	
	if (Candidates.Num() == 0)
	{
		return 0;
	}
	
	// ID 가 낮은 것부터 해제함
	// 표에 우선순위 칸이 없어서 정한 임시 규칙
	Candidates.Sort([](const TPair<int32, FActiveGameplayEffectHandle>& Left,
		const TPair<int32, FActiveGameplayEffectHandle>& Right)
		{
			return Left.Key < Right.Key;
		}
		);
	
	const int32 CureCount = FMath::Min(Count, Candidates.Num());
	
	for (int32 Index = 0; Index < CureCount; ++Index)
	{
		const int32 StatusEffectId = Candidates[Index].Key;
		
		RemoveActiveGameplayEffect(Candidates[Index].Value);
		TurnBasedEffects.Remove(Candidates[Index].Value);
		
		const FNUStatusEffectDefinition* Definition = FindStatusEffectDefinition(StatusEffectId);
		const FString StatusName = Definition ? Definition->KoreanName.ToString() : TEXT("?");
		
		UE_LOG(LogTemp, Warning, TEXT("상태이상 해제 : %d %s"), StatusEffectId, *StatusName);
	}
	
	// 아이콘 줄과 상태이상 조건 패시브가 같이 맞춰짐
	NotifyStatusEffectsChanged();
	
	return CureCount;
}

bool UNUAbilitySystemComponent::IsImmuneStatusEffect(const FNUStatusEffectDefinition& Definition)
{
	// 속성 면역 확인
	if (Definition.ImmuneElement.IsValid() && HasMatchingGameplayTag(Definition.ImmuneElement))
	{
		return true;
	}
	
	// 패시브에 따른 면역
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		if (Effect.EffectType == ENUPassiveEffectType::StatusImmune
			&& Effect.StatusEffectId == Definition.StatusEffectId)
		{
			return true;
		}
	}
	
	return false;
}

void UNUAbilitySystemComponent::GetActiveStatusEffects(TArray<const FNUStatusEffectDefinition*>& OutDefinitions) const
{
	OutDefinitions.Reset();

	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		// 다른 방식으로 제거된 효과가 목록에 남아 있을 수 있음
		if (!GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}

		if (const FNUStatusEffectDefinition* Definition =
			FindStatusEffectDefinition(Pair.Value.StatusEffectId))
		{
			OutDefinitions.Add(Definition);
		}
	}
}

void UNUAbilitySystemComponent::GetActiveStatusViews(TArray<FNUStatusEffectView>& OutViews) const
{
	OutViews.Reset();

	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		// 다른 방식으로 제거된 효과가 목록에 남아 있을 수 있음
		if (!GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}

		const FNUStatusEffectDefinition* Definition =
			FindStatusEffectDefinition(Pair.Value.StatusEffectId);

		if (!Definition)
		{
			continue;
		}

		FNUStatusEffectView& View = OutViews.AddDefaulted_GetRef();
		View.StatusEffectId = Pair.Value.StatusEffectId;
		View.StatusEffectTag = Definition->StatusEffectTag;
		View.KoreanName = Definition->KoreanName;
		View.Description = Definition->Description;
		View.StatusCategory = Definition->StatusCategory;
		View.MaxStack = Definition->MaxStack;

		// 스택형은 지속 턴 없이 스택만 쌓이므로 스택 수를, 나머지는 남은 턴을 띄움
		View.bBadgeIsStack = Definition->MaxStack > 1;
		View.BadgeNumber = View.bBadgeIsStack ? Pair.Value.StackCount : Pair.Value.RemainingTurns;
	}

	// TMap 순회 순서는 만료로 빈 슬롯이 생기면 바뀜
	// 정렬하지 않으면 아이콘 자리가 전투 중에 뒤바뀌고 상한에 걸렸을 때 숨는 것도 매번 달라짐
	OutViews.Sort([](const FNUStatusEffectView& Left, const FNUStatusEffectView& Right)
		{
			return Left.StatusEffectId < Right.StatusEffectId;
		}
		);
}

void UNUAbilitySystemComponent::NotifyJobResourcesChanged()
{
	OnJobResourcesChanged.Broadcast();
}

void UNUAbilitySystemComponent::NotifyStatusEffectsChanged()
{
	// 상태이상 유무를 조건으로 켜지는 스탯 패시브를 먼저 맞춘 뒤에 알림
	// 표시가 먼저 나가면 능력치 창이 한 박자 이전 값을 그림
	RefreshStatusConditionPassives();
	
	OnStatusEffectsChanged.Broadcast();
}

float UNUAbilitySystemComponent::SumStatusEffectAxis(TFunctionRef<float(const FNUStatusEffectDefinition&)> Selector) const
{
	float Sum = 0.f;
	
	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		// 다른 방식으로 제거된 효과가 목록에 남아 있을 수 있음
		if (!GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}
		
		if (const FNUStatusEffectDefinition* Definition =
			FindStatusEffectDefinition(Pair.Value.StatusEffectId))
		{
			Sum += Selector(*Definition);
		}
	}
	
	return Sum;
}

float UNUAbilitySystemComponent::GetCaptureBonus() const
{
	float Bonus = 1.f;

	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		// 다른 방식으로 제거된 효과가 목록에 남아 있을 수 있음
		if (!GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}

		if (const FNUStatusEffectDefinition* Definition = FindStatusEffectDefinition(Pair.Value.StatusEffectId))
		{
			if (Bonus <= Definition->CaptureBonus)
			{
				Bonus = Definition->CaptureBonus;
			}
		}
	}

	return Bonus;
}

int32 UNUAbilitySystemComponent::ApplyStatusSPCost(int32 BaseCost) const
{
	if (BaseCost <= 0)
	{
		return BaseCost;
	}
	
	// 증가율은 더함, 젖음 외에 SP소모량을 증가시키면 합연산으로 비율이 늘어남
	const float Rate = SumStatusEffectAxis(
		[](const FNUStatusEffectDefinition& Definition)
		{
			return Definition.SPCostRate;
		}
		);
	
	if (Rate <= KINDA_SMALL_NUMBER)
	{
		return BaseCost;
	}
	
	// 늘어나는 비용은 내림 처리
	return FMath::FloorToInt(BaseCost * (1.f + Rate));
}

int32 UNUAbilitySystemComponent::ApplySkillSPCost(int32 BaseCost) const
{
	const int32 StatusCost = ApplyStatusSPCost(BaseCost);
	if (StatusCost <= 0)
	{
		return StatusCost;
	}

	// 카드가 바꾸는 스킬 비용 (3301 운명 읽기 — 대길 -40% · 흉 +30%), 조건이 참인 줄만 더함
	float Rate = 0.f;
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;

		if (Row.EffectType != ENUJobEffectType::SkillCostMod
			|| Row.AffectedStat != ENUStatusAffectedStat::SP
			|| Row.ImplementBy == ENUEffectImplementBy::Pending
			|| !IsJobConditionMet(Row, this, nullptr))
		{
			continue;
		}

		Rate += Row.GetScaledValue();
	}

	if (FMath::IsNearlyZero(Rate))
	{
		return StatusCost;
	}

	// 줄어든 비용도 내림, 공짜가 되지 않게 1 은 남김
	return FMath::Max(1, FMath::FloorToInt(StatusCost * FMath::Max(0.f, 1.f + Rate)));
}

bool UNUAbilitySystemComponent::HasStatusEffectMatching(TFunctionRef<bool(const FNUStatusEffectDefinition&)> Predicate) const
{
	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		// 다른 방식으로 제거된 효과가 목록에 남아 있을 수 있음
		// FActiveGameplayEffectHandle::IsValid() 는 제거해도 true라서 실제 GE를 봐야 함
		if (!GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}
		
		const FNUStatusEffectDefinition* Definition =
			FindStatusEffectDefinition(Pair.Value.StatusEffectId);
		
		if (Definition && Predicate(*Definition))
		{
			return true;
		}
	}
	
	return false;
}

void UNUAbilitySystemComponent::OnRep_CurrentAvatar()
{
	RefreshAbilityActorInfo();
}

void UNUAbilitySystemComponent::OnRep_Encounter()
{
	// 전투 입력 or UI 전환 델리게이트 호출 부분
	OnBattleEncounterChanged.Broadcast(Encounter);
}

void UNUAbilitySystemComponent::RefreshAbilityActorInfo()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}
	
	// AvatarActor가 없는 경우에 Avatar를 Owner로 지정
	AActor* ActualAvatar = IsValid(CurrentAvatar) ? CurrentAvatar.Get() : Owner;
	
	InitAbilityActorInfo(Owner, ActualAvatar); 
}

void UNUAbilitySystemComponent::GrantTurnBasedEffects(const FActiveGameplayEffectHandle& Handle, int32 Turns,
	int32 StatusEffectId, UNUAbilitySystemComponent* SourceASC)
{
	FNUTurnBasedEffect Effect;
	Effect.RemainingTurns = Turns;
	Effect.StatusEffectId = StatusEffectId;
	Effect.SourceASC = SourceASC;
	
	TurnBasedEffects.Add(Handle, Effect);

	NotifyStatusEffectsChanged();
}

void UNUAbilitySystemComponent::ApplyStatusTickDamage(const FNUTurnBasedEffect& Effect)
{
	const FNUStatusEffectDefinition* Definition = FindStatusEffectDefinition(Effect.StatusEffectId);
	
	if (!Definition)
	{
		return;
	}
	
	// 턴 종료에 피해를 주는 상태이상만 여기서 처리함
	// 폭발과 감전은 TickTiming 이 OnStackFull 이라 스택이 찰 때 따로 터짐
	if (Definition->TickTiming != ENUStatusTickTiming::TurnEnd)
	{
		return;
	}
	
	// 피해가 없다면 종료
	if (Definition->DamagePerTick <= 0.f)
	{
		return;
	}
	
	// 최대체력 비례 데미지 구하기
	// 걸려있는 스택 비례
	float TickDamage = 
		GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) 
		* Definition->DamagePerTick
		* FMath::Max(1, Effect.StackCount);
	
	if (TickDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	// 틱데미지 효과를 부여한 전투원이 유효하다면 해당 컨텍스트로 보냄
	// HandleDeath 에서 Instigator로 사용
	// 부여자가 사라졌다면 자기자신을 대상으로 사용함, 처치 대상이 자기자신이 됨
	UNUAbilitySystemComponent* SourceASC = Effect.SourceASC.IsValid() ? Effect.SourceASC.Get() : this;
	
	SendStatusDamage(this, SourceASC, TickDamage);
}

void UNUAbilitySystemComponent::SendStatusDamage(UNUAbilitySystemComponent* TargetASC,
	UNUAbilitySystemComponent* SourceASC, float Damage)
{
	if (!IsValid(TargetASC) || !IsValid(SourceASC) || Damage <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		UNUGE_StatusEffect_TickDamage::StaticClass(), 1.f, SourceASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::StatusEffect::TickDamage, Damage);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void UNUAbilitySystemComponent::SendHeal(UNUAbilitySystemComponent* TargetASC,
	UNUAbilitySystemComponent* SourceASC, ENUStatusAffectedStat Stat, float Value, bool bIsPercent)
{
	if (!IsValid(TargetASC) || !IsValid(SourceASC) || Value <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	// 이미 죽은 대상은 적용 X
	if (!TargetASC->IsAlive())
	{
		return;
	}
	
	// SP 는 IncomingHealing 같은 메타 어트리뷰트를 추가하면 여기서 처리하면 됨
	if (Stat != ENUStatusAffectedStat::Health)
	{
		UE_LOG(LogTemp, Warning, TEXT("아직 회복할 수 없는 스탯입니다 : %d"),
			static_cast<int32>(Stat));
		return;
	}
	
	// 비율은 최대치 기준 회복, 비율이 아니면 고정 회복
	const float HealAmount = bIsPercent
		? TargetASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) * Value
		: Value;
	
	if (HealAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		UNUGE_SkillHeal::StaticClass(), 1.f, SourceASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Skill::Heal, HealAmount);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void UNUAbilitySystemComponent::ApplyTurnBasedHeal(UNUAbilitySystemComponent* TargetASC,
	const FNUActiveEffectRow& Row)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(TargetASC))
	{
		return;
	}
	
	if (Row.GetDurationTurns() <= 0 || Row.AffectedStat == ENUStatusAffectedStat::None)
	{
		return;
	}
	
	// 이미 부여된 회복을 갱신하는 경우
	// 같은 스킬이 이미 걸려 있으면 남은 턴만 다시 채움
	// 축까지 같이 보는 이유는 한 스킬이 HP 와 SP 를 같이 회복할 때 서로 덮지 않게 하려는 것
	for (TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TargetASC->TurnBasedEffects)
	{
		if (Pair.Value.SkillId != Row.SkillId || Pair.Value.HealStat != Row.AffectedStat)
		{
			continue;
		}
		
		// 다른 방식으로 제거된 효과가 목록에 남아 있을 수 있음
		if (!TargetASC->GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}
		
		// 그 자리에서 회복을 한 번 더 하지 않음, 다음 턴 끝에 정상 발동함
		Pair.Value.RemainingTurns = Row.GetDurationTurns();
		Pair.Value.HealValue = Row.GetScaledValue();
		Pair.Value.bHealIsPercent = Row.bIsPercent;
		Pair.Value.SourceASC = this;
		return;
	}
	
	// 새로 부여하는 경우
	// 턴 자리만 잡으면 되므로 마커 GE 사용, 실제 회복은 매 턴 따로 쏨
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
		UNUGE_StatusEffect_Marker::StaticClass(), 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	// 상태이상이 아니므로 StatusEffectId 는 0, 찾을 때는 SkillId 를 씀
	const FActiveGameplayEffectHandle Handle =
		ApplyTurnBasedEffect(SpecHandle, TargetASC, Row.GetDurationTurns(), 0);
	
	if (FNUTurnBasedEffect* Effect = TargetASC->TurnBasedEffects.Find(Handle))
	{
		Effect->SkillId = Row.SkillId;
		Effect->HealStat = Row.AffectedStat;
		Effect->HealValue = Row.Value;
		Effect->bHealIsPercent = Row.bIsPercent;
	}
}

void UNUAbilitySystemComponent::ApplyTurnBasedHealTick(const FNUTurnBasedEffect& Effect)
{
	if (Effect.HealStat == ENUStatusAffectedStat::None)
	{
		return;
	}
	
	// 부여자가 사라졌으면 자기 자신을 시전자로 씀, 회복은 처치 판정이 없어 문제 X
	UNUAbilitySystemComponent* SourceASC =
		Effect.SourceASC.IsValid() ? Effect.SourceASC.Get() : this;
	
	SendHeal(this, SourceASC, Effect.HealStat, Effect.HealValue, Effect.bHealIsPercent);
}

void UNUAbilitySystemComponent::ApplyStatusStackBurst(const FActiveGameplayEffectHandle& Handle,
	const FNUStatusEffectDefinition& Definition, UNUAbilitySystemComponent* SourceASC)
{
	// 최대 HP 비율 피해, 스택 수를 곱하지 않음
	// 표의 DamagePerTick 이 데미지 비율
	const float BurstDamage =
		GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) * Definition.DamagePerTick;
	
	// 광역 대상은 피해를 넣기 전에 모아둠
	// 폭발로 본인이 죽으면 사망 처리가 참가자 상태를 건드릴 수 있음
	TArray<UNUAbilitySystemComponent*> SplashTargets;
	if (Definition.SplashRate > KINDA_SMALL_NUMBER)
	{
		CollectSplashTargets(SplashTargets);
	}
	
	// 스택을 먼저 소멸시킴
	// 피해로 사망하면 사망 처리가 TurnBasedEffects 를 건드리므로 그 전에 정리해야 함
	RemoveActiveGameplayEffect(Handle);
	TurnBasedEffects.Remove(Handle);
	
	UNUAbilitySystemComponent* Instigator = IsValid(SourceASC) ? SourceASC : this;
	
	SendStatusDamage(this, Instigator, BurstDamage);
	
	// 감전은 대상의 아군 전체가 그 피해의 일부를 같이 받음
	const float SplashDamage = BurstDamage * Definition.SplashRate;
	
	for (UNUAbilitySystemComponent* TargetASC : SplashTargets)
	{
		SendStatusDamage(TargetASC, Instigator, SplashDamage);
	}
}

int32 UNUAbilitySystemComponent::ResolveCombatantId() const
{
	if (const ANUBattleAvatar* SelfAvatar = Cast<ANUBattleAvatar>(CurrentAvatar))
	{
		return SelfAvatar->GetCombatantId();
	}

	return IsValid(Encounter) ? Encounter->FindCombatantIdByOwner(GetOwner()) : INDEX_NONE;
}

void UNUAbilitySystemComponent::CollectSplashTargets(TArray<UNUAbilitySystemComponent*>& OutTargets) const
{
	CollectTeamTargets(true, OutTargets);
}
	
void UNUAbilitySystemComponent::CollectOpposingTargets(TArray<UNUAbilitySystemComponent*>& OutTargets) const
{
	CollectTeamTargets(false, OutTargets);
}
	
void UNUAbilitySystemComponent::CollectJobRowOwners(TArray<const UNUAbilitySystemComponent*>& OutOwners) const
{
	OutOwners.Reset();
	OutOwners.Add(this);

	// 같은 편 목록은 자신을 빼고 주므로 자신을 두 번 세지 않음, 전투 밖이면 비어서 자신만 남음
	TArray<UNUAbilitySystemComponent*> Allies;
	CollectTeamTargets(true, Allies);

	for (const UNUAbilitySystemComponent* Ally : Allies)
	{
		OutOwners.Add(Ally);
	}
}

void UNUAbilitySystemComponent::CollectTeamTargets(bool bSameTeam, TArray<UNUAbilitySystemComponent*>& OutTargets) const
{
	OutTargets.Reset();
	
	// 자기 진영을 알아야 하므로 전투원 ID 를 구함
	const int32 SelfCombatantId = ResolveCombatantId();
	if (!IsValid(Encounter) || SelfCombatantId == INDEX_NONE)
	{
		return;
	}

	FNUBattleParticipant Self;
	if (!Encounter->GetParticipant(SelfCombatantId, Self))
	{
		return;
	}
	
	TArray<int32> Allies;
	const ENUBattleTeam Team = bSameTeam
		? Self.Team
		: Self.Team == ENUBattleTeam::Ally ? ENUBattleTeam::Enemy : ENUBattleTeam::Ally;
	
	Encounter->GetLivingCombatantIds(Team, Allies);
	
	for (int32 CombatantId : Allies)
	{
		// 본인은 제외
		if (CombatantId == Self.CombatantId)
		{
			continue;
		}
		
		FNUBattleParticipant Ally;
		if (!Encounter->GetParticipant(CombatantId, Ally))
		{
			continue;
		}
		
		UNUAbilitySystemComponent* AllyASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Ally.ASCOwner));
		
		if (IsValid(AllyASC))
		{
			OutTargets.Add(AllyASC);
		}
	}
}

FActiveGameplayEffectHandle UNUAbilitySystemComponent::FindStatusEffectHandle(int32 StatusEffectId)
{
	if (StatusEffectId <= 0)
	{
		return FActiveGameplayEffectHandle();
	}
	
	for (const TPair<FActiveGameplayEffectHandle, FNUTurnBasedEffect>& Pair : TurnBasedEffects)
	{
		if (Pair.Value.StatusEffectId != StatusEffectId)
		{
			continue;
		}
		
		if (!GetActiveGameplayEffect(Pair.Key))
		{
			continue;
		}
		
		return Pair.Key;
	}
	
	return FActiveGameplayEffectHandle();
}

void UNUAbilitySystemComponent::AddStatusEffectStack(const FActiveGameplayEffectHandle& Handle, int32 MaxStack,
	int32 Turns, UNUAbilitySystemComponent* SourceASC)
{
	FNUTurnBasedEffect* Effect = TurnBasedEffects.Find(Handle);
	if (!Effect)
	{
		return;
	}
	
	const int32 Maximum = FMath::Max(1, MaxStack);
	
	if (Effect->StackCount < Maximum)
	{
		++Effect->StackCount;
	}
	
	// 다시 부여할 경우엔 지속시간을 갱신함
	Effect->RemainingTurns = Turns;
	
	// 마지막으로 부여한사람이 처치 판정되도록 기록
	Effect->SourceASC = SourceASC;
	
	// 상한에 닿았으므로 터뜨림
	// 아래에서 GE 를 제거하면 Effect 포인터가 무효가 되므로 필요한 값을 먼저 읽어야 함
	if (Effect->StackCount >= Maximum)
	{
		const FNUStatusEffectDefinition* Definition = FindStatusEffectDefinition(Effect->StatusEffectId);

		if (Definition
			&& Definition->TickTiming == ENUStatusTickTiming::OnStackFull
			&& Definition->DamagePerTick > 0.f)
		{
			ApplyStatusStackBurst(Handle, *Definition, SourceASC);
		}
	}

	// 스택만 오른 경우와 터진 경우 모두 표시가 바뀌므로 변경 알림
	NotifyStatusEffectsChanged();
}

const FNUStatusEffectDefinition* UNUAbilitySystemComponent::FindStatusEffectDefinition(int32 StatusId) const
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UNUBattleDataSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
	
	return IsValid(Subsystem) ? Subsystem->FindStatusEffectDefinition(StatusId) : nullptr;
}


void UNUAbilitySystemComponent::RebuildOwnedPassiveEffects()
{
	OwnedPassiveEffects.Reset();
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Subsystem = IsValid(GameInstance) 
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() 
		: nullptr;
	
	if (!IsValid(Subsystem))
	{
		return;
	}
	
	int32 MissingRows = 0;
	
	for (int32 PassiveId : PassiveSkillIds)
	{
		const TArray<FNUPassiveEffectRow>* Effects = Subsystem->FindPassiveEffects(PassiveId);
		if (!Effects || Effects->Num() == 0)
		{
			MissingRows += 1;
			continue;
		}
		
		// 행을 값으로 복사한다, 표를 재임포트하면 서브시스템 배열이 재할당되기 때문
		for (const FNUPassiveEffectRow& Row : *Effects)
		{
			FNUOwnedPassiveEffect& Owned = OwnedPassiveEffects.AddDefaulted_GetRef();
			Owned.PassiveId = PassiveId;
			Owned.Row = Row;
		}
	}
	
	if (MissingRows > 0 && OwnedPassiveEffects.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("패시브 효과 줄을 하나도 찾지 못했습니다 (보유 %d개) - "
				"DT_PassiveSkillEffect 와 설정의 PassiveEffectTable 을 확인해야합니다"),
			PassiveSkillIds.Num());
	}
}

const FNUPassiveEffectRow* UNUAbilitySystemComponent::FindOwnedPassiveEffect(ENUPassiveEffectType EffectType,
	int32& OutPassiveId) const
{
	OutPassiveId = 0;
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		if (Owned.Row.EffectType != EffectType)
		{
			continue;
		}
		
		OutPassiveId = Owned.PassiveId;
		return &Owned.Row;
	}
	
	return nullptr;
}

FString UNUAbilitySystemComponent::GetPassiveName(int32 PassiveId) const
{
	const FNUPassiveDefinition* Definition = FindPassiveDefinition(PassiveId);
	return Definition ? Definition->KoreanName.ToString() : FString::FromInt(PassiveId);
}

const FNUPassiveDefinition* UNUAbilitySystemComponent::FindPassiveDefinition(int32 PassiveId) const
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UNUBattleDataSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
	
	return IsValid(Subsystem) ? Subsystem->FindPassiveDefinition(PassiveId) : nullptr;
}

void UNUAbilitySystemComponent::ApplyPassiveStatModifiers()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	// 목록이 바뀌었으니 이전 것을 지우고 새로 걸음
	if (PassiveStatHandle.IsValid())
	{
		RemoveActiveGameplayEffect(PassiveStatHandle);
		PassiveStatHandle.Invalidate();
	}
	
	TMap<FGameplayTag, float> Magnitudes;
	NUStatusEffect::FillNeutralStatModifiers(Magnitudes);
	
	// 상태이상시 조건이 붙은 행(ex) 2827 고난의 갑주)이 지금 켜지는지
	// 이 값을 들고 있다가 상태이상이 바뀌었을 때 다시 걸지 판단함
	const bool bStatusActive = HasStatusEffectMatching(
		[](const FNUStatusEffectDefinition&)
		{
			return true;
		}
		);
	
	bStatusConditionApplied = bStatusActive;
	bHasStatusConditionPassive = false;
	
	bool bAnyAxis = false;
	
	// 패시브 목록이 아니라 효과 줄 목록을 확인함
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const int32 PassiveId = Owned.PassiveId;
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		// 스탯 증감이 아니면 여기서 다룰 것이 아님
		// 발동 타이밍이 정해진건 해당 시점에서 따로 걸어야 함
		if (Effect.EffectType != ENUPassiveEffectType::StatModifier)
		{
			continue;
		}
		
		// 상시와 상태이상시 두 트리거만 이 GE 에 실림
		const bool bIsOnStatus = Effect.Trigger == ENUPassiveTrigger::OnStatusEffect;
		if (Effect.Trigger != ENUPassiveTrigger::Always && !bIsOnStatus)
		{
			continue;
		}
		
		if (bIsOnStatus)
		{
			// 조건이 붙은 행을 갖고 있다는 표시, 상태이상이 바뀔 때 다시 걸 대상인지 가름
			bHasStatusConditionPassive = true;
			
			if (!bStatusActive)
			{
				continue;
			}
		}
		
		// 변경하려는 축 찾기
		const NUStatusEffect::FNUStatModifierAxis* Axis =
			NUStatusEffect::GetStatModifierAxes().Find(Effect.AffectedStat);
		
		if (!Axis)
		{
			UE_LOG(LogTemp, Warning, TEXT("패시브 %d 의 스탯 축이 GE에 없습니다"), PassiveId);
			continue;
		}
		
		// 같은 스탯을 변경하려 하면 비율들을 더함 (합연산)
		if (float* Multiplier = Magnitudes.Find(Axis->MultiplierTag))
		{
			*Multiplier += Effect.GetScaledValue();
			bAnyAxis = true;
		}
	}
	
	// 상시 직업 스킬의 스탯 증감도 같은 GE 에 실림, 풀이 하나여야 합연산이 맞음
	{
		TArray<int32> JobIndices;
		SelectJobRows(OwnedJobEffects, ENUJobEffectType::StatModifier, this, nullptr, JobIndices);
		
		for (const int32 Index : JobIndices)
		{
			const FNUJobSkillEffectRow& Row = OwnedJobEffects[Index].Row;
			
			// 공격마다 값이 갈리는 치명타 줄(3114 단발 · 3015 잃은 HP)은 실행기가, 자원 비례 스탯(3104 템포)과
			// 상태 중 스탯(3104 번아웃)은 따로 건 GE 가 맡음 — 여기 실으면 GE 를 걸 때 값으로 굳음
			if (IsPerAttackCriticalRow(Row) || IsResourceScaledStatRow(Row, GetWorld()) || IsJobStateStatRow(Row))
			{
				continue;
			}
			
			// 조건이 붙은 스탯 줄은 여기서 적용하면 안됨
			if (Row.ConditionType != ENUEffectCondition::None)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("직업 %d 의 스탯 줄에 조건이 붙어 있어 건너뜁니다 — 다시 거는 자리가 필요합니다"),
					OwnedJobEffects[Index].JobSkillId);
				continue;
			}
			
			const NUStatusEffect::FNUStatModifierAxis* Axis =
				NUStatusEffect::GetStatModifierAxes().Find(Row.AffectedStat);
			
			if (!Axis)
			{
				UE_LOG(LogTemp, Warning, TEXT("직업 %d 의 스탯 축이 GE에 없습니다"),
					OwnedJobEffects[Index].JobSkillId);
				continue;
			}
			
			const float Amount = JobAdditiveAmount(Row, JobValueSteps(Row, this));
			
			// 어트리뷰트 자체가 비율인 축(치명타 확률·피해)은 백분율 값이 합으로 들어감
			//  ex) 치명타 확률 20% 에 +12%는 32%
			const bool bAdditiveAxis = NUStatusEffect::IsRatioStat(Row.AffectedStat) || !Row.bIsPercent;
			
			if (float* Slot = Magnitudes.Find(bAdditiveAxis ? Axis->AdditiveTag : Axis->MultiplierTag))
			{
				*Slot += Amount;
				bAnyAxis = true;
			}
		}
	}
	
	// 변경할 게 없었다면 종료
	if (!bAnyAxis)
	{
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
		UNUGE_PassiveStatModifier::StaticClass(), 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
	}
	
	PassiveStatHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UNUAbilitySystemComponent::RefreshStatusConditionPassives()
{
	// 이런 행을 안 가진 전투원이 대다수라 여기서 바로 끊음
	if (!bHasStatusConditionPassive)
	{
		return;
	}
	
	// 조건이 뒤집힐 때만 다시 걸음
	// 부르는 쪽이 부여·스택·만료·정리 네 곳이라, 스택이 오를 때마다 다시 걸면
	// 값은 그대로인데 GE 만 제거했다 거는 일이 반복됨
	const bool bStatusActive = HasStatusEffectMatching(
		[](const FNUStatusEffectDefinition&)
		{
			return true;
		}
		);
	
	if (bStatusActive == bStatusConditionApplied)
	{
		return;
	}
	
	ApplyPassiveStatModifiers();
}

void UNUAbilitySystemComponent::RefreshResourceScaledStats(FName ResourceName)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	const UWorld* World = GetWorld();
	
	// 바뀐 자원을 기준으로 삼는 줄이 없으면 할 것이 없음, 이름을 비우면 자원 아무거나
	const bool bRelevant = OwnedJobEffects.ContainsByPredicate(
		[World, ResourceName](const FNUOwnedJobEffect& Owned)
		{
			return IsResourceScaledStatRow(Owned.Row, World)
				&& (ResourceName.IsNone() || Owned.Row.ValuePer == ResourceName);
		});
	
	if (!bRelevant)
	{
		// 보유 줄이 빠졌으면 걸어 둔 것도 걷음
		if (ResourceName.IsNone() && ResourceScaledStatHandle.IsValid())
		{
			RemoveActiveGameplayEffect(ResourceScaledStatHandle);
			ResourceScaledStatHandle.Invalidate();
		}
		
		return;
	}
	
	TMap<FGameplayTag, float> Magnitudes;
	NUStatusEffect::FillNeutralStatModifiers(Magnitudes);
	
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;
		
		if (!IsResourceScaledStatRow(Row, World))
		{
			continue;
		}
		
		const NUStatusEffect::FNUStatModifierAxis* Axis =
			NUStatusEffect::GetStatModifierAxes().Find(Row.AffectedStat);
		
		if (!Axis)
		{
			continue;
		}
		
		// 한 번도 안 쌓인 자원은 칸이 없어 0 으로 봄
		const float Amount = JobAdditiveAmount(Row, FMath::Max(0.f, JobValueSteps(Row, this)));
		
		// 상시 스탯 GE 와 같은 규칙 — 수치 줄은 합으로, 백분율 줄은 배율에 더함
		if (float* Slot = Magnitudes.Find(Row.bIsPercent ? Axis->MultiplierTag : Axis->AdditiveTag))
		{
			*Slot += Amount;
		}
	}
	
	// 걸어 둔 것이 있으면 값만 고침
	// 지웠다 다시 걸면 그 사이 최대치가 내려가 AdjustVitalToMaxChange 가 현재 SP 를 깎고, 다시 올라도 안 돌아옴
	if (ResourceScaledStatHandle.IsValid() && GetActiveGameplayEffect(ResourceScaledStatHandle))
	{
		UpdateActiveGameplayEffectSetByCallerMagnitudes(ResourceScaledStatHandle, Magnitudes);
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
		UNUGE_PassiveStatModifier::StaticClass(), 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
	}
	
	ResourceScaledStatHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UNUAbilitySystemComponent::RefreshJobStateStats()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	const bool bRelevant = OwnedJobEffects.ContainsByPredicate(
		[](const FNUOwnedJobEffect& Owned)
		{
			return IsJobStateStatRow(Owned.Row);
		});

	if (!bRelevant)
	{
		// 보유 줄이 빠졌으면 걸어 둔 것도 걷음
		if (JobStateStatHandle.IsValid())
		{
			RemoveActiveGameplayEffect(JobStateStatHandle);
			JobStateStatHandle.Invalidate();
		}

		return;
	}

	TMap<FGameplayTag, float> Magnitudes;
	NUStatusEffect::FillNeutralStatModifiers(Magnitudes);

	bool bAnyActive = false;

	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;

		// 꺼진 상태의 줄은 중립값으로 남김
		if (!IsJobStateStatRow(Row) || !IsJobStateActive(Row.GetConditionName()))
		{
			continue;
		}

		const NUStatusEffect::FNUStatModifierAxis* Axis =
			NUStatusEffect::GetStatModifierAxes().Find(Row.AffectedStat);

		if (!Axis)
		{
			continue;
		}

		const float Amount = JobAdditiveAmount(Row, FMath::Max(0.f, JobValueSteps(Row, this)));

		// 상시 스탯 GE 와 같은 규칙 - 수치 줄은 합으로, 백분율 줄은 배율에 더함
		if (float* Slot = Magnitudes.Find(Row.bIsPercent ? Axis->MultiplierTag : Axis->AdditiveTag))
		{
			*Slot += Amount;
			bAnyActive = true;
		}
	}

	// 걸어 둔 것이 있으면 값만 고침
	// 지웠다 다시 걸면 그 사이 최대치가 내려가 AdjustVitalToMaxChange 가 현재 SP 를 깎고, 다시 올라도 안 돌아옴
	if (JobStateStatHandle.IsValid() && GetActiveGameplayEffect(JobStateStatHandle))
	{
		UpdateActiveGameplayEffectSetByCallerMagnitudes(JobStateStatHandle, Magnitudes);
		return;
	}

	// 켜진 상태가 없으면 걸 것이 없음, 처음 켜질 때 건다
	if (!bAnyActive)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
		UNUGE_PassiveStatModifier::StaticClass(), 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
	}

	JobStateStatHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UNUAbilitySystemComponent::ApplyBattleStartPassives()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	// 보유 목록 자체를 바꾸는 패시브라 목록을 돌면서 부르면 안 됨, 타입으로 직접 물음
	int32 SyncPassiveId = 0;
	if (FindOwnedPassiveEffect(ENUPassiveEffectType::CopyPassive, SyncPassiveId))
	{
		ApplySynchronizePassive(SyncPassiveId);
	}
}

void UNUAbilitySystemComponent::ApplySynchronizePassive(int32 SelfPassiveId)
{
	// 이미 한 번 복사했으면 다시 하지 않음
	if (bPassiveSkillIdsSynced)
	{
		return;
	}
	
	// 여러 명일 때는 랜덤 적
	TArray<UNUAbilitySystemComponent*> Opponents;
	CollectOpposingTargets(Opponents);
	
	if (Opponents.Num() == 0)
	{
		return;
	}
	
	const UNUAbilitySystemComponent* Source = Opponents[FMath::RandRange(0, Opponents.Num() - 1)];
	if (!IsValid(Source))
	{
		return;
	}
	
	TArray<int32> Synced = PassiveSkillIds;
	int32 CopiedCount = 0;
	
	for (int32 PassiveId : Source->GetPassiveSkillIds())
	{
		// 동기화끼리 만나면 아무 일도 없음
		if (PassiveId == SelfPassiveId)
		{
			continue;
		}
		
		if (Synced.Contains(PassiveId))
		{
			continue;
		}
		
		Synced.Add(PassiveId);
		++CopiedCount;
	}
	
	if (CopiedCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("패시브 %d 동기화 : %s 에게서 가져올 것이 없음"),
			SelfPassiveId, *GetNameSafe(Source->GetOwner()));
		
		return;
	}
	
	// 되돌릴 목록을 먼저 챙김, 이 전투가 끝나면 ClearBattleScopedState 가 이것으로 돌려놓음
	PassiveSkillIdsBeforeSync = PassiveSkillIds;
	bPassiveSkillIdsSynced = true;
	
	// 상시 스탯 패시브가 다시 걸려야 하므로 세터를 거쳐 넣음
	SetPassiveSkillIds(Synced);
	
	UE_LOG(LogTemp, Warning, TEXT("패시브 %d 동기화 : %s 에게서 %d개를 가져옴 (%s)"),
		SelfPassiveId, *GetNameSafe(Source->GetOwner()), CopiedCount,
		*FString::JoinBy(Synced, TEXT(", "), [](int32 Id) { return FString::FromInt(Id); }));
}

void UNUAbilitySystemComponent::ApplyTurnStartPassives()
{
	DrawJobCard();
	ApplyHealPassivesFor(ENUPassiveTrigger::OnTurnStart);
}

FName UNUAbilitySystemComponent::GetCurrentJobCard() const
{
	return CurrentJobCard;
}

void UNUAbilitySystemComponent::DrawJobCard()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	// 카드 조건을 가진 줄이 없으면 카드를 뽑는 직업이 아님
	const bool bHasCardRows = OwnedJobEffects.ContainsByPredicate([](const FNUOwnedJobEffect& Owned)
	{
		return Owned.Row.ConditionType == ENUEffectCondition::OnCardDrawn;
	});

	if (!bHasCardRows)
	{
		CurrentJobCard = NAME_None;
		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	// 마이너 아르카나는 성배를 여는 전직 노드를 가진 쪽, 확률표가 갈래마다 다름
	const FNUJobResourceDefinition* Chalice = IsValid(Data) ? Data->FindJobResource(TEXT("성배")) : nullptr;
	const bool bMinor = Chalice && JobSkillIds.Contains(Chalice->JobNodeId);

	// 대길 · 길 · 흉 · 대흉 확률(%), 대흉은 마이너에만 있음
	float Rates[4] = { 0.f, 100.f, 0.f, 0.f };

	TArray<FNUFateCardRateRow> Rows;
	if (IsValid(Data))
	{
		Data->GetFateCardRates(bMinor ? ENUFateArcana::Minor : ENUFateArcana::Major, Rows);
	}

	const float Luck = GetNumericAttribute(UNUStatAttributeSet::GetLuckAttribute());

	if (!Rows.IsEmpty())
	{
		const auto ToRates = [](const FNUFateCardRateRow& Row, float (&Out)[4])
		{
			Out[0] = Row.Rate_GreatFortune;
			Out[1] = Row.Rate_Fortune;
			Out[2] = Row.Rate_Misfortune;
			Out[3] = Row.Rate_GreatMisfortune;
		};

		// LUK 사이는 앞뒤 구간을 선형으로 이음 (기획 확정), 양끝 밖은 끝 줄 그대로
		int32 UpperIndex = 0;
		while (UpperIndex < Rows.Num() && Rows[UpperIndex].LUK < Luck)
		{
			++UpperIndex;
		}

		if (UpperIndex <= 0)
		{
			ToRates(Rows[0], Rates);
		}
		else if (UpperIndex >= Rows.Num())
		{
			ToRates(Rows.Last(), Rates);
		}
		else
		{
			float Lower[4];
			float Upper[4];
			ToRates(Rows[UpperIndex - 1], Lower);
			ToRates(Rows[UpperIndex], Upper);

			const float Span = static_cast<float>(Rows[UpperIndex].LUK - Rows[UpperIndex - 1].LUK);
			const float Alpha = Span > 0.f ? (Luck - Rows[UpperIndex - 1].LUK) / Span : 1.f;

			for (int32 Index = 0; Index < 4; ++Index)
			{
				Rates[Index] = FMath::Lerp(Lower[Index], Upper[Index], Alpha);
			}
		}
	}
	else
	{
		// 표가 아직 없으면 설정의 임시 확률표(메이저 기준)로 돌아감, 그것도 없으면 길만 나옴
		const TArray<FNUGamblerCardOdds>& Odds = GetDefault<UNUBattleDataSettings>()->GamblerCardOdds;

		if (Odds.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[직업] 갬블러 카드 확률표(FateCardRateTable)가 없어 길만 뽑음"));
		}
		else
		{
			int32 UpperIndex = 0;
			while (UpperIndex < Odds.Num() && Odds[UpperIndex].Luck < Luck)
			{
				++UpperIndex;
			}

			const int32 Low = FMath::Clamp(UpperIndex - 1, 0, Odds.Num() - 1);
			const int32 High = FMath::Clamp(UpperIndex, 0, Odds.Num() - 1);
			const float Span = static_cast<float>(Odds[High].Luck - Odds[Low].Luck);
			const float Alpha = Span > 0.f ? FMath::Clamp((Luck - Odds[Low].Luck) / Span, 0.f, 1.f) : 0.f;

			Rates[0] = FMath::Lerp(Odds[Low].GreatPercent, Odds[High].GreatPercent, Alpha);
			Rates[1] = FMath::Lerp(Odds[Low].GoodPercent, Odds[High].GoodPercent, Alpha);
			Rates[2] = FMath::Max(0.f, 100.f - Rates[0] - Rates[1]);
			Rates[3] = 0.f;
		}
	}

	// 카드 이름은 효과표 ConditionValue 와 같은 글자여야 조건이 걸림
	static const FName GreatCard(TEXT("대길"));
	static const FName GoodCard(TEXT("길"));
	static const FName BadCard(TEXT("흉"));
	static const FName GreatBadCard(TEXT("대흉"));

	const float Roll = FMath::FRandRange(0.f, 100.f);

	FName Drawn = GreatBadCard;
	if (Roll < Rates[0])
	{
		Drawn = GreatCard;
	}
	else if (Roll < Rates[0] + Rates[1])
	{
		Drawn = GoodCard;
	}
	else if (Roll < Rates[0] + Rates[1] + Rates[2])
	{
		Drawn = BadCard;
	}

	// 마이너 아르카나는 흉 · 대흉을 저장하고 그 턴은 길 효과를 대신 받음 (기획)
	// 성배 · 검에 쌓는 것은 아직 안 함
	const bool bStoredBad = bMinor && (Drawn == BadCard || Drawn == GreatBadCard);
	CurrentJobCard = bStoredBad ? GoodCard : Drawn;

	UE_LOG(LogTemp, Log, TEXT("[직업] %s 카드 뽑음 : %s%s (%s, LUK %.0f, 대길 %.1f%% · 길 %.1f%% · 흉 %.1f%% · 대흉 %.1f%%, 굴림 %.1f)"),
		*GetNameSafe(Owner), *Drawn.ToString(), bStoredBad ? TEXT(" -> 길 효과") : TEXT(""),
		bMinor ? TEXT("마이너") : TEXT("메이저"), Luck, Rates[0], Rates[1], Rates[2], Rates[3], Roll);

	OnJobCardDrawn.Broadcast(CurrentJobCard);
	NotifyJobResourcesChanged();
}

void UNUAbilitySystemComponent::ApplyOnKillPassives()
{
	ApplyHealPassivesFor(ENUPassiveTrigger::OnKill);
	
	// 회복이 아니라 SP 상한을 올리는 행이라 위 목록을 안 탄다
	ApplyEnergyCyclePassive();
}
	
void UNUAbilitySystemComponent::ApplyEnergyCyclePassive()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	int32 PassiveId = 0;
	const FNUPassiveEffectRow* Effect = FindOwnedPassiveEffect(ENUPassiveEffectType::ResourceGain, PassiveId);
	
	// 다음 턴에는 조건 칸에서 옴, 즉시 주는 자원 획득 행이 생기면 여기서 나뉨
	if (!Effect || Effect->ConditionType != ENUEffectCondition::NextTurn)
	{
		return;
	}
	
	// 지금 실행 중인 행동이 내 것일 때만 값이 나온다
	// 들고 있지 않고 그때그때 묻는 이유 - 들고 있으면 이 값은 언제 지우나가 또 생기고,
	// 도트로 죽인 경우에 지난 행동의 소모량을 물려받게 된다
	const ANUBattleEncounter* BattleEncounter = GetBattleEncounter();
	const int32 SPSpent = IsValid(BattleEncounter)
		? BattleEncounter->GetRunningActionSPSpend(this) : 0;
	
	// 표의 값은 비율 (2834 는 0.5 = 50%), 올림이 아니라 반올림
	const int32 BonusSP = FMath::RoundToInt(SPSpent * Effect->GetScaledValue());
	if (BonusSP <= 0)
	{
		return;
	}
	
	// SP 는 행동이 끝날 때마다 MaxSP 로 가득차고, 그 위로는 못 올라감(PreAttributeChange 에서 Clamp)
	// 그래서 추가 획득을 상한 증가로 처리함, 올려두면 RefillSP 가 알아서 그만큼 채운다
	// 만료될 때는 AdjustVitalToMaxChange 가 남은 SP 를 새 상한으로 깎아줌
	ApplyTurnScopedStatModifier(this, ENUStatusAffectedStat::MaxSP,
		static_cast<float>(BonusSP), false, BonusResourceTurns);
	
	UE_LOG(LogTemp, Warning, TEXT("패시브 %d %s 발동 : 소모 SP %d -> 다음 턴 SP 상한 +%d"),
		PassiveId, *GetPassiveName(PassiveId), SPSpent, BonusSP);
}

void UNUAbilitySystemComponent::ApplyShield(UNUAbilitySystemComponent* SourceASC, int32 SourceSkillId,
	float Value, bool bIsPercent, int32 Turns)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(SourceASC))
	{
		return;
	}
	
	// 쓰러진 전투원에게는 걸지 않음, 회복과 같은 기준
	if (!IsAlive())
	{
		return;
	}
	
	// 비율은 이 자리에서 절대값으로 풀어 담음
	// 비율인 채로 들고 있으면 전투 중에 최대 체력이 오를 때 이미 걸린 보호막이 같이 커짐
	// 같은 편이 가진 "아군 보호막 x2" (3444 하나된 존재) 도 걸리는 이 순간에 곱함
	const float Amount = (bIsPercent
		? GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) * Value
		: Value) 
		* CalcShieldReceivedMultiplier();
	
	if (Amount <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	// 같은 스킬이 이미 걸려 있으면 중첩하지 않고 덮어씀
	int32 Index = Shields.IndexOfByPredicate([SourceSkillId](const FNUShieldInstance& Shield)
	{
		return Shield.SourceSkillId == SourceSkillId;
	});
	
	if (Index == INDEX_NONE)
	{
		Index = Shields.AddDefaulted();
	}
	
	FNUShieldInstance& Shield = Shields[Index];
	
	Shield.SourceSkillId = SourceSkillId;
	Shield.Amount = Amount;
	Shield.RemainingTurns = Turns;
	Shield.SourceASC = SourceASC;
	
	OnShieldChanged.Broadcast();
}

float UNUAbilitySystemComponent::AbsorbWithShield(float IncomingDamage)
{
	if (Shields.Num() == 0 || IncomingDamage <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	
	float Remaining = IncomingDamage;
	float Absorbed = 0.f;
	
	// 먼저 걸린 것부터 닳게 함, 나중에 걸린 것이 보통 더 오래 남기 때문
	for (FNUShieldInstance& Shield : Shields)
	{
		if (Remaining <= KINDA_SMALL_NUMBER)
		{
			break;
		}
		
		const float Taken = FMath::Min(Shield.Amount, Remaining);
		
		Shield.Amount -= Taken;
		Remaining -= Taken;
		Absorbed += Taken;
	}
	
	// 다 닳은 보호막은 삭제
	Shields.RemoveAll([](const FNUShieldInstance& Shield)
	{
		return Shield.Amount <= KINDA_SMALL_NUMBER;
	});
	
	if (Absorbed > KINDA_SMALL_NUMBER)
	{
		OnShieldChanged.Broadcast();
	}
	
	return Absorbed;
}

float UNUAbilitySystemComponent::GetShieldAmount() const
{
	float Total = 0.f;
	
	for (const FNUShieldInstance& Shield : Shields)
	{
		Total += Shield.Amount;
	}
	
	return Total;
}

bool UNUAbilitySystemComponent::HasShield() const
{
	return GetShieldAmount() > KINDA_SMALL_NUMBER;
}

const TArray<FNUShieldInstance>& UNUAbilitySystemComponent::GetShields() const
{
	return Shields;
}

void UNUAbilitySystemComponent::ApplyReflect(UNUAbilitySystemComponent* SourceASC, int32 SourceSkillId,
	float Ratio, int32 Turns)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(SourceASC))
	{
		return;
	}
	
	if (Ratio <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 %d 반사 비율이 비어 있어 걸지 않습니다"), SourceSkillId);
		return;
	}
	
	int32 Index = Reflects.IndexOfByPredicate([SourceSkillId](const FNUReflectInstance& Reflect)
	{
		return Reflect.SourceSkillId == SourceSkillId;
	});
	
	if (Index == INDEX_NONE)
	{
		Index = Reflects.AddDefaulted();
	}
	
	FNUReflectInstance& Reflect = Reflects[Index];
	
	Reflect.SourceSkillId = SourceSkillId;
	Reflect.Ratio = Ratio;
	Reflect.RemainingTurns = Turns;
	Reflect.SourceASC = SourceASC;
}

void UNUAbilitySystemComponent::TriggerReflectOnAttack(float DamageDealt)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	// 반사 피해가 다시 반사를 부르는 것을 막음
	// 반사도 IncomingDamage 로 들어가 피해 처리 자리를 다시 지나가기 때문
	if (GResolvingReflect || Reflects.Num() == 0 || !IsAlive())
	{
		return;
	}
	
	TGuardValue<bool> ReflectGuard(GResolvingReflect, true);
	
	// 반사로 쓰러지면 사망 처리가 목록을 비울 수 있어 복사해두고 순회함
	const TArray<FNUReflectInstance> Snapshot = Reflects;
	
	for (const FNUReflectInstance& Reflect : Snapshot)
	{
		const float ReflectDamage = DamageDealt * Reflect.Ratio;
		if (ReflectDamage <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		
		// 이 효과를 건 쪽을 가해자로 남김, 반사로 쓰러뜨렸을 때 처치 판정이 그쪽으로 가야 함
		// 건 쪽이 이미 사라졌으면 효과를 들고 있는 자신을 출처로 둠
		UNUAbilitySystemComponent* InstigatorASC =
			Reflect.SourceASC.IsValid() 
				? Reflect.SourceASC.Get() 
				: this;
		
		// 방어·치명타·상성을 타지 않는 고정 피해라 도트와 같은 통로를 씀
		SendStatusDamage(this, InstigatorASC, ReflectDamage);
		
		UE_LOG(LogTemp, Warning, TEXT("반사 피해 %.0f (스킬 %d, 가한 피해 %.0f × %.0f%%)"),
			ReflectDamage, Reflect.SourceSkillId, DamageDealt, Reflect.Ratio * 100.f);
	}
}

const TArray<FNUReflectInstance>& UNUAbilitySystemComponent::GetReflects() const
{
	return Reflects;
}

void UNUAbilitySystemComponent::ApplyElementPowerMod(UNUAbilitySystemComponent* SourceASC,
	int32 SourceSkillId, const FGameplayTag& Element, float Multiplier, int32 Turns, int32 RequiredUses)
{
	if (!Element.IsValid() || Multiplier <= 0.f)
	{
		return;
	}
	
	// 쓰러진 전투원에게는 걸지 않음
	if (!IsAlive())
	{
		return;
	}
	
	// 같은 스킬이 이미 걸려 있으면 중첩하지 않고 덮어씀
	// 속성까지 같이 봄, 한 스킬이 속성 여럿을 건드리게 되면 번호만으로는 서로 덮어씀
	int32 Index = ElementPowers.IndexOfByPredicate(
		[SourceSkillId, &Element](const FNUElementPowerInstance& Instance)
		{
			return Instance.SourceSkillId == SourceSkillId && Instance.Element == Element;
		});
	
	if (Index == INDEX_NONE)
	{
		Index = ElementPowers.AddDefaulted();
	}
	
	FNUElementPowerInstance& Instance = ElementPowers[Index];
	
	Instance.SourceSkillId = SourceSkillId;
	Instance.Element = Element;
	Instance.Multiplier = Multiplier;
	Instance.RemainingTurns = Turns;
	
	// 지금까지 쓴 횟수를 기준선으로 잡음, 이 스킬 자체는 이미 세어진 뒤라 포함되지 않음
	Instance.ActivateAtUseCount = GetElementSkillUseCount(Element) + FMath::Max(0, RequiredUses);
}

void UNUAbilitySystemComponent::NoteElementSkillUsed(const FGameplayTag& Element)
{
	if (!Element.IsValid())
	{
		return;
	}
	
	++ElementSkillUseCounts.FindOrAdd(Element);
}

int32 UNUAbilitySystemComponent::GetElementSkillUseCount(const FGameplayTag& Element) const
{
	const int32* Found = ElementSkillUseCounts.Find(Element);
	
	return Found ? *Found : 0;
}

float UNUAbilitySystemComponent::GetElementPowerMultiplier(const FGameplayTagContainer& AttackTags) const
{
	if (ElementPowers.Num() == 0 || !AttackTags.HasTag(NU::Element::Root))
	{
		return 1.f;
	}
	
	float Multiplier = 1.f;
	
	for (const FNUElementPowerInstance& Instance : ElementPowers)
	{
		// 이번 공격이 그 속성이어야 함
		if (!Instance.Element.IsValid() || !AttackTags.HasTagExact(Instance.Element))
		{
			continue;
		}
		
		// 아직 조건 횟수를 못 채운 것은 걸려만 있고 꺼져 있음 (2407 과전류)
		if (GetElementSkillUseCount(Instance.Element) < Instance.ActivateAtUseCount)
		{
			continue;
		}
		
		Multiplier *= Instance.Multiplier;
	}
	
	return Multiplier;
}

const TArray<FNUElementPowerInstance>& UNUAbilitySystemComponent::GetElementPowers() const
{
	return ElementPowers;
}

void UNUAbilitySystemComponent::ApplyDamageTakenMod(int32 SourceSkillId, float Multiplier, int32 Turns)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	// 쓰러진 전투원에게는 걸지 않음
	if (!IsAlive())
	{
		return;
	}

	// 배수가 1이면 곱해도 그대로라 무시함
	if (Multiplier <= 0.f || FMath::IsNearlyEqual(Multiplier, 1.f))
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 %d 받는 피해 배수가 %.2f 라 걸지 않습니다"), SourceSkillId, Multiplier);
		return;
	}

	// 같은 스킬이 이미 걸려 있으면 중첩하지 않고 덮어씀
	int32 Index = DamageTakenMods.IndexOfByPredicate(
		[SourceSkillId](const FNUDamageTakenModInstance& Instance)
		{
			return Instance.SourceSkillId == SourceSkillId;
		});

	if (Index == INDEX_NONE)
	{
		Index = DamageTakenMods.AddDefaulted();
	}

	FNUDamageTakenModInstance& Instance = DamageTakenMods[Index];
	Instance.SourceSkillId = SourceSkillId;
	Instance.Multiplier = Multiplier;
	Instance.RemainingTurns = Turns;
}

const TArray<FNUDamageTakenModInstance>& UNUAbilitySystemComponent::GetDamageTakenMods() const
{
	return DamageTakenMods;
}

bool UNUAbilitySystemComponent::TryGetJobResource(FName ResourceName, int32& OutAmount) const
{
	OutAmount = 0;
	
	const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), ResourceName);
	if (!Def)
	{
		return false;
	}
	
	// 쌓이는 자원이 아니면 칸이 없음, 부르는 쪽이 다른 길로 세야 함
	if (Def->IsDerived() || Def->IsResolvedByJobNode())
	{
		return false;
	}
	
	if (const FNUJobResourceInstance* Found = JobResources.FindByPredicate(
		[ResourceName](const FNUJobResourceInstance& Instance)
		{
			return Instance.ResourceName == ResourceName;
		}))
	{
		OutAmount = Found->Amount;
	}
	
	// 칸이 아직 없는 것은 0 이라는 뜻, 자원 자체는 있는 것이므로 true
	return true;
}

int32 UNUAbilitySystemComponent::GetJobResourceMax(FName ResourceName) const
{
	const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), ResourceName);
	if (!Def)
	{
		return 0;
	}
	
	float ScaleValue = 0.f;
	
	switch (Def->MaxScaleStat)
	{
	case ENUJobResourceScaleStat::Speed:
		ScaleValue = GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute());
		break;
	
	case ENUJobResourceScaleStat::SP:
		ScaleValue = GetNumericAttribute(UNUVitalAttributeSet::GetMaxSPAttribute());
		break;
	
	case ENUJobResourceScaleStat::Intelligence:
		// INT 는 전투 어트리뷰트가 아님
		break;
	
	default:
		break;
	}
	
	return Def->ResolveMaxValue(ScaleValue);
}

bool UNUAbilitySystemComponent::IsJobResourceAtMax(FName ResourceName) const
{
	int32 Amount = 0;
	if (!TryGetJobResource(ResourceName, Amount))
	{
		return false;
	}
	
	const int32 Max = GetJobResourceMax(ResourceName);
	
	// 상한을 못 세면 false
	return Max > 0 && Amount >= Max;
}

int32 UNUAbilitySystemComponent::AddJobResource(FName ResourceName, int32 Delta, bool bApplyGainModifiers)
{
	if (Delta == 0)
	{
		return 0;
	}
	
	const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), ResourceName);
	if (!Def || Def->IsDerived() || Def->IsResolvedByJobNode())
	{
		// 자원표가 이름은 아는데 쌓는 규칙을 코드에 맡긴 자리 (공용 `자원` · 속도차이)
		UE_LOG(LogTemp, Warning, TEXT("아직 못 쌓는 자원입니다 : %s (전직 노드나 파생값이 정하는 자리)"),
			*ResourceName.ToString());
		return 0;
	}
	
	int32 Index = JobResources.IndexOfByPredicate(
		[ResourceName](const FNUJobResourceInstance& Instance)
		{
			return Instance.ResourceName == ResourceName;
		});
	
	if (Index == INDEX_NONE)
	{
		Index = JobResources.AddDefaulted();
		JobResources[Index].ResourceName = ResourceName;
		JobResources[Index].Amount = Def->InitialValue;
	}
	
	// 보유 줄이 획득량에 손대는 자리 (3437 배수 · 3230 턴당 상한), 소모에는 안 걸림
	if (bApplyGainModifiers)
	{
		Delta = ApplyJobResourceGainModifiers(JobResources[Index], Delta);
	}
	
	if (Delta == 0)
	{
		return 0;
	}
	
	const int32 Max = GetJobResourceMax(ResourceName);
	const int32 Before = JobResources[Index].Amount;
	
	const int32 After = Max > 0
		? FMath::Clamp(Before + Delta, 0, Max)
		: FMath::Max(0, Before + Delta);
	
	JobResources[Index].Amount = After;

	// 상한에 닿으면 전량 소멸하고 페널티 상태에 들어감 (3104 템포 -> 번아웃)
	// 넘는 순간이 아니라 닿았을 때 판단함 - 상한이 속도에 따라 움직여 지금 값 아래로 내려갈 수 있음
	// 다타 중이면 그 타격에서 0 이 되고 남은 타격이 0 부터 다시 쌓임
	const bool bBurnedOut = Delta > 0 && Max > 0 && After >= Max
		&& Def->OnReachMax == ENUJobResourceOnMax::BurnOut;

	if (bBurnedOut)
	{
		JobResources[Index].Amount = 0;
	}
	
	// 감소 칸을 같이 맞춰 둠, 안 맞으면 다음 턴에 엉뚱한 수가 사라짐
	if (After > Before)
	{
		// 새로 쌓였으므로 안 쌓인 턴이 0으로 돌아감 (마크의 3턴 대기)
		JobResources[Index].IdleTurns = 0;
		
		// 턴당 획득 상한이 세는 값 (3230 초과 부하), 상한을 안 거친 것(계측)은 세지 않음
		if (bApplyGainModifiers)
		{
			JobResources[Index].GainedThisTurn += After - Before;
		}
	}
	
	if (Def->DecayType == ENUJobResourceDecay::TimeoutEach)
	{
		TArray<int32>& StackTurns = JobResources[Index].StackTurns;
		
		while (StackTurns.Num() > After)
		{
			// 먼저 붙은 것이 먼저 빠짐
			StackTurns.RemoveAt(0);
		}
		
		while (StackTurns.Num() < After)
		{
			StackTurns.Add(FMath::Max(1, Def->DecayInterval));
		}
	}
	
	// 이 자원에 비례하는 스탯 줄이 있으면 값을 고침 (3104 템포 3당 SP 상한 +1)
	if (After != Before || bBurnedOut)
	{
		RefreshResourceScaledStats(ResourceName);
		
		// 대상에게 붙는 자원이면 상대가 가진 1당 줄도 (3107 마크 1중첩당 속도 -5)
		if (Def->OwnerScope == ENUJobResourceOwner::Target)
		{
			RefreshOpponentScaledStats();

			// 상대가 "가장 많이 쌓인 적" 의 이 자원으로 세는 스탯 줄도 (3207 문양 1당 SP 상한 -2)
			RefreshOpponentResourceScaledStats(ResourceName);
		}
	}

	if (bBurnedOut)
	{
		EnterJobState(*Def);
	}
	// 턴 제한이 없는 상태는 문턱에 닿는 순간 켜짐 (17018 임계 — 마법을 쓰면 닫힘)
	// 기간이 있는 AtValue 상태(공진)는 기획 답을 기다려 아직 안 켬
	if (Def->StateEnterRule == ENUJobStateEnterRule::AtValue
		&& Def->StateDurationTurns <= 0
		&& Before < Def->StateEnterValue
		&& After >= Def->StateEnterValue)
	{
		EnterJobState(*Def);
	}

	if (After != Before || bBurnedOut)
	{
		NotifyJobResourcesChanged();
	}

	return After - Before;
}

bool UNUAbilitySystemComponent::IsJobStateActive(FName StateName) const
{
	// 전투 밖에서는 상태가 없음, 턴을 셀 수 없으면 켜졌다고 하지 않음
	if (StateName.IsNone() || !IsValid(Encounter))
	{
		return false;
	}

	const int32 Turn = Encounter->GetTurnNumber();

	return JobStates.ContainsByPredicate(
		[StateName, Turn](const FNUJobStateInstance& State)
		{
			return State.StateName == StateName && State.StartTurn <= Turn && Turn <= State.EndTurn;
		});
}

bool UNUAbilitySystemComponent::IsJobStateAfterEnd(FName StateName, int32 TurnsAfter) const
{
	if (StateName.IsNone() || !IsValid(Encounter))
	{
		return false;
	}

	// 이 상태를 켜는 자원의 기간으로 갈림, 기간이 있으면 켜져 있는 동안이 곧 그 턴
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	bool bOpenEnded = false;
	if (IsValid(Data))
	{
		for (const TPair<FName, FNUJobResourceDefinition>& Pair : Data->GetJobResources())
		{
			if (Pair.Value.StateName == StateName)
			{
				bOpenEnded = Pair.Value.StateDurationTurns <= 0;
				break;
			}
		}
	}

	if (!bOpenEnded)
	{
		return IsJobStateActive(StateName);
	}

	// 소모된 그 턴의 다음 턴(TurnsAfter 가 1)에만 참
	const int32* EndedTurn = JobStateEndTurns.Find(StateName);
	return EndedTurn && Encounter->GetTurnNumber() == *EndedTurn + FMath::Max(1, TurnsAfter);
}

void UNUAbilitySystemComponent::EndJobState(FName StateName)
{
	const int32 Index = JobStates.IndexOfByPredicate(
		[StateName](const FNUJobStateInstance& State)
		{
			return State.StateName == StateName;
		});

	if (Index == INDEX_NONE)
	{
		return;
	}

	JobStates.RemoveAt(Index);

	const int32 Turn = IsValid(Encounter) ? Encounter->GetTurnNumber() : 0;
	JobStateEndTurns.Add(StateName, Turn);

	UE_LOG(LogTemp, Log, TEXT("[직업] 상태 %s : %d턴에 끝남"), *StateName.ToString(), Turn);

	RefreshJobStateStats();
	NotifyJobResourcesChanged();
}

void UNUAbilitySystemComponent::ConsumeJobResourcesOnMagicUse(int32 SkillId,
	const TArray<FNUJobSkillEffectRow>& Effects)
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	const FNUJobSkillDefinition* Definition = IsValid(Data) ? Data->FindJobSkillDefinition(SkillId) : nullptr;

	// 필살기는 마법으로 치지 않음 (기획 — 임계는 필살기를 써도 소모되지 않음), 기본 공격은 스킬 번호가 없어 여기 안 옴
	static const FName UltimateType(TEXT("필살기"));
	if (!Definition || Definition->SkillType == UltimateType)
	{
		return;
	}

	// 마법 스킬은 마법 공격력을 기준으로 피해를 주는 줄이 있는 것
	const bool bMagic = Effects.ContainsByPredicate([](const FNUJobSkillEffectRow& Row)
	{
		return Row.EffectType == ENUJobEffectType::Damage
			&& Row.AffectedStat == ENUStatusAffectedStat::MagicAttackPower;
	});

	if (!bMagic)
	{
		return;
	}

	// 비우면서 칸이 움직이므로 이름을 먼저 모아 둠
	TArray<FName> Consumed;
	for (const FNUJobResourceInstance& Instance : JobResources)
	{
		const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), Instance.ResourceName);

		if (Def && Def->DecayType == ENUJobResourceDecay::OnMagicUse && Instance.Amount > 0)
		{
			Consumed.Add(Instance.ResourceName);
		}
	}

	for (const FName ResourceName : Consumed)
	{
		const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), ResourceName);

		int32 Amount = 0;
		if (!Def || !TryGetJobResource(ResourceName, Amount))
		{
			continue;
		}

		AddJobResource(ResourceName, -Amount, false);

		UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 마법을 써서 %s 소모"), SkillId, *ResourceName.ToString());

		if (!Def->StateName.IsNone())
		{
			EndJobState(Def->StateName);
		}
	}
}

void UNUAbilitySystemComponent::EnterJobState(const FNUJobResourceDefinition& Definition, bool bForceImmediate)
{
	if (!IsValid(Encounter) || Definition.StateName.IsNone())
	{
		return;
	}

	const int32 Turn = Encounter->GetTurnNumber();

	// 행동 중에 켜지는 즉시 상태는 그 턴이 첫 턴, 다음 턴 시작 상태와 라운드 끝 처리 중에 켜지는 상태는 다음 턴이 첫 턴
	const bool bNextTurn = !bForceImmediate
		&& (Definition.StateStartTiming == ENUJobStateStartTiming::NextTurn || Encounter->IsAdvancingRound());

	int32 StartTurn = bNextTurn ? Turn + 1 : Turn;
	// 기간 0 은 턴 제한이 없음, 자원이 소모될 때 EndJobState 가 닫음 (임계)
	int32 EndTurn = Definition.StateDurationTurns > 0
		? StartTurn + Definition.StateDurationTurns - 1
		: FNUJobStateInstance::OpenEnded;

	FNUJobStateInstance* Existing = JobStates.FindByPredicate(
		[&Definition](const FNUJobStateInstance& State)
		{
			return State.StateName == Definition.StateName;
		});

	if (Existing)
	{
		// 한 이름에 하나, 새로 만들지 않고 끝 턴만 늘림
		Existing->EndTurn = FMath::Max(Existing->EndTurn, EndTurn);
		StartTurn = Existing->StartTurn;
		EndTurn = Existing->EndTurn;
	}
	else
	{
		FNUJobStateInstance& State = JobStates.AddDefaulted_GetRef();
		State.StateName = Definition.StateName;
		State.ResourceName = Definition.KoreanName;
		State.StartTurn = StartTurn;
		State.EndTurn = EndTurn;
	}

	if (EndTurn >= FNUJobStateInstance::OpenEnded)
	{
		UE_LOG(LogTemp, Log, TEXT("[직업] 상태 %s : %d턴부터 소모될 때까지"),
			*Definition.StateName.ToString(), StartTurn);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[직업] 상태 %s : %d턴부터 %d턴까지"),
			*Definition.StateName.ToString(), StartTurn, EndTurn);
	}

	// 지금 켜지는 상태면 상태 스탯이 바로 걸려야 함
	RefreshJobStateStats();

	NotifyJobResourcesChanged();
}

void UNUAbilitySystemComponent::AdvanceJobStates(int32 TurnNumber)
{
	for (int32 Index = JobStates.Num() - 1; Index >= 0; --Index)
	{
		if (JobStates[Index].EndTurn >= TurnNumber)
		{
			continue;
		}

		const FNUJobStateInstance Ended = JobStates[Index];
		JobStates.RemoveAt(Index);

		UE_LOG(LogTemp, Log, TEXT("[직업] 상태 %s : 끝남"), *Ended.StateName.ToString());

		// 자원표가 Reset 이라고 한 상태는 끝날 때 자원도 전량 소멸, Keep 은 그대로 (번아웃 · 지맥 붕괴)
		const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), Ended.ResourceName);

		int32 Amount = 0;
		if (Def && Def->StateOnEnd == ENUJobStateOnEnd::Reset
			&& TryGetJobResource(Ended.ResourceName, Amount) && Amount > 0)
		{
			AddJobResource(Ended.ResourceName, -Amount, false);
		}
	}

	RefreshJobStateStats();

	// 턴이 넘어가 대기 중이던 상태가 켜지거나 남은 턴이 줄어듦
	NotifyJobResourcesChanged();
}

const TArray<FNUJobStateInstance>& UNUAbilitySystemComponent::GetJobStates() const
{
	return JobStates;
}

void UNUAbilitySystemComponent::ResetJobResourceIdleTurns(FName ResourceName)
{
	for (FNUJobResourceInstance& Instance : JobResources)
	{
		if (Instance.ResourceName == ResourceName)
		{
			Instance.IdleTurns = 0;
			return;
		}
	}
}

const UNUAbilitySystemComponent* UNUAbilitySystemComponent::FindMostStackedOpponent(FName ResourceName) const
{
	TArray<UNUAbilitySystemComponent*> Opponents;
	CollectOpposingTargets(Opponents);

	const UNUAbilitySystemComponent* Best = nullptr;
	int32 BestAmount = -1;

	for (const UNUAbilitySystemComponent* Opponent : Opponents)
	{
		if (!IsValid(Opponent))
		{
			continue;
		}

		// 칸이 아직 없는 상대는 0 으로 셈
		int32 Amount = 0;
		Opponent->TryGetJobResource(ResourceName, Amount);

		if (Amount > BestAmount)
		{
			Best = Opponent;
			BestAmount = Amount;
		}
	}

	return Best;
}

void UNUAbilitySystemComponent::RefreshOpponentResourceScaledStats(FName ResourceName, bool bFromDeath)
{
	TArray<UNUAbilitySystemComponent*> Opponents;
	CollectOpposingTargets(Opponents);

	for (UNUAbilitySystemComponent* Opponent : Opponents)
	{
		if (IsValid(Opponent))
		{
			Opponent->RefreshResourceScaledStats(ResourceName);
			Opponent->EvaluateDropToZeroStates(bFromDeath);
		}
	}
}

void UNUAbilitySystemComponent::EvaluateDropToZeroStates(bool bFromDeath)
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	if (!IsValid(Data) || !IsValid(Encounter))
	{
		return;
	}

	for (const TPair<FName, FNUJobResourceDefinition>& Pair : Data->GetJobResources())
	{
		const FNUJobResourceDefinition& Def = Pair.Value;

		// 이 자원을 여는 전직 노드를 가진 전투원만 (3207 정령술사)
		if (Def.StateEnterRule != ENUJobStateEnterRule::DropToZero
			|| Def.StateName.IsNone()
			|| Def.JobNodeId <= 0
			|| !OwnedJobEffects.ContainsByPredicate(
				[&Def](const FNUOwnedJobEffect& Owned)
				{
					return Owned.JobSkillId == Def.JobNodeId;
				}))
		{
			continue;
		}

		// 살아 있는 상대 중 가장 많이 쌓인 쪽의 지금 값, 죽은 상대는 목록에서 빠져 있음
		int32 Now = 0;
		if (const UNUAbilitySystemComponent* Top = FindMostStackedOpponent(Def.KoreanName))
		{
			Top->TryGetJobResource(Def.KoreanName, Now);
		}

		const int32 Last = LastSeenMostStacked.FindRef(Def.KoreanName);
		LastSeenMostStacked.Add(Def.KoreanName, Now);

		// 1 이상이었다가 0 이 될 때만 켜짐, 사망으로 사라진 자원은 값만 고침
		// 적마다 감쇠가 차례로 돌아도 마지막 적이 0 이 되는 순간에 한 번만 켜져 참가자 순서와 무관함
		if (!bFromDeath && Last >= 1 && Now == 0)
		{
			EnterJobState(Def);
		}
	}
}

const TMap<FName, int32>& UNUAbilitySystemComponent::GetLastSeenMostStacked() const
{
	return LastSeenMostStacked;
}

FName UNUAbilitySystemComponent::GetJobClass() const
{
	// JobId로 직업 구분
	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwnerActor());
	const UNUJobComponent* Job = IsValid(PlayerState) ? PlayerState->GetJobComponent() : nullptr;
	
	// 적과 나이아에게는 직업이 없음, 비워서 돌려줌
	if (!IsValid(Job) || !Job->HasJob())
	{
		return NAME_None;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Progression = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	
	if (!IsValid(Progression))
	{
		return NAME_None;
	}
	
	const FNUJobDefinitionRow* Definition = Progression->FindJob(Job->GetJobId());
	
	return Definition ? Definition->JobKey : NAME_None;
}

void UNUAbilitySystemComponent::TriggerJobResourceGain(ENUJobResourceGainTrigger Trigger,
	UNUAbilitySystemComponent* TargetASC, bool bFromOwnAction)
{
	if (Trigger == ENUJobResourceGainTrigger::None)
	{
		return;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	
	if (!IsValid(Data))
	{
		return;
	}
	
	const FName JobClass = GetJobClass();
	
	// 직업이 없으면 직업 자원도 없음, 적과 나이아가 AG 를 쌓는 것을 막는다
	if (JobClass.IsNone())
	{
		return;
	}
	
	for (const TPair<FName, FNUJobResourceDefinition>& Pair : Data->GetJobResources())
	{
		const FNUJobResourceDefinition& Definition = Pair.Value;
		
		// 통로가 둘인 자원이 있어 구분함 (AG — 턴 종료와 패링 성공)
		const bool bFirst = Definition.GainTrigger == Trigger;
		const bool bSecond = Definition.GainTrigger2 == Trigger;
		
		if (!bFirst && !bSecond)
		{
			continue;
		}
		
		// 직업 자원은 그 직업만 쌓음, Common 은 가리지 않음
		if (!Definition.BelongsToJob(JobClass))
		{
			continue;
		}
		
		// 그 자원을 여는 전직 노드를 가진 전투원만 쌓음 (서버와 같은 규칙, 전직 전 트래커는 템포 · 번아웃이 없음)
		// 게이지를 보일지와 같은 판단 (GetJobResourceViews)
		if (Definition.JobNodeId > 0 && !JobSkillIds.Contains(Definition.JobNodeId))
		{
			continue;
		}
		
		// 이번 행동이 그 자원 줄을 들고 있으면 기본 축적은 넘김
		// 안 그러면 3028 이중 발현이 표의 영력 +2 와 직업 규칙의 +2 를 둘 다 받아 +4 가 됨
		if (bFromOwnAction && ActionGrantsJobResource(Definition.KoreanName))
		{
			continue;
		}
		
		// 대상에게 붙는 자원 (마크 · 약점 · 문양)
		UNUAbilitySystemComponent* Owner = Definition.OwnerScope == ENUJobResourceOwner::Target
			? TargetASC
			: this;
		
		if (!IsValid(Owner))
		{
			continue;
		}
		
		const float Amount = bFirst ? Definition.GainAmount : Definition.GainAmount2;
		
		// 자원표가 정한 자연 충전량을 보유 줄이 갈아끼울 수 있음 (3127 각성 촉진 — AG 20 -> 29)
		// 배수와 턴당 상한은 AddJobResource 안에서 걸리므로 여기서 안 봄
		Owner->AddJobResource(Definition.KoreanName,
			Owner->ResolveNaturalGainAmount(Definition.KoreanName, FMath::RoundToInt(Amount)));
	}
}

bool UNUAbilitySystemComponent::ActionGrantsJobResource(FName ResourceName) const
{
	// 대기 슬롯은 발동 순간 비어서 타격 시점엔 늘 빈 컨텍스트였음, 억제가 한 번도 안 걸렸던 이유
	const FNUBattleActionContext& Context = GetCurrentActionContext();
	
	if (!Context.bValid)
	{
		return false;
	}
	
	return Context.JobRuntimeEffects.ContainsByPredicate(
		[ResourceName](const FNUJobSkillEffectRow& Row)
		{
			return Row.ResourceName == ResourceName
				&& (Row.EffectType == ENUJobEffectType::ResourceGain
					|| Row.EffectType == ENUJobEffectType::DebuffStack);
		});
}

void UNUAbilitySystemComponent::ApplyJobResourceTickDamage()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsAlive())
	{
		return;
	}

	// 쌓인 자원이 하나도 없으면 상대의 줄을 볼 필요가 없음
	const bool bAnyAmount = JobResources.ContainsByPredicate(
		[](const FNUJobResourceInstance& Instance)
		{
			return Instance.Amount > 0;
		});

	if (!bAnyAmount)
	{
		return;
	}

	// 줄 주인은 이 전투원의 상대, 쓰러진 시전자의 줄은 돌지 않음
	TArray<UNUAbilitySystemComponent*> Casters;
	CollectOpposingTargets(Casters);

	for (UNUAbilitySystemComponent* Caster : Casters)
	{
		if (!IsValid(Caster))
		{
			continue;
		}

		for (const FNUOwnedJobEffect& Owned : Caster->OwnedJobEffects)
		{
			const FNUJobSkillEffectRow& Row = Owned.Row;

			if (!IsJobTickDamageRow(Row))
			{
				continue;
			}

			// 적 대상 줄이라 이 전투원 칸의 자원으로 셈, 문양이 있는 적마다 그 적의 수로 계산됨
			const float Steps = JobValueSteps(Row, Caster, this);
			if (Steps <= 0.f)
			{
				continue;
			}

			float BaseStat = 0.f;

			if (Row.AffectedStat == ENUStatusAffectedStat::MagicAttackPower)
			{
				BaseStat = Caster->GetNumericAttribute(UNUCombatAttributeSet::GetMagicAttackPowerAttribute());
			}
			else if (Row.AffectedStat == ENUStatusAffectedStat::AttackPower)
			{
				BaseStat = Caster->GetNumericAttribute(UNUCombatAttributeSet::GetAttackPowerAttribute());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 지속 피해의 기준 스탯을 모름 : %s"),
					Owned.JobSkillId, Row.EffectIndex, *UEnum::GetValueAsString(Row.AffectedStat));

				continue;
			}

			// 방어력을 무시하고 계수 그대로만 들어감, 치명타도 주는 피해 증감도 안 받음
			const float Damage = BaseStat * Row.GetScaledValue() * Steps;
			if (Damage <= KINDA_SMALL_NUMBER)
			{
				UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 지속 피해가 0 이라 건너뜁니다 (공격력 %.0f)"),
					Owned.JobSkillId, Row.EffectIndex, BaseStat);

				continue;
			}

			UE_LOG(LogTemp, Log, TEXT("[직업] 턴 끝 지속 피해 : %s -> %s, %s %.0f개 x %.0f%% x 공격력 %.0f = %.0f"),
				*GetNameSafe(Caster->GetOwnerActor()), *GetNameSafe(GetOwnerActor()),
				*Row.ValuePer.ToString(), Steps, Row.GetScaledValue() * 100.f, BaseStat, Damage);

			SendStatusDamage(this, Caster, Damage);

			// 이 피해로 쓰러졌으면 더 줄 것이 없음
			if (!IsAlive())
			{
				return;
			}
		}
	}
}

void UNUAbilitySystemComponent::AdvanceJobResourceDecay()
{
	// 이번 턴 문양 수로 지속 피해를 줌, 아래에서 줄이기 전이어야 함
	ApplyJobResourceTickDamage();

	for (int32 Index = JobResources.Num() - 1; Index >= 0; --Index)
	{
		FNUJobResourceInstance& Instance = JobResources[Index];
		
		const FNUJobResourceDefinition* Definition = FindResourceDef(GetWorld(), Instance.ResourceName);
		if (!Definition)
		{
			continue;
		}
		
		// 턴이 닫히므로 이번 턴 획득 셈과 기간제 배수를 여기서 정리해야 함
		Instance.GainedThisTurn = 0;
		
		if (Instance.GainBonusTurns > 0)
		{
			--Instance.GainBonusTurns;
		}
		
		// 감쇠 면역 — 기간제는 칸이 턴을 세고(3237 지맥 연결), 상시는 보유 줄이 정함(3035)
		// 수치가 0 이어도 턴은 지나가야 함
		if (Instance.DecayImmuneTurns > 0)
		{
			--Instance.DecayImmuneTurns;
			continue;
		}
		
		if (Instance.Amount <= 0 || HasPermanentDecayImmunity(Instance.ResourceName))
		{
			continue;
		}
		
		switch (Definition->DecayType)
		{
		case ENUJobResourceDecay::PerTurn:
			// 매 턴 정해진 만큼 줄어듦 (문양 -1)
			Instance.Amount = FMath::Max(0, Instance.Amount - FMath::RoundToInt(Definition->DecayAmount));
			break;
		
		case ENUJobResourceDecay::TimeoutAll:
			// 정해진 턴 동안 새로 안 쌓이면 통째로 사라짐 (마크 3턴)
			++Instance.IdleTurns;
			
			if (Definition->DecayInterval > 0 && Instance.IdleTurns >= Definition->DecayInterval)
			{
				Instance.Amount = 0;
				Instance.IdleTurns = 0;
			}
			break;
		
		case ENUJobResourceDecay::TimeoutEach:
		{
			// 중첩마다 따로 만료됨 (약점), 먼저 붙은 것이 먼저 빠짐
			for (int32 Stack = Instance.StackTurns.Num() - 1; Stack >= 0; --Stack)
			{
				if (--Instance.StackTurns[Stack] <= 0)
				{
					Instance.StackTurns.RemoveAt(Stack);
				}
			}
			
			Instance.Amount = Instance.StackTurns.Num();
			break;
		}
		
		default:
			// None · Redraw(카드) · ByJobNode · Derived 는 여기서 줄지 않음
			break;
		}
	}
	
	// 줄어든 자원에 비례하는 스탯 줄의 값을 고침
	RefreshResourceScaledStats();
	
	// 3턴 안 쌓여 사라진 마크의 속도 감소도 같이 풀림 (3107)
	RefreshOpponentScaledStats();

	// 줄어든 문양을 세는 상대의 스탯 줄도 (3207)
	RefreshOpponentResourceScaledStats();

	// 감쇠는 칸 값을 직접 고쳐 획득 통로의 알림을 안 거침
	NotifyJobResourcesChanged();
}

const TArray<FNUJobResourceInstance>& UNUAbilitySystemComponent::GetJobResources() const
{
	return JobResources;
}

void UNUAbilitySystemComponent::GetJobResourceViews(TArray<FNUJobResourceView>& OutGauges,
	TArray<FNUJobResourceView>& OutBadges) const
{
	OutGauges.Reset();
	OutBadges.Reset();

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	if (!IsValid(Data))
	{
		return;
	}

	// 무엇을 띄울지 · 남은 턴을 어떻게 세는지는 NUJobResourceView 가 정함 (넷 전투와 같은 규칙), 여기서는 칸을 읽어 넘기기만 함
	for (const TPair<FName, FNUJobResourceDefinition>& Pair : Data->GetJobResources())
	{
		const FNUJobResourceDefinition& Def = Pair.Value;

		if (!NUJobResourceView::IsShownResource(Def))
		{
			continue;
		}

		const FNUJobResourceInstance* Instance = JobResources.FindByPredicate(
			[&Def](const FNUJobResourceInstance& Candidate)
			{
				return Candidate.ResourceName == Def.KoreanName;
			});

		const int32 Amount = Instance ? Instance->Amount : 0;
		const int32 MaxAmount = GetJobResourceMax(Def.KoreanName);

		if (Def.OwnerScope == ENUJobResourceOwner::Self)
		{
			if (NUJobResourceView::ShouldShowGauge(Instance != nullptr, JobSkillIds.Contains(Def.JobNodeId)))
			{
				OutGauges.Add(NUJobResourceView::MakeResourceView(Def, Amount, MaxAmount, 0));
			}

			continue;
		}

		if (NUJobResourceView::ShouldShowBadge(Amount))
		{
			OutBadges.Add(NUJobResourceView::MakeResourceView(
				Def, Amount, MaxAmount, NUJobResourceView::ResolveRemainingTurns(Def, Instance)));
		}
	}

	// 상태는 자원 뒤에, 끝난 것은 만들어지지 않고 대기 중인 것은 기간 전체를 보여 줌
	TArray<FNUJobResourceView> States;

	if (IsValid(Encounter))
	{
		const int32 Turn = Encounter->GetTurnNumber();

		for (const FNUJobStateInstance& State : JobStates)
		{
			FNUJobResourceView View;
			if (NUJobResourceView::MakeStateView(Data->FindJobResource(State.ResourceName), State.ResourceName,
				State.StateName, State.StartTurn, State.EndTurn, Turn, View))
			{
				States.Add(View);
			}
		}
	}

	NUJobResourceView::SortViews(OutGauges, OutBadges, States);
}

void UNUAbilitySystemComponent::ApplySelfBoundState(int32 SourceSkillId, int32 Turns, bool bBlocksAction)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsAlive() || SourceSkillId <= 0 || Turns <= 0)
	{
		return;
	}
	
	int32 Index = SelfBoundStates.IndexOfByPredicate([SourceSkillId](const FNUSelfBoundState& State)
	{
		return State.SourceSkillId == SourceSkillId;
	});
	
	if (Index == INDEX_NONE)
	{
		Index = SelfBoundStates.AddDefaulted();
	}
	
	FNUSelfBoundState& State = SelfBoundStates[Index];
	
	State.SourceSkillId = SourceSkillId;
	State.RemainingTurns = Turns;
	State.bBlocksAction = bBlocksAction;
}

void UNUAbilitySystemComponent::SetSelfBoundLifesteal(int32 SourceSkillId, float Lifesteal)
{
	for (FNUSelfBoundState& State : SelfBoundStates)
	{
		if (State.SourceSkillId == SourceSkillId)
		{
			// 더하지 않고 덮어씀
			State.Lifesteal = Lifesteal;
			return;
		}
	}
	
	// 상태를 거는 줄보다 먼저 오면 얹힐 자리가 없음, 효과표 순번이 어긋난 것이라 알려둠
	UE_LOG(LogTemp, Warning, TEXT("스킬 %d : 자기 구속 상태가 없는데 흡혈을 얹으려 했습니다"), SourceSkillId);
}

void UNUAbilitySystemComponent::SetSelfBoundRelease(int32 SourceSkillId)
{
	for (FNUSelfBoundState& State : SelfBoundStates)
	{
		if (State.SourceSkillId == SourceSkillId)
		{
			State.bReleaseOnEnd = true;
			return;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("스킬 %d : 자기 구속 상태가 없는데 방출을 예약하려 했습니다"), SourceSkillId);
}

int32 UNUAbilitySystemComponent::GetSelfBoundRemainingTurns(int32 SourceSkillId) const
{
	for (const FNUSelfBoundState& State : SelfBoundStates)
	{
		if (State.SourceSkillId == SourceSkillId)
		{
			return State.RemainingTurns;
		}
	}
	
	return 0;
}

const TArray<FNUSelfBoundState>& UNUAbilitySystemComponent::GetSelfBoundStates() const
{
	return SelfBoundStates;
}

void UNUAbilitySystemComponent::ApplyElementAffliction(UNUAbilitySystemComponent* SourceASC,
	int32 SourceSkillId, const FGameplayTag& Element, float DamageTakenMultiplier,
	bool bBlocksSkill, int32 Turns)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsAlive() || !Element.IsValid())
	{
		return;
	}
	
	// 스킬 번호만으로 가르면 안 됨 — 2109 는 한 스킬이 물과 불 두 속성을 같이 걸음
	// 속성까지 같이 봐야 따로 걸 수 있음
	int32 Index = ElementAfflictions.IndexOfByPredicate(
		[SourceSkillId, &Element](const FNUElementAfflictionInstance& Instance)
		{
			return Instance.SourceSkillId == SourceSkillId && Instance.Element == Element;
		});
	
	if (Index == INDEX_NONE)
	{
		Index = ElementAfflictions.AddDefaulted();
	}
	
	FNUElementAfflictionInstance& Instance = ElementAfflictions[Index];
	
	Instance.SourceSkillId = SourceSkillId;
	Instance.Element = Element;
	Instance.DamageTakenMultiplier = DamageTakenMultiplier;
	Instance.bBlocksSkill = bBlocksSkill;
	Instance.RemainingTurns = Turns;
}

float UNUAbilitySystemComponent::GetElementDamageTakenMultiplier(const FGameplayTagContainer& AttackTags) const
{
	if (ElementAfflictions.Num() == 0 || !AttackTags.HasTag(NU::Element::Root))
	{
		return 1.f;
	}
	
	float Multiplier = 1.f;
	
	for (const FNUElementAfflictionInstance& Instance : ElementAfflictions)
	{
		// 이번 공격이 그 속성이어야 함, 상성 배율이 보는 것과 같은 태그
		if (Instance.Element.IsValid() && AttackTags.HasTagExact(Instance.Element))
		{
			Multiplier *= Instance.DamageTakenMultiplier;
		}
	}
	
	return Multiplier;
}

bool UNUAbilitySystemComponent::IsElementSkillBlocked(const FGameplayTag& Element) const
{
	// 무속성 스킬은 가릴 속성이 없어 막히지 않음
	if (!Element.IsValid())
	{
		return false;
	}
	
	for (const FNUElementAfflictionInstance& Instance : ElementAfflictions)
	{
		if (Instance.bBlocksSkill && Instance.Element == Element)
		{
			return true;
		}
	}
	
	return false;
}

const TArray<FNUElementAfflictionInstance>& UNUAbilitySystemComponent::GetElementAfflictions() const
{
	return ElementAfflictions;
}

void UNUAbilitySystemComponent::ApplyOnAttackGrant(int32 SourceSkillId, int32 StatusEffectId,
	float Chance, const FGameplayTagContainer& Elements, bool bIncludeBasicAttack, int32 Turns)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsAlive() || StatusEffectId <= 0)
	{
		return;
	}
	
	int32 Index = OnAttackGrants.IndexOfByPredicate([SourceSkillId](const FNUOnAttackGrant& Grant)
	{
		return Grant.SourceSkillId == SourceSkillId;
	});
	
	if (Index == INDEX_NONE)
	{
		Index = OnAttackGrants.AddDefaulted();
	}
	
	FNUOnAttackGrant& Grant = OnAttackGrants[Index];
	
	Grant.SourceSkillId = SourceSkillId;
	Grant.StatusEffectId = StatusEffectId;
	Grant.Chance = Chance;
	Grant.Elements = Elements;
	Grant.bIncludeBasicAttack = bIncludeBasicAttack;
	Grant.RemainingTurns = Turns;
}

void UNUAbilitySystemComponent::TriggerOnAttackGrants(UNUAbilitySystemComponent* TargetASC,
	const FGameplayTag& AttackElement, bool bIsSkill)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(TargetASC) || OnAttackGrants.Num() == 0)
	{
		return;
	}
	
	// 굴리는 도중 목록이 바뀔 수 있어 복사해두고 순회함, 반사와 같은 이유
	const TArray<FNUOnAttackGrant> Snapshot = OnAttackGrants;
	
	for (const FNUOnAttackGrant& Grant : Snapshot)
	{
		// 이번 공격이 조건에 맞는지, 기본 공격은 속성이 아니라 스킬이 아님으로 가름
		const bool bElementMatched = Grant.Elements.IsEmpty()
			|| (AttackElement.IsValid() && Grant.Elements.HasTagExact(AttackElement));
		
		const bool bBasicMatched = Grant.bIncludeBasicAttack && !bIsSkill;
		
		if (!bElementMatched && !bBasicMatched)
		{
			continue;
		}
		
		// 회피·면역·확률 판정은 전부 TryApplyStatusEffect 안에서 처리됨
		TryApplyStatusEffect(TargetASC, Grant.StatusEffectId, Grant.Chance, 0);
	}
}

const TArray<FNUOnAttackGrant>& UNUAbilitySystemComponent::GetOnAttackGrants() const
{
	return OnAttackGrants;
}

bool UNUAbilitySystemComponent::IsSelfBoundActionBlocked() const
{
	for (const FNUSelfBoundState& State : SelfBoundStates)
	{
		if (State.bBlocksAction && State.RemainingTurns > 0)
		{
			return true;
		}
	}
	
	return false;
}

void UNUAbilitySystemComponent::ReleaseSelfBoundState(const FNUSelfBoundState& State)
{
	// 자기 진영을 알아야 하므로 전투원 ID 를 구함, CollectTeamTargets 와 같은 방식 (적 ASC 는 CurrentAvatar 가 없음)
	const int32 SelfCombatantId = ResolveCombatantId();
	if (!IsValid(Encounter) || SelfCombatantId == INDEX_NONE || !IsAlive())
	{
		return;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* BattleData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	const FNUActiveSkillDefinition* Definition = IsValid(BattleData)
		? BattleData->FindActiveSkillDefinition(State.SourceSkillId) 
		: nullptr;
	
	if (!Definition)
	{
		return;
	}
	
	// 상태가 풀릴 때 터지는 피해라 계수는 OnStateEnd 줄이 들고 있음 (2210 개화 #2)
	const NUActiveSkill::FDamageParams DamageParams = NUActiveSkill::GatherDamageParams(
		BattleData->FindSkillEffects(State.SourceSkillId), ENUEffectCondition::OnStateEnd);
	
	if (!DamageParams.bHasDamage)
	{
		return;
	}
	
	TArray<AActor*> Targets;
	Encounter->ResolveAbilityTargetActors(
		SelfCombatantId, Definition->TargetPolicy, INDEX_NONE, Targets);
	
	for (AActor* Target : Targets)
	{
		UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));
		
		if (IsValid(TargetASC))
		{
			SendSkillDamage(TargetASC, *Definition, DamageParams);
		}
	}
	
	UE_LOG(LogTemp, Warning,
		TEXT("스킬 %d %s 방출 : 대상 %d명, 위력 %.0f"),
		State.SourceSkillId, *Definition->KoreanName.ToString(), Targets.Num(),
		DamageParams.Coefficient * 100.f);
}

void UNUAbilitySystemComponent::SendSkillDamage(UNUAbilitySystemComponent* TargetASC,
	const FNUActiveSkillDefinition& Definition, const NUActiveSkill::FDamageParams& DamageParams)
{
	if (!IsValid(TargetASC) || !TargetASC->IsAlive())
	{
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
		UNUGE_SkillDamage::StaticClass(), 1.f, MakeEffectContext());
	
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	// 어빌리티가 싣는 것과 같은 칸들, 여기만 빠뜨리면 계수 없는 맨손 피해가 나감
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::Coefficient, DamageParams.Coefficient);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::DefenseIgnore, DamageParams.DefenseIgnore);
	
	// 흡혈은 싣지 않음, 방출은 스킬의 평범한 타격이 아니라 상태가 풀리는 것이고,
	// 그 상태인 동안의 흡혈은 SumLifestealBonus 가 이미 더함
	// 여기서 정의표 값까지 실으면 같은 100% 가 두 번 들어가 200% 흡혈이 됨
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::Lifesteal, 0.f);
	
	// 물리·마법과 속성 태그, 상성과 속성 위력이 이 태그를 보고 갈림
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(Definition.DamageType.IsValid() ? Definition.DamageType : NU::Damage::Type::Physical);
	
	if (Definition.AttackElement.IsValid())
	{
		TagContainer.AddTag(Definition.AttackElement);
	}
	
	SpecHandle.Data->AppendDynamicAssetTags(TagContainer);
	
	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void UNUAbilitySystemComponent::AdvanceBattleAxes()
{
	// 턴이 닫히므로 이번 턴 공격 횟수를 되돌림 (3050 연쇄 · 3105 연사)
	HitsDealtThisTurn = 0;
	
	// 턴이 닫힐 때 쌓이는 자원과 턴마다 줄어드는 자원, 쌓고 나서 줄임
	TriggerJobResourceGain(ENUJobResourceGainTrigger::OnTurnEnd);
	AdvanceJobResourceDecay();
	
	bool bShieldChanged = false;
	
	for (int32 Index = Shields.Num() - 1; Index >= 0; --Index)
	{
		FNUShieldInstance& Shield = Shields[Index];
		
		// 0으로 들어온 것은 전투가 끝날 때까지라 줄이지 않음
		if (Shield.RemainingTurns <= 0)
		{
			continue;
		}
		
		--Shield.RemainingTurns;
		
		if (Shield.RemainingTurns <= 0)
		{
			Shields.RemoveAt(Index);
			bShieldChanged = true;
		}
	}
	
	for (int32 Index = Reflects.Num() - 1; Index >= 0; --Index)
	{
		FNUReflectInstance& Reflect = Reflects[Index];
		
		if (Reflect.RemainingTurns <= 0)
		{
			continue;
		}
		
		--Reflect.RemainingTurns;
		
		if (Reflect.RemainingTurns <= 0)
		{
			Reflects.RemoveAt(Index);
		}
	}
	
	// 방출을 상태보다 먼저 처리함 — 그 상태인 동안 켜지는 것들이 이 일격에 실려야 함 (피해 x2, 흡혈) 
	// 지우고 터뜨리면 평범한 한 방 스킬
	for (int32 Index = SelfBoundStates.Num() - 1; Index >= 0; --Index)
	{
		FNUSelfBoundState& State = SelfBoundStates[Index];
		
		if (State.RemainingTurns <= 0 || --State.RemainingTurns > 0)
		{
			continue;
		}
		
		if (State.bReleaseOnEnd)
		{
			ReleaseSelfBoundState(State);
		}
		
		SelfBoundStates.RemoveAt(Index);
	}
	
	for (int32 Index = OnAttackGrants.Num() - 1; Index >= 0; --Index)
	{
		FNUOnAttackGrant& Grant = OnAttackGrants[Index];
		
		if (Grant.RemainingTurns <= 0)
		{
			continue;
		}
		
		--Grant.RemainingTurns;
		
		if (Grant.RemainingTurns <= 0)
		{
			OnAttackGrants.RemoveAt(Index);
		}
	}
	
	for (int32 Index = ElementAfflictions.Num() - 1; Index >= 0; --Index)
	{
		FNUElementAfflictionInstance& Affliction = ElementAfflictions[Index];
		
		if (Affliction.RemainingTurns <= 0)
		{
			continue;
		}
		
		--Affliction.RemainingTurns;
		
		if (Affliction.RemainingTurns <= 0)
		{
			ElementAfflictions.RemoveAt(Index);
		}
	}
	
	for (int32 Index = DamageTakenMods.Num() - 1; Index >= 0; --Index)
	{
		FNUDamageTakenModInstance& Mod = DamageTakenMods[Index];

		if (Mod.RemainingTurns <= 0)
		{
			continue;
		}

		--Mod.RemainingTurns;

		if (Mod.RemainingTurns <= 0)
		{
			DamageTakenMods.RemoveAt(Index);
		}
	}

	for (int32 Index = ElementPowers.Num() - 1; Index >= 0; --Index)
	{
		FNUElementPowerInstance& ElementPower = ElementPowers[Index];
		
		if (ElementPower.RemainingTurns <= 0)
		{
			continue;
		}
		
		--ElementPower.RemainingTurns;
		
		if (ElementPower.RemainingTurns <= 0)
		{
			ElementPowers.RemoveAt(Index);
		}
	}
	
	if (bShieldChanged)
	{
		OnShieldChanged.Broadcast();
	}
}

void UNUAbilitySystemComponent::ClearBattleScopedState()
{
	const bool bHadShield = Shields.Num() > 0;
	
	Shields.Reset();
	Reflects.Reset();
	ElementPowers.Reset();
	DamageTakenMods.Reset();
	ElementSkillUseCounts.Reset();
	SelfBoundStates.Reset();
	ElementAfflictions.Reset();
	OnAttackGrants.Reset();
	
	// 시간 초과로 끝나지 못한 행동이 다음 전투의 도트 판정에 남지 않게 비움
	EndCurrentAction(CurrentActionAbility.Get());
	
	// 턴 도중에 끝난 전투의 공격 횟수가 다음 전투의 첫 턴으로 넘어가지 않게 비움
	HitsDealtThisTurn = 0;
	
	// 전투당 1회 상태도 이 전투에만 (3027 불굴)
	ActiveJobOnceStates.Reset();
	UsedJobOnceStates.Reset();
	ActiveSkillUsesThisBattle.Reset();
	CurrentJobCard = NAME_None;
	JobStates.Reset();
	LastSeenMostStacked.Reset();
	JobStateEndTurns.Reset();
	
	// 상대 줄로 걸린 스탯(3107 마크 속도)은 그 전투의 상대가 사라지면 걷을 쪽이 없어 여기서 걷음
	if (OpponentScaledStatHandle.IsValid())
	{
		RemoveActiveGameplayEffect(OpponentScaledStatHandle);
		OpponentScaledStatHandle.Invalidate();
	}
	
	// 자원표가 Keep 이라고 한 것은 전투 밖으로 가져감 (영력)
	for (int32 Index = JobResources.Num() - 1; Index >= 0; --Index)
	{
		const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), JobResources[Index].ResourceName);
		
		if (!Def || Def->OnBattleEnd == ENUJobResourceOnBattleEnd::Reset)
		{
			JobResources.RemoveAt(Index);
			continue;
		}
		
		JobResources[Index].DecayImmuneTurns = 0;
		JobResources[Index].GainedThisTurn = 0;
		JobResources[Index].GainBonusTurns = 0;
		JobResources[Index].GainMultiplier = 1.f;
	}
	
	// 비운 자원에 비례하는 스탯 줄도 되돌림, 안 하면 마지막 전투의 템포만큼 SP 상한이 필드에 남음
	RefreshResourceScaledStats();

	// 상태 스탯도 같이 (번아웃 도중에 끝난 전투의 SP 상한 -70% 가 필드에 남지 않게)
	RefreshJobStateStats();

	// 비운 자원 · 상태를 표시에서도 걷음
	NotifyJobResourcesChanged();
	
	if (bHadShield)
	{
		OnShieldChanged.Broadcast();
	}
	
	// 2833 동기화로 늘어난 보유 목록을 원래대로 되돌림
	// 나이아 상태는 로스터에 남아 전투 밖에서도 살아 있어, 안 되돌리면 복사한 패시브가 남아있음
	if (bPassiveSkillIdsSynced)
	{
		bPassiveSkillIdsSynced = false;
		
		// 상시 스탯 패시브가 다시 걸려야 하므로 세터를 거쳐 넣음
		SetPassiveSkillIds(PassiveSkillIdsBeforeSync);
		PassiveSkillIdsBeforeSync.Reset();
	}
}

FActiveGameplayEffectHandle UNUAbilitySystemComponent::ApplyStatModifierEffect(
	UNUAbilitySystemComponent* SourceASC, ENUStatusAffectedStat Stat, float Value, bool bIsPercent)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(SourceASC))
	{
		return FActiveGameplayEffectHandle();
	}
	
	// 쓰지 않는 축까지 중립값으로 채워야 함, 안 채우면 그 축이 0으로 곱해짐
	TMap<FGameplayTag, float> Magnitudes;
	NUStatusEffect::FillNeutralStatModifiers(Magnitudes);
	
	if (!NUStatusEffect::SetStatModifier(Magnitudes, Stat, Value, bIsPercent))
	{
		UE_LOG(LogTemp, Warning, TEXT("스탯 변경의 축이 GE에 없습니다 : %d"),
			static_cast<int32>(Stat));
	
		return FActiveGameplayEffectHandle();
	}
	
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		UNUGE_StatusEffect_StatModifier::StaticClass(), 1.f, SourceASC->MakeEffectContext());
	
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	
	for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
	}
	
	return SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), this);
}
	
void UNUAbilitySystemComponent::ApplyBattleScopedStatModifier(UNUAbilitySystemComponent* SourceASC,
	ENUStatusAffectedStat Stat, float Value, bool bIsPercent)
{
	// 턴 기반에 넣지 않는다, 줄일 턴이 없어서 그 목록이 하는 일이 없기 때문
	// 대신 전투가 바뀔 때 SetBattleEncounter가 제거함
	const FActiveGameplayEffectHandle Handle =
		ApplyStatModifierEffect(SourceASC, Stat, Value, bIsPercent);
	
	if (Handle.IsValid())
	{
		BattleScopedEffects.Add(Handle);
	}
}
	
void UNUAbilitySystemComponent::ApplyTurnScopedStatModifier(UNUAbilitySystemComponent* SourceASC,
	ENUStatusAffectedStat Stat, float Value, bool bIsPercent, int32 Turns)
{
	const FActiveGameplayEffectHandle Handle = ApplyStatModifierEffect(SourceASC, Stat, Value, bIsPercent);
	if (!Handle.IsValid())
	{
		return;
	}
	
	// 정의에 없는 MaxSP 증가라 StatusEffectId 를 0으로 넣음
	GrantTurnBasedEffects(Handle, Turns, 0, SourceASC);
}

void UNUAbilitySystemComponent::GainResource(UNUAbilitySystemComponent* SourceASC,
	ENUStatusAffectedStat Stat, float Value, bool bIsPercent, int32 Turns)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsAlive())
	{
		return;
	}
	
	// SP는 행동 후 전량 회복하므로 최대치를 올려야 주는것처럼 될 수 있음
	if (Stat == ENUStatusAffectedStat::SP || Stat == ENUStatusAffectedStat::MaxSP)
	{
		const float SPBefore = GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute());
		const float MaxSPBefore = GetNumericAttribute(UNUVitalAttributeSet::GetMaxSPAttribute());
		const int32 BonusTurns = Turns > 0 ? Turns : BonusResourceTurns;
		
		ApplyTurnScopedStatModifier(SourceASC, ENUStatusAffectedStat::MaxSP, Value, bIsPercent, BonusTurns);
		
		// 올린 상한은 이번 턴 행동이 남은 쪽(시전자 포함)만 그 행동이 끝날 때 채워짐
		// 이미 행동을 마친 아군은 다음 명령까지 다시 안 차서 그 뒤 반응(패링 · 회피)에 쓴 만큼 못 받았음, 지금 SP 에도 더함
		// 행동 단계 밖(턴 시작 등)에서 받으면 상한은 1턴이어야 맞음 — 지금 그런 줄은 없음 (3243 발동 시점이 정해지면 볼 것)
		const ANUBattleEncounter* BattleEncounter = GetBattleEncounter();
		const bool bAddNow = Stat == ENUStatusAffectedStat::SP
			&& !(IsValid(BattleEncounter) && BattleEncounter->HasActionLeftThisTurn(this));
		
		const float MaxSPAfter = GetNumericAttribute(UNUVitalAttributeSet::GetMaxSPAttribute());
		
		if (bAddNow)
		{
			SetNumericAttributeBase(UNUVitalAttributeSet::GetSPAttribute(),
				FMath::Min(MaxSPAfter, SPBefore + (MaxSPAfter - MaxSPBefore)));
		}
		
		// 상한이 같이 올라 SP 칸만 봐서는 얼마를 받았는지 가리기 어려워 남김
		UE_LOG(LogTemp, Log, TEXT("[SP] %s : 상한 %.0f -> %.0f (%d턴), SP %.0f -> %.0f%s"),
			*GetNameSafe(GetOwnerActor()), MaxSPBefore, MaxSPAfter, BonusTurns, SPBefore,
			GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute()),
			bAddNow ? TEXT(" — 이번 턴 행동을 이미 마쳐 지금 SP 에도 더함") : TEXT(""));
		
		return;
	}
	
	// 체력 쪽은 회복과 같은 길로 보냄, 처치 판정도 회복과 같아야 함
	SendHeal(this, SourceASC, Stat, Value, bIsPercent);
}
	
void UNUAbilitySystemComponent::TriggerOnHitTakenPassives(UNUAbilitySystemComponent* AttackerASC)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(AttackerASC))
	{
		return;
	}
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const int32 PassiveId = Owned.PassiveId;
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		if (Effect.Trigger != ENUPassiveTrigger::OnHitTaken)
		{
			continue;
		}
	
		// 맞고 나서 거는 것만 여기 남는다
		// 회피와 받는 피해 감소는 피해를 계산하기 전에 이미 끝났다
		if (Effect.EffectType != ENUPassiveEffectType::StatModifier
			|| Effect.AffectedStat == ENUStatusAffectedStat::None
			|| FMath::IsNearlyZero(Effect.GetScaledValue()))
		{
			continue;
		}
	
		if (Effect.TargetScope != ENUPassiveTargetScope::Attacker)
		{
			UE_LOG(LogTemp, Warning, TEXT("패시브 %d 의 피격시 대상을 아직 처리할 수 없습니다 : %d"),
				PassiveId, static_cast<int32>(Effect.TargetScope));
	
			continue;
		}
	
		if (!RollPassiveChance(Effect))
		{
			continue;
		}
	
		// 비율인지 고정값인지도 이제 표가 말해준다 (예전에는 전부 비율이라 true 로 박아뒀다)
		AttackerASC->ApplyBattleScopedStatModifier(this, Effect.AffectedStat,
			Effect.GetScaledValue(), Effect.bIsPercent);
	
		UE_LOG(LogTemp, Warning, TEXT("패시브 %d %s 발동 : 공격자 스탯 %d 를 %.0f%% (전투 끝까지)"),
			PassiveId, *GetPassiveName(PassiveId),
			static_cast<int32>(Effect.AffectedStat), Effect.GetScaledValue() * 100.f);
	}
}

void UNUAbilitySystemComponent::TriggerOnDefeatedPassives()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	// ★ 패시브 목록이 아니라 효과 줄 목록을 돈다
	//   한 패시브가 여러 줄이라(2808 ATK+/MATK-) 목록을 돌면 첫 줄만 보게 된다
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const int32 PassiveId = Owned.PassiveId;
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		if (Effect.Trigger != ENUPassiveTrigger::OnDefeated)
		{
			continue;
		}
	
		if (Effect.EffectType != ENUPassiveEffectType::StatusApply
			|| Effect.StatusEffectId <= 0)
		{
			continue;
		}
	
		// 지금 표에 있는 전투불능시 행은 랜덤 적 하나뿐
		// 다른 대상이 생기면 여기에 분기를 늘리면 됨
		if (Effect.TargetScope != ENUPassiveTargetScope::RandomEnemy)
		{
			UE_LOG(LogTemp, Warning, TEXT("패시브 %d 의 전투불능시 대상을 아직 처리할 수 없습니다 : %d"),
				PassiveId, static_cast<int32>(Effect.TargetScope));
	
			continue;
		}
	
		TArray<UNUAbilitySystemComponent*> Opponents;
		CollectOpposingTargets(Opponents);
	
		// 같이 전멸했으면 부여할 곳이 없음
		if (Opponents.Num() == 0)
		{
			continue;
		}
	
		UNUAbilitySystemComponent* Picked = Opponents[FMath::RandRange(0, Opponents.Num() - 1)];
	
		// 굴림은 부여 함수 안에서 한 번만 실행, 밖에서 또 굴리면 확률이 제곱이 됨
		const float Chance = Effect.TriggerChance > KINDA_SMALL_NUMBER
			? Effect.TriggerChance
			: 1.f;
	
		TryApplyStatusEffect(Picked, Effect.StatusEffectId, Chance);
	
		UE_LOG(LogTemp, Warning, TEXT("패시브 %d %s 발동 : 적 %d 중 하나에게 상태이상 %d"),
			PassiveId, *GetPassiveName(PassiveId), Opponents.Num(), Effect.StatusEffectId);
	}
}

void UNUAbilitySystemComponent::ApplyHealPassivesFor(ENUPassiveTrigger Trigger)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	// 쓰러진 전투원은 발동하지 않음
	if (!IsAlive())
	{
		return;
	}
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const int32 PassiveId = Owned.PassiveId;
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		if (Effect.Trigger != Trigger
			|| Effect.EffectType != ENUPassiveEffectType::Heal)
		{
			continue;
		}
		
		// 확률은 여기서 굴려도 된다
		// 사건이 한 번 일어날 때 한 번만 불림
		if (Effect.TriggerChance > KINDA_SMALL_NUMBER
			&& FMath::FRand() >= Effect.TriggerChance)
		{
			continue;
		}
		
		// 지금 표의 두 행은 모두 자신이 대상이다
		// 남을 회복시키는 행이 생기면 여기에 갈래를 늘릴 것
		if (Effect.TargetScope != ENUPassiveTargetScope::Self)
		{
			UE_LOG(LogTemp, Warning, TEXT("패시브 %d 의 회복 대상을 아직 처리할 수 없습니다"), PassiveId);
			continue;
		}
		
		// 가득 찬 상태에서도 SendHeal 은 회복 팝업을 띄움
		// 이 패시브는 턴마다 도는 것이라 그대로 두면 숫자만 뜨고 체력은 그대로인 팝업이 매 턴 뜨게 됨
		if (Effect.AffectedStat == ENUStatusAffectedStat::Health
			&& GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute())
				>= GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) - KINDA_SMALL_NUMBER)
		{
			continue;
		}
		
		SendHeal(this, this, Effect.AffectedStat, Effect.GetScaledValue(), Effect.bIsPercent);
	}
}

bool UNUAbilitySystemComponent::IsPassiveTriggerSatisfied(const FNUPassiveEffectRow& Effect) const
{
	// 확률형 패시브는 여기서 굴리지 않음
	// 이 함수는 피해를 계산할 때마다 불려서, 굴리면 같은 턴에도 타격마다 결과가 달라짐
	// 확률 있는 행이 생기면 굴림을 한 번만 하는 자리(행동 시작)에 따로 둬야 함
	if (Effect.TriggerChance > KINDA_SMALL_NUMBER)
	{
		return false;
	}
	
	switch (Effect.Trigger)
	{
	case ENUPassiveTrigger::Always:
		return true;
		
	case ENUPassiveTrigger::OnLowHealth:
	{
		const UNUVitalAttributeSet* Vital = GetVitalAttributeSet();
		if (!Vital)
		{
			return false;
		}
		
		const float MaxHealth = Vital->GetMaxHealth();
		if (MaxHealth <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		
		const float Ratio = Vital->GetHealth() / MaxHealth;
		
		return Effect.ThresholdCompare == ENUThresholdCompare::AtLeast
			? Ratio >= Effect.TriggerThreshold
			: Ratio <= Effect.TriggerThreshold;
	}
	
	default:
		// 나머지는 사건이 일어나는 자리에서 판정해야 하는 것들이라 여기서는 항상 거짓
		return false;
	}
}

float UNUAbilitySystemComponent::SumElementPowerBonus(const FGameplayTagContainer& AttackTags) const
{
	if (!AttackTags.HasTag(NU::Element::Root))
	{
		return 0.f;
	}
	
	float Bonus = 0.f;
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		if (Effect.EffectType != ENUPassiveEffectType::ElementPowerMod
			|| !Effect.Element.IsValid()
			|| FMath::IsNearlyZero(Effect.GetScaledValue()))
		{
			continue;
		}
		
		// 이번 공격이 그 속성이어야 함
		if (!AttackTags.HasTagExact(Effect.Element))
		{
			continue;
		}
		
		if (!IsPassiveTriggerSatisfied(Effect))
		{
			continue;
		}
		
		// 같은 속성 패시브가 둘 이상이면 더함, 상한은 실행기가 풀 전체에서 한 번만 자름
		Bonus += Effect.GetScaledValue();
	}
	
	return Bonus;
}

float UNUAbilitySystemComponent::SumAlwaysDamageBonus() const
{
	float Bonus = 0.f;
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const int32 PassiveId = Owned.PassiveId;
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		// 조건없이 상시로 켜져 있는 주는 피해만 여기서 셈
		// 공격시·저체력시처럼 발동 시점이 있는 것은 그 시점에서 컨텍스트에 실어야 함
		if (Effect.Trigger != ENUPassiveTrigger::Always
			|| Effect.EffectType != ENUPassiveEffectType::DamageDealtMod)
		{
			continue;
		}
		
		// 상한은 여기서 안 막음, 풀 전체의 상한이라 실행기 한 곳에서만 잘라야 함
		Bonus += Effect.GetScaledValue();
	}
	
	return Bonus;
}

bool UNUAbilitySystemComponent::TryConsumeOncePerBattle(int32 PassiveId)
{
	if (UsedOncePerBattlePassives.Contains(PassiveId))
	{
		return false;
	}
	
	UsedOncePerBattlePassives.Add(PassiveId);
	return true;
}
	
bool UNUAbilitySystemComponent::HasUsedOncePerBattle(int32 PassiveId) const
{
	return UsedOncePerBattlePassives.Contains(PassiveId);
}
	
bool UNUAbilitySystemComponent::ConsumeFirstStrike()
{
	int32 PassiveId = 0;
	const FNUPassiveEffectRow* Effect = FindOwnedPassiveEffect(ENUPassiveEffectType::TurnOrderPriority, PassiveId);
	
	if (!Effect)
	{
		return false;
	}
	
	// 첫 턴에만 이 맞는지 확인
	if (Effect->ConditionType != ENUEffectCondition::FirstTurnOnly)
	{
		return false;
	}
	
	// 전투당 몇 번인지는 표에서 확인, 0이면 제한 X
	if (Effect->MaxPerBattle > 0 && !TryConsumeOncePerBattle(PassiveId))
	{
		return false;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("패시브 %d %s 발동 : 이번 턴 가장 먼저 행동"),
		PassiveId, *GetPassiveName(PassiveId));
	
	return true;
}
	
float UNUAbilitySystemComponent::ConsumeOnceDamageMultiplier()
{
	float Multiplier = 1.f;
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const int32 PassiveId = Owned.PassiveId;
		const FNUPassiveEffectRow& Effect = Owned.Row;
	
		// 저체력시에 켜지는 주는 피해는 곱연산
		if (Effect.Trigger != ENUPassiveTrigger::OnLowHealth
			|| Effect.EffectType != ENUPassiveEffectType::DamageDealtMod
			|| FMath::IsNearlyZero(Effect.GetScaledValue()))
		{
			continue;
		}
	
		if (!IsPassiveTriggerSatisfied(Effect))
		{
			continue;
		}
	
		// 전투당 몇 번인지는 표에서 확인, 0이면 제한 X
		if (Effect.MaxPerBattle > 0 && !TryConsumeOncePerBattle(PassiveId))
		{
			continue;
		}
	
		// 값을 어떻게 얹는지는 StackType 에 따라 다름
		// 곱연산이면 배수(2 = x2), 합연산이면 비율 더하기(0.5 = x1.5)
		const float Applied = Effect.StackType == ENUEffectStackType::Multiplicative
			? Effect.GetScaledValue()
			: 1.f + Effect.GetScaledValue();
	
		Multiplier *= Applied;
	
		UE_LOG(LogTemp, Warning, TEXT("패시브 %d %s 발동 : 주는 피해 x%.2f"),
			PassiveId, *GetPassiveName(PassiveId), Applied);
	}
	
	return Multiplier;
}
	
bool UNUAbilitySystemComponent::ConsumeDeathPrevention(float HealthRatioBefore,
	float& OutSurvivalHealth)
{
	// 표가 안 적으면 겨우 버틴 것이다
	OutSurvivalHealth = 1.f;
	
	int32 PassiveId = 0;
	const FNUPassiveEffectRow* Effect =
		FindOwnedPassiveEffect(ENUPassiveEffectType::DeathPrevent, PassiveId);
	
	// 패시브가 없거나 조건이 안 맞거나 이미 썼으면 직업 줄을 이어서 봄
	if (Effect)
	{
		const bool bSatisfied = Effect->ThresholdCompare == ENUThresholdCompare::AtLeast
			? HealthRatioBefore >= Effect->TriggerThreshold
			: HealthRatioBefore <= Effect->TriggerThreshold;
		
		if (bSatisfied && (Effect->MaxPerBattle <= 0 || TryConsumeOncePerBattle(PassiveId)))
		{
			UE_LOG(LogTemp, Warning, TEXT("패시브 %d %s 발동 : HP 1로 버팀 (맞기 전 %.0f%%)"),
				PassiveId, *GetPassiveName(PassiveId), HealthRatioBefore * 100.f);
			
			return true;
		}
	}
	
	return ConsumeJobDeathPrevention(HealthRatioBefore, OutSurvivalHealth);
}

bool UNUAbilitySystemComponent::ConsumeJobDeathPrevention(float HealthRatioBefore,
	float& OutSurvivalHealth)
{
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;
		
		if (Row.EffectType != ENUJobEffectType::DeathPrevent
			|| Row.ImplementBy == ENUEffectImplementBy::Pending)
		{
			continue;
		}
		
		// IsJobConditionMet 을 쓰면 안됨 — IsJobConditionMet은 지금 체력을 보는데 여기선 이미 0 이라
		// HPBelow 가 늘 참이 되어 조건 없는 부활이 됨
		if (Row.ConditionType != ENUEffectCondition::HPBelow
			|| HealthRatioBefore * 100.f > Row.GetConditionNumber())
		{
			continue;
		}
		
		// 전투당 횟수 확인
		if (Row.MaxPerBattle > 0 && !TryConsumeOncePerBattle(Owned.JobSkillId))
		{
			continue;
		}
		
		OutSurvivalHealth = FindDeathPreventSurvivalHealth(Owned.JobSkillId);
		
		UE_LOG(LogTemp, Warning,
			TEXT("직업 스킬 %d 발동 : HP %.0f 으로 버팀 (맞기 전 %.0f%%)"),
			Owned.JobSkillId, OutSurvivalHealth, HealthRatioBefore * 100.f);
		
		return true;
	}
	
	return false;
}

float UNUAbilitySystemComponent::FindDeathPreventSurvivalHealth(int32 JobSkillId) const
{
	const float MaxHealth = GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
	
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;
		
		if (Owned.JobSkillId != JobSkillId
			|| Row.EffectType != ENUJobEffectType::Heal
			|| Row.DurationScope != ENUEffectDurationScope::Instant)
		{
			continue;
		}
		
		// 표가 백분율이면 최대 HP 기준, 아니면 절댓값
		return Row.bIsPercent ? MaxHealth * Row.GetScaledValue() : Row.Value;
	}
	
	return 1.f;
}

int32 UNUAbilitySystemComponent::SumStatusDurationBonus() const
{
	int32 Bonus = 0;
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		if (Effect.EffectType != ENUPassiveEffectType::StatusDurationMod
			|| Effect.ConditionType != ENUEffectCondition::OnStatusApplied)
		{
			continue;
		}
		
		// 현재 상태이상은 상대에게만 걸고있음
		// 나중에 아군에게 거는 상태이상이 생기면 여기서 분기해야함
		Bonus += FMath::RoundToInt(Effect.GetScaledValue());
	}
	
	return Bonus;
}

bool UNUAbilitySystemComponent::RollPassiveChance(const FNUPassiveEffectRow& Effect) const
{
	// 확률이 없는 행은 조건만 맞으면 확정 발동
	if (Effect.TriggerChance <= KINDA_SMALL_NUMBER)
	{
		return true;
	}
	
	return FMath::FRand() < Effect.TriggerChance;
}
	
void UNUAbilitySystemComponent::TriggerOnAttackPassives(UNUAbilitySystemComponent* TargetASC)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(TargetASC))
	{
		return;
	}
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		if (Effect.Trigger != ENUPassiveTrigger::OnAttack)
		{
			continue;
		}
	
		// 상태이상을 거는 행만 여기서 처리함
		// 주는 피해 증가(2803)·흡혈(2806)은 값을 묻는 축이라 피해 계산 쪽에서 이미 더해짐
		if (Effect.EffectType != ENUPassiveEffectType::StatusApply
			|| Effect.StatusEffectId <= 0)
		{
			continue;
		}
	
		// 확률 굴림은 TryApplyStatusEffect 안에서 한번만 처리함
		const float Chance = Effect.TriggerChance > KINDA_SMALL_NUMBER
			? Effect.TriggerChance
			: 1.f;
	
		TryApplyStatusEffect(TargetASC, Effect.StatusEffectId, Chance);
	}
}
	
bool UNUAbilitySystemComponent::RollPassiveEvade() const
{
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const int32 PassiveId = Owned.PassiveId;
		const FNUPassiveEffectRow& Effect = Owned.Row;
	
		if (Effect.Trigger != ENUPassiveTrigger::OnHitTaken
			|| Effect.EffectType != ENUPassiveEffectType::Evade)
		{
			continue;
		}
	
		if (RollPassiveChance(Effect))
		{
			UE_LOG(LogTemp, Warning, TEXT("패시브 회피 성공 : %d %s (확률 %.0f%%)"),
				PassiveId, *GetPassiveName(PassiveId), Effect.TriggerChance * 100.f);
	
			return true;
		}
	}
	
	return false;
}

float UNUAbilitySystemComponent::SumConditionalDamageBonus(const UNUAbilitySystemComponent* TargetASC) const
{
	if (!IsValid(TargetASC))
	{
		return 0.f;
	}
	
	float Bonus = 0.f;
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const FNUPassiveEffectRow& Effect = Owned.Row;
	
		// 공격시에 켜지는 "주는 피해" 중 대상의 상태를 조건으로 다는 것만 여기서 셈
		// 조건이 붙지 않은 행은 SumAlwaysDamageBonus 가 상시로 이미 세고 있음
		if (Effect.Trigger != ENUPassiveTrigger::OnAttack
			|| Effect.EffectType != ENUPassiveEffectType::DamageDealtMod
			|| Effect.StatusEffectId <= 0
			|| FMath::IsNearlyZero(Effect.GetScaledValue()))
		{
			continue;
		}
	
		// 확률이 붙은 행은 여기서 굴리지 않음, 피해 계산마다 불려 타격마다 결과가 달라짐
		if (Effect.TriggerChance > KINDA_SMALL_NUMBER)
		{
			continue;
		}
	
		const int32 RequiredStatusId = Effect.StatusEffectId;
		const bool bTargetHasStatus = TargetASC->HasStatusEffectMatching(
			[RequiredStatusId](const FNUStatusEffectDefinition& Status)
			{
				return Status.StatusEffectId == RequiredStatusId;
			});
	
		if (!bTargetHasStatus)
		{
			continue;
		}
	
		// 상한은 여기서 안 막음, 풀 전체의 상한이라 실행기 한 곳에서만 잘라야 함
		Bonus += Effect.GetScaledValue();
	}
	
	return Bonus;
}
	
float UNUAbilitySystemComponent::SumLifestealBonus() const
{
	float Bonus = 0.f;
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const FNUPassiveEffectRow& Effect = Owned.Row;
		
		// 공격시 자기 회복은 흡혈뿐
		// 처치시(2839)·턴시작(2840) 회복은 사건이 달라 ApplyHealPassivesFor 가 따로 처리함
		if (Effect.Trigger != ENUPassiveTrigger::OnAttack
			|| Effect.EffectType != ENUPassiveEffectType::Heal
			|| Effect.TargetScope != ENUPassiveTargetScope::Self)
		{
			continue;
		}
		
		if (Effect.TriggerChance > KINDA_SMALL_NUMBER)
		{
			continue;
		}
		
		Bonus += Effect.GetScaledValue();
	}
	
	// 스킬이 건 상태인 동안만 켜지는 흡혈 (2210 개화)
	for (const FNUSelfBoundState& State : SelfBoundStates)
	{
		Bonus += State.Lifesteal;
	}
	
	return Bonus;
}

float UNUAbilitySystemComponent::CalcDamageTakenMultiplier(float AttackerSpeed,
	const UNUAbilitySystemComponent* AttackerASC) const
{
	float Multiplier = 1.f;
	
	const float SelfSpeed = GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute());
	
	for (const FNUOwnedPassiveEffect& Owned : OwnedPassiveEffects)
	{
		const FNUPassiveEffectRow& Effect = Owned.Row;
	
		if (Effect.EffectType != ENUPassiveEffectType::DamageTakenMod
			|| FMath::IsNearlyZero(Effect.GetScaledValue()))
		{
			continue;
		}
	
		if (Effect.Trigger == ENUPassiveTrigger::OnHitTaken)
		{
			// 피격시는 여기서 판정함
			// IsPassiveTriggerSatisfied 는 자기만 보는 함수라 상대와 견주는 조건을 담을 수 없음
			if (Effect.ConditionType != ENUEffectCondition::AttackerSpeedLower)
			{
				continue;
			}
	
			if (AttackerSpeed >= SelfSpeed)
			{
				continue;
			}
		}
		else if (!IsPassiveTriggerSatisfied(Effect))
		{
			continue;
		}
	
		Multiplier *= FMath::Max(0.f, 1.f + Effect.GetScaledValue());
	}
	
	// 상시 직업 스킬의 받는 피해 항
	// 공격자를 대상 자리로 넘김 — 마크 · 문양은 적에게 붙어 있어 때린 쪽 칸을 봐야 함 (3148 마크 5중첩 · 3248 문양 상한)
	// 같은 편이 들고 있는 아군 범위 줄도 이 전투원에게 닿으면 함께 곱함 (3423 나이아 받는 피해 x0.94)
	TArray<const UNUAbilitySystemComponent*> JobOwners;
	CollectJobRowOwners(JobOwners);

	for (const UNUAbilitySystemComponent* Owner : JobOwners)
	{
		TArray<int32> JobIndices;
		SelectJobRows(Owner->OwnedJobEffects, ENUJobEffectType::DamageTakenMod, Owner, AttackerASC, JobIndices, this);

		for (const int32 Index : JobIndices)
		{
			const FNUJobSkillEffectRow& Row = Owner->OwnedJobEffects[Index].Row;
			const float Steps = JobValueSteps(Row, Owner, AttackerASC);

			// 곱연산 줄의 값은 배수 그대로(x0.88), 합연산 줄은 풀에 더할 비율(-0.12)
			Multiplier *= Row.StackType == ENUEffectStackType::Multiplicative
				? FMath::Max(0.f, JobMultiplierAmount(Row, Steps))
				: FMath::Max(0.f, 1.f + JobAdditiveAmount(Row, Steps));
		}
	}
	
	for (const FNUDamageTakenModInstance& Mod : DamageTakenMods)
	{
		Multiplier *= FMath::Max(0.f, Mod.Multiplier);
	}

	return Multiplier;
}

float UNUAbilitySystemComponent::GetHealReceivedMultiplier() const
{
	// 직업 스킬은 필드에서도 들고 있어서 전투 밖을 막지 않으면 필드 포션까지 깎임
	// 넷 전투는 서버가 회복량을 정하므로 여기서 곱하지 않음 (Encounter 가 없음)
	if (!IsValid(Encounter))
	{
		return 1.f;
	}

	return JobCodeRowMultiplier(OwnedJobEffects, ENUJobEffectType::HealReceivedMod, ENUJobTargetScope::Self, this);
}

float UNUAbilitySystemComponent::CalcShieldReceivedMultiplier() const
{
	// "아군 보호막" 줄(3444 하나된 존재)은 줄 주인이 아닌 같은 편 캐릭터가 받는 보호막에 곱함
	// 줄 주인 자신과 나이아는 안 닿음
	// 같은 편 목록은 받는 쪽인 자신을 빼고 살아 있는 전투원을 주므로, 줄 주인이 곧 받는 쪽인 경우는 이미 빠져 있음
	float Multiplier = 1.f;

	TArray<UNUAbilitySystemComponent*> Allies;
	CollectTeamTargets(true, Allies);

	for (const UNUAbilitySystemComponent* Ally : Allies)
	{
		if (Ally->DoesJobScopeReach(ENUJobTargetScope::Ally, this))
		{
			Multiplier *= JobCodeRowMultiplier(Ally->OwnedJobEffects, ENUJobEffectType::ShieldMod, ENUJobTargetScope::Ally, Ally);
		}
	}

	return Multiplier;
}

bool UNUAbilitySystemComponent::IsPlayerCharacter() const
{
	// 캐릭터의 ASC 는 PlayerState 가 들고 있음, 나이아는 SpiritState 라 거짓
	return Cast<ANUPlayerState>(GetOwnerActor()) != nullptr;
}

bool UNUAbilitySystemComponent::DoesJobScopeReach(ENUJobTargetScope Scope,
	const UNUAbilitySystemComponent* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	switch (Scope)
	{
	case ENUJobTargetScope::Self:
	case ENUJobTargetScope::None:
		return Candidate == this;

	case ENUJobTargetScope::Ally:
		return Candidate != this && Candidate->IsPlayerCharacter();

	case ENUJobTargetScope::AllyAndNaia:
		return Candidate != this;

	case ENUJobTargetScope::Party:
		return true;

	default:
		// 적 범위는 다른 편이라 여기서 다루지 않음
		return false;
	}
}

void UNUAbilitySystemComponent::SetActiveSkillIds(const TArray<int32>& InSkillIds)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	ActiveSkillIds = InSkillIds;
}

void UNUAbilitySystemComponent::SetPassiveSkillIds(const TArray<int32>& InPassiveIds)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	PassiveSkillIds = InPassiveIds;
	
	// 효과 줄 캐시를 먼저 채움, 아래 상시 스탯 적용이 그 캐시를 읽음
	RebuildOwnedPassiveEffects();
	
	ApplyPassiveStatModifiers();
}

const TArray<int32>& UNUAbilitySystemComponent::GetPassiveSkillIds() const
{
	return PassiveSkillIds;
}

const TArray<FNUOwnedPassiveEffect>& UNUAbilitySystemComponent::GetOwnedPassiveEffects() const
{
	return OwnedPassiveEffects;
}

void UNUAbilitySystemComponent::SetJobSkillIds(const TArray<int32>& InSkillIds)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	JobSkillIds = InSkillIds;

	// 효과 줄 캐시를 먼저 채움, 아래 상시 스탯 적용이 그 캐시를 읽는다
	RebuildOwnedJobEffects();

	ApplyPassiveStatModifiers();

	// 자원 비례 스탯은 따로 걸려 있어 보유 줄이 바뀌면 같이 다시 셈
	RefreshResourceScaledStats();
	RefreshJobStateStats();
	
	// 전투 중에 보유 줄이 바뀌면(계측) 체력 조건 상태와 상대에게 걸린 1당 스탯도 다시 셈, 전투 밖이면 상대가 없음
	RefreshJobOnceStates();
	
	TArray<UNUAbilitySystemComponent*> Opponents;
	CollectOpposingTargets(Opponents);
	
	for (UNUAbilitySystemComponent* Opponent : Opponents)
	{
		Opponent->RefreshOpponentScaledStats();
	}

	// 어떤 자원 게이지를 보일지는 보유 노드가 정함 (전투 중 계측으로 붙여도 바로 뜨게)
	NotifyJobResourcesChanged();
}

const TArray<int32>& UNUAbilitySystemComponent::GetJobSkillIds() const
{
	return JobSkillIds;
}

const TArray<FNUOwnedJobEffect>& UNUAbilitySystemComponent::GetOwnedJobEffects() const
{
	return OwnedJobEffects;
}

void UNUAbilitySystemComponent::RebuildOwnedJobEffects()
{
	OwnedJobEffects.Reset();

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Subsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	if (!IsValid(Subsystem))
	{
		return;
	}

	int32 MissingRows = 0;

	for (int32 SkillId : JobSkillIds)
	{
		const TArray<FNUJobSkillEffectRow>* Effects = Subsystem->FindJobSkillEffects(SkillId);
		if (!Effects || Effects->Num() == 0)
		{
			MissingRows += 1;
			continue;
		}

		// 행을 값으로 복사한다, 표를 재임포트하면 서브시스템 배열이 재할당되기 때문
		for (const FNUJobSkillEffectRow& Row : *Effects)
		{
			FNUOwnedJobEffect& Owned = OwnedJobEffects.AddDefaulted_GetRef();
			Owned.JobSkillId = SkillId;
			Owned.Row = Row;
		}
	}

	if (MissingRows > 0 && OwnedJobEffects.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("직업 스킬 효과 줄을 하나도 찾지 못했습니다 (보유 %d개) - "
				"DT_JobSkillEffect 와 설정의 JobSkillEffectTable 을 확인해야합니다"),
			JobSkillIds.Num());
	}
}

FNUJobResourceInstance* UNUAbilitySystemComponent::FindOrAddJobResourceSlot(FName ResourceName)
{
	const FNUJobResourceDefinition* Definition = FindResourceDef(GetWorld(), ResourceName);
	if (!Definition)
	{
		return nullptr;
	}
	
	int32 Index = JobResources.IndexOfByPredicate(
		[ResourceName](const FNUJobResourceInstance& Instance)
		{
			return Instance.ResourceName == ResourceName;
		});
	
	if (Index == INDEX_NONE)
	{
		Index = JobResources.AddDefaulted();
		JobResources[Index].ResourceName = ResourceName;
		JobResources[Index].Amount = Definition->InitialValue;
	}
	
	return &JobResources[Index];
}

void UNUAbilitySystemComponent::ApplyJobResourceGainBonus(const FNUJobSkillEffectRow& Row)
{
	// 상시 배수는 보유만으로 켜지므로 여기선 제외
	if (Row.DurationScope != ENUEffectDurationScope::Turns || Row.DurationTurns <= 0)
	{
		return;
	}
	
	if (Row.StackType != ENUEffectStackType::Multiplicative)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("직업 스킬 %d #%d : 기간제 획득 변조인데 StackType 이 곱연산이 아닙니다"),
			Row.JobSkillId, Row.EffectIndex);
		
		return;
	}
	
	FNUJobResourceInstance* Instance = FindOrAddJobResourceSlot(Row.ResourceName);
	if (!Instance)
	{
		return;
	}
	
	// 거는 턴이 끝날 때 한 번 깎으므로 2면 이번 턴과 다음 턴
	Instance->GainMultiplier = Row.GetScaledValue();
	Instance->GainBonusTurns = FMath::Max(Instance->GainBonusTurns, Row.DurationTurns);
}

int32 UNUAbilitySystemComponent::ResolveNaturalGainAmount(FName ResourceName, int32 BaseAmount) const
{
	int32 Amount = BaseAmount;
	
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;
		
		// 곱연산 줄은 획득 자체에 붙으므로 AddJobResource 에서 처리함, 여기는 덮어쓰기만
		// 판정 자리가 없는 조건이 붙은 줄은 켜지 않음 (3433 동화의 파동 — 다음 턴만인데 보유만으로 켜졌음)
		if (Row.EffectType != ENUJobEffectType::ResourceGainMod
			|| Row.ResourceName != ResourceName
			|| Row.DurationScope != ENUEffectDurationScope::Permanent
			|| Row.StackType != ENUEffectStackType::None
			|| Row.HasGatingCondition())
		{
			continue;
		}
		
		Amount = FMath::Max(Amount, FMath::RoundToInt(Row.Value));
	}
	
	return Amount;
}

int32 UNUAbilitySystemComponent::ApplyJobResourceGainModifiers(const FNUJobResourceInstance& Instance, int32 Delta) const
{
	if (Delta <= 0)
	{
		return Delta;
	}
	
	float Amount = Delta;
	
	// 기간제 배수 (3437 대지의 뿌리 — 2턴간 응전 획득 2배)
	if (Instance.GainBonusTurns > 0)
	{
		Amount *= Instance.GainMultiplier;
	}
	
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;
		
		// 조건이 붙은 줄은 판정 자리가 없어 건너뜀 (3433 동화의 파동 x2 — 다음 턴만)
		if (Row.EffectType == ENUJobEffectType::ResourceGainMod
			&& Row.ResourceName == Instance.ResourceName
			&& Row.DurationScope == ENUEffectDurationScope::Permanent
			&& Row.StackType == ENUEffectStackType::Multiplicative
			&& !Row.HasGatingCondition())
		{
			Amount *= Row.GetScaledValue();
		}
	}
	
	int32 Result = FMath::RoundToInt(Amount);
	
	// 턴당 획득 상한 (3230 초과 부하 — 마력 응축 턴당 1)
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;
		
		if (Row.EffectType != ENUJobEffectType::ResourceGainCap
			|| Row.ResourceName != Instance.ResourceName
			|| Row.HasGatingCondition())
		{
			continue;
		}
		
		const int32 Remaining = FMath::RoundToInt(Row.Value) - Instance.GainedThisTurn;
		
		Result = FMath::Min(Result, FMath::Max(0, Remaining));
	}
	
	return Result;
}

void UNUAbilitySystemComponent::ApplyActionJobResourceRows(int32 SkillId)
{
	if (SkillId <= 0)
	{
		return;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	
	const TArray<FNUJobSkillEffectRow>* Effects =
		IsValid(Data) ? Data->FindJobSkillEffects(SkillId) : nullptr;
	
	if (!Effects)
	{
		return;
	}
	
	// 마법을 쓰면 비는 자원 (임계), 이 행동이 새로 얻는 것보다 먼저 비워야 함
	ConsumeJobResourcesOnMagicUse(SkillId, *Effects);

	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		// 표가 이 갈래로 보낸 줄만 여기서 확인함, Data면 타격마다 도는 길이 따로 있음
		if (Row.ImplementBy != ENUEffectImplementBy::CodeResource || Row.ResourceName.IsNone())
		{
			continue;
		}
		
		if (Row.EffectType == ENUJobEffectType::ResourceGainMod)
		{
			ApplyJobResourceGainBonus(Row);
			continue;
		}
		
		if (Row.EffectType == ENUJobEffectType::ResourceDecayImmune)
		{
			ApplyJobResourceDecayImmunity(Row);
			continue;
		}
		
		if (Row.EffectType != ENUJobEffectType::ResourceGain)
		{
			continue;
		}
		
		// 어트리뷰트(SP)로 가는 줄과 무엇 1당 붙은 줄은 다른 쪽에서 처리함
		if (!Row.ValuePer.IsNone() || Row.AffectedStat != ENUStatusAffectedStat::None)
		{
			continue;
		}
		
		// 자원은 개수라 나누지 않음 (3406 야수의 발톱 — 다타여도 동화율 총 +1)
		int32 Delta = FMath::RoundToInt(Row.Value);
		
		if (Row.bIsPercent)
		{
			// 보유량의 비율 (3133 템포 절반 · 3441 응전 절반)
			int32 Amount = 0;
			if (!TryGetJobResource(Row.ResourceName, Amount) || Amount <= 0)
			{
				continue;
			}
			
			// 홀수의 절반을 어느 쪽으로 할지는 표에 칸이 없어 가까운 쪽으로 둔다
			Delta = FMath::RoundToInt(Amount * Row.GetScaledValue());
		}
		
		AddJobResource(Row.ResourceName, Delta);
	}

	// 패시브가 "공격 스킬을 쓸 때마다" 쌓는 자원 (3105 연사 — 템포 +2), 피해를 주는 스킬일 때만 셈
	const bool bAttackSkill = Effects->ContainsByPredicate([](const FNUJobSkillEffectRow& Candidate)
	{
		return Candidate.EffectType == ENUJobEffectType::Damage;
	});

	if (!bAttackSkill)
	{
		return;
	}

	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;

		// 액티브 스킬의 줄은 그 스킬을 쓸 때만 돌아야 해서 패시브만 받음, 가지고만 있어서 쌓이면 안 됨
		if (Row.ConditionType != ENUEffectCondition::OnAttackSkillUse
			|| Row.ImplementBy == ENUEffectImplementBy::Pending
			|| Row.EffectType != ENUJobEffectType::ResourceGain
			|| Row.ResourceName.IsNone()
			|| !Row.ValuePer.IsNone()
			|| Row.bIsPercent
			|| (Row.TargetScope != ENUJobTargetScope::Self && Row.TargetScope != ENUJobTargetScope::None)
			|| HasActiveSkill(Owned.JobSkillId))
		{
			continue;
		}

		const int32 Gained = AddJobResource(Row.ResourceName, FMath::RoundToInt(Row.Value));

		UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 공격 스킬을 써서 %s +%d"),
			Owned.JobSkillId, *Row.ResourceName.ToString(), Gained);
	}
}

void UNUAbilitySystemComponent::ApplyJobResourceDecayImmunity(const FNUJobSkillEffectRow& Row)
{
	// 상시 줄은 보유만으로 켜지므로 여기서 걸 것이 없음 (3035 영력 고정)
	if (Row.DurationScope != ENUEffectDurationScope::Turns || Row.DurationTurns <= 0)
	{
		return;
	}
	
	const FNUJobResourceDefinition* Definition = FindResourceDef(GetWorld(), Row.ResourceName);
	if (!Definition)
	{
		return;
	}
	
	// 대상에게 붙는 자원(문양)은 칸이 상대에게 있고 감쇠도 거기서 돎, 시전자 칸에 걸면 아무 일도 안 함 (3237 지맥 연결)
	// 그래서 살아 있는 상대 전원의 칸에 검
	TArray<UNUAbilitySystemComponent*> Holders;
	if (Definition->OwnerScope == ENUJobResourceOwner::Target)
	{
		CollectOpposingTargets(Holders);
	}
	else
	{
		Holders.Add(this);
	}
	
	for (UNUAbilitySystemComponent* Holder : Holders)
	{
		if (!IsValid(Holder))
		{
			continue;
		}
		
		// 칸이 아직 없으면 생성함 — 수치가 0 이어도 이번 턴 면역은 걸려 있어야 함
		FNUJobResourceInstance* Instance = Holder->FindOrAddJobResourceSlot(Row.ResourceName);
		if (!Instance)
		{
			continue;
		}
		
		// 거는 턴이 끝날 때 AdvanceJobResourceDecay 가 한 번 깎으므로 1 이면 이번 턴만
		Instance->DecayImmuneTurns = FMath::Max(Instance->DecayImmuneTurns, Row.DurationTurns);
	}
}

bool UNUAbilitySystemComponent::HasPermanentDecayImmunity(FName ResourceName) const
{
	if (ResourceName.IsNone())
	{
		return false;
	}
	
	// 상시 면역은 켜고 끄는 것이 아니라 그 노드를 가지고 있는 것이라 보유 줄을 그대로 봄
	return OwnedJobEffects.ContainsByPredicate([ResourceName](const FNUOwnedJobEffect& Owned)
	{
		return Owned.Row.EffectType == ENUJobEffectType::ResourceDecayImmune
			&& Owned.Row.DurationScope == ENUEffectDurationScope::Permanent
			&& Owned.Row.ResourceName == ResourceName;
	});
}

float UNUAbilitySystemComponent::ConsumeJobCoefficientAdd(int32 SkillId)
{
	if (SkillId <= 0)
	{
		return 0.f;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	
	// 나이아 스킬은 이 표에 없으므로 그냥 0, 번호가 겹치지 않아 오인될 일이 없음
	const TArray<FNUJobSkillEffectRow>* Effects =
		IsValid(Data) ? Data->FindJobSkillEffects(SkillId) : nullptr;
	
	if (!Effects)
	{
		return 0.f;
	}
	
	float Total = 0.f;
	
	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		// Pending 만 막음 — 표가 아직 덜 완성되어 읽으면 조건 없는 효과가 됨
		if (Row.EffectType != ENUJobEffectType::CoefficientAdd
			|| Row.ImplementBy == ENUEffectImplementBy::Pending)
		{
			continue;
		}
		
		// 대상에게 붙는 자원이 기준인 줄은 발동할 때 대상을 몰라 못 셈, 타격할 때 대상마다 셈
		if (IsTargetCoefficientRow(Row, GetWorld()))
		{
			continue;
		}
		
		// 못 세는 기준은 0 이 아니라 줄 빼기
		// 지금 빠지는 것은 3151 롱샷(속도차이) 하나, 대상이 있어야 세는 파생값
		const float Steps = JobValueSteps(Row, this);
		if (Steps < 0.f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("직업 스킬 %d #%d : 계수 가산의 기준 '%s' 을 못 세서 건너뜁니다"),
				SkillId, Row.EffectIndex, *Row.ValuePer.ToString());
			
			continue;
		}
		
		Total += JobAdditiveAmount(Row, Steps);
		
		// 표가 소모라고 적은 줄만 자원을 씀 (3233 응축 파열은 쓰고 3229 마력 방출은 안 씀)
		// 얼마를 쓰는지 적는 칸이 없고 설명이 여섯 줄 모두 "전량 소모" 라 전부 비움
		if (!Row.bConsumeResource || Row.ValuePer.IsNone())
		{
			continue;
		}
		
		int32 Amount = 0;
		if (TryGetJobResource(Row.ValuePer, Amount) && Amount > 0)
		{
			AddJobResource(Row.ValuePer, -Amount);
		}
	}
	
	return Total;
}

float UNUAbilitySystemComponent::SumJobActionDamageBonus(int32 SkillId) const
{
	if (SkillId <= 0)
	{
		return 0.f;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	
	// 나이아 스킬은 이 표에 없으므로 그냥 0
	const TArray<FNUJobSkillEffectRow>* Effects =
		IsValid(Data) ? Data->FindJobSkillEffects(SkillId) : nullptr;
	
	if (!Effects)
	{
		return 0.f;
	}
	
	float Total = 0.f;
	
	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		// 컨텍스트가 표만 보고 실을 수 없었던 줄만 — 거르는 조건 없는 Data 줄에 1당이 붙은 것
		if (Row.EffectType != ENUJobEffectType::DamageDealtMod
			|| Row.ImplementBy != ENUEffectImplementBy::Data
			|| Row.HasGatingCondition()
			|| Row.ValuePer.IsNone())
		{
			continue;
		}
		
		// 곱연산 1당 줄은 아직 없음, 합연산 풀에 섞으면 값이 틀리므로 뺌
		if (Row.StackType == ENUEffectStackType::Multiplicative)
		{
			UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 곱연산 1당 줄은 아직 자리가 없어 건너뜁니다"),
				SkillId, Row.EffectIndex);
			
			continue;
		}
		
		// 못 세는 기준은 0 이 아니라 줄 빼기 (3217 공용 `자원` · 3333 패 — 카드가 아직 없음)
		const float Steps = JobValueSteps(Row, this);
		if (Steps < 0.f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("직업 스킬 %d #%d : 주는 피해의 기준 '%s' 을 못 세서 건너뜁니다"),
				SkillId, Row.EffectIndex, *Row.ValuePer.ToString());
			
			continue;
		}
		
		const float Amount = JobAdditiveAmount(Row, Steps);
		Total += Amount;
		
		// 피해 숫자만으로는 몇 개분이 붙었는지 가리기 어려워 남김
		UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : %s 기준 %.0f 번이라 주는 피해 +%.1f%% (합연산)"),
			SkillId, *Row.ValuePer.ToString(), Steps, Amount * 100.f);
	}
	
	return Total;
}

float UNUAbilitySystemComponent::SumTargetJobCoefficientAdd(int32 SkillId,
	const UNUAbilitySystemComponent* TargetASC) const
{
	if (SkillId <= 0 || !IsValid(TargetASC))
	{
		return 0.f;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	
	const TArray<FNUJobSkillEffectRow>* Effects =
		IsValid(Data) ? Data->FindJobSkillEffects(SkillId) : nullptr;
	
	if (!Effects)
	{
		return 0.f;
	}
	
	float Total = 0.f;
	
	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		if (!IsTargetCoefficientRow(Row, GetWorld()))
		{
			continue;
		}
		
		// 대상 쪽 칸을 읽음, 한 번도 안 쌓인 대상은 칸이 없어 셀 것이 없음
		const float Steps = JobValueSteps(Row, this, TargetASC);
		if (Steps <= 0.f)
		{
			continue;
		}
		
		const float Amount = JobAdditiveAmount(Row, Steps);
		Total += Amount;
		
		// 피해 숫자만으로는 몇 개분이 붙었는지 가리기 어려워 남김
		UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 대상의 %s %.0f 개라 계수 +%.0f%%p"),
			SkillId, *Row.ValuePer.ToString(), Steps, Amount * 100.f);
	}
	
	return Total;
}

void UNUAbilitySystemComponent::ConsumeTargetJobCoefficientResources(int32 SkillId,
	UNUAbilitySystemComponent* TargetASC)
{
	if (SkillId <= 0 || !IsValid(TargetASC))
	{
		return;
	}
	
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	
	const TArray<FNUJobSkillEffectRow>* Effects =
		IsValid(Data) ? Data->FindJobSkillEffects(SkillId) : nullptr;
	
	if (!Effects)
	{
		return;
	}
	
	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		// 표가 소모라고 적은 줄만 대상에게서 씀 (3140 · 3241 은 전량 소모, 3209 는 안 씀)
		if (!Row.bConsumeResource || !IsTargetCoefficientRow(Row, GetWorld()))
		{
			continue;
		}
		
		int32 Amount = 0;
		if (TargetASC->TryGetJobResource(Row.ValuePer, Amount) && Amount > 0)
		{
			TargetASC->AddJobResource(Row.ValuePer, -Amount);
		}
	}
}

float UNUAbilitySystemComponent::SumJobDamageBonus(const UNUAbilitySystemComponent* TargetASC) const
{
	TArray<const UNUAbilitySystemComponent*> Owners;
	CollectJobRowOwners(Owners);

	float Bonus = 0.f;

	for (const UNUAbilitySystemComponent* Owner : Owners)
	{
		TArray<int32> Indices;
		SelectJobRows(Owner->OwnedJobEffects, ENUJobEffectType::DamageDealtMod, Owner, TargetASC, Indices, this);

		for (const int32 Index : Indices)
		{
			const FNUJobSkillEffectRow& Row = Owner->OwnedJobEffects[Index].Row;

			if (Row.StackType == ENUEffectStackType::Multiplicative)
			{
				continue;
			}

			// 상한은 줄마다 다르므로 여기서 자름, 풀 전체의 상한은 실행기가 한 번 더 자름
			Bonus += JobAdditiveAmount(Row, JobValueSteps(Row, Owner, TargetASC));
		}
	}

	return Bonus;
}

float UNUAbilitySystemComponent::GetJobDamageMultiplier(const UNUAbilitySystemComponent* TargetASC) const
{
	TArray<const UNUAbilitySystemComponent*> Owners;
	CollectJobRowOwners(Owners);

	float Multiplier = 1.f;

	for (const UNUAbilitySystemComponent* Owner : Owners)
	{
		TArray<int32> Indices;
		SelectJobRows(Owner->OwnedJobEffects, ENUJobEffectType::DamageDealtMod, Owner, TargetASC, Indices, this);

		for (const int32 Index : Indices)
		{
			const FNUJobSkillEffectRow& Row = Owner->OwnedJobEffects[Index].Row;

			if (Row.StackType != ENUEffectStackType::Multiplicative)
			{
				continue;
			}

			Multiplier *= FMath::Max(0.f, JobMultiplierAmount(Row, JobValueSteps(Row, Owner, TargetASC)));
		}
	}

	return Multiplier;
}

float UNUAbilitySystemComponent::SumJobPerAttackCriticalBonus(ENUStatusAffectedStat Stat,
	const UNUAbilitySystemComponent* TargetASC) const
{
	// 실행기가 어트리뷰트 값에 바로 더할 수 있는 것은 비율 축(치명타 확률 · 피해)뿐임
	if (!NUStatusEffect::IsRatioStat(Stat))
	{
		return 0.f;
	}

	TArray<const UNUAbilitySystemComponent*> Owners;
	CollectJobRowOwners(Owners);

	float Bonus = 0.f;

	for (const UNUAbilitySystemComponent* Owner : Owners)
	{
		TArray<int32> Indices;
		SelectJobRows(Owner->OwnedJobEffects, ENUJobEffectType::StatModifier, Owner, TargetASC, Indices, this);

		for (const int32 Index : Indices)
		{
			const FNUJobSkillEffectRow& Row = Owner->OwnedJobEffects[Index].Row;

			// 공격마다 값이 갈리는 줄만, 나머지는 이미 상시 스탯 GE 로 어트리뷰트에 들어가 있음
			if (Row.AffectedStat != Stat || !IsPerAttackCriticalRow(Row))
			{
				continue;
			}

			// 상시 스탯 GE 와 같은 단위, 치명타 두 축은 백분율 값이 합으로 들어감
			Bonus += JobAdditiveAmount(Row, JobValueSteps(Row, Owner, TargetASC));
		}
	}

	return Bonus;
}

const TArray<int32>& UNUAbilitySystemComponent::GetActiveSkillIds() const
{
	return ActiveSkillIds;
}

bool UNUAbilitySystemComponent::HasActiveSkill(int32 SkillId) const
{
	if (SkillId <= 0)
	{
		return false;
	}

	if (ActiveSkillIds.Contains(SkillId))
	{
		return true;
	}

	// 직업 스킬은 패시브가 아니라 액티브인지도 확인해야 함
	if (!JobSkillIds.Contains(SkillId))
	{
		return false;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* BattleData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	const FNUJobSkillDefinition* Definition = IsValid(BattleData)
		? BattleData->FindJobSkillDefinition(SkillId)
		: nullptr;

	if (!Definition)
	{
		return false;
	}

	static const FName ActiveType(TEXT("액티브"));
	static const FName UltimateType(TEXT("필살기"));

	return Definition->SkillType == ActiveType || Definition->SkillType == UltimateType;
}

FGameplayAbilitySpecHandle UNUAbilitySystemComponent::FindOrGiveExecutor(
	TSubclassOf<UNUBattleGameplayAbility> ExecutorClass)
{
	if (!ExecutorClass)
	{
		return FGameplayAbilitySpecHandle();
	}
	
	if (const FGameplayAbilitySpecHandle* Found = ExecutorHandles.Find(ExecutorClass))
	{
		return *Found;
	}
	
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return FGameplayAbilitySpecHandle();
	}
	
	// 스킬 목록이 바뀌어도 다시 부여할 것이 없도록 처음 사용할 때 부여함
	const FGameplayAbilitySpecHandle Handle = GiveAbility(FGameplayAbilitySpec(ExecutorClass, 1));
	ExecutorHandles.Add(ExecutorClass, Handle);
	
	return Handle;
}

void UNUAbilitySystemComponent::SetPendingActionContext(const FNUBattleActionContext& Context)
{
	PendingActionContext = Context;
}

const FNUBattleActionContext& UNUAbilitySystemComponent::PeekActionContext() const
{
	return PendingActionContext;
}

FNUBattleActionContext UNUAbilitySystemComponent::ConsumeActionContext()
{
	FNUBattleActionContext Consumed = MoveTemp(PendingActionContext);
	PendingActionContext = FNUBattleActionContext();	// 명시적 초기화
	
	return Consumed;
}

void UNUAbilitySystemComponent::ClearPendingActionContext()
{
	PendingActionContext = FNUBattleActionContext();
}

void UNUAbilitySystemComponent::BeginCurrentAction(const UNUBattleGameplayAbility* Ability,
	const FNUBattleActionContext& Context, int32 HitCount)
{
	CurrentActionAbility = Ability;
	CurrentActionContext = Context;
	CurrentActionHitCount = FMath::Max(1, HitCount);
}

void UNUAbilitySystemComponent::EndCurrentAction(const UNUBattleGameplayAbility* Ability)
{
	if (CurrentActionAbility.Get() != Ability)
	{
		return;
	}
	
	CurrentActionAbility.Reset();
	CurrentActionContext = FNUBattleActionContext();
	CurrentActionHitCount = 0;
}

const FNUBattleActionContext& UNUAbilitySystemComponent::GetCurrentActionContext() const
{
	return CurrentActionContext;
}

int32 UNUAbilitySystemComponent::GetCurrentActionHitCount() const
{
	return CurrentActionHitCount;
}

void UNUAbilitySystemComponent::NoteAttackHitDealt()
{
	++HitsDealtThisTurn;
	
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		const FNUJobSkillEffectRow& Row = Owned.Row;
		
		// 기준에 닿는 순간만 봄, 넘긴 뒤의 공격에서는 할 일이 없음
		if (Row.ConditionType != ENUEffectCondition::HitCountAtLeast
			|| Row.ImplementBy != ENUEffectImplementBy::Data
			|| HitsDealtThisTurn != FMath::RoundToInt(Row.GetConditionNumber()))
		{
			continue;
		}
		
		// 상시 줄은 조건 판정이 다음 공격부터 켬 (3050 연쇄 ×1.4), 피해 숫자만으로는 켜진 때를 가리기 어려워 남김
		if (IsJobAlwaysRow(Row))
		{
			UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 이번 턴 %d번 공격해서 다음 공격부터 #%d 줄이 켜짐"),
				Owned.JobSkillId, HitsDealtThisTurn, Row.EffectIndex);
			
			continue;
		}
		
		// 닿는 순간 한 번 쌓이는 자원 줄 (3105 연사 — 템포 +2), 표가 Instant 로 적어 넘긴 뒤의 공격마다 쌓지 않음
		// 액티브 스킬의 줄은 그 스킬을 쓸 때만 돌아야 해서 패시브만 받음, 가지고만 있어서 쌓이면 안 됨
		if (Row.EffectType != ENUJobEffectType::ResourceGain
			|| Row.ResourceName.IsNone()
			|| (Row.TargetScope != ENUJobTargetScope::Self && Row.TargetScope != ENUJobTargetScope::None)
			|| HasActiveSkill(Owned.JobSkillId))
		{
			continue;
		}
		
		// 대상에게 붙는 자원은 여기서 대상을 몰라 못 쌓음, 지금 표에는 이런 줄이 없음
		const FNUJobResourceDefinition* Def = FindResourceDef(GetWorld(), Row.ResourceName);
		if (Def && Def->OwnerScope == ENUJobResourceOwner::Target)
		{
			UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 공격 횟수 줄의 자원 '%s' 이 대상에게 붙는 것이라 건너뜁니다"),
				Owned.JobSkillId, Row.EffectIndex, *Row.ResourceName.ToString());
			
			continue;
		}
		
		const int32 Gained = AddJobResource(Row.ResourceName, FMath::RoundToInt(Row.Value));
		
		UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 이번 턴 %d번째 공격이라 %s +%d"),
			Owned.JobSkillId, HitsDealtThisTurn, *Row.ResourceName.ToString(), Gained);
	}
}

int32 UNUAbilitySystemComponent::GetHitsDealtThisTurn() const
{
	return HitsDealtThisTurn;
}

void UNUAbilitySystemComponent::SetHitsDealtThisTurn(int32 Count)
{
	HitsDealtThisTurn = FMath::Max(0, Count);
}

void UNUAbilitySystemComponent::RefreshJobOnceStates()
{
	// 전투 밖의 체력 변화(필드 포션)로 켜지면 안 됨
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(Encounter))
	{
		return;
	}
	
	// 스킬마다 한 번 판정함, 한 스킬의 줄들은 같은 조건을 들고 옴 (3027 #1 · #2 — HP 25% 이하)
	TSet<int32> Checked;
	
	for (const FNUOwnedJobEffect& Owned : OwnedJobEffects)
	{
		if (!IsJobOnceStateRow(Owned.Row) || Checked.Contains(Owned.JobSkillId))
		{
			continue;
		}
		
		Checked.Add(Owned.JobSkillId);
		
		const bool bMet = IsJobConditionMet(Owned.Row, this, nullptr);
		
		// 아이콘이 없어 켜지고 끝나는 때를 로그로만 볼 수 있음
		if (bMet && !UsedJobOnceStates.Contains(Owned.JobSkillId))
		{
			ActiveJobOnceStates.Add(Owned.JobSkillId);
			UsedJobOnceStates.Add(Owned.JobSkillId);
			
			UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 전투당 1회 상태 켜짐"), Owned.JobSkillId);
		}
		else if (!bMet && ActiveJobOnceStates.Remove(Owned.JobSkillId) > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 전투당 1회 상태 끝남 — 이번 전투에 다시 안 켜짐"), Owned.JobSkillId);
		}
	}
}

bool UNUAbilitySystemComponent::IsJobOnceStateActive(int32 JobSkillId) const
{
	return ActiveJobOnceStates.Contains(JobSkillId);
}

const TSet<int32>& UNUAbilitySystemComponent::GetUsedJobOnceStates() const
{
	return UsedJobOnceStates;
}

void UNUAbilitySystemComponent::NoteActiveSkillUsed(int32 SkillId)
{
	if (SkillId <= 0)
	{
		return;
	}

	++ActiveSkillUsesThisBattle.FindOrAdd(SkillId);
}

int32 UNUAbilitySystemComponent::GetActiveSkillUseCount(int32 SkillId) const
{
	const int32* Found = ActiveSkillUsesThisBattle.Find(SkillId);

	return Found ? *Found : 0;
}

const TMap<int32, int32>& UNUAbilitySystemComponent::GetActiveSkillUseCounts() const
{
	return ActiveSkillUsesThisBattle;
}

void UNUAbilitySystemComponent::RefreshOpponentScaledStats()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	
	const UWorld* World = GetWorld();
	
	// 전투 밖이면 상대가 없어 아무 줄도 안 모임, 걸어 둔 것이 있으면 아래에서 걷음
	TArray<UNUAbilitySystemComponent*> Opponents;
	CollectOpposingTargets(Opponents);
	
	TMap<FGameplayTag, float> Magnitudes;
	NUStatusEffect::FillNeutralStatModifiers(Magnitudes);
	
	bool bAnyAmount = false;
	
	for (const UNUAbilitySystemComponent* Opponent : Opponents)
	{
		for (const FNUOwnedJobEffect& Owned : Opponent->GetOwnedJobEffects())
		{
			const FNUJobSkillEffectRow& Row = Owned.Row;
			
			if (!IsOpponentScaledStatRow(Row, World))
			{
				continue;
			}
			
			const NUStatusEffect::FNUStatModifierAxis* Axis =
				NUStatusEffect::GetStatModifierAxes().Find(Row.AffectedStat);
			
			if (!Axis)
			{
				continue;
			}
			
			// 이 전투원에게 붙은 자원으로 셈, 한 번도 안 쌓였으면 칸이 없어 0
			const float Amount = JobAdditiveAmount(Row, FMath::Max(0.f, JobValueSteps(Row, Opponent, this)));
			
			if (FMath::IsNearlyZero(Amount))
			{
				continue;
			}
			
			// 상시 스탯 GE 와 같은 규칙 — 수치 줄은 합으로, 백분율 줄은 배율에 더함
			if (float* Slot = Magnitudes.Find(Row.bIsPercent ? Axis->MultiplierTag : Axis->AdditiveTag))
			{
				*Slot += Amount;
				bAnyAmount = true;
			}
		}
	}
	
	const bool bHasHandle = OpponentScaledStatHandle.IsValid() && GetActiveGameplayEffect(OpponentScaledStatHandle);
	const float SpeedBefore = GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute());
	
	if (!bAnyAmount)
	{
		// 붙은 자원이 다 사라졌거나 줄을 가진 상대가 없음, 속도라 지웠다 다시 걸어도 깎이는 현재값이 없음
		if (bHasHandle)
		{
			RemoveActiveGameplayEffect(OpponentScaledStatHandle);
		}
		
		OpponentScaledStatHandle.Invalidate();
	}
	else
	{
		// 속도는 설명의 바닥 밑으로 안 내림, 이 GE 가 지금 빼고 있는 만큼을 되돌려 이 GE 없는 속도를 구함
		// 이미 바닥 밑이면 더 내리지도 올리지도 않음 — 속도 배율이 걸린 적은 대략만 맞음(지금은 그런 적이 없음)
		const NUStatusEffect::FNUStatModifierAxis* SpeedAxis =
			NUStatusEffect::GetStatModifierAxes().Find(ENUStatusAffectedStat::Speed);
		
		float* SpeedSlot = SpeedAxis ? Magnitudes.Find(SpeedAxis->AdditiveTag) : nullptr;
		if (SpeedSlot && *SpeedSlot < 0.f)
		{
			const FActiveGameplayEffect* Active = bHasHandle ? GetActiveGameplayEffect(OpponentScaledStatHandle) : nullptr;
			const float Applied = Active ? Active->Spec.GetSetByCallerMagnitude(SpeedAxis->AdditiveTag, false, 0.f) : 0.f;
			const float SpeedWithout = SpeedBefore - Applied;
			
			*SpeedSlot = FMath::Min(0.f, FMath::Max(*SpeedSlot, MarkSpeedFloor - SpeedWithout));
		}
		
		// 걸어 둔 것이 있으면 값만 고침, 스탯이 상한 축이면 지웠다 다시 걸 때 현재값이 깎이므로 같은 규칙을 따름
		if (bHasHandle)
		{
			UpdateActiveGameplayEffectSetByCallerMagnitudes(OpponentScaledStatHandle, Magnitudes);
		}
		else
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
				UNUGE_PassiveStatModifier::StaticClass(), 1.f, MakeEffectContext());
			
			if (SpecHandle.IsValid())
			{
				for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
				{
					SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
				}
				
				OpponentScaledStatHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}
	
	// 속도 칸만 봐서는 마크 때문인지 가리기 어려워 남김
	const float SpeedAfter = GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute());
	if (!FMath::IsNearlyEqual(SpeedBefore, SpeedAfter))
	{
		UE_LOG(LogTemp, Log, TEXT("[직업] %s : 상대 줄(대상에게 붙은 자원 1당)로 속도 %.0f -> %.0f"),
			*GetNameSafe(GetOwnerActor()), SpeedBefore, SpeedAfter);
	}
}
