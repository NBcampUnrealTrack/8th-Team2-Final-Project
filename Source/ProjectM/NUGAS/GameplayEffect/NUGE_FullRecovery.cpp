#include "NUGAS/GameplayEffect/NUGE_FullRecovery.h"

#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

UNUGE_FullRecovery::UNUGE_FullRecovery()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 플레이어와 나이아의 최대 HP가 다르므로 대상 ASC에서 계산한 값을 받는다.
	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = UNUVitalAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Override;
	FSetByCallerFloat HealthMagnitude;
	HealthMagnitude.DataTag = NU::Data::Recovery::Health;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealthMagnitude);
	Modifiers.Add(HealthModifier);

}
