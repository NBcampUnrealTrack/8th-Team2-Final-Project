// Fill out your copyright notice in the Description page of Project Settings.


#include "NUStatusGameplayEffect.h"

#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"


namespace
{
	FGameplayModifierInfo MakeSetByCallerModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op,
		const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCallerFloat;
		SetByCallerFloat.DataTag = DataTag;
		
		FGameplayModifierInfo ModInfo;
		ModInfo.Attribute = Attribute;
		ModInfo.ModifierOp = Op;
		ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerFloat);
		
		return ModInfo;
	}
}

const TMap<ENUStatusAffectedStat, NUStatusEffect::FNUStatModifierAxis>& NUStatusEffect::GetStatModifierAxes()
{
	static const TMap<ENUStatusAffectedStat, FNUStatModifierAxis> Axes = []
	{
		TMap<ENUStatusAffectedStat, FNUStatModifierAxis> Result;
		
		Result.Add(ENUStatusAffectedStat::Speed, FNUStatModifierAxis{
			UNUCombatAttributeSet::GetSpeedAttribute(),
			NU::Data::StatusEffect::Multiplier::Speed,
			NU::Data::StatusEffect::Additive::Speed });
		
		Result.Add(ENUStatusAffectedStat::Defense, FNUStatModifierAxis{
			UNUCombatAttributeSet::GetDefenseAttribute(),
			NU::Data::StatusEffect::Multiplier::Defense,
			NU::Data::StatusEffect::Additive::Defense });
		
		// 치명타 확률은 합으로 처리해야 함
		Result.Add(ENUStatusAffectedStat::CriticalRate, FNUStatModifierAxis{
			UNUCombatAttributeSet::GetCriticalRateAttribute(),
			NU::Data::StatusEffect::Multiplier::CriticalRate,
			NU::Data::StatusEffect::Additive::CriticalRate });

		// 치명타 피해도 합으로 처리해야 함
		Result.Add(ENUStatusAffectedStat::CriticalDamage, FNUStatModifierAxis{
			UNUCombatAttributeSet::GetCriticalDamageAttribute(),
			NU::Data::StatusEffect::Multiplier::CriticalDamage,
			NU::Data::StatusEffect::Additive::CriticalDamage });
		
		Result.Add(ENUStatusAffectedStat::AttackPower, FNUStatModifierAxis{
			UNUCombatAttributeSet::GetAttackPowerAttribute(),
			NU::Data::StatusEffect::Multiplier::AttackPower,
			NU::Data::StatusEffect::Additive::AttackPower });
		
		Result.Add(ENUStatusAffectedStat::MagicAttackPower, FNUStatModifierAxis{
			UNUCombatAttributeSet::GetMagicAttackPowerAttribute(),
			NU::Data::StatusEffect::Multiplier::MagicAttackPower,
			NU::Data::StatusEffect::Additive::MagicAttackPower });
		
		
		// 최대치 두 축은 Vital 쪽이라 어트리뷰트 셋이 다름
		// 최대 체력을 올리면 현재 체력은 따라 오르지 않으므로 채우는 쪽에서 만피 처리를 해야 함
		Result.Add(ENUStatusAffectedStat::MaxHealth, FNUStatModifierAxis{
			UNUVitalAttributeSet::GetMaxHealthAttribute(),
			NU::Data::StatusEffect::Multiplier::MaxHealth,
			NU::Data::StatusEffect::Additive::MaxHealth });
		
		Result.Add(ENUStatusAffectedStat::MaxSP, FNUStatModifierAxis{
			UNUVitalAttributeSet::GetMaxSPAttribute(),
			NU::Data::StatusEffect::Multiplier::MaxSP,
			NU::Data::StatusEffect::Additive::MaxSP });
		
		return Result;
	}();
	
	return Axes;
}

void NUStatusEffect::FillNeutralStatModifiers(TMap<FGameplayTag, float>& OutMagnitudes)
{
	for (const TPair<ENUStatusAffectedStat, FNUStatModifierAxis>& Pair : GetStatModifierAxes())
	{
		OutMagnitudes.Add(Pair.Value.MultiplierTag, 1.f);
		OutMagnitudes.Add(Pair.Value.AdditiveTag, 0.f);
	}
}

bool NUStatusEffect::IsRatioStat(ENUStatusAffectedStat Stat)
{
	// 어트리뷰트 자체가 비율인 축, 백분율로 적힌 값이 곱이 아니라 합으로 들어가야 함
	return Stat == ENUStatusAffectedStat::CriticalRate
		|| Stat == ENUStatusAffectedStat::CriticalDamage;
}

bool NUStatusEffect::SetStatModifier(TMap<FGameplayTag, float>& OutMagnitudes,
	ENUStatusAffectedStat Stat, float Value, bool bIsPercent)
{
	const FNUStatModifierAxis* Axis = GetStatModifierAxes().Find(Stat);
	if (!Axis)
	{
		return false;
	}
	
	if (bIsPercent)
	{
		// 배율 변경 (곱)
		// 0.3 -> 1.3배,  -0.2 -> 0.8배
		OutMagnitudes.Add(Axis->MultiplierTag, 1.f + Value);
	}
	else
	{
		// 수치 변경 (합)
		// 어트리뷰트가 전부 0~1이라 그대로 더함
		OutMagnitudes.Add(Axis->AdditiveTag, Value);
	}
	
	return true;
}

UNUGE_StatusEffect_Marker::UNUGE_StatusEffect_Marker()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UNUGE_StatusEffect_StatModifier::UNUGE_StatusEffect_StatModifier()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	
	// 모디파이어의 연산은 CDO라 런타임에 Attribute를 선택할 수 없음
	// 그래서 사용할 스탯을 모두 켜두고 필요한 Attribute만 사용해야 함
	for (const TPair<ENUStatusAffectedStat, NUStatusEffect::FNUStatModifierAxis>& Pair
		: NUStatusEffect::GetStatModifierAxes())
	{
		Modifiers.Add(MakeSetByCallerModifier(
			Pair.Value.Attribute, EGameplayModOp::Multiplicitive, Pair.Value.MultiplierTag));
		
		Modifiers.Add(MakeSetByCallerModifier(
			Pair.Value.Attribute, EGameplayModOp::Additive, Pair.Value.AdditiveTag));
	}
}

UNUGE_PassiveStatModifier::UNUGE_PassiveStatModifier()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	
	// 위 상태이상용과 같은 이유로 축을 전부 켜두고 필요한 것만 SetByCaller 로 채움
	for (const TPair<ENUStatusAffectedStat, NUStatusEffect::FNUStatModifierAxis>& Pair
		: NUStatusEffect::GetStatModifierAxes())
	{
		Modifiers.Add(MakeSetByCallerModifier(
			Pair.Value.Attribute, EGameplayModOp::Multiplicitive, Pair.Value.MultiplierTag));
		
		Modifiers.Add(MakeSetByCallerModifier(
			Pair.Value.Attribute, EGameplayModOp::Additive, Pair.Value.AdditiveTag));
	}
}

UNUGE_StatusEffect_TickDamage::UNUGE_StatusEffect_TickDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	
	Modifiers.Add(MakeSetByCallerModifier(
		UNUVitalAttributeSet::GetIncomingDamageAttribute(),
		EGameplayModOp::Additive,
		NU::Data::StatusEffect::TickDamage)
		);
}
