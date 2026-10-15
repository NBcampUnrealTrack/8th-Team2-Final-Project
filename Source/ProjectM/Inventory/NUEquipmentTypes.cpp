// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUEquipmentTypes.h"

namespace NUEquip
{

ENUEquipSlot ResolveTargetSlot(ENUEquipType Type, const TArray<FNUEquippedItem>& Equipped)
{
	switch (Type)
	{
	case ENUEquipType::Head:    return ENUEquipSlot::Head;
	case ENUEquipType::Chest:   return ENUEquipSlot::Chest;
	case ENUEquipType::Gloves:  return ENUEquipSlot::Gloves;
	case ENUEquipType::Boots:   return ENUEquipSlot::Boots;
	case ENUEquipType::Weapon:  return ENUEquipSlot::Weapon;
	case ENUEquipType::Ring1:   return ENUEquipSlot::Ring1;
	case ENUEquipType::Ring2:   return ENUEquipSlot::Ring2;
	case ENUEquipType::Earring: return ENUEquipSlot::Earring;
	case ENUEquipType::Legs:    return ENUEquipSlot::Legs;
	default:
		return ENUEquipSlot::MAX;
	}
}

int32 ToServerEquipSlot(ENUEquipSlot Slot)
{
	switch (Slot)
	{
	case ENUEquipSlot::Weapon:  return 0;
	case ENUEquipSlot::Head:    return 1; // 서버는 Helmet
	case ENUEquipSlot::Chest:   return 2; // 서버는 Armor
	case ENUEquipSlot::Gloves:  return 3;
	case ENUEquipSlot::Boots:   return 4;
	case ENUEquipSlot::Ring1:   return 5;
	case ENUEquipSlot::Ring2:   return 6;
	case ENUEquipSlot::Earring: return 7;
	case ENUEquipSlot::Legs:    return 8; // 하의는 맨 뒤에 붙였다 (서버와 합의한 번호)
	default:                    return INDEX_NONE;
	}
}

ENUEquipSlot FromServerEquipSlot(int32 ServerSlot)
{
	switch (ServerSlot)
	{
	case 0: return ENUEquipSlot::Weapon;
	case 1: return ENUEquipSlot::Head;
	case 2: return ENUEquipSlot::Chest;
	case 3: return ENUEquipSlot::Gloves;
	case 4: return ENUEquipSlot::Boots;
	case 5: return ENUEquipSlot::Ring1;
	case 6: return ENUEquipSlot::Ring2;
	case 7: return ENUEquipSlot::Earring;
	case 8: return ENUEquipSlot::Legs;
	default: return ENUEquipSlot::MAX;
	}
}

FText GetSlotDisplayName(ENUEquipSlot Slot)
{
	switch (Slot)
	{
	case ENUEquipSlot::Head:    return FText::FromString(TEXT("투구"));
	case ENUEquipSlot::Chest:   return FText::FromString(TEXT("갑옷"));
	case ENUEquipSlot::Gloves:  return FText::FromString(TEXT("장갑"));
	case ENUEquipSlot::Boots:   return FText::FromString(TEXT("신발"));
	case ENUEquipSlot::Weapon:  return FText::FromString(TEXT("무기"));
	case ENUEquipSlot::Ring1:   return FText::FromString(TEXT("반지 1"));
	case ENUEquipSlot::Ring2:   return FText::FromString(TEXT("반지 2"));
	case ENUEquipSlot::Earring: return FText::FromString(TEXT("귀걸이"));
	case ENUEquipSlot::Legs:    return FText::FromString(TEXT("하의"));
	default:                    return FText::GetEmpty();
	}
}

FText GetEquipTypeDisplayName(ENUEquipType Type)
{
	switch (Type)
	{
	case ENUEquipType::Head:    return FText::FromString(TEXT("투구"));
	case ENUEquipType::Chest:   return FText::FromString(TEXT("갑옷"));
	case ENUEquipType::Gloves:  return FText::FromString(TEXT("장갑"));
	case ENUEquipType::Boots:   return FText::FromString(TEXT("신발"));
	case ENUEquipType::Weapon:  return FText::FromString(TEXT("무기"));
	case ENUEquipType::Ring1:   return FText::FromString(TEXT("반지 1"));
	case ENUEquipType::Ring2:   return FText::FromString(TEXT("반지 2"));
	case ENUEquipType::Earring: return FText::FromString(TEXT("귀걸이"));
	case ENUEquipType::Legs:    return FText::FromString(TEXT("하의"));
	default:                    return FText::GetEmpty();
	}
}

FText GetGradeDisplayName(ENUItemGrade Grade)
{
	switch (Grade)
	{
	case ENUItemGrade::Common:    return FText::FromString(TEXT("일반"));
	case ENUItemGrade::Uncommon:  return FText::FromString(TEXT("고급"));
	case ENUItemGrade::Rare:      return FText::FromString(TEXT("희귀"));
	case ENUItemGrade::Heroic:    return FText::FromString(TEXT("영웅"));
	case ENUItemGrade::Legendary: return FText::FromString(TEXT("전설"));
	default:                      return FText::GetEmpty();
	}
}

} // namespace NUEquip
