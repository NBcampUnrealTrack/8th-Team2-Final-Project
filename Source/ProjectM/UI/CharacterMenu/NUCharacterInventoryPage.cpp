#include "UI/CharacterMenu/NUCharacterInventoryPage.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Core/NUGameUserSettings.h"
#include "InputCoreTypes.h"
#include "Inventory/NUEquipmentTypes.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUInventoryWindowWidget.h" // UNUItemDragOperation
#include "UI/CharacterMenu/NUCharacterMenuMotion.h"
#include "Inventory/UI/NUItemUI.h"
#include "Inventory/UI/NUSplitStackDialog.h"
#include "Layout/WidgetPath.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUItemContextMenu.h"
#include "UI/NUUIGeometry.h"
#include "UI/NUUIManager.h"

namespace
{
	// 필터 탭 (0 전체 / 1 무기 / 2 방어구 / 3 장신구) --- 장비 분류에서만 쓴다
	bool PassesFilter(const FNUItemDefRow& Def, int32 Filter)
	{
		switch (Filter)
		{
		case 1: return Def.EquipType == ENUEquipType::Weapon;
		case 2: return Def.EquipType == ENUEquipType::Head || Def.EquipType == ENUEquipType::Chest || Def.EquipType == ENUEquipType::Gloves
			|| Def.EquipType == ENUEquipType::Boots || Def.EquipType == ENUEquipType::Legs;
		case 3: return Def.EquipType == ENUEquipType::Ring1 || Def.EquipType == ENUEquipType::Ring2 || Def.EquipType == ENUEquipType::Earring;
		default: return true;
		}
	}

	// 칸 WBP 하나 (칸 안 버튼의 주인)
	UUserWidget* GetCellRoot(const FNUCharacterMenuSlot& Cell)
	{
		return Cell.Button.IsValid() ? Cell.Button->GetTypedOuter<UUserWidget>() : nullptr;
	}

	// 버튼 안에 그려진 그림들 (버튼 내용이 패널일 때)
	TArray<UImage*, TInlineAllocator<9>> GetButtonImages(const UButton* Button)
	{
		TArray<UImage*, TInlineAllocator<9>> Images;
		if (const UPanelWidget* Content = Button != nullptr ? Cast<UPanelWidget>(Button->GetContent()) : nullptr)
		{
			for (UWidget* Child : Content->GetAllChildren())
			{
				if (UImage* Image = Cast<UImage>(Child))
				{
					Images.Add(Image);
				}
			}
		}
		return Images;
	}

	FText GetSortLabel(ENUItemSortMode Mode)
	{
		return Mode == ENUItemSortMode::Grade ? NSLOCTEXT("NUCharacterMenu", "SortByGrade", "등급 순")
			: NSLOCTEXT("NUCharacterMenu", "SortByKind", "종류 순");
	}
}

void UNUCharacterInventoryPage::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		EnsureCells(PreviewCellCount);
	}
}

void UNUCharacterInventoryPage::EnsureCells(int32 Count)
{
	if (CellClass == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("[통합 메뉴] %s에 CellClass가 없습니다 --- 인벤토리 칸을 그릴 수 없습니다"), *GetClass()->GetName());
		return;
	}
	const int32 OldCount = Cells.Num();
	while (Cells.Num() < Count)
	{
		const int32 Index = Cells.Num();
		UUserWidget* Widget = CreateWidget<UUserWidget>(this, CellClass);
		FNUCharacterMenuSlot Cell;
		if (Widget == nullptr || !Cell.Bind(*Widget, TEXT("Cell")))
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[통합 메뉴] %s에 CellButton이 없습니다 --- 인벤토리 칸을 그릴 수 없습니다"), *GetNameSafe(CellClass));
			return;
		}
		// 빈 칸은 몸체(버튼)가 접혀 있어 칸 자체가 패드 포커스를 받는다 --- 빈 칸에 설 수 있어야 집은 아이템을 놓는다
		Widget->SetIsFocusable(true);
		if (UUniformGridSlot* GridSlot = InventoryGrid->AddChildToUniformGrid(Widget))
		{
			// 칸이 격자 칸(칸 + 여백)을 꼭 채워 어느 정렬이든 자리가 같다 --- 가운데로 둔다. 줄·열은 ApplyCellView가 놓는다
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		ButtonRelays.Add(UNUIndexedButtonRelay::Bind(this, Cell.Button.Get(), Index,
			FOnNUIndexedButtonClicked::CreateUObject(this, &UNUCharacterInventoryPage::HandleCellClicked)));
		Cells.Add(MoveTemp(Cell));
	}
	if (Cells.Num() > OldCount)
	{
		ApplyCellView();
	}
}

void UNUCharacterInventoryPage::SizeCell(UUserWidget& CellWidget)
{
	// 칸 크기는 칸 WBP의 뿌리(SizeBox)가 정한다. 큰 칸은 거기 그려진 크기 그대로라 처음 본 값을 기억해 둔다
	USizeBox* Box = Cast<USizeBox>(CellWidget.GetRootWidget());
	if (Box == nullptr)
	{
		return;
	}
	if (DesignCellSize <= 0.f)
	{
		DesignCellSize = Box->GetWidthOverride();
	}
	const float Size = bCompactView ? CompactCellSize : DesignCellSize;
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
}

void UNUCharacterInventoryPage::ApplyCellView()
{
	const int32 Columns = bCompactView ? CompactCellColumns : CellColumns;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		UUserWidget* Root = GetCellRoot(Cells[Index]);
		if (Root == nullptr)
		{
			continue;
		}
		SizeCell(*Root);
		if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Root->Slot))
		{
			GridSlot->SetRow(Index / Columns);
			GridSlot->SetColumn(Index % Columns);
		}
	}

	// 격자 칸은 칸 + 여백이다 (WBP에 그려진 값) --- 칸이 줄어든 만큼만 같이 줄여 여백은 그대로 둔다
	if (DesignSlotMin.IsZero())
	{
		DesignSlotMin = FVector2D(InventoryGrid->GetMinDesiredSlotWidth(), InventoryGrid->GetMinDesiredSlotHeight());
	}
	const float Shrink = bCompactView && DesignCellSize > 0.f ? DesignCellSize - CompactCellSize : 0.f; // 칸 WBP 뿌리가 SizeBox가 아니면 칸도 안 줄었다
	InventoryGrid->SetMinDesiredSlotWidth(DesignSlotMin.X - Shrink);
	InventoryGrid->SetMinDesiredSlotHeight(DesignSlotMin.Y - Shrink);
}

void UNUCharacterInventoryPage::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BindTabGroup(CategoryTabs, { TEXT("CategoryEquipment"), TEXT("CategoryConsumable"), TEXT("CategoryMaterial"), TEXT("CategoryOther") },
		&UNUCharacterInventoryPage::HandleCategoryClicked);
	BindTabGroup(FilterTabs, { TEXT("FilterAll"), TEXT("FilterWeapon"), TEXT("FilterArmor"), TEXT("FilterAccessory") },
		&UNUCharacterInventoryPage::HandleFilterClicked);
	BindTabGroup(ViewTabs, { TEXT("ViewLarge"), TEXT("ViewCompact") }, &UNUCharacterInventoryPage::HandleViewClicked, true);
	BindTabGroup(SortOptionTabs, { TEXT("SortOptionKind"), TEXT("SortOptionGrade") }, &UNUCharacterInventoryPage::HandleSortOptionClicked);
	for (int32 Index = 0; Index < SortOptionTabs.Labels.Num(); ++Index)
	{
		if (SortOptionTabs.Labels[Index].IsValid())
		{
			SortOptionTabs.Labels[Index]->SetText(GetSortLabel(static_cast<ENUItemSortMode>(Index))); // 정렬 상자에 적는 글자와 같게
		}
	}
	if (const UNUGameUserSettings* Settings = UNUGameUserSettings::Get())
	{
		bCompactView = Settings->IsInventoryCompactView();
	}
	if (const UCanvasPanelSlot* RuleSlot = CategorySelectedRule != nullptr ? Cast<UCanvasPanelSlot>(CategorySelectedRule->Slot) : nullptr)
	{
		CategoryRuleLayout = RuleSlot->GetLayout();
		CategoryRuleZOrder = RuleSlot->GetZOrder();
	}

	Card.Bind(*this);
	if (UButton* ActionButton = Card.GetActionButton())
	{
		ActionButton->OnClicked.AddUniqueDynamic(this, &UNUCharacterInventoryPage::HandleActionClicked);
	}
	if (SortButton != nullptr)
	{
		SortButton->OnClicked.AddUniqueDynamic(this, &UNUCharacterInventoryPage::HandleSortClicked);
	}
	if (SortLabel != nullptr)
	{
		// 이 메뉴를 연 뒤 마지막으로 정렬한 기준을 적는다 (열 때마다 종류 순에서 시작)
		SortLabel->SetText(GetSortLabel(ENUItemSortMode::Kind));
	}
	// 패드 포커스가 스크롤 밖 칸으로 가면 따라 스크롤한다 (WBP 기본값 NoScroll만 바꾼다). 매니저도 창을 열 때 같은 값을 넣지만
	// 메뉴가 다른 페이지로 열린 뒤에 만들어지는 페이지는 그때 없었다
	if (InventoryScroll != nullptr && InventoryScroll->GetScrollWhenFocusChanges() == EScrollWhenFocusChanges::NoScroll)
	{
		InventoryScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
	}
	for (UButton* Unsupported : { DismantleButton.Get(), LockButton.Get() })
	{
		if (Unsupported != nullptr)
		{
			Unsupported->SetIsEnabled(false);
		}
	}
}

void UNUCharacterInventoryPage::NativeConstruct()
{
	Super::NativeConstruct();

	const ANUPlayerState* State = GetOwningPlayerState<ANUPlayerState>();
	Inventory = State != nullptr ? State->GetInventoryComponent() : nullptr;
	if (Inventory.IsValid())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UNUCharacterInventoryPage::RefreshPage);
		Inventory->OnPadHeldItemChanged.RemoveAll(this);
		Inventory->OnPadHeldItemChanged.AddUObject(this, &UNUCharacterInventoryPage::UpdateHeld);
		Inventory->OnOwnerBattleChanged.RemoveAll(this);
		Inventory->OnOwnerBattleChanged.AddUObject(this, &UNUCharacterInventoryPage::HandleBattleChanged);
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
		UI->OnGamepadModeChanged.AddUObject(this, &UNUCharacterInventoryPage::HandleGamepadModeChanged);
	}
	SetSortOptionsOpen(false, false); // 목록을 편 채 떠났다 돌아온 경우 --- 포커스는 매니저가 준다
	UpdateTabs();
	RefreshPage();
}

void UNUCharacterInventoryPage::NativeDestruct()
{
	if (Inventory.IsValid())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UNUCharacterInventoryPage::RefreshPage);
		Inventory->OnPadHeldItemChanged.RemoveAll(this);
		Inventory->OnOwnerBattleChanged.RemoveAll(this);
		Inventory->ClearPadHeldItem(); // 페이지가 내려가면 든 것도 없다
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
	}
	Inventory.Reset();
	ClearDrag(); // 그림은 뷰포트에 따로 떠 있다
	Super::NativeDestruct();
}

void UNUCharacterInventoryPage::BindTabGroup(FTabGroup& Group, std::initializer_list<const TCHAR*> Prefixes, void (UNUCharacterInventoryPage::*Handler)(int32),
	bool bTintImages)
{
	for (const TCHAR* Prefix : Prefixes)
	{
		const int32 Index = Group.Buttons.Num();
		UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*(FString(Prefix) + TEXT("Button")))));
		Group.Buttons.Add(Button);
		Group.Labels.Add(Cast<UTextBlock>(GetWidgetFromName(FName(*(FString(Prefix) + TEXT("Label"))))));
		ButtonRelays.Add(UNUIndexedButtonRelay::Bind(this, Button, Index, FOnNUIndexedButtonClicked::CreateUObject(this, Handler)));
	}

	// WBP에 그려진 처음 상태: 첫 탭이 고른 모양, 둘째 탭이 보통 모양
	if (Group.Buttons.Num() > 1 && Group.Buttons[0].IsValid() && Group.Buttons[1].IsValid())
	{
		Group.SelectedStyle = Group.Buttons[0]->GetStyle();
		Group.IdleStyle = Group.Buttons[1]->GetStyle();
	}
	if (Group.Labels.Num() > 1 && Group.Labels[0].IsValid() && Group.Labels[1].IsValid())
	{
		Group.SelectedLabelColor = Group.Labels[0]->GetColorAndOpacity();
		Group.IdleLabelColor = Group.Labels[1]->GetColorAndOpacity();
	}
	if (bTintImages && Group.Buttons.Num() > 1)
	{
		const TArray<UImage*, TInlineAllocator<9>> SelectedImages = GetButtonImages(Group.Buttons[0].Get());
		const TArray<UImage*, TInlineAllocator<9>> IdleImages = GetButtonImages(Group.Buttons[1].Get());
		if (!SelectedImages.IsEmpty() && !IdleImages.IsEmpty())
		{
			Group.bTintImages = true;
			Group.SelectedImageTint = SelectedImages[0]->GetBrush().TintColor;
			Group.IdleImageTint = IdleImages[0]->GetBrush().TintColor;
		}
	}
}

void UNUCharacterInventoryPage::ApplyTabGroup(const FTabGroup& Group, int32 SelectedIndex) const
{
	for (int32 Index = 0; Index < Group.Buttons.Num(); ++Index)
	{
		const bool bSelected = Index == SelectedIndex;
		if (Group.Buttons[Index].IsValid())
		{
			Group.Buttons[Index]->SetStyle(bSelected ? Group.SelectedStyle : Group.IdleStyle);
		}
		if (Group.Labels[Index].IsValid())
		{
			Group.Labels[Index]->SetColorAndOpacity(bSelected ? Group.SelectedLabelColor : Group.IdleLabelColor);
		}
		if (Group.bTintImages)
		{
			for (UImage* Image : GetButtonImages(Group.Buttons[Index].Get()))
			{
				Image->SetBrushTintColor(bSelected ? Group.SelectedImageTint : Group.IdleImageTint);
			}
		}
	}
}

void UNUCharacterInventoryPage::UpdateTabs()
{
	ApplyTabGroup(CategoryTabs, Category);
	ApplyTabGroup(FilterTabs, Filter);
	ApplyTabGroup(ViewTabs, bCompactView ? 1 : 0);

	// 밑줄은 분류 탭 안(버튼 내용 캔버스)에 그려져 있다 --- 고른 탭의 같은 자리로 옮긴다
	UCanvasPanel* Content = CategoryTabs.Buttons.IsValidIndex(Category) && CategoryTabs.Buttons[Category].IsValid()
		? Cast<UCanvasPanel>(CategoryTabs.Buttons[Category]->GetContent()) : nullptr;
	if (CategorySelectedRule != nullptr && Content != nullptr && CategorySelectedRule->GetParent() != Content)
	{
		CategorySelectedRule->RemoveFromParent();
		if (UCanvasPanelSlot* RuleSlot = Content->AddChildToCanvas(CategorySelectedRule))
		{
			RuleSlot->SetLayout(CategoryRuleLayout);
			RuleSlot->SetZOrder(CategoryRuleZOrder);
		}
	}

	for (const TWeakObjectPtr<UButton>& Button : FilterTabs.Buttons)
	{
		if (Button.IsValid())
		{
			Button->SetVisibility(Category == NUBagCategory::Equipment ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

void UNUCharacterInventoryPage::RefreshPage()
{
	const int32 FocusedIndex = FindFocusedCell();
	const TArray<FNUItemInstance>* Items = Inventory.IsValid() ? Inventory->GetContainerItems(NUContainer::Inventory) : nullptr;

	// 보일 가방 칸 --- 분류 구간이 있으면 이 탭의 구간, 없으면 가방 전체
	const FNUBagSection* Section = Inventory.IsValid() ? Inventory->GetBagSection(Category) : nullptr;
	const FNUBagSection Range = Section != nullptr ? *Section
		: FNUBagSection{ 0, Inventory.IsValid() ? Inventory->GetContainerSlotCount(NUContainer::Inventory) : 0 };

	TArray<const FNUItemInstance*, TInlineAllocator<64>> ItemBySlot;
	ItemBySlot.SetNumZeroed(Range.Count);
	int32 UsedCount = 0;
	if (Items != nullptr)
	{
		for (const FNUItemInstance& Item : *Items)
		{
			if (Range.Contains(Item.SlotNo))
			{
				ItemBySlot[Item.SlotNo - Range.Start] = &Item;
				++UsedCount;
			}
		}
	}

	// 가방 칸 순서대로 --- 빈 칸과 이 분류의 아이템이 든 칸만. 구간이 있으면 구간의 칸이 곧 이 분류의 칸이고,
	// 구간이 없으면 다른 분류가 차지한 칸은 건너뛴다
	ShownSlots.Reset(Range.Count);
	for (int32 Offset = 0; Offset < Range.Count; ++Offset)
	{
		const int32 SlotNo = Range.Start + Offset;
		const FNUItemInstance* Item = ItemBySlot[Offset];
		if (Item == nullptr)
		{
			ShownSlots.Add({ SlotNo });
			continue;
		}
		const FNUItemDefRow* Def = Inventory->FindItemDef(Item->ItemRowName);
		const bool bInTab = Section != nullptr || Inventory->GetBagCategory(Def) == Category;
		const bool bPassesFilter = Category != NUBagCategory::Equipment || Filter == 0 || (Def != nullptr && PassesFilter(*Def, Filter));
		if (bInTab && bPassesFilter)
		{
			ShownSlots.Add({ SlotNo, Item->InstanceId, Item->ItemRowName, Item->Count });
		}
	}

	// 칸 위젯은 보일 가방 칸 수만큼 만들어 두고, 가리킬 가방 칸이 없는 뒤쪽 칸은 접는다
	EnsureCells(Range.Count);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const bool bShown = ShownSlots.IsValidIndex(Index);
		if (UUserWidget* Root = GetCellRoot(Cells[Index]))
		{
			// 칸 자체가 마우스에 잡혀야 한다 --- 패드 이동 후보는 히트 테스트 격자에 있는 것만이고, 격자는 잡히는 위젯만 담는다
			Root->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (bShown)
		{
			FillCell(Cells[Index], ShownSlots[Index]);
		}
	}
	UpdateHeld();

	// 포커스가 서 있던 칸은 그 자리에 둔다 (옛 소지품 창과 같다). 몸체(버튼)에 서 있었는데 그 자리가 비어 버튼이 접혔으면
	// 칸 자체로 옮긴다 --- 접힌 위젯에 남은 포커스는 키를 못 받는다. 보이는 칸이 줄어 그 칸이 접혔으면 마지막 칸으로
	if (FocusedIndex != INDEX_NONE && !ShownSlots.IsEmpty())
	{
		const int32 Stay = FMath::Min(FocusedIndex, ShownSlots.Num() - 1);
		UUserWidget* Root = GetCellRoot(Cells[Stay]);
		if (Root != nullptr && (Stay != FocusedIndex || !HasItemAt(Stay)))
		{
			Root->SetFocus();
		}
		// 패드는 선 칸의 아이템이 고른 아이템이다 --- 서버가 옮긴 칸은 식별자가 새로 매겨져 고른 것을 따로 기억할 수 없다
		if (IsGamepadMode() && HasItemAt(Stay))
		{
			SelectedInstanceId = ShownSlots[Stay].InstanceId;
		}
	}

	if (InventoryCount != nullptr)
	{
		// 분류 구간이 있으면 이 탭의 구간 기준, 없으면 가방 전체 기준 (필터와 상관없이)
		InventoryCount->SetText(FText::Format(NSLOCTEXT("NUCharacterMenu", "InventoryCount", "{0} / {1}"),
			FText::AsNumber(UsedCount), FText::AsNumber(Range.Count)));
	}

	UpdateSelection();
}

void UNUCharacterInventoryPage::FillCell(const FNUCharacterMenuSlot& Cell, const FShownSlot& Shown) const
{
	const FShownSlot* Item = Shown.InstanceId.IsValid() ? &Shown : nullptr;
	Cell.Button->SetVisibility(Item != nullptr ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Cell.SetEmptyMarkVisible(Item == nullptr);
	if (Item == nullptr)
	{
		return;
	}

	const FNUItemDefRow* Def = Inventory->FindItemDef(Item->RowName);
	UTexture2D* IconTexture = NUItemUI::LoadItemIcon(Item->RowName, true); // 등급은 아래 등급 선이 가른다
	Cell.SetIcon(IconTexture);
	Cell.SetGrade(Def != nullptr ? Def->Grade : ENUItemGrade::None, SlotStyle);
	if (!Cell.Level.IsValid())
	{
		return;
	}

	// 아래 글자 줄 --- 아이콘이 없는 아이템은 이름이 아이콘 노릇, 장비는 필요 레벨, 묶음은 개수
	FText Line;
	if (IconTexture == nullptr)
	{
		Line = Def != nullptr ? Def->DisplayName : FText::FromName(Item->RowName);
	}
	else if (Def != nullptr && Def->ItemType == ENUItemType::Equipment)
	{
		if (Def->RequiredLevel > 0)
		{
			Line = FText::Format(NSLOCTEXT("NUCharacterMenu", "CellLevel", "Lv. {0}"), FText::AsNumber(Def->RequiredLevel));
		}
	}
	else if (Item->Count > 1)
	{
		Line = FText::Format(NSLOCTEXT("NUCharacterMenu", "CellCount", "x{0}"), FText::AsNumber(Item->Count));
	}
	Cell.Level->SetText(Line);
}

void UNUCharacterInventoryPage::UpdateSelection()
{
	const FShownSlot* Selected = FindShownItem(SelectedInstanceId);
	if (Selected == nullptr)
	{
		SelectedInstanceId.Invalidate(); // 쓰거나 장착해서 가방에서 사라졌다
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const bool bSelected = Selected != nullptr && ShownSlots.IsValidIndex(Index) && &ShownSlots[Index] == Selected;
		Cells[Index].SetSelected(bSelected, SlotStyle);
		if (bSelected && NUCharacterMenuMotion::TrackName(this, TEXT("Selected"), FName(*SelectedInstanceId.ToString())))
		{
			NUCharacterMenuMotion::Pulse(Cells[Index].Icon.Get(), 1.1f, 0.15f);
		}
	}
	if (Selected == nullptr)
	{
		NUCharacterMenuMotion::TrackName(this, TEXT("Selected"), NAME_None);
	}

	const FNUItemDefRow* Def = Selected != nullptr ? Inventory->FindItemDef(Selected->RowName) : nullptr;
	if (Def == nullptr)
	{
		Card.Clear();
		return;
	}
	// 쓸 수 없는 아이템(재료 등)은 꺼진 [사용]으로 보인다
	const FText ActionLabel = Def->ItemType == ENUItemType::Equipment ? NSLOCTEXT("NUCharacterMenu", "ActionEquip", "장착")
		: NSLOCTEXT("NUCharacterMenu", "ActionUse", "사용");
	Card.Show(*Def, Selected->RowName, Selected->Count, ActionLabel, CanActivate(Def), SlotStyle);
}

const UNUCharacterInventoryPage::FShownSlot* UNUCharacterInventoryPage::FindShownItem(const FGuid& InstanceId) const
{
	return InstanceId.IsValid()
		? ShownSlots.FindByPredicate([&InstanceId](const FShownSlot& Shown) { return Shown.InstanceId == InstanceId; })
		: nullptr;
}

bool UNUCharacterInventoryPage::HasItemAt(int32 Index) const
{
	return ShownSlots.IsValidIndex(Index) && ShownSlots[Index].InstanceId.IsValid();
}

int32 UNUCharacterInventoryPage::FindCell(bool (UWidget::*Test)() const) const
{
	return Cells.IndexOfByPredicate([Test](const FNUCharacterMenuSlot& Cell) { return Cell.Button.IsValid() && (Cell.Button.Get()->*Test)(); });
}

int32 UNUCharacterInventoryPage::FindFocusedCell() const
{
	for (int32 Index = 0; Index < ShownSlots.Num() && Index < Cells.Num(); ++Index)
	{
		const UUserWidget* Root = GetCellRoot(Cells[Index]);
		if (Root != nullptr && (Root->HasKeyboardFocus() || Root->HasFocusedDescendants()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

UWidget* UNUCharacterInventoryPage::GetPadFocusTarget() const
{
	if (ShownSlots.IsEmpty() || Cells.IsEmpty())
	{
		return nullptr;
	}
	// D패드로 설 때와 같은 자리 --- 아이템이 든 칸은 몸체(버튼), 빈 칸은 칸 자체
	return HasItemAt(0) ? static_cast<UWidget*>(Cells[0].Button.Get()) : GetCellRoot(Cells[0]);
}

int32 UNUCharacterInventoryPage::FindCellAt(const FVector2D& ScreenPos) const
{
	if (InventoryScroll != nullptr && !InventoryScroll->GetCachedGeometry().IsUnderLocation(ScreenPos))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < ShownSlots.Num() && Index < Cells.Num(); ++Index)
	{
		const UUserWidget* Root = GetCellRoot(Cells[Index]);
		if (Root != nullptr && Root->GetCachedGeometry().IsUnderLocation(ScreenPos))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool UNUCharacterInventoryPage::IsInBattle() const
{
	return Inventory.IsValid() && Inventory->IsOwnerInBattle();
}

bool UNUCharacterInventoryPage::CanActivate(const FNUItemDefRow* Def) const
{
	if (Def == nullptr || IsInBattle())
	{
		return false;
	}
	return Def->ItemType == ENUItemType::Equipment ? Def->EquipType != ENUEquipType::None
		: Def->ItemType == ENUItemType::Consumable && Def->bUsableInField;
}

void UNUCharacterInventoryPage::ActivateItem(const FShownSlot& Item)
{
	const FNUItemDefRow* Def = Inventory.IsValid() ? Inventory->FindItemDef(Item.RowName) : nullptr;
	if (!CanActivate(Def))
	{
		// 재료처럼 쓸 수 없는 아이템을 우클릭·RT로 눌렀다 --- 아무 일도 없으면 고장으로 보인다 (전투 중에는 메뉴가 닫히므로 알리지 않는다)
		UNUUIManager* UI = Def != nullptr && !IsInBattle() ? UNUUIManager::Get(GetOwningPlayer()) : nullptr;
		if (UI != nullptr)
		{
			UI->ShowNotice(Def->ItemType == ENUItemType::Equipment ? NUInventory::GetResultMessage(1306)
				: NSLOCTEXT("NUCharacterMenu", "CannotUse", "사용할 수 없는 아이템입니다."));
		}
		return;
	}
	if (Def->ItemType == ENUItemType::Equipment)
	{
		NUItemUI::EquipChecked(GetOwningPlayer(), Item.InstanceId);
	}
	else
	{
		Inventory->ServerUseItem(Item.InstanceId); // 성공하면 인벤 변경 통지가 목록을 다시 그린다
	}
}

UUserWidget* UNUCharacterInventoryPage::GetOwningWindow()
{
	// 페이지는 셸의 NamedSlot에 끼워진다 --- 그 슬롯 위젯의 주인이 셸이다
	const UWidget* SlotHost = GetParent();
	UUserWidget* Shell = SlotHost != nullptr ? SlotHost->GetTypedOuter<UUserWidget>() : nullptr;
	return Shell != nullptr ? Shell : this;
}

void UNUCharacterInventoryPage::OpenItemMenu(const FShownSlot& Item, const FVector2D& ViewportPos)
{
	const FNUItemDefRow* Def = Inventory.IsValid() ? Inventory->FindItemDef(Item.RowName) : nullptr;
	const FGuid InstanceId = Item.InstanceId;
	TArray<FNUItemMenuEntry> Entries;

	if (CanActivate(Def))
	{
		Entries.Add({ Def->ItemType == ENUItemType::Equipment ? NSLOCTEXT("NUInventory", "MenuEquip", "장착") : NSLOCTEXT("NUInventory", "MenuUse", "사용"),
			FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId]()
			{
				if (const FShownSlot* Current = FindShownItem(InstanceId))
				{
					ActivateItem(*Current);
				}
			}) });
	}
	if (Item.Count > 1)
	{
		Entries.Add({ NSLOCTEXT("NUInventory", "MenuSplit", "나누기"), FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId]()
		{
			// 전부 떼는 건 나누기가 아니라 이동이라 상한은 하나 적다
			const FShownSlot* Current = FindShownItem(InstanceId);
			if (Current == nullptr || Current->Count < 2)
			{
				return;
			}
			FOnNUSplitConfirmed OnConfirmed;
			OnConfirmed.BindWeakLambda(this, [this, SlotNo = Current->SlotNo](int32 Count)
			{
				if (Inventory.IsValid())
				{
					Inventory->SplitStack(NUContainer::Inventory, SlotNo, Count);
				}
			});
			UNUSplitStackDialog::Show(GetOwningWindow(), Current->Count - 1, OnConfirmed);
		}) });
	}
	if (!IsInBattle() && (Def == nullptr || NUItem::IsDroppable(Def->ItemType)))
	{
		Entries.Add({ NSLOCTEXT("NUInventory", "MenuDrop", "버리기"), FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId]()
		{
			NUItemUI::RequestDrop(GetOwningPlayer(), InstanceId);
		}) });
	}
	UNUItemContextMenu::Show(GetOwningWindow(), MoveTemp(Entries), ViewportPos);
}

void UNUCharacterInventoryPage::ActOnCell(int32 Index, bool bOpenMenu, const FVector2D& MenuViewportPos)
{
	const FShownSlot Item = ShownSlots[Index]; // 고르면서 목록이 다시 그려질 수 있어 복사해 둔다
	SelectedInstanceId = Item.InstanceId;
	UpdateSelection();
	if (bOpenMenu)
	{
		OpenItemMenu(Item, MenuViewportPos);
	}
	else
	{
		ActivateItem(Item);
	}
}

FReply UNUCharacterInventoryPage::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 펴진 정렬 기준 목록은 목록·정렬 상자 밖을 누르면 접는다 (그 누름은 그대로 아래 위젯이 받는다)
	if (SortOptions != nullptr && SortOptions->IsVisible())
	{
		const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
		if (!SortOptions->GetCachedGeometry().IsUnderLocation(ScreenPos)
			&& (SortButton == nullptr || !SortButton->GetCachedGeometry().IsUnderLocation(ScreenPos)))
		{
			SetSortOptionsOpen(false);
		}
	}

	const FKey MouseButton = InMouseEvent.GetEffectingButton();
	const int32 Index = MouseButton == EKeys::LeftMouseButton || MouseButton == EKeys::RightMouseButton ? FindCell(&UWidget::IsHovered) : INDEX_NONE;
	if (!HasItemAt(Index))
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (MouseButton == EKeys::LeftMouseButton)
	{
		// 고르기도 여기서 한다 (버튼까지 안 내려간다. 버튼의 클릭은 키보드 Enter·Space로만 온다). 끌기는 커서가 움직여야 시작된다
		HandleCellClicked(Index);
		DragSourceSlotNo = ShownSlots[Index].SlotNo;
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	// Ctrl = 아이템 메뉴, 단독 = 바로 장착·사용 (옛 소지품 창과 같은 규칙)
	FVector2D PixelPos, ViewportPos;
	USlateBlueprintLibrary::AbsoluteToViewport(this, InMouseEvent.GetScreenSpacePosition(), PixelPos, ViewportPos);
	ActOnCell(Index, InMouseEvent.IsControlDown(), ViewportPos);
	return FReply::Handled();
}

void UNUCharacterInventoryPage::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	// 누른 뒤 목록이 다시 그려졌을 수 있어 가방 칸 번호로 다시 찾는다
	const int32 Index = ShownSlots.IndexOfByPredicate([this](const FShownSlot& Shown) { return Shown.SlotNo == DragSourceSlotNo; });
	if (!HasItemAt(Index) || !Cells.IsValidIndex(Index))
	{
		return;
	}

	UNUItemDragOperation* Operation = NewObject<UNUItemDragOperation>(this);
	Operation->SourceContainer = NUContainer::Inventory;
	Operation->SourceSlotNo = DragSourceSlotNo;
	OutOperation = Operation;
	if (Inventory.IsValid())
	{
		Inventory->ClearPadHeldItem(); // 마우스로 끌기 시작하면 패드로 집은 것은 버린다
	}

	// 커서를 따라가는 그림 --- 칸 WBP를 하나 더 만들어 같은 내용을 넣는다. 크기는 화면에 보이는 칸 그대로 (셸이 페이지를 줄여 그린다)
	// ClearDrag는 출발 칸 번호까지 지운다 --- 남은 이전 끌기만 정리하고 이번 출발 칸은 되살려야 놓을 때(제자리·칸 밖 포함) 그 칸을 다시 채운다
	const int32 SourceSlotNo = DragSourceSlotNo;
	ClearDrag();
	DragSourceSlotNo = SourceSlotNo;
	UUserWidget* Visual = CreateWidget<UUserWidget>(this, CellClass);
	FNUCharacterMenuSlot VisualCell;
	const TSharedPtr<SWidget> CellWidget = Cells[Index].Button->GetCachedWidget();
	if (Visual != nullptr && CellWidget.IsValid() && VisualCell.Bind(*Visual, TEXT("Cell")))
	{
		SizeCell(*Visual);
		FillCell(VisualCell, ShownSlots[Index]);
		FVector2D CellPos = FVector2D::ZeroVector, CellSize = FVector2D::ZeroVector;
		NUUIGeometry::GetViewportRect(this, *CellWidget, CellPos, CellSize);
		DragVisual = UNUDragVisualWidget::Show(GetOwningPlayer(), Visual, CellSize.X, false);
	}
	// The lifted item leaves an empty slot behind and grows/tilts under the cursor; the cell under
	// the cursor rises with the selection outline to show where it will land.
	FillCell(Cells[Index], FShownSlot());
	if (Visual)
	{
		NUCharacterMenuMotion::Lift(Visual);
	}
	TArray<TWeakObjectPtr<UWidget>> Targets;
	for (const FNUCharacterMenuSlot& Cell : Cells)
	{
		UButton* CellButton = Cell.Button.Get();
		Targets.Add(CellButton ? CellButton->GetTypedOuter<UUserWidget>() : nullptr);
	}
	const TWeakObjectPtr<UNUCharacterInventoryPage> WeakThis(this);
	NUCharacterMenuMotion::TrackDropTargets(this, MoveTemp(Targets), [WeakThis](UWidget* Previous, UWidget* Current)
	{
		UNUCharacterInventoryPage* Page = WeakThis.Get();
		if (!Page) return;
		for (const FNUCharacterMenuSlot& Cell : Page->Cells)
		{
			UButton* CellButton = Cell.Button.Get();
			UWidget* CellWidget = CellButton ? CellButton->GetTypedOuter<UUserWidget>() : nullptr;
			if (CellWidget && (CellWidget == Previous || CellWidget == Current))
			{
				Cell.SetSelected(CellWidget == Current, Page->SlotStyle);
			}
		}
		if (Previous && !Current) Page->UpdateSelection(); // back to the real selection outline
	});
}

bool UNUCharacterInventoryPage::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UNUItemDragOperation* Operation = Cast<UNUItemDragOperation>(InOperation);
	if (Operation == nullptr)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	ClearDrag();

	// 칸 위에 놓았을 때만 옮긴다 (빈 칸 = 이동, 같은 아이템 = 합치기, 그 밖의 찬 칸 = 맞바꾸기 --- 판정은 서버가 한다). 그 밖의 자리는 삼키기만 한다
	const int32 Index = FindCellAt(InDragDropEvent.GetScreenSpacePosition());
	if (ShownSlots.IsValidIndex(Index) && Inventory.IsValid() && !Operation->bFromTradeOffer
		&& Operation->SourceContainer == NUContainer::Inventory && Operation->SourceSlotNo != ShownSlots[Index].SlotNo)
	{
		Inventory->ServerMoveItemBetween(NUContainer::Inventory, Operation->SourceSlotNo, NUContainer::Inventory, ShownSlots[Index].SlotNo);
		if (UButton* CellButton = Cells.IsValidIndex(Index) ? Cells[Index].Button.Get() : nullptr)
		{
			NUCharacterMenuMotion::Pulse(CellButton->GetTypedOuter<UUserWidget>(), 1.08f, 0.18f);
		}
	}
	return true;
}

void UNUCharacterInventoryPage::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	ClearDrag(); // 페이지 밖에 놓았다 --- 아무 일도 없다 (화면 전체가 창이라 "창 밖 = 버리기"는 쓰지 않는다)
}

void UNUCharacterInventoryPage::ClearDrag()
{
	NUCharacterMenuMotion::TrackDropTargets(this, {});
	// Put the lifted item back into its slot; a successful move redraws the bag when the server answers.
	const int32 SourceIndex = DragSourceSlotNo == INDEX_NONE ? INDEX_NONE
		: ShownSlots.IndexOfByPredicate([this](const FShownSlot& Shown) { return Shown.SlotNo == DragSourceSlotNo; });
	if (Cells.IsValidIndex(SourceIndex) && ShownSlots.IsValidIndex(SourceIndex) && Inventory.IsValid())
	{
		FillCell(Cells[SourceIndex], ShownSlots[SourceIndex]);
	}
	DragSourceSlotNo = INDEX_NONE;
	if (DragVisual != nullptr)
	{
		DragVisual->RemoveFromParent();
		DragVisual = nullptr;
	}
}

void UNUCharacterInventoryPage::PadPickOrPlace(int32 Index)
{
	if (!Inventory.IsValid() || !ShownSlots.IsValidIndex(Index))
	{
		return;
	}
	const FShownSlot Target = ShownSlots[Index]; // 옮기면 목록이 다시 그려진다
	const FNUPadHeldItem Held = Inventory->GetPadHeldItem();
	if (!Held.IsSet())
	{
		if (Target.InstanceId.IsValid())
		{
			Inventory->SetPadHeldItem(NUContainer::Inventory, Target.SlotNo, Target.InstanceId);
		}
		return;
	}
	if (Held.Container == NUContainer::Inventory && Held.SlotNo != Target.SlotNo)
	{
		// 분류 구간이 있으면 든 것을 다른 탭의 칸에 놓을 수 없다 (서버에도 거절을 요청했다) --- 든 채로 두고 알린다
		const FNUItemInstance* HeldItem = Inventory->GetBagSection(Category) != nullptr ? Inventory->FindBySlotNo(Held.SlotNo) : nullptr;
		if (HeldItem != nullptr && Inventory->GetBagCategory(Inventory->FindItemDef(HeldItem->ItemRowName)) != Category)
		{
			if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
			{
				UI->ShowNotice(NSLOCTEXT("NUCharacterMenu", "WrongBagTab", "이 탭에는 둘 수 없는 아이템입니다."));
			}
			return;
		}
		Inventory->ServerMoveItemBetween(NUContainer::Inventory, Held.SlotNo, NUContainer::Inventory, Target.SlotNo); // 끌어 놓기와 같다
	}
	Inventory->ClearPadHeldItem();
}

void UNUCharacterInventoryPage::UpdateHeld()
{
	const FNUPadHeldItem Held = Inventory.IsValid() ? Inventory->GetPadHeldItem() : FNUPadHeldItem();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cells[Index].SetHeld(Held.Container == NUContainer::Inventory && HasItemAt(Index) && ShownSlots[Index].SlotNo == Held.SlotNo);
	}
}

bool UNUCharacterInventoryPage::IsGamepadMode() const
{
	const ANUPlayerController* NUPlayer = Cast<ANUPlayerController>(GetOwningPlayer());
	return NUPlayer != nullptr && NUPlayer->IsGamepadMode();
}

void UNUCharacterInventoryPage::HandleGamepadModeChanged(bool bGamepad)
{
	// 마우스로 돌아오면 집은 것은 뜻이 없다 (끌기가 대신한다)
	if (!bGamepad && Inventory.IsValid())
	{
		Inventory->ClearPadHeldItem();
	}
}

FReply UNUCharacterInventoryPage::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const int32 Index = InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom ? FindFocusedCell() : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}
	PadPickOrPlace(Index);
	return FReply::Handled();
}

void UNUCharacterInventoryPage::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	// 패드 포커스가 옮겨 갈 때만 본다. 마우스·키보드로 넘어오며 매니저가 포커스를 뷰포트로 돌리는 것은 여기서 빠진다
	if (!IsGamepadMode() || !NewWidgetPath.IsValid())
	{
		return;
	}

	// 펴진 정렬 기준 목록은 포커스가 목록·정렬 상자 밖으로 나가면 접는다 (방향키로 벗어난 경우 --- 포커스는 간 곳에 그대로 둔다).
	// 마우스로 넘어올 때도 접으면 기준을 고르려던 누름이 목록 아래 칸에 들어간다 --- 그쪽은 밖을 누를 때 접는다
	if (SortOptions != nullptr && SortOptions->IsVisible())
	{
		const TSharedPtr<SWidget> ListWidget = SortOptions->GetCachedWidget();
		const TSharedPtr<SWidget> BoxWidget = SortButton != nullptr ? SortButton->GetCachedWidget() : nullptr;
		if (!(ListWidget.IsValid() && NewWidgetPath.ContainsWidget(ListWidget.Get()))
			&& !(BoxWidget.IsValid() && NewWidgetPath.ContainsWidget(BoxWidget.Get())))
		{
			SetSortOptionsOpen(false, false);
		}
	}

	// 옛 소지품 창의 포커스 툴팁 자리 --- 빈 칸이나 칸 밖으로 가면 고른 것은 그대로 둔다 (카드의 [장착]/[사용]으로 가는 길)
	for (int32 Index = 0; Index < ShownSlots.Num() && Index < Cells.Num(); ++Index)
	{
		const UUserWidget* Root = GetCellRoot(Cells[Index]);
		const TSharedPtr<SWidget> RootWidget = Root != nullptr ? Root->GetCachedWidget() : nullptr;
		if (RootWidget.IsValid() && NewWidgetPath.ContainsWidget(RootWidget.Get()))
		{
			HandleCellClicked(Index);
			return;
		}
	}
}

FReply UNUCharacterInventoryPage::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	// 펴진 정렬 기준 목록부터 접는다 (든 것 내려놓기·메뉴 닫기보다 먼저)
	if ((Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Escape) && SortOptions != nullptr && SortOptions->IsVisible())
	{
		SetSortOptionsOpen(false);
		return FReply::Handled();
	}
	// B = 든 것 내려놓기 (메뉴 닫기보다 먼저). 든 것이 없으면 흘려보내 셸이 메뉴를 닫는다
	if (Key == EKeys::Gamepad_FaceButton_Right && Inventory.IsValid() && Inventory->GetPadHeldItem().IsSet())
	{
		Inventory->ClearPadHeldItem();
		return FReply::Handled();
	}

	const bool bMenuKey = Key == EKeys::Gamepad_FaceButton_Left;
	const int32 Index = bMenuKey || Key == EKeys::Gamepad_RightTrigger ? FindFocusedCell() : INDEX_NONE;
	if (!HasItemAt(Index))
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	// 메뉴는 칸 오른쪽에 (화면 밖이면 메뉴가 안쪽으로 당긴다)
	FVector2D CellPos = FVector2D::ZeroVector, CellSize = FVector2D::ZeroVector;
	if (const TSharedPtr<SWidget> CellWidget = Cells[Index].Button->GetCachedWidget())
	{
		NUUIGeometry::GetViewportRect(this, *CellWidget, CellPos, CellSize);
	}
	ActOnCell(Index, bMenuKey, CellPos + FVector2D(CellSize.X + 4.f, 0.f));
	return FReply::Handled();
}

void UNUCharacterInventoryPage::HandleCellClicked(int32 Index)
{
	if (HasItemAt(Index))
	{
		SelectedInstanceId = ShownSlots[Index].InstanceId;
		UpdateSelection();
	}
}

void UNUCharacterInventoryPage::HandleCategoryClicked(int32 Index)
{
	if (Category == Index)
	{
		return;
	}
	Category = Index;
	Filter = 0;
	SelectedInstanceId.Invalidate();
	if (InventoryScroll != nullptr)
	{
		InventoryScroll->ScrollToStart();
	}
	UpdateTabs();
	RefreshPage();
}

void UNUCharacterInventoryPage::HandleFilterClicked(int32 Index)
{
	if (Filter == Index)
	{
		return;
	}
	Filter = Index;
	SelectedInstanceId.Invalidate();
	if (InventoryScroll != nullptr)
	{
		InventoryScroll->ScrollToStart();
	}
	UpdateTabs();
	RefreshPage();
}

void UNUCharacterInventoryPage::HandleBattleChanged()
{
	UpdateSelection(); // 전투 중에는 [장착]/[사용]이 꺼진다
}

void UNUCharacterInventoryPage::HandleViewClicked(int32 Index)
{
	const bool bCompact = Index == 1;
	if (bCompactView == bCompact)
	{
		return;
	}
	bCompactView = bCompact;
	if (UNUGameUserSettings* Settings = UNUGameUserSettings::Get())
	{
		Settings->SetInventoryCompactView(bCompact);
		Settings->SaveSettings();
	}
	ApplyTabGroup(ViewTabs, Index);
	ApplyCellView();
	if (InventoryScroll != nullptr)
	{
		InventoryScroll->ScrollToStart();
	}
}

void UNUCharacterInventoryPage::SetSortOptionsOpen(bool bOpen, bool bMovePadFocus)
{
	if (SortOptions == nullptr || SortOptions->IsVisible() == bOpen)
	{
		return;
	}
	SortOptions->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bMovePadFocus && IsGamepadMode())
	{
		UWidget* Target = bOpen && !SortOptionTabs.Buttons.IsEmpty() ? static_cast<UWidget*>(SortOptionTabs.Buttons[0].Get()) : SortButton.Get();
		if (Target != nullptr)
		{
			Target->SetFocus();
		}
	}
}

void UNUCharacterInventoryPage::HandleSortClicked()
{
	if (SortOptions != nullptr)
	{
		SetSortOptionsOpen(!SortOptions->IsVisible());
	}
	else if (Inventory.IsValid())
	{
		Inventory->SortInventory(); // 기준 목록이 없는 WBP --- 예전처럼 바로 종류 순
	}
}

void UNUCharacterInventoryPage::HandleSortOptionClicked(int32 Index)
{
	// 고른 기준으로 바로 정렬한다 (가방 칸을 실제로 다시 놓는다). 거래 중 같은 차단 사유와 완료·중단 안내는 컴포넌트가 배너로 알린다.
	// 거절된 정렬(거래 중·앞 정렬의 응답 대기 등)에는 정렬 상자의 글자를 바꾸지 않는다
	const ENUItemSortMode Mode = static_cast<ENUItemSortMode>(Index);
	SetSortOptionsOpen(false);
	if (Inventory.IsValid() && Inventory->SortInventory(Mode) && SortLabel != nullptr)
	{
		SortLabel->SetText(GetSortLabel(Mode));
	}
}

void UNUCharacterInventoryPage::HandleActionClicked()
{
	if (const FShownSlot* Selected = FindShownItem(SelectedInstanceId))
	{
		ActivateItem(*Selected);
	}
}
