// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// 인벤토리 도메인 태그.
// NUGameplayTags(전투 담당 소유)를 수정하지 않기 위해 별도 파일에서 NU 네임스페이스를 확장한다.
// 추후 중앙 태그 파일로 이관 협의 가능.
namespace NU
{
	// 아이템 식별 태그
	namespace Item
	{
		// 교감(포획) 아이템 분류, 개별 아이템 식별이 아니라 어빌리티에 종류를 알리는 용도
		// 인카운터가 ItemType == Commune 일 때 Payload로 보냄
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Commune);
		
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(CommuneStone);

		namespace Potion
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(SmallHeal);
		}
	}

	namespace Data
	{
		namespace Item
		{
			// 아이템 사용 GE에 수치(회복량 등)를 주입하는 SetByCaller 키
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal);
		}

		// 장비 스탯 GE(UNUGE_EquipmentStats)의 SetByCaller 키 — FNUEquipStats 8필드와 1:1
		namespace Equip
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackPower);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(MagicAttackPower);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defense);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Speed);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalRate);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalDamage);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxHealth);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxSP);
		}
	}

	namespace Event
	{
		namespace Item
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Used);
		}
	}
}
