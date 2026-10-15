// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUGE_EquipmentStats.h"

#include "Inventory/NUInventoryTags.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

namespace
{
	FGameplayModifierInfo MakeEquipModifier(const FGameplayAttribute& Attribute, const FGameplayTag& SetByCallerTag)
	{
		FGameplayModifierInfo ModInfo;
		ModInfo.Attribute = Attribute;
		ModInfo.ModifierOp = EGameplayModOp::AddBase;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = SetByCallerTag;
		ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return ModInfo;
	}
}

UNUGE_EquipmentStats::UNUGE_EquipmentStats()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	Modifiers.Add(MakeEquipModifier(UNUCombatAttributeSet::GetAttackPowerAttribute(), NU::Data::Equip::AttackPower));
	Modifiers.Add(MakeEquipModifier(UNUCombatAttributeSet::GetMagicAttackPowerAttribute(), NU::Data::Equip::MagicAttackPower));
	Modifiers.Add(MakeEquipModifier(UNUCombatAttributeSet::GetDefenseAttribute(), NU::Data::Equip::Defense));
	Modifiers.Add(MakeEquipModifier(UNUCombatAttributeSet::GetSpeedAttribute(), NU::Data::Equip::Speed));
	Modifiers.Add(MakeEquipModifier(UNUCombatAttributeSet::GetCriticalRateAttribute(), NU::Data::Equip::CriticalRate));
	Modifiers.Add(MakeEquipModifier(UNUCombatAttributeSet::GetCriticalDamageAttribute(), NU::Data::Equip::CriticalDamage));
	Modifiers.Add(MakeEquipModifier(UNUVitalAttributeSet::GetMaxHealthAttribute(), NU::Data::Equip::MaxHealth));
	Modifiers.Add(MakeEquipModifier(UNUVitalAttributeSet::GetMaxSPAttribute(), NU::Data::Equip::MaxSP));
}
