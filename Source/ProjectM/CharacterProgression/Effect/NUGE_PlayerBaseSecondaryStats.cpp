#include "CharacterProgression/Effect/NUGE_PlayerBaseSecondaryStats.h"

#include "CharacterProgression/Data/NUProgressionTypes.h"
#include "CharacterProgression/Data/NUStatTuning.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

namespace
{
	// 기본 스탯 값을 AddBase 방식으로 적용할 Modifier 생성
	FGameplayModifierInfo MakeBaseModifier(const FGameplayAttribute& Attribute, float Magnitude)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::AddBase;
		Modifier.ModifierMagnitude = FScalableFloat(Magnitude);
		
		return Modifier;
	}
}

UNUGE_PlayerBaseSecondaryStats::UNUGE_PlayerBaseSecondaryStats()
{
	// 캐릭터에게 한 번 적용한 뒤 제거될 때까지 유지
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 잘못된 테이블 값으로 GE가 만들어지는 것을 막음
	const FNUStatTuning& Tuning = FNUStatTuning::Get();
	FString ValidationError;
	if (!Tuning.Validate(ValidationError))
	{
		UE_LOG(LogNUProgression, Error, TEXT("기본 2차 스탯 GE 생성 실패: %s"), *ValidationError);
		return;
	}

	const FNUSecondaryStats& Base = Tuning.BaseSecondaryStats;
	Modifiers.Reserve(8);
	Modifiers.Add(MakeBaseModifier(UNUVitalAttributeSet::GetMaxHealthAttribute(), Base.MaxHealth));
	Modifiers.Add(MakeBaseModifier(UNUVitalAttributeSet::GetMaxSPAttribute(), Base.MaxSP));
	Modifiers.Add(MakeBaseModifier(UNUCombatAttributeSet::GetAttackPowerAttribute(), Base.AttackPower));
	Modifiers.Add(MakeBaseModifier(UNUCombatAttributeSet::GetMagicAttackPowerAttribute(), Base.MagicAttackPower));
	Modifiers.Add(MakeBaseModifier(UNUCombatAttributeSet::GetDefenseAttribute(), Base.Defense));
	Modifiers.Add(MakeBaseModifier(UNUCombatAttributeSet::GetSpeedAttribute(), Base.Speed));
	Modifiers.Add(MakeBaseModifier(UNUCombatAttributeSet::GetCriticalRateAttribute(), Base.CriticalRate));
	Modifiers.Add(MakeBaseModifier(UNUCombatAttributeSet::GetCriticalDamageAttribute(), Base.CriticalDamage));
}
