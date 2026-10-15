#include "CharacterProgression/Effect/NUGE_PlayerSecondaryStatScaling.h"

#include "CharacterProgression/Data/NUStatTuning.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUStatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

namespace
{
	// GAS가 적용할 GE의 규칙
	// 즉시 플레이어의 스탯을 바꾸는 함수는 아님
	FGameplayModifierInfo MakeSecondaryStatModifier(const FGameplayAttribute& TargetAttribute, const FGameplayAttribute& SourceAttribute, float Coefficient)
	{
		// 다른 Attribute 값을 참조해서 Modifier 크기를 계산하기 위한 GAS 구조체
		FAttributeBasedFloat AttributeBased;
		AttributeBased.Coefficient = FScalableFloat(Coefficient);	// 계수 설정
		AttributeBased.BackingAttribute = 
			FGameplayEffectAttributeCaptureDefinition(
			SourceAttribute,										// Primary Stat
			EGameplayEffectAttributeCaptureSource::Target,			// GE가 적용되는 캐릭터의 Attribute
			/*bSnapshot=*/false										// 현재 값을 계속 참조해 Primary Stat 변경 즉시 다시 계산
			);
		AttributeBased.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;

		FGameplayModifierInfo ModInfo;
		ModInfo.Attribute = TargetAttribute;
		ModInfo.ModifierOp = EGameplayModOp::AddBase;
		ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(AttributeBased);
		
		return ModInfo;
	}

	// Primary Stat 하나에 연결된 모든 Secondary Stat 규칙 추가
	void AddModifiersForStat(TArray<FGameplayModifierInfo>& OutModifiers, const FGameplayAttribute& SourceAttribute, const FNUSecondaryStats& PerPoint)
	{
		// 계수가 0인 Modifier를 만들지 않음
		// Speed += Str * 0 같은 Modifier 등록 X
		const auto Add = [&OutModifiers, &SourceAttribute](const FGameplayAttribute& Target, float Coefficient)
		{
			if (Coefficient != 0.f)
			{
				OutModifiers.Add(MakeSecondaryStatModifier(Target, SourceAttribute, Coefficient));
			}
		};

		Add(UNUVitalAttributeSet::GetMaxHealthAttribute(),        PerPoint.MaxHealth);
		Add(UNUVitalAttributeSet::GetMaxSPAttribute(),            PerPoint.MaxSP);
		Add(UNUCombatAttributeSet::GetAttackPowerAttribute(),      PerPoint.AttackPower);
		Add(UNUCombatAttributeSet::GetMagicAttackPowerAttribute(), PerPoint.MagicAttackPower);
		Add(UNUCombatAttributeSet::GetDefenseAttribute(),          PerPoint.Defense);
		Add(UNUCombatAttributeSet::GetSpeedAttribute(),            PerPoint.Speed);
		Add(UNUCombatAttributeSet::GetCriticalRateAttribute(),     PerPoint.CriticalRate);
		Add(UNUCombatAttributeSet::GetCriticalDamageAttribute(),   PerPoint.CriticalDamage);
	}
}

// Secondary Attribute += Primary Attribute * 계수
UNUGE_PlayerSecondaryStatScaling::UNUGE_PlayerSecondaryStatScaling()
{
	// GE가 제거되기 전까지 유지
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 데이터 검증
	const FNUStatTuning& Tuning = FNUStatTuning::Get();
	FString ValidationError;
	if (!Tuning.Validate(ValidationError))
	{
		UE_LOG(LogNUProgression, Error, TEXT("2차 스탯 환산 GE 생성 실패: %s"), *ValidationError);
		return;
	}

	// Primary Stat 관련 환산 규칙을 GE의 Modifier에 추가
	/**
	* Strength     + PerStrength
	* Dexterity    + PerDexterity
	* Intelligence + PerIntelligence
	* Luck         + PerLuck
	* Vitality     + PerVitality
	*/
	AddModifiersForStat(Modifiers, UNUStatAttributeSet::GetStrengthAttribute(),     Tuning.PerStrength);
	AddModifiersForStat(Modifiers, UNUStatAttributeSet::GetDexterityAttribute(),    Tuning.PerDexterity);
	AddModifiersForStat(Modifiers, UNUStatAttributeSet::GetIntelligenceAttribute(), Tuning.PerIntelligence);
	AddModifiersForStat(Modifiers, UNUStatAttributeSet::GetLuckAttribute(),         Tuning.PerLuck);
	AddModifiersForStat(Modifiers, UNUStatAttributeSet::GetVitalityAttribute(),     Tuning.PerVitality);
}
