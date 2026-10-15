// Fill out your copyright notice in the Description page of Project Settings.


#include "NUJobSkillContext.h"

#include "NUBattleDataSettings.h"
#include "NUBattleDataSubsystem.h"
#include "NUBattleSkillInfo.h"
#include "NUJobSkillDefinition.h"
#include "Battle/NUBattleActionContext.h"
#include "NUGAS/Ability/NUActiveSkillAbility.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/GameplayEffect/NUSkillGameplayEffect.h"
#include "NUGAS/GameplayEffect/NUStatusGameplayEffect.h"

namespace
{
	// 표만 보고 이 줄을 지금 실행할 수 있는지
	// ImplementBy 가 Data 가 아니면 전용 구현을 기다리는 줄이고, 거르는 조건이 붙었으면 판정할 자리가 아직 없다
	// 턴당 1회 · 첫 행동 같은 사용 제한은 한 턴에 명령이 하나라 늘 지켜지므로 거르는 조건이 아님
	// 효과 항목(GE)으로 못 담고 ASC 가 직접 처리해야 하는 줄인지
	bool IsRuntimeRow(const FNUJobSkillEffectRow& Row)
	{
		if (Row.ImplementBy != ENUEffectImplementBy::Data
			|| Row.HasGatingCondition())
		{
			return false;
		}

		if (Row.EffectType == ENUJobEffectType::Heal)
		{
			return true;
		}

		// 자원을 쌓거나 쓰는 줄
		if (Row.EffectType == ENUJobEffectType::ResourceGain
			|| Row.EffectType == ENUJobEffectType::DebuffStack)
		{
			return true;
		}

		// 받는 피해는 기간이 있는 줄만 여기로 온다
		if (Row.EffectType == ENUJobEffectType::DamageTakenMod)
		{
			return Row.DurationScope == ENUEffectDurationScope::Turns && Row.DurationTurns > 0;
		}

		// 보호막도 기간이 있는 줄만 (3429 생명의 결계 - 아군 전체 최대 HP 18%, 3턴)
		// 상시 줄(3405 수호의 결 - 매 턴 시작 시 부여)은 행동이 아니라 턴 시작에 걸려야 해서 여기로 오면 안 됨
		if (Row.EffectType == ENUJobEffectType::Shield)
		{
			return Row.DurationScope == ENUEffectDurationScope::Turns && Row.DurationTurns > 0;
		}

		return false;
	}

	bool IsRowRunnable(const FNUJobSkillEffectRow& Row)
	{
		if (Row.ImplementBy != ENUEffectImplementBy::Data)
		{
			return false;
		}

		if (Row.HasGatingCondition())
		{
			return false;
		}

		switch (Row.EffectType)
		{
		case ENUJobEffectType::Damage:
		case ENUJobEffectType::DamageDealtMod:
		case ENUJobEffectType::StatModifier:
			return true;

		default:
			return false;
		}
	}

	// 계수의 기준 스탯을 피해 종류 · 기준으로 옮김, 모르는 스탯이면 false
	bool ApplyDamageStat(ENUStatusAffectedStat Stat, FNUBattleActionContext& Out)
	{
		switch (Stat)
		{
		case ENUStatusAffectedStat::AttackPower:
			Out.DamageType = NU::Damage::Type::Physical;
			return true;

		case ENUStatusAffectedStat::MagicAttackPower:
			Out.DamageType = NU::Damage::Type::Magical;
			return true;

		case ENUStatusAffectedStat::MaxHealth:
			Out.DamageBase = NU::Damage::Base::MaxHealth;
			return true;

		default:
			return false;
		}
	}

	// 계수의 기준은 효과표가 행마다 들고 있음, AffectedStat 행
	bool ApplyDamageBase(int32 SkillId, const FNUJobSkillEffectRow& Row, FNUBattleActionContext& Out)
	{
		if (ApplyDamageStat(Row.AffectedStat, Out))
		{
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 피해의 기준 스탯을 모름 : %s"),
			SkillId, Row.EffectIndex, *UEnum::GetValueAsString(Row.AffectedStat));
		return false;
	}

	// 이 행동의 기준 피해 줄 — 조건 없는 첫 피해 줄, 시전자가 뽑은 카드의 피해 줄이 있으면 그것이 대신함
	// (3302 점괘 투사는 카드 줄뿐 · 3325 태양은 대길이면 기본 줄 위에 갈아끼움)
	const FNUJobSkillEffectRow* FindBaseDamageRow(const TArray<FNUJobSkillEffectRow>* Effects, FName CasterCard)
	{
		if (!Effects)
		{
			return nullptr;
		}

		const FNUJobSkillEffectRow* Base = nullptr;

		for (const FNUJobSkillEffectRow& Row : *Effects)
		{
			if (Row.EffectType != ENUJobEffectType::Damage)
			{
				continue;
			}

			if (!CasterCard.IsNone() && NUJobSkill::IsCardDamageRow(Row) && Row.GetConditionName() == CasterCard)
			{
				return &Row;
			}

			// 조건이 붙은 줄은 계수를 갈아끼우는 쪽이라 기준이 아님 (대상 HP 35% 이하면 380%)
			// 턴당 1회 같은 사용 제한은 조건으로 안 침 (3208 각인)
			if (!Base && !Row.HasGatingCondition())
			{
				Base = &Row;
			}
		}

		return Base;
	}

	// 피해가 없으면 중앙으로 가지 않음 (버프, 디버프, 회복)
	ENUBattleActionMotion ResolveMotion(const TArray<FNUJobSkillEffectRow>* Effects)
	{
		return NUJobSkill::HasDamageRow(Effects)
			? ENUBattleActionMotion::MoveToCenter
			: ENUBattleActionMotion::InPlace;
	}

	void AppendStatModifier(int32 SkillId, const FNUJobSkillEffectRow& Row, FNUBattleActionContext& Out)
	{
		FNUAbilityEffectEntry Entry;
		Entry.EffectClass = UNUGE_StatusEffect_StatModifier::StaticClass();
		// 적 대상 줄(3111 견제 화살 속도 -6 · 3411 포효 공격력 -8%)은 대상에게 걸고, 다시 걸면 겹치지 않고 지속만 새로 잡음
		const bool bOnEnemy = Row.TargetScope == ENUJobTargetScope::Enemy;
		Entry.ApplyTo = bOnEnemy ? ENUEffectApplyTo::Target : ENUEffectApplyTo::Self;
		if (bOnEnemy)
		{
			Entry.ReplaceStat = Row.AffectedStat;
		}
		Entry.Timing = ENUEffectTiming::BeforeHit;
		// 기간 칸이 턴이면 그 턴 수, 아니면 "이번 턴" 이라 1 (3011 질주 · 3337 펜타클 — Instant)
		// 0 을 넘기면 기한 없는 GE 로 걸려 어디서도 안 지워지고, 쓸 때마다 겹쳐 전투 뒤까지 남음
		// 거는 턴이 끝날 때 한 번 깎이므로 1 이면 이번 턴만
		Entry.DurationTurns = Row.DurationScope == ENUEffectDurationScope::Turns
			? FMath::Max(1, Row.DurationTurns)
			: 1;

		// 적용 전 사용하지 않는 어트리뷰트를 반드시 채워둬야 함
		NUStatusEffect::FillNeutralStatModifiers(Entry.Magnitudes);

		if (!NUStatusEffect::SetStatModifier(Entry.Magnitudes, Row.AffectedStat,
			Row.GetScaledValue(), Row.bIsPercent))
		{
			UE_LOG(LogTemp, Error, TEXT("직업 스킬 %d 의 스탯 축이 GE에 없습니다 : %s"),
				SkillId, *UEnum::GetValueAsString(Row.AffectedStat));

			return;
		}

		Out.Effects.Add(Entry);
	}
}

float NUJobSkill::FindDamageCoefficient(const TArray<FNUJobSkillEffectRow>* Effects, FName CasterCard)
{
	const FNUJobSkillEffectRow* Row = FindBaseDamageRow(Effects, CasterCard);

	return Row ? Row->GetScaledValue() : 0.f;
}

bool NUJobSkill::IsCardDamageRow(const FNUJobSkillEffectRow& Row)
{
	return Row.EffectType == ENUJobEffectType::Damage
		&& Row.ConditionType == ENUEffectCondition::OnCardDrawn
		&& Row.ImplementBy == ENUEffectImplementBy::Data;
}

const FNUSpendAllSPSkill* NUJobSkill::FindSpendAllSPSkill(int32 SkillId)
{
	if (SkillId <= 0)
	{
		return nullptr;
	}

	return GetDefault<UNUBattleDataSettings>()->SpendAllSPSkills.FindByPredicate(
		[SkillId](const FNUSpendAllSPSkill& Skill) { return Skill.SkillId == SkillId; });
}

float NUJobSkill::ComputeSpendAllSPCoefficient(const FNUSpendAllSPSkill& Skill, int32 SpentSP)
{
	if (SpentSP <= 0)
	{
		return 0.f;
	}

	// 평시 계수표는 백분율이라 1.0 = 1배로 바꿔 돌려줌
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	const float Percent = Settings->SkillCoefficientCurveBase
		* FMath::Pow(static_cast<float>(SpentSP), Settings->SkillCoefficientCurveExponent);

	return Percent * Skill.CoefficientScale / 100.f;
}

bool NUJobSkill::HasDamageRow(const TArray<FNUJobSkillEffectRow>* Effects)
{
	if (!Effects)
	{
		return false;
	}

	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		if (Row.EffectType == ENUJobEffectType::Damage)
		{
			return true;
		}
	}

	return false;
}

int32 NUJobSkill::FindMaxUsesPerBattle(const TArray<FNUJobSkillEffectRow>* Effects)
{
	if (!Effects)
	{
		return 0;
	}

	int32 MaxUses = 0;

	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		MaxUses = FMath::Max(MaxUses, Row.MaxPerBattle);
	}

	return MaxUses;
}

bool NUJobSkill::CanRunFromTable(const FNUJobSkillDefinition& Definition,
	const TArray<FNUJobSkillEffectRow>* Effects)
{
	static const FName UltimateType(TEXT("필살기"));

	// 필살기는 AG 로 여는데 AG는 아직 구현하지 않았으므로 닫아둠
	if (Definition.SkillType == UltimateType)
	{
		return false;
	}

	// 표에 칸이 없어 설정이 정하는 동작 (3203 나이아 개화 — 남은 SP 전부)
	if (FindSpendAllSPSkill(Definition.Skill_ID))
	{
		return true;
	}

	if (!Effects)
	{
		return false;
	}


	// 표가 시킨 것을 하나도 못 하면 턴과 SP 만 버리게 되므로 메뉴에도 올리지 않음
	// 카드 피해 줄은 그 턴 카드에 맞는 하나가 돎 (3302 점괘 투사)
	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		if (IsRowRunnable(Row) || IsRuntimeRow(Row) || IsCardDamageRow(Row))
		{
			return true;
		}
	}

	return false;
}

bool NUJobSkill::HasAllySideRecipient(const FNUJobSkillDefinition& Definition,
	const TArray<FNUJobSkillEffectRow>* Effects, bool bOtherCharacterAlive, bool bNaiaAlive)
{
	// 적을 때리는 스킬은 아군 줄이 덤이라 받을 사람이 없어도 쓸모가 있음
	if (!Effects
		|| (Definition.TargetPolicy != ENUTargetPolicy::SingleAlly
			&& Definition.TargetPolicy != ENUTargetPolicy::AllAllies))
	{
		return true;
	}

	bool bHasAllySideRow = false;

	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		// 실제로 도는 줄만 셈, 전용 구현을 기다리는 줄은 아무에게도 안 닿음
		if (!IsRowRunnable(Row) && !IsRuntimeRow(Row))
		{
			continue;
		}

		switch (Row.TargetScope)
		{
		case ENUJobTargetScope::Ally:
			bHasAllySideRow = true;

			if (bOtherCharacterAlive)
			{
				return true;
			}
			break;

		case ENUJobTargetScope::AllyAndNaia:
			bHasAllySideRow = true;

			if (bOtherCharacterAlive || bNaiaAlive)
			{
				return true;
			}
			break;

		case ENUJobTargetScope::Party:
			// 자신이 받으므로 늘 받을 사람이 있음
			return true;

		default:
			break;
		}
	}

	// 아군 쪽 줄이 없는 아군 대상 스킬은 이 검사와 무관
	return !bHasAllySideRow;
}

bool NUJobSkill::BuildActionContext(int32 SkillId, const UNUBattleDataSubsystem& Data,
	FNUBattleActionContext& Out, FName CasterCard)
{
	const FNUJobSkillDefinition* Definition = Data.FindJobSkillDefinition(SkillId);
	if (!Definition)
	{
		UE_LOG(LogTemp, Error, TEXT("직업 스킬 정의가 없습니다 : %d"), SkillId);
		return false;
	}

	const TArray<FNUJobSkillEffectRow>* Effects = Data.FindJobSkillEffects(SkillId);
	if (!CanRunFromTable(*Definition, Effects))
	{
		UE_LOG(LogTemp, Warning, TEXT("아직 표만으로는 못 도는 직업 스킬입니다 : %d(%s)"),
			SkillId, *Definition->Name_KR.ToString());
		return false;
	}

	// 이름 · SP · 대상 범위는 명령창이 읽는 것과 같은 자리에서 가져옴
	FNUBattleSkillInfo Info;
	if (!Data.TryGetBattleSkillInfo(SkillId, Info))
	{
		return false;
	}

	Out = FNUBattleActionContext();

	Out.SkillId = SkillId;
	Out.DisplayName = Info.DisplayName;
	Out.SPCost = Info.SPCost;
	Out.Motion = ResolveMotion(Effects);
	Out.HitCount = FMath::Max(1, Definition->HitCount);

	// 계수는 효과표의 Damage 줄이 들고 있음, 카드로 갈리는 스킬은 그 턴 카드의 줄
	const FNUJobSkillEffectRow* BaseDamageRow = FindBaseDamageRow(Effects, CasterCard);
	Out.DamageCoefficient = BaseDamageRow ? BaseDamageRow->GetScaledValue() : 0.f;

	// 자기 체력 소모
	// 효과표의 SelfCost 행이 같은 값을 백분율로 들고 있지만(0.1 ↔ 10) 두 곳을 더하면 두 배가 되므로 한칸만 읽음
	Out.SelfHPCostRatio = Definition->HP_Cost;

	// 표가 대상 범위를 안 보내면 어디로 나갈지 모르므로 막음, 지어내면 단일 스킬이 전체 공격이 될 수 있음
	if (!Info.TargetPolicy.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d 의 대상 범위를 몰라 발동하지 않습니다"), SkillId);
		return false;
	}
	Out.TargetPolicy = Info.TargetPolicy.GetValue();

	// 가하는 피해를 키우는 것은 피해량에 곱해짐
	// 피해 항목을 만들기 전에 먼저 찾아둬야 함
	FNUAbilityEffectEntry Damage;
	Damage.EffectClass = UNUGE_SkillDamage::StaticClass();
	Damage.ApplyTo = ENUEffectApplyTo::Target;
	Damage.Timing = ENUEffectTiming::OnHit;

	bool bAnyEffect = false;
	bool bAllowDamage = false;

	// 남은 SP 를 전부 쓰는 스킬 (3203 나이아 개화) — 표에 피해 줄이 없어 기준 스탯은 설정에서 읽고,
	// 계수는 쓴 SP 로 정해지므로 인카운터가 비용과 같이 채움
	if (const FNUSpendAllSPSkill* SpendAll = FindSpendAllSPSkill(SkillId))
	{
		Out.bSpendAllSP = true;
		Out.Motion = ENUBattleActionMotion::MoveToCenter;

		if (ApplyDamageStat(SpendAll->DamageStat, Out))
		{
			bAllowDamage = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d : 설정의 피해 기준 스탯을 모름 : %s"),
				SkillId, *UEnum::GetValueAsString(SpendAll->DamageStat));
		}
	}

	for (const FNUJobSkillEffectRow& Row : *Effects)
	{
		// 대가는 정의표의 HP_Cost 로 이미 셌다, 여기서 또 세면 두 배가 됨
		if (Row.EffectType == ENUJobEffectType::SelfCost)
		{
			continue;
		}

		// 계수 가산은 자원을 봐야 해서 여기선 못 셈, 발동할 때 ASC가 계수에 더해야 함
		if (Row.EffectType == ENUJobEffectType::CoefficientAdd)
		{
			continue;
		}

		// 대상 체력 조건으로 계수를 갈아끼우는 줄 (5직업 처형기 - 35% 이하면 380%), 기본 계수 줄과 배타
		// 대상 중첩 조건도 같은 모양 (3216 연쇄 폭발 - 약점 5중첩이면 440%)
		// 대상마다 갈리므로 피해 항목에 실어 두고 때리는 순간 대상 체력 · 중첩으로 고름
		// 기준 스탯이 기본 줄과 다르면 계수만 바꿔서는 맞지 않으므로 뺌
		const bool bTargetHPSwap = Row.ConditionType == ENUEffectCondition::TargetHPBelow;
		const bool bTargetStackSwap = Row.ConditionType == ENUEffectCondition::StackAtCount;
		if (Row.EffectType == ENUJobEffectType::Damage
			&& (bTargetHPSwap || bTargetStackSwap)
			&& Row.ImplementBy == ENUEffectImplementBy::Data)
		{
			const FNUJobSkillEffectRow* BaseRow = Effects->FindByPredicate([](const FNUJobSkillEffectRow& Candidate)
			{
				return Candidate.EffectType == ENUJobEffectType::Damage
					&& !Candidate.HasGatingCondition();
			});

			// 중첩은 대상에게 붙는 자원이어야 대상 쪽 칸에서 셀 수 있음 (약점 · 마크 · 문양)
			const FNUJobResourceDefinition* StackResource = bTargetStackSwap
				? Data.FindJobResource(Row.GetConditionResourceName())
				: nullptr;

			if (!BaseRow || BaseRow->AffectedStat != Row.AffectedStat)
			{
				UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 대상 조건 계수의 기준 스탯이 기본 줄과 달라 뺍니다"),
					SkillId, Row.EffectIndex);
			}
			else if (Damage.ConditionalCoefficient > 0.f)
			{
				// 교체 계수 칸이 하나라 조건 둘을 함께 실을 수 없음, 지금 표에 그런 스킬은 없음
				UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 교체 계수 조건이 둘이라 뒤의 것을 뺍니다"),
					SkillId, Row.EffectIndex);
			}
			else if (bTargetHPSwap)
			{
				Damage.ConditionTargetHPBelow = Row.GetConditionNumber();
				Damage.ConditionalCoefficient = Row.GetScaledValue();
			}
			else if (StackResource && StackResource->OwnerScope == ENUJobResourceOwner::Target)
			{
				Damage.ConditionTargetStack = Row.GetConditionResourceName();
				Damage.ConditionTargetStackCount = FMath::Max(1, FMath::RoundToInt(Row.GetConditionNumber()));
				Damage.ConditionalCoefficient = Row.GetScaledValue();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 중첩 조건의 자원 '%s' 이 대상에게 붙는 것이 아니라 뺍니다"),
					SkillId, Row.EffectIndex, *Row.GetConditionResourceName().ToString());
			}
			continue;
		}

		// 반드시 치명타 (3121 나이아의 유도) - 전용 구현 줄이라 아래 IsRowRunnable 이 거름
		// 이 스킬의 피해 항목에 실어 실행기가 치명타를 굴리지 않게 함, 조건이 붙으면 판정할 자리가 없어 뺌
		if (Row.EffectType == ENUJobEffectType::ForceCritical)
		{
			if (!Row.HasGatingCondition())
			{
				Damage.bForceCritical = true;
			}
			continue;
		}

		// 방어력 무시 (3025 · 3110 · 3210 · 3410 — 50%), 전용 구현 줄이라 아래 IsRowRunnable 이 거름
		// 행동 전체에 걸리는 값이라 컨텍스트에 실어 실행기가 피해에 넘기게 함, 조건이 붙으면 판정할 자리가 없어 뺌
		if (Row.EffectType == ENUJobEffectType::DefenseIgnore)
		{
			if (!Row.HasGatingCondition() && Row.ImplementBy != ENUEffectImplementBy::Pending)
			{
				Out.DefenseIgnore = FMath::Max(Out.DefenseIgnore, FMath::Clamp(Row.GetScaledValue(), 0.f, 1.f));

				UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 방어력 %.0f%% 무시"), SkillId, Out.DefenseIgnore * 100.f);
			}
			continue;
		}

		// 자원의 소멸 대기를 처음부터 다시 셈 (3137 약점 관측 — 맞은 대상의 마크 3턴), 전용 구현 줄이라 여기서 받음
		// 자원이 대상에게 있어 대상마다 보내야 함, 조건이 붙으면 판정할 자리가 없어 뺌
		if (Row.EffectType == ENUJobEffectType::ResourceTimerReset)
		{
			if (!Row.HasGatingCondition() && Row.ImplementBy != ENUEffectImplementBy::Pending)
			{
				Out.JobRuntimeEffects.Add(Row);
			}
			continue;
		}

		// 항목으로 못 담는 줄은 실행기가 ASC에 직접 적용함
		if (IsRuntimeRow(Row))
		{
			FNUJobSkillEffectRow& Added = Out.JobRuntimeEffects.Add_GetRef(Row);

			// 자원표가 대상에게 붙는다고 한 자원은 표 대상 칸이 Self 여도 적에게 감
			// (3208 각인 · 3240 지맥 개방 — 문양 +2, 설명도 "[각인] 계열로 적진에 새긴다")
			if (Row.TargetScope == ENUJobTargetScope::Self || Row.TargetScope == ENUJobTargetScope::None)
			{
				const FNUJobResourceDefinition* Resource = Data.FindJobResource(Row.ResourceName);
				if (Resource && Resource->OwnerScope == ENUJobResourceOwner::Target)
				{
					Added.TargetScope = ENUJobTargetScope::Enemy;
				}
			}

			bAnyEffect = true;
			continue;
		}

		// 카드로 갈리는 피해 줄은 이번 카드의 줄만 피해의 기준 스탯을 정함, 다른 카드 줄은 이 턴에 안 돎
		if (IsCardDamageRow(Row))
		{
			if (&Row == BaseDamageRow && !bAllowDamage && Out.DamageCoefficient > 0.f
				&& ApplyDamageBase(SkillId, Row, Out))
			{
				bAllowDamage = true;
			}
			continue;
		}

		if (!IsRowRunnable(Row))
		{
			UE_LOG(LogTemp, Verbose, TEXT("직업 스킬 %d #%d : 아직 안 도는 줄 (%s / %s / %s)"),
				SkillId, Row.EffectIndex,
				*UEnum::GetValueAsString(Row.EffectType),
				*UEnum::GetValueAsString(Row.ConditionType),
				*UEnum::GetValueAsString(Row.ImplementBy));
			continue;
		}

		switch (Row.EffectType)
		{
		case ENUJobEffectType::Damage:
			// 기준 스탯은 이 줄이 들고 있음, 못 읽으면 때리지 않음
			// 어느 줄이 기준인지는 FindBaseDamageRow 가 고름, 카드 줄이 대신하면 이 줄은 기준이 아님
			if (!bAllowDamage && Out.DamageCoefficient > 0.f
				&& &Row == BaseDamageRow
				&& ApplyDamageBase(SkillId, Row, Out))
			{
				bAllowDamage = true;
			}
			break;

		case ENUJobEffectType::DamageDealtMod:
			// 무엇 1당이 붙은 줄은 자원을 봐야 해서 여기선 못 셈, 발동할 때 ASC 가 셈 (3133 템포 1당 +5.8%)
			if (!Row.ValuePer.IsNone())
			{
				break;
			}

			// 곱연산은 값이 곧 이 스킬의 피해 배수, 합연산은 합연산 풀로 감 — 표의 StackType 이 가름
			// 합연산을 1에 더해 곱하면 풀의 다른 항과 합쳐지지 않고 따로 곱해져 값이 커짐
			if (Row.StackType == ENUEffectStackType::Multiplicative)
			{
				Damage.ConditionalDamageMultiplier *= Row.GetScaledValue();
			}
			else
			{
				Out.AdditiveDamageBonus += Row.GetScaledValue();
			}
			break;

		case ENUJobEffectType::StatModifier:
			AppendStatModifier(SkillId, Row, Out);
			bAnyEffect = true;
			break;

		default:
			break;
		}
	}

	if (bAllowDamage)
	{
		Out.Effects.Add(Damage);
		bAnyEffect = true;
	}

	if (!bAnyEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("전용 구현이 필요한 직업 스킬입니다 : %d"), SkillId);
		return false;
	}

	Out.ExecutorClass = UNUActiveSkillAbility::StaticClass();
	Out.bValid = true;

	return true;
}
