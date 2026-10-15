// Fill out your copyright notice in the Description page of Project Settings.


#include "NUActiveSkillContext.h"

#include "NUActiveSkillDefinition.h"
#include "NUBattleDataSubsystem.h"
#include "Battle/NUBattleActionContext.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Ability/NUActiveSkillAbility.h"
#include "NUGAS/GameplayEffect/NUSkillGameplayEffect.h"
#include "NUGAS/GameplayEffect/NUStatusGameplayEffect.h"

namespace
{
	// 접근 / 액션 / 복귀 시간과 판정 지점은 스킬마다 다른 값이 아니라
	// 어떻게 움직이는가마다 다른 값이고 애니메이션 길이에 종속되므로
	// 테이블에 넣지 않고 여기서 판정함
	ENUBattleActionMotion ResolveMotion(bool bHasAnyDamageRow)
	{
		// 피해가 없으면 붙을 이유가 없음 (버프, 디버프, 회복)
		return bHasAnyDamageRow
			? ENUBattleActionMotion::MoveToCenter
			: ENUBattleActionMotion::InPlace;
	}
	
	// 이 액티브 스킬의 대상
	enum class ESide : uint8
	{
		Self,
		Ally,
		Enemy
	};
	
	ESide SideOfScope(ENUSkillTargetScope Scope)
	{
		switch (Scope)
		{
		case ENUSkillTargetScope::Self:		return ESide::Self;
		case ENUSkillTargetScope::Ally:		return ESide::Ally;
		default:							return ESide::Enemy;
		}
	}
	
	ESide SideOfPolicy(ENUTargetPolicy Policy)
	{
		switch (Policy)
		{
		case ENUTargetPolicy::Self:			return ESide::Self;
		case ENUTargetPolicy::SingleAlly:
		case ENUTargetPolicy::AllAllies:	return ESide::Ally;
		default:							return ESide::Enemy;
		}
	}
	
	// 이 줄을 어빌리티의 대상 집합으로 보낼 수 있는지
	// 지금 적을 대상으로 공격하고, 아군 대상으로 회복하는 건 구현이 안되어서 건너뜀
	bool CanRouteToTargets(const FNUActiveEffectRow& Row, ENUTargetPolicy Policy)
	{
		const ESide RowSide = SideOfScope(Row.TargetScope);
		
		return RowSide == ESide::Self || RowSide == SideOfPolicy(Policy);
	}
	
	// 이 줄의 조건을 지금 코드가 감당하는지
	bool IsConditionHandled(const FNUActiveEffectRow& Row)
	{
		if (Row.ConditionType == ENUEffectCondition::None)
		{
			return true;
		}
		
		switch (Row.EffectType)
		{
		case ENUSkillEffectType::Reflect:
			// 반사가 언제 터지는지는 ASC가 알고있음
			// 표가 자기 관점(OnHitTaken)으로 적든 상대 관점(OnAttack)으로 적든 같은 것을 말함
			return Row.ConditionType == ENUEffectCondition::OnAttack;
		
		case ENUSkillEffectType::StatusCure:
			// 해제는 걸린 것 중에서 고르는 것이라 아무 상태이상이나 해제함
			return Row.ConditionType == ENUEffectCondition::AnyStatus;
		
		case ENUSkillEffectType::StatusApply:
			// 대상이 그 상태여야 켜지는 것 (2410 뇌신의 심판) 과 몇 턴 동안 들고 있다가 때릴 때 거는 것 (2007 · 2008)
			return Row.ConditionType == ENUEffectCondition::TargetHasStatus
				|| Row.ConditionType == ENUEffectCondition::OnAttack;
		
		case ENUSkillEffectType::DamageDealtMod:
			// 대상이 그 상태여야 켜지는 것, 적용하는 자리에서 대상마다 따로 봄 (2410 뇌신의 심판)
			return Row.ConditionType == ENUEffectCondition::TargetHasStatus;
		
		case ENUSkillEffectType::Damage:
			// 상태가 풀릴 때 터지는 피해, 지금 때리는 것이 아니라 예약 (2210 개화)
			return Row.ConditionType == ENUEffectCondition::OnStateEnd;
		
		case ENUSkillEffectType::Lifesteal:
			// 그 상태인 동안만 얹히는 흡혈, ASC 가 상태와 함께 들고 있음 (2210 개화)
			return Row.ConditionType == ENUEffectCondition::DuringState;
		
		case ENUSkillEffectType::ElementPowerMod:
			// 그 속성 스킬을 몇 번 쓰면 켜지는 것, ASC 가 누적 횟수를 셈 (2407 과전류)
			// 그 상태인 동안만 켜지는 것은 상태의 남은 턴을 자기 턴 수로 씀 (2210 개화)
			return Row.ConditionType == ENUEffectCondition::AfterElementUse
				|| Row.ConditionType == ENUEffectCondition::DuringState;
		
		default:
			return false;
		}
	}
	
	ENUEffectApplyTo ResolveApplyTo(const FNUActiveEffectRow& Row)
	{
		return SideOfScope(Row.TargetScope) == ESide::Self
			? ENUEffectApplyTo::Self
			: ENUEffectApplyTo::Target;
	}
	
	// GE 를 붙일 타이밍, 새 표에는 이 칸이 없어 효과 종류에서 판정함
	ENUEffectTiming ResolveTiming(const FNUActiveEffectRow& Row)
	{
		return Row.EffectType == ENUSkillEffectType::StatusApply
			? ENUEffectTiming::AfterHit
			: ENUEffectTiming::BeforeHit;
	}
	
	// 상태이상 부여
	// EffectClass를 비워두면 ApplyEffectEntry가 TryApplyStatusEffect로 넘기고
	// 거기서 면역 검사, 확률 판정, 지속 턴, 정의별 GE 선택이 전부 처리됨
	// 대상 조건이 붙은 줄이면 그 조건을 항목에 실어 보냄
	// 적용은 대상을 아는 자리에서 하므로 여기서는 옮겨 담기만 함
	void FillTargetCondition(const FNUActiveEffectRow& Row, FNUAbilityEffectEntry& Entry)
	{
		if (Row.ConditionType != ENUEffectCondition::TargetHasStatus)
		{
			return;
		}
		
		Row.GetConditionStatusIds(Entry.ConditionStatusIds);
		
		// 여기 오는 항목은 조건을 못 채우면 아예 안 붙는 것들이다
		// 피해 배수는 항목이 아니라 피해 줄에 얹히므로 이 길로 오지 않음
		Entry.bSkipWhenConditionFails = true;
	}
	
	void AppendStatusApply(const FNUActiveEffectRow& Row, FNUBattleActionContext& Out)
	{
		FNUAbilityEffectEntry Entry;
		Entry.ApplyTo = ResolveApplyTo(Row);
		Entry.Timing = ResolveTiming(Row);
		Entry.DurationTurns = Row.GetDurationTurns();
		Entry.StatusEffectId = Row.StatusEffectId;
		Entry.StatusEffectChance = Row.Chance;
		
		FillTargetCondition(Row, Entry);
		
		Out.Effects.Add(Entry);
	}
	
	// 스탯 변경 버프, 디버프
	void AppendStatModifier(int32 SkillId, const FNUActiveEffectRow& Row, FNUBattleActionContext& Out)
	{
		FNUAbilityEffectEntry Entry;
		Entry.EffectClass = UNUGE_StatusEffect_StatModifier::StaticClass();
		Entry.ApplyTo = ResolveApplyTo(Row);
		Entry.Timing = ResolveTiming(Row);
		Entry.DurationTurns = Row.GetDurationTurns();
		
		// 적용 전 사용하지 않는 어트리뷰트를 반드시 채워둬야 함
		NUStatusEffect::FillNeutralStatModifiers(Entry.Magnitudes);
		
		// 값은 표의 단위 그대로라 여기서 풀어야 함, 30 을 그냥 넣으면 +3000% 가 됨
		if (!NUStatusEffect::SetStatModifier(Entry.Magnitudes, Row.AffectedStat,
			Row.GetScaledValue(), Row.bIsPercent))
		{
			// 적용할 어트리뷰트가 없다면 적용 X
			UE_LOG(LogTemp, Error, TEXT("스킬 %d 의 스탯 축이 GE에 없습니다 : %s"),
				SkillId, *UEnum::GetValueAsString(Row.AffectedStat));
			
			return;
		}
		
		Out.Effects.Add(Entry);
	}
}

NUActiveSkill::FDamageParams NUActiveSkill::GatherDamageParams(
	const TArray<FNUActiveEffectRow>* Effects, ENUEffectCondition DamageCondition)
{
	FDamageParams Params;
	
	if (!Effects)
	{
		return Params;
	}
	
	for (const FNUActiveEffectRow& Row : *Effects)
	{
		switch (Row.EffectType)
		{
		case ENUSkillEffectType::Damage:
			Params.bHasAnyDamageRow = true;
			
			// 기준이 되는 줄 하나만 읽음, 뒤에 오는 조건부 교체 줄에 덮이면 안 됨
			if (!Params.bHasDamage && Row.ConditionType == DamageCondition
				&& Row.GetScaledValue() > 0.f)
			{
				Params.bHasDamage = true;
				Params.Coefficient = Row.GetScaledValue();
			}
			break;
		
		case ENUSkillEffectType::DefenseIgnore:
			Params.DefenseIgnore = Row.GetScaledValue();
			break;
		
		case ENUSkillEffectType::Lifesteal:
			// 조건이 붙은 흡혈은 그 상태인 동안 얹히는 것이라 이 타격의 것이 아님 (2210 개화)
			// ASC 가 상태와 함께 들고 있고 SumLifestealBonus 가 더함
			if (Row.ConditionType == ENUEffectCondition::None)
			{
				Params.Lifesteal = Row.GetScaledValue();
			}
			break;
		
		default:
			break;
		}
	}
	
	return Params;
}

bool NUActiveSkill::BuildActionContext(int32 SkillId, const UNUBattleDataSubsystem& Data,
	FNUBattleActionContext& Out)
{
	const FNUActiveSkillDefinition* Definition = Data.FindActiveSkillDefinition(SkillId);
	if (!Definition)
	{
		UE_LOG(LogTemp, Error, TEXT("스킬 정의가 없습니다 : %d"), SkillId);
		return false;
	}
	
	const TArray<FNUActiveEffectRow>* Effects = Data.FindSkillEffects(SkillId);
	
	Out = FNUBattleActionContext();
	
	Out.SkillId = SkillId;
	Out.DisplayName = Definition->KoreanName;
	Out.SPCost = Definition->SPCost;
	Out.TargetPolicy = Definition->TargetPolicy;
	Out.HitCount = FMath::Max(1, Definition->HitCount);
	
	// 계수 · 방어력 무시 · 흡혈은 효과표의 줄이 들고 있음, 정의표 칸에서 옮겨온 것
	const FDamageParams DamageParams = GatherDamageParams(Effects);
	
	Out.Motion = ResolveMotion(DamageParams.bHasAnyDamageRow);
	Out.DamageCoefficient = DamageParams.Coefficient;
	Out.DefenseIgnore = DamageParams.DefenseIgnore;
	Out.Lifesteal = DamageParams.Lifesteal;
	Out.DamageType = Definition->DamageType;
	Out.AttackElement = Definition->AttackElement;
	
	// 이 스킬이 표가 시킨 것 중 하나라도 했는지
	// 하나도 못 했으면 턴과 SP 만 버리게 되므로 아예 발동을 막음
	bool bAnyEffect = false;
	
	// 피해를 낼지는 효과표의 Damage 줄이 정함
	bool bAllowDamage = DamageParams.bHasDamage;
	
	// 가하는 피해를 키우는 것은 피해량에 곱해짐
	// 피해 항목을 만들기 전에 먼저 찾아둬야 함
	FNUAbilityEffectEntry Damage;
	Damage.EffectClass = UNUGE_SkillDamage::StaticClass();
	Damage.ApplyTo = ENUEffectApplyTo::Target;
	Damage.Timing = ENUEffectTiming::OnHit;
	
	if (Effects)
	{
		for (const FNUActiveEffectRow& Row : *Effects)
		{
			if (Row.EffectType == ENUSkillEffectType::Damage)
			{
				// 조건이 붙은 피해 줄은 그 조건을 구현하기 전까지 때리지 않음 (개화의 행동불가 이후 공격)
				bAllowDamage = DamageParams.bHasDamage
					&& Row.ConditionType == ENUEffectCondition::None
					&& CanRouteToTargets(Row, Definition->TargetPolicy);
				
				continue;
			}
			
			if (Row.EffectType != ENUSkillEffectType::DamageDealtMod || !IsConditionHandled(Row))
			{
				continue;
			}
			
			// 표가 곱연산이라고 하면 값이 곧 배수, 합연산이면 1에 더함
			Damage.ConditionalDamageMultiplier *= Row.StackType == ENUEffectStackType::Multiplicative
				? Row.GetScaledValue()
				: 1.f + Row.GetScaledValue();
			
			Row.GetConditionStatusIds(Damage.ConditionStatusIds);
		}
	}
	
	if (bAllowDamage)
	{
		Out.Effects.Add(Damage);
		bAnyEffect = true;
	}
	
	if (Effects)
	{
		for (const FNUActiveEffectRow& Row : *Effects)
		{
			// 계수에 딸린 줄은 GatherDamageParams 가 이미 읽었음
			// 조건이 붙은 흡혈은 런타임으로 내려보내야 하므로 여기서 빼지 않음
			if (Row.EffectType == ENUSkillEffectType::DefenseIgnore
				|| (Row.EffectType == ENUSkillEffectType::Lifesteal
					&& Row.ConditionType == ENUEffectCondition::None))
			{
				continue;
			}
			
			// 피해 줄과 피해 배수 줄은 위에서 확인함
			// 예약 피해 줄(OnStateEnd)은 아래로 내려보내야 하므로 여기서 빼지 않음
			if ((Row.EffectType == ENUSkillEffectType::Damage 
					&& Row.ConditionType == ENUEffectCondition::None)
				|| (Row.EffectType == ENUSkillEffectType::DamageDealtMod && IsConditionHandled(Row)))
			{
				continue;
			}
			
			// 조건을 모르면 줄을 통째로 버림
			if (!IsConditionHandled(Row))
			{
				UE_LOG(LogTemp, Warning, TEXT("스킬 %d #%d : 아직 없는 조건이라 건너뜁니다 : %s"),
					SkillId, Row.EffectIndex, *UEnum::GetValueAsString(Row.ConditionType));
				continue;
			}
			
			if (!CanRouteToTargets(Row, Definition->TargetPolicy))
			{
				UE_LOG(LogTemp, Warning, TEXT("스킬 %d #%d : 대상 집합과 다른 편이라 건너뜁니다 : %s"),
					SkillId, Row.EffectIndex, *UEnum::GetValueAsString(Row.TargetScope));
				continue;
			}
			
			// 공격시 부여는 그 자리에서 거는 것이 아니라 몇 턴 동안 들고 있는 것이라 런타임으로 감
			if (Row.EffectType == ENUSkillEffectType::StatusApply
				&& Row.ConditionType == ENUEffectCondition::OnAttack)
			{
				Out.RuntimeEffects.Add(Row);
				bAnyEffect = true;
				continue;
			}
			
			switch (Row.EffectType)
			{
			case ENUSkillEffectType::StatusApply:
				AppendStatusApply(Row, Out);
				bAnyEffect = true;
				break;
			
			case ENUSkillEffectType::StatModifier:
				AppendStatModifier(SkillId, Row, Out);
				bAnyEffect = true;
				break;
			
			case ENUSkillEffectType::Heal:
			case ENUSkillEffectType::StatusCure:
			case ENUSkillEffectType::Shield:
			case ENUSkillEffectType::Reflect:
			case ENUSkillEffectType::Damage:
			case ENUSkillEffectType::ActionBlock:
			case ENUSkillEffectType::ElementDamageTakenMod:
			case ENUSkillEffectType::ElementSkillBlock:
			case ENUSkillEffectType::Lifesteal:
			case ENUSkillEffectType::ResourceGain:
			case ENUSkillEffectType::ElementPowerMod:
				// 전용구현이 필요하므로 RuntimeEffect에 담음
				// 보호막·반사·속성 위력은 GE 가 아니라 ASC 가 들고 있는 축이라 여기로 옴
				Out.RuntimeEffects.Add(Row);
				bAnyEffect = true;
				break;
			
			default:
				// 표에는 있는데 아직 구현이 없는 효과, 그 줄만 빠지고 나머지는 그대로 사용됨
				UE_LOG(LogTemp, Warning, TEXT("스킬 %d #%d : 아직 없는 효과라 건너뜁니다 : %s"),
					SkillId, Row.EffectIndex, *UEnum::GetValueAsString(Row.EffectType));
				break;
			}
		}
	}
	
	// 표가 시킨 것을 하나도 못 했다면 발동시키지 않음
	if (!bAnyEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("전용 구현이 필요한 스킬입니다 : %d"), SkillId);
		return false;
	}
	
	Out.ExecutorClass = UNUActiveSkillAbility::StaticClass();
	Out.bValid = true;
	
	return true;
}
