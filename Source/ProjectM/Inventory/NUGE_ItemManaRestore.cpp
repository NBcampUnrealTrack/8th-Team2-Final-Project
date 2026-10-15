// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUGE_ItemManaRestore.h"

#include "Inventory/NUInventoryTags.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

UNUGE_ItemManaRestore::UNUGE_ItemManaRestore()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	// 클래스 이름은 ItemTable.csv 4행이 참조하고 있어 유지, 회복 대상만 SP로 바꿈
	ModInfo.Attribute = UNUVitalAttributeSet::GetSPAttribute();
	ModInfo.ModifierOp = EGameplayModOp::AddBase;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = NU::Data::Item::Heal;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModInfo);

	// SP 회복 GameplayCue 사용
	FGameplayEffectCue SPRestoreCue;
	SPRestoreCue.GameplayCueTags.AddTag(GameplayCue::Battle::SPRestore);
	SPRestoreCue.MagnitudeAttribute = UNUVitalAttributeSet::GetSPAttribute();

	GameplayCues.Add(SPRestoreCue);
}
