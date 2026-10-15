#include "UI/CharacterMenu/NUCharacterEquipmentPage.h"

#include "CharacterState/NUPlayerState.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameplayEffectTypes.h"
#include "InputCoreTypes.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUEquipmentTypes.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUItemUI.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "UI/CharacterMenu/NUCharacterMenuMotion.h"

namespace
{
	// 장착 칸 이름의 앞머리 --- ENUEquipSlot 순서
	const TCHAR* const SlotPrefixes[] = {
		TEXT("EquipHead"), TEXT("EquipChest"), TEXT("EquipGloves"), TEXT("EquipBoots"), TEXT("EquipWeapon"),
		TEXT("EquipRing1"), TEXT("EquipRing2"), TEXT("EquipEarring"), TEXT("EquipLegs") };
	static_assert(UE_ARRAY_COUNT(SlotPrefixes) == static_cast<int32>(ENUEquipSlot::MAX), "장착 부위가 늘면 칸 이름도 추가할 것");

	// 종합 능력치 칸과 그 어트리뷰트
	struct FSummaryStat
	{
		const TCHAR* WidgetName;
		FGameplayAttribute Attribute;
	};

	TArray<FSummaryStat, TInlineAllocator<4>> GetSummaryStats()
	{
		return {
			{ TEXT("SummaryAttackValue"), UNUCombatAttributeSet::GetAttackPowerAttribute() },
			{ TEXT("SummaryDefenseValue"), UNUCombatAttributeSet::GetDefenseAttribute() },
			{ TEXT("SummaryHealthValue"), UNUVitalAttributeSet::GetMaxHealthAttribute() },
			{ TEXT("SummarySpeedValue"), UNUCombatAttributeSet::GetSpeedAttribute() } };
	}
}

void UNUCharacterEquipmentPage::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Slots.SetNum(UE_ARRAY_COUNT(SlotPrefixes));
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].Bind(*this, SlotPrefixes[Index]))
		{
			ButtonRelays.Add(UNUIndexedButtonRelay::Bind(this, Slots[Index].Button.Get(), Index,
				FOnNUIndexedButtonClicked::CreateUObject(this, &UNUCharacterEquipmentPage::HandleSlotClicked)));
		}
		else
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[통합 메뉴] %s에 %sButton이 없습니다 --- 그 부위는 안 보입니다"), *GetClass()->GetName(), SlotPrefixes[Index]);
		}
	}

	Card.Bind(*this);
	if (UButton* ActionButton = Card.GetActionButton())
	{
		ActionButton->OnClicked.AddUniqueDynamic(this, &UNUCharacterEquipmentPage::HandleActionClicked);
	}
}

void UNUCharacterEquipmentPage::NativeConstruct()
{
	Super::NativeConstruct(); // 미리보기 대여와 정보 글자 구독

	const ANUPlayerState* State = GetOwningPlayerState<ANUPlayerState>();
	Inventory = State != nullptr ? State->GetInventoryComponent() : nullptr;
	Equipment = State != nullptr ? State->GetEquipmentComponent() : nullptr;
	ASC = State != nullptr ? State->GetNUASC() : nullptr;
	if (Inventory.IsValid())
	{
		Inventory->OnOwnerBattleChanged.RemoveAll(this);
		Inventory->OnOwnerBattleChanged.AddUObject(this, &UNUCharacterEquipmentPage::HandleBattleChanged);
	}
	if (Equipment.IsValid())
	{
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UNUCharacterEquipmentPage::RefreshSlots);
	}
	if (ASC.IsValid())
	{
		for (const FSummaryStat& Stat : GetSummaryStats())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Stat.Attribute).RemoveAll(this);
			ASC->GetGameplayAttributeValueChangeDelegate(Stat.Attribute).AddUObject(this, &UNUCharacterEquipmentPage::HandleAttributeChanged);
		}
	}
	RefreshSlots();
}

void UNUCharacterEquipmentPage::NativeDestruct()
{
	if (Inventory.IsValid())
	{
		Inventory->OnOwnerBattleChanged.RemoveAll(this);
	}
	if (Equipment.IsValid())
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UNUCharacterEquipmentPage::RefreshSlots);
	}
	if (ASC.IsValid())
	{
		for (const FSummaryStat& Stat : GetSummaryStats())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Stat.Attribute).RemoveAll(this);
		}
	}
	Inventory.Reset();
	Equipment.Reset();
	ASC.Reset();
	Super::NativeDestruct();
}

FName UNUCharacterEquipmentPage::GetWornRowName(int32 SlotIndex) const
{
	if (!Equipment.IsValid())
	{
		return NAME_None;
	}
	const TArray<FNUEquippedItem>& Worn = Equipment->GetEquippedItems();
	return Worn.IsValidIndex(SlotIndex) && Worn[SlotIndex].InstanceId.IsValid() ? Worn[SlotIndex].ItemRowName : NAME_None;
}

bool UNUCharacterEquipmentPage::IsInBattle() const
{
	return Inventory.IsValid() && Inventory->IsOwnerInBattle();
}

void UNUCharacterEquipmentPage::RefreshSlots()
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].Button.IsValid())
		{
			continue;
		}
		// 장착 칸은 비어 있어도 몸체(버튼)가 보인다 --- 아이콘과 빈 칸 표시만 서로 바뀐다
		const FName RowName = GetWornRowName(Index);
		Slots[Index].SetIcon(NUItemUI::LoadItemIcon(RowName, true)); // 장착 칸은 그림만 그린다 --- 이름은 상세 카드에
		Slots[Index].SetEmptyMarkVisible(RowName.IsNone());
		if (NUCharacterMenuMotion::TrackName(this, FName(*FString::Printf(TEXT("Slot%d"), Index)), RowName) && !RowName.IsNone())
		{
			NUCharacterMenuMotion::Pulse(Slots[Index].Icon.Get(), 1.2f, 0.22f); // 방금 입은 장비
		}
	}
	UpdateSummary();
	UpdateSelection();
}

void UNUCharacterEquipmentPage::UpdateSummary()
{
	for (const FSummaryStat& Stat : GetSummaryStats())
	{
		if (UTextBlock* Value = Cast<UTextBlock>(GetWidgetFromName(Stat.WidgetName)))
		{
			Value->SetText(ASC.IsValid()
				? FText::AsNumber(FMath::RoundToInt(ASC->GetNumericAttribute(Stat.Attribute)))
				: FText::FromString(TEXT("—")));
			if (ASC.IsValid()) NUCharacterMenuMotion::FlashOnChange(this, Value, ASC->GetNumericAttribute(Stat.Attribute));
		}
	}
}

void UNUCharacterEquipmentPage::UpdateSelection()
{
	const FName RowName = GetWornRowName(SelectedSlot);
	if (RowName.IsNone())
	{
		SelectedSlot = INDEX_NONE; // 빈 칸이거나 벗어서 비었다
	}
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		Slots[Index].SetSelected(Index == SelectedSlot, SlotStyle);
	}
	if (NUCharacterMenuMotion::TrackValue(this, TEXT("SelectedSlot"), SelectedSlot) != 0.f && Slots.IsValidIndex(SelectedSlot))
	{
		NUCharacterMenuMotion::Pulse(Slots[SelectedSlot].Icon.Get(), 1.1f, 0.15f);
	}

	const FNUItemDefRow* Def = Inventory.IsValid() && !RowName.IsNone() ? Inventory->FindItemDef(RowName) : nullptr;
	if (Def == nullptr)
	{
		Card.Clear();
		return;
	}
	Card.Show(*Def, RowName, 0, FText::GetEmpty(), !IsInBattle(), SlotStyle); // 버튼 글자는 WBP 것("장비 해제") 그대로
}

void UNUCharacterEquipmentPage::UnequipSlot(int32 SlotIndex)
{
	if (Equipment.IsValid() && !IsInBattle() && !GetWornRowName(SlotIndex).IsNone())
	{
		Equipment->ServerUnequipSlot(static_cast<ENUEquipSlot>(SlotIndex));
	}
}

int32 UNUCharacterEquipmentPage::FindSlot(bool (UWidget::*Test)() const) const
{
	return Slots.IndexOfByPredicate([Test](const FNUCharacterMenuSlot& Entry) { return Entry.Button.IsValid() && (Entry.Button.Get()->*Test)(); });
}

FReply UNUCharacterEquipmentPage::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 Index = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton ? FindSlot(&UWidget::IsHovered) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}
	UnequipSlot(Index);
	return FReply::Handled();
}

FReply UNUCharacterEquipmentPage::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const int32 Index = InKeyEvent.GetKey() == EKeys::Gamepad_RightTrigger ? FindSlot(&UWidget::HasKeyboardFocus) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	UnequipSlot(Index);
	return FReply::Handled();
}

void UNUCharacterEquipmentPage::HandleSlotClicked(int32 SlotIndex)
{
	SelectedSlot = SlotIndex;
	UpdateSelection(); // 빈 칸을 누르면 고른 것이 없는 상태로 돌아간다
}

void UNUCharacterEquipmentPage::HandleBattleChanged()
{
	UpdateSelection(); // 전투 중에는 [장비 해제]가 꺼진다
}

void UNUCharacterEquipmentPage::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	UpdateSummary();
}

void UNUCharacterEquipmentPage::HandleActionClicked()
{
	UnequipSlot(SelectedSlot);
}
