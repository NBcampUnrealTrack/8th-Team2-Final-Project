// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/UI/NUEquipmentWidget.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "InputCoreTypes.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUItemCellWidget.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "UI/NUItemContextMenu.h"
#include "UI/NUUIGeometry.h"
#include "UI/NUUISettings.h"

void UNUEquipmentWidget::UnequipSlot(ENUEquipSlot InSlot)
{
	if (Equipment.IsValid())
	{
		Equipment->ServerUnequipSlot(InSlot);
	}
}

void UNUEquipmentWidget::RefreshData()
{
	if (!ResolveComponents())
	{
		return;
	}

	bInBattle = Inventory->IsOwnerInBattle();

	// 장비 슬롯 11칸
	EquipSlots.Reset(static_cast<int32>(ENUEquipSlot::MAX));
	const TArray<FNUEquippedItem>& Equipped = Equipment->GetEquippedItems();
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(ENUEquipSlot::MAX); ++SlotIndex)
	{
		FNUSheetSlotEntry& Entry = EquipSlots.AddDefaulted_GetRef();
		Entry.Slot = static_cast<ENUEquipSlot>(SlotIndex);
		Entry.SlotName = NUEquip::GetSlotDisplayName(Entry.Slot);
		if (Equipped.IsValidIndex(SlotIndex) && Equipped[SlotIndex].InstanceId.IsValid())
		{
			Entry.bOccupied = true;
			Entry.ItemRowName = Equipped[SlotIndex].ItemRowName;
			const FNUItemDefRow* Def = Inventory->FindItemDef(Entry.ItemRowName);
			Entry.ItemName = Def ? Def->DisplayName : FText::FromName(Entry.ItemRowName);
		}
	}

	// 미리보기는 안 둔다 --- 렌더타깃 초상은 정보창(P키) 몫이고, 장비창은 칸이 본체다
	PopulateSlotList();
	OnEquipmentSlotsChanged();
}

void UNUEquipmentWidget::PopulateSlotList()
{
	// 다시 지으면 칸 위젯이 새것이라 포커스(패드)가 사라진다 --- 부위를 기억했다 되돌린다
	HideFocusTooltip();
	const UNUItemCellWidget* FocusedCell = FindFocusedCell();
	const int32 FocusedSlot = FocusedCell != nullptr ? FocusedCell->GetSlotNo() : INDEX_NONE;

	MainSlotGrid->ClearChildren();
	SubSlotGrid->ClearChildren();

	// 2열 그리드는 가로로 채워지므로 왼쪽·오른쪽을 한 줄씩 짝지어 적는다.
	// 왼쪽은 몸에 걸치는 것(투구 상의 하의), 오른쪽은 손발에 드는 것(무기 장갑 신발)
	// enum 순서를 그대로 쓰지 않는 이유: 하의가 나중에 추가돼 맨 뒤 번호를 받았다
	static const TArray<ENUEquipSlot> MainOrder = {
		ENUEquipSlot::Head,  ENUEquipSlot::Weapon,
		ENUEquipSlot::Chest, ENUEquipSlot::Gloves,
		ENUEquipSlot::Legs,  ENUEquipSlot::Boots };
	static const TArray<ENUEquipSlot> SubOrder = {
		ENUEquipSlot::Earring, ENUEquipSlot::Ring1, ENUEquipSlot::Ring2 };

	BuildSlotGrid(MainSlotGrid, MainOrder, SlotGridColumns);
	BuildSlotGrid(SubSlotGrid, SubOrder, SubSlotGridColumns);

	if (UNUItemCellWidget* Same = FindSlotCell(FocusedSlot))
	{
		Same->SetFocus();
	}
}

UNUItemCellWidget* UNUEquipmentWidget::FindSlotCell(int32 SlotNo) const
{
	if (SlotNo == INDEX_NONE)
	{
		return nullptr;
	}
	for (const UUniformGridPanel* Grid : { MainSlotGrid.Get(), SubSlotGrid.Get() })
	{
		for (int32 Index = 0; Grid != nullptr && Index < Grid->GetChildrenCount(); ++Index)
		{
			UNUItemCellWidget* Cell = Cast<UNUItemCellWidget>(Grid->GetChildAt(Index));
			if (Cell != nullptr && Cell->GetSlotNo() == SlotNo)
			{
				return Cell;
			}
		}
	}
	return nullptr;
}

void UNUEquipmentWidget::BuildSlotGrid(UUniformGridPanel* Grid, const TArray<ENUEquipSlot>& Order, int32 Columns)
{
	const int32 ColumnCount = FMath::Max(1, Columns);
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		const int32 SlotIndex = static_cast<int32>(Order[Index]);
		if (!EquipSlots.IsValidIndex(SlotIndex))
		{
			continue;
		}
		UNUItemCellWidget* Cell = BuildSlotCell(EquipSlots[SlotIndex]);
		if (Cell == nullptr)
		{
			return; // 칸 클래스 미지정 --- 나머지도 못 만든다
		}
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Cell, Index / ColumnCount, Index % ColumnCount))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

UNUItemCellWidget* UNUEquipmentWidget::BuildSlotCell(const FNUSheetSlotEntry& Entry)
{
	UNUItemCellWidget* Cell = MakeItemCell(SlotCellSize);
	if (Cell == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("ItemCellClass 미지정 --- 장비 칸을 그릴 수 없습니다 (프로젝트 세팅 NU UI 확인)"));
		return nullptr;
	}
	Cell->SetCoords(NUContainer::Equipment, static_cast<int32>(Entry.Slot));
	Cell->OnCellMouseDown.BindUObject(this, &UNUEquipmentWidget::HandleCellMouseDown);

	if (!Entry.bOccupied)
	{
		Cell->SetEmpty(Entry.SlotName); // 빈 칸은 어느 부위인지 보여 준다
		return Cell;
	}

	// 아이콘 없는 장비는 이름을 대신 보여 준다. 툴팁은 장착 중인 칸에만
	Cell->SetItem(LoadItemIconTexture(Entry.ItemRowName), 0, Entry.ItemName, false);
	if (UWidget* Tooltip = BuildItemTooltip(Entry.ItemRowName))
	{
		Cell->SetToolTip(Tooltip);
	}
	return Cell;
}

FReply UNUEquipmentWidget::HandleCellMouseDown(int32 Container, int32 SlotNo, const FPointerEvent& MouseEvent)
{
	// 우클릭 = 해제 (인벤 우클릭 = 장착과 짝). 전투 중에는 장비를 못 바꾸고, 빈 칸은 벗을 게 없다. Ctrl+우클릭 = 메뉴
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && !bInBattle
		&& EquipSlots.IsValidIndex(SlotNo) && EquipSlots[SlotNo].bOccupied)
	{
		if (MouseEvent.IsControlDown())
		{
			FVector2D PixelPos, ViewportPos;
			USlateBlueprintLibrary::AbsoluteToViewport(this, MouseEvent.GetScreenSpacePosition(), PixelPos, ViewportPos);
			OpenSlotMenu(SlotNo, ViewportPos);
		}
		else
		{
			UnequipSlot(static_cast<ENUEquipSlot>(SlotNo));
		}
	}
	return FReply::Handled(); // 칸 클릭이 창 드래그로 번지지 않게 소비
}

void UNUEquipmentWidget::CloseSelf()
{
	if (Inventory.IsValid())
	{
		Inventory->ToggleEquipmentUI();
	}
}

FReply UNUEquipmentWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const UNUItemCellWidget* Cell = FindFocusedCell();
	if (Cell != nullptr && EquipSlots.IsValidIndex(Cell->GetSlotNo()))
	{
		const int32 SlotNo = Cell->GetSlotNo();
		if (Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			// 소지품에서 집어 온 장비를 여기 놓으면 장착 (부위는 장비 종류가 정한다). 든 것이 없으면 아무것도 안 한다 --- 해제는 RT·메뉴.
			// 거래 창에서 집은 거래 칸은 장착 대상이 아니라 내려놓기만 한다
			if (Inventory.IsValid() && Inventory->GetPadHeldItem().IsSet())
			{
				const FNUPadHeldItem Held = Inventory->GetPadHeldItem();
				Inventory->ClearPadHeldItem();
				if (Held.Container != NUContainer::TradeOffer)
				{
					EquipItemChecked(Held.InstanceId);
				}
			}
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_RightTrigger)
		{
			if (!bInBattle && EquipSlots[SlotNo].bOccupied)
			{
				UnequipSlot(static_cast<ENUEquipSlot>(SlotNo));
			}
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_FaceButton_Left)
		{
			FVector2D CellPos = FVector2D::ZeroVector, CellSize = FVector2D::ZeroVector;
			if (Cell->GetCachedWidget().IsValid())
			{
				NUUIGeometry::GetViewportRect(this, *Cell->GetCachedWidget(), CellPos, CellSize);
			}
			OpenSlotMenu(SlotNo, CellPos + FVector2D(CellSize.X + 4.f, 0.f));
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNUEquipmentWidget::OpenSlotMenu(int32 SlotNo, const FVector2D& ScreenPos)
{
	if (bInBattle || !EquipSlots.IsValidIndex(SlotNo) || !EquipSlots[SlotNo].bOccupied)
	{
		return;
	}
	TArray<FNUItemMenuEntry> Entries;
	Entries.Add({ NSLOCTEXT("NUInventory", "MenuUnequip", "해제"),
		FSimpleDelegate::CreateWeakLambda(this, [this, SlotNo]() { UnequipSlot(static_cast<ENUEquipSlot>(SlotNo)); }) });
	UNUItemContextMenu::Show(this, MoveTemp(Entries), ScreenPos);
}

void UNUEquipmentWidget::FocusForPad()
{
	if (MainSlotGrid != nullptr && MainSlotGrid->GetChildrenCount() > 0)
	{
		MainSlotGrid->GetChildAt(0)->SetFocus();
		return;
	}
	Super::FocusForPad();
}

FName UNUEquipmentWidget::GetCellItemRowName(int32 Container, int32 SlotNo) const
{
	return EquipSlots.IsValidIndex(SlotNo) && EquipSlots[SlotNo].bOccupied ? EquipSlots[SlotNo].ItemRowName : NAME_None;
}
