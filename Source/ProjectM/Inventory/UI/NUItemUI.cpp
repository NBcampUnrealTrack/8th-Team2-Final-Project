#include "Inventory/UI/NUItemUI.h"

#include "AssetRegistry/AssetData.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUEquipmentTypes.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

UTexture2D* NUItemUI::LoadItemIcon(FName ItemRowName, bool bFamilyFallback)
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (ItemRowName.IsNone() || Settings == nullptr || Settings->ItemIconFolder.IsEmpty())
	{
		return nullptr;
	}

	// 없는 아이콘을 로드하려 들면 그릴 때마다 디스크를 뒤진다 --- 애셋 레지스트리(메모리)에 있는 것만 로드한다.
	// 이미 올라온 텍스처는 TryLoad가 바로 찾아 준다
	const auto LoadExisting = [Settings](const FString& RowName) -> UTexture2D*
	{
		const FString AssetName = TEXT("T_Item_") + RowName;
		const FSoftObjectPath IconPath(FString::Printf(TEXT("%s/%s.%s"), *Settings->ItemIconFolder, *AssetName, *AssetName));
		return IAssetRegistry::GetChecked().GetAssetByObjectPath(IconPath).IsValid() ? Cast<UTexture2D>(IconPath.TryLoad()) : nullptr;
	};

	const FString RowName = ItemRowName.ToString();
	if (UTexture2D* Icon = LoadExisting(RowName))
	{
		return Icon;
	}

	// 장비(EQ_<계열>_<등급 두 자리>)는 제 그림이 없으면 같은 계열의 다른 등급 그림을 쓴다 --- 가까운 등급부터, 같은 거리면 아래 등급 먼저.
	// 계열에 그림이 하나도 없으면 nullptr (칸에 이름이 나온다)
	int32 Underscore = INDEX_NONE;
	if (bFamilyFallback && RowName.StartsWith(TEXT("EQ_")) && RowName.FindLastChar(TEXT('_'), Underscore) && RowName.Len() - Underscore == 3
		&& RowName.Mid(Underscore + 1).IsNumeric())
	{
		const FString Family = RowName.Left(Underscore + 1);
		const int32 Tier = FCString::Atoi(*RowName.Mid(Underscore + 1));
		const int32 MaxTier = static_cast<int32>(ENUItemGrade::Legendary);
		for (int32 Step = 1; Step < MaxTier; ++Step)
		{
			for (const int32 Other : { Tier - Step, Tier + Step })
			{
				UTexture2D* Icon = Other >= 1 && Other <= MaxTier ? LoadExisting(FString::Printf(TEXT("%s%02d"), *Family, Other)) : nullptr;
				if (Icon != nullptr)
				{
					return Icon;
				}
			}
		}
	}
	return nullptr;
}

TArray<FNUEquipStatEntry> NUItemUI::GetEquipStatEntries(const FNUEquipStats& Stats)
{
	return {
		{ NSLOCTEXT("NUInventory", "StatAttack", "공격력"), Stats.AttackPower, true },
		{ NSLOCTEXT("NUInventory", "StatMagicAttack", "마법 공격력"), Stats.MagicAttackPower, false },
		{ NSLOCTEXT("NUInventory", "StatDefense", "방어력"), Stats.Defense, false },
		{ NSLOCTEXT("NUInventory", "StatSpeed", "속도"), Stats.Speed, false },
		{ NSLOCTEXT("NUInventory", "StatCriticalRate", "치명타 확률"), Stats.CriticalRate, true },
		{ NSLOCTEXT("NUInventory", "StatCriticalDamage", "치명타 피해"), Stats.CriticalDamage, true },
		{ NSLOCTEXT("NUInventory", "StatMaxHealth", "최대 체력"), Stats.MaxHealth, true },
		// 장비 테이블 컬럼이 아직 MaxMana라 그대로 읽어 SP로 표기함
		{ NSLOCTEXT("NUInventory", "StatMaxSP", "최대 SP"), Stats.MaxMana, false },
		// 아래 둘은 어트리뷰트가 없어 아직 실제 적용은 안 된다 --- 시트값 표시만 (양수=좋음이라 부호 처리 공용)
		{ NSLOCTEXT("NUInventory", "StatDamageReduce", "피해 감소"), Stats.DamageReduce, true },
		{ NSLOCTEXT("NUInventory", "StatHealBonus", "회복량"), Stats.HealBonus, true },
	};
}

FString NUItemUI::FormatEquipStatValue(float Value, bool bPercent)
{
	// Printf 포맷은 리터럴이어야 해서(컴파일 시 검사) 분기로 나눈다
	return bPercent ? FString::Printf(TEXT("%+.1f%%"), Value * 100.f) : FString::Printf(TEXT("%+.0f"), Value);
}

FText NUItemUI::GetCategoryText(const FNUItemDefRow& Def)
{
	switch (Def.ItemType)
	{
	case ENUItemType::Equipment:
		// 부위가 없는 장비(아이템 표에 든 장비 행)는 뒤에 붙일 말이 없다
		return Def.EquipType == ENUEquipType::None ? NSLOCTEXT("NUInventory", "TipEquipNoPart", "장비")
			: FText::Format(NSLOCTEXT("NUInventory", "TipEquip", "장비 · {0}"), NUEquip::GetEquipTypeDisplayName(Def.EquipType));
	case ENUItemType::Consumable: return NSLOCTEXT("NUInventory", "TipConsum", "소모품");
	case ENUItemType::Quest:      return NSLOCTEXT("NUInventory", "TipQuest", "퀘스트");
	case ENUItemType::Commune:    return NSLOCTEXT("NUInventory", "TipCommune", "교감");
	case ENUItemType::Blueprint:  return NSLOCTEXT("NUInventory", "TipBlueprint", "도면");
	case ENUItemType::KeyItem:    return NSLOCTEXT("NUInventory", "TipKeyItem", "중요");
	case ENUItemType::Keepsake:   return NSLOCTEXT("NUInventory", "TipKeepsake", "기념품");
	default:                      return NSLOCTEXT("NUInventory", "TipMaterial", "재료");
	}
}

FText NUItemUI::GetRequiredLevelText(const FNUItemDefRow& Def)
{
	return Def.ItemType == ENUItemType::Equipment && Def.RequiredLevel > 0
		? FText::Format(NSLOCTEXT("NUInventory", "TipRequiredLevel", "필요 레벨 {0}"), FText::AsNumber(Def.RequiredLevel))
		: FText::GetEmpty();
}

FText NUItemUI::GetRequiredJobText(const FNUItemDefRow& Def)
{
	return Def.ItemType == ENUItemType::Equipment && !Def.RequiredJobText.IsEmpty()
		? FText::Format(NSLOCTEXT("NUInventory", "TipRequiredJob", "{0} 전용"), Def.RequiredJobText)
		: FText::GetEmpty();
}

FString NUItemUI::GetEffectValuesText(const FNUItemDefRow& Def)
{
	FString EffectValues;
	if (Def.Effect1_Type != ENUItemEffectType::None && Def.Effect1_Type != ENUItemEffectType::CureStatus)
	{
		EffectValues = FString::Printf(TEXT("%d%s"), Def.Effect1_Value, Def.Effect1_IsPercent ? TEXT("%") : TEXT(""));
	}
	if (Def.Effect2_Type != ENUItemEffectType::None && Def.Effect2_Type != ENUItemEffectType::CureStatus)
	{
		EffectValues += FString::Printf(TEXT("%s%d%s"), EffectValues.IsEmpty() ? TEXT("") : TEXT(" · "),
			Def.Effect2_Value, Def.Effect2_IsPercent ? TEXT("%") : TEXT(""));
	}
	return EffectValues;
}

FText NUItemUI::GetUsageText(const FNUItemDefRow& Def)
{
	if (Def.bUsableInField && Def.bUsableInBattle)
	{
		return NSLOCTEXT("NUInventory", "TipUseBoth", "필드 · 전투에서 사용 가능");
	}
	if (Def.bUsableInField)
	{
		return NSLOCTEXT("NUInventory", "TipUseField", "필드에서만 사용 가능");
	}
	if (Def.bUsableInBattle)
	{
		return NSLOCTEXT("NUInventory", "TipUseBattle", "전투에서만 사용 가능");
	}
	return NSLOCTEXT("NUInventory", "TipUseNone", "사용 불가");
}

void NUItemUI::EquipChecked(const APlayerController* Player, const FGuid& InstanceId)
{
	const ANUPlayerState* State = Player != nullptr ? Player->GetPlayerState<ANUPlayerState>() : nullptr;
	UNUEquipmentComponent* Equipment = State != nullptr ? State->GetEquipmentComponent() : nullptr;
	if (Equipment == nullptr)
	{
		return;
	}
	// 다른 직업 전용 장비는 보내지 않고 알린다 --- 서버는 직업 제한을 몰라서 클라가 막는다 (9/14)
	const UNUInventoryComponent* Inventory = State->GetInventoryComponent();
	const FNUItemInstance* Instance = Inventory != nullptr ? Inventory->FindInstance(InstanceId) : nullptr;
	const FNUItemDefRow* Def = Instance != nullptr ? Inventory->FindItemDef(Instance->ItemRowName) : nullptr;
	// 장비가 아닌 것을 장착 칸에 놓았다 (패드로 집어 장비 칸에 놓기 등) --- 컴포넌트는 로그만 남기고 거절한다
	if (Def == nullptr || Def->ItemType != ENUItemType::Equipment || Def->EquipType == ENUEquipType::None)
	{
		if (UNUUIManager* UI = UNUUIManager::Get(Player))
		{
			UI->ShowNotice(NUInventory::GetResultMessage(1306));
		}
		return;
	}
	if (!Equipment->IsJobAllowed(*Def))
	{
		if (UNUUIManager* UI = UNUUIManager::Get(Player))
		{
			UI->ShowNotice(FText::Format(NSLOCTEXT("NUInventory", "EquipJobOnly", "{0} 전용 장비입니다."), Def->RequiredJobText));
		}
		return;
	}
	// 레벨이 모자라면 서버가 1308 로 거절하고 알림을 띄운다 --- 판정은 서버에 맡기고 장착 소리만 뺀다
	const UNUProgressionComponent* Progression = State->GetProgressionComponent();
	const int32 MyLevel = Progression != nullptr ? Progression->GetLevel() : 0;
	if (MyLevel <= 0 || MyLevel >= Def->RequiredLevel)
	{
		UNUSFXSubsystem::Play(Player, NU::SFX::Item::Equip);
	}
	Equipment->ServerEquipItem(InstanceId);
}

void NUItemUI::RequestConfirm(APlayerController* Player, TSubclassOf<UNUConfirmDialog> DialogClass,
	const FText& Message, const FText& ConfirmLabel, FSimpleDelegate OnConfirmed)
{
	// 창 WBP가 자기 것을 지정했으면 그것을, 아니면 프로젝트 세팅의 공용 확인 창을 쓴다
	if (DialogClass == nullptr)
	{
		if (const UNUUISettings* Settings = GetDefault<UNUUISettings>())
		{
			DialogClass = Settings->ConfirmDialogClass.LoadSynchronous();
		}
	}

	if (UNUConfirmDialog::Show(Player, DialogClass, Message, ConfirmLabel, OnConfirmed) != nullptr)
	{
		return;
	}

	// 확인 창 WBP 미지정 — 동작 자체는 살려두고 경고만 (지정하면 바로 확인 창이 붙는다)
	UE_LOG(LogNUInventory, Warning, TEXT("확인 창 미지정 — 확인 없이 실행: %s (프로젝트 세팅 > 게임 > NU UI)"), *Message.ToString());
	OnConfirmed.ExecuteIfBound();
}

void NUItemUI::RequestDrop(APlayerController* Player, const FGuid& InstanceId, TSubclassOf<UNUConfirmDialog> DialogClass)
{
	const ANUPlayerState* State = Player != nullptr ? Player->GetPlayerState<ANUPlayerState>() : nullptr;
	UNUInventoryComponent* Inventory = State != nullptr ? State->GetInventoryComponent() : nullptr;
	const FNUItemInstance* Instance = Inventory != nullptr ? Inventory->FindInstance(InstanceId) : nullptr;
	if (Instance == nullptr)
	{
		return;
	}

	const FNUItemDefRow* Def = Inventory->FindItemDef(Instance->ItemRowName);
	const FText ItemName = Def ? Def->DisplayName : FText::FromName(Instance->ItemRowName);
	const int32 DropCount = Instance->Count;

	// 되돌릴 수 없으므로 확인을 받는다. InstanceId만 캡처 — 확인 사이에 목록이 바뀔 수 있어 포인터는 잡지 않는다
	RequestConfirm(Player, DialogClass,
		DropCount > 1
			? FText::Format(NSLOCTEXT("NUInventory", "DropConfirmN", "{0} {1}개를 버리시겠습니까?"), ItemName, FText::AsNumber(DropCount))
			: FText::Format(NSLOCTEXT("NUInventory", "DropConfirm", "{0}을(를) 버리시겠습니까?"), ItemName),
		NSLOCTEXT("NUInventory", "Drop", "버리기"),
		FSimpleDelegate::CreateWeakLambda(Inventory, [Inventory, InstanceId]()
		{
			const FNUItemInstance* Confirmed = Inventory->FindInstance(InstanceId);
			if (Confirmed != nullptr)
			{
				Inventory->ServerDropItem(InstanceId, Confirmed->Count);
			}
		}));
}
