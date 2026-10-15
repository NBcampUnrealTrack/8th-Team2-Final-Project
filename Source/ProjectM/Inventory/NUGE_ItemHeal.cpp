// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUGE_ItemHeal.h"

#include "Inventory/NUInventoryTags.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

UNUGE_ItemHeal::UNUGE_ItemHeal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UNUVitalAttributeSet::GetIncomingHealingAttribute();
	ModInfo.ModifierOp = EGameplayModOp::AddBase;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = NU::Data::Item::Heal;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModInfo);

	// 회복 GameplayCue 사용, 전투 GE와 동일한 태그를 공유함
	FGameplayEffectCue HealCue;
	HealCue.GameplayCueTags.AddTag(GameplayCue::Battle::Heal);
	HealCue.MagnitudeAttribute = UNUVitalAttributeSet::GetIncomingHealingAttribute();

	GameplayCues.Add(HealCue);
}
