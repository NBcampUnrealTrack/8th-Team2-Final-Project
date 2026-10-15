// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUInventoryTags.h"

namespace NU
{
	namespace Item
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Commune, "NU.Item.Commune", "교감(포획) 아이템 분류 — 어빌리티에 종류 전달")
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(CommuneStone, "NU.Item.CommuneStone", "교감(포획) 아이템 — 공명석")

		namespace Potion
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(SmallHeal, "NU.Item.Potion.SmallHeal", "소모품 — 작은 회복 물약")
		}
	}

	namespace Data
	{
		namespace Item
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Heal, "NU.Data.Item.Heal", "아이템 사용 GE의 회복량 SetByCaller 키")
		}

		namespace Equip
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackPower, "NU.Data.Equip.AttackPower", "장비 GE SetByCaller — 공격력")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(MagicAttackPower, "NU.Data.Equip.MagicAttackPower", "장비 GE SetByCaller — 마법 공격력")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Defense, "NU.Data.Equip.Defense", "장비 GE SetByCaller — 방어력")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Speed, "NU.Data.Equip.Speed", "장비 GE SetByCaller — 속도")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalRate, "NU.Data.Equip.CriticalRate", "장비 GE SetByCaller — 치명타 확률(%)")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalDamage, "NU.Data.Equip.CriticalDamage", "장비 GE SetByCaller — 치명타 피해(%)")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxHealth, "NU.Data.Equip.MaxHealth", "장비 GE SetByCaller — 최대 HP")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxSP, "NU.Data.Equip.MaxSP", "장비 GE SetByCaller — 최대 SP")
		}
	}

	namespace Event
	{
		namespace Item
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Used, "NU.Event.Item.Used", "아이템 사용 성공 이벤트")
		}
	}
}
