// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/UI/NUInventoryWindowWidget.h"

#include "UI/NUUISettings.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUItemCellWidget.h"
#include "Inventory/UI/NUItemContainerPanelWidget.h"
#include "Inventory/UI/NUItemUI.h"
#include "Inventory/UI/NUSplitStackDialog.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "PlayerController/NUPlayerController.h"
#include "Trade/NUTradeSubsystem.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUItemContextMenu.h"
#include "UI/NUUIGeometry.h"
#include "UI/NUUIManager.h" // NUUILayer

void UNUInventoryWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 창고 캐시 변경 통지 (인벤 변경은 베이스가 OnInventoryChanged로 구독)
	if (Inventory.IsValid())
	{
		Inventory->OnStorageChanged.RemoveDynamic(this, &UNUInventoryWindowWidget::HandleStorageChanged);
		Inventory->OnStorageChanged.AddDynamic(this, &UNUInventoryWindowWidget::HandleStorageChanged);
	}

	// 거래에 올린 칸 표시(금색)를 제안 변경에 맞춰 갱신
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->OnTradeOfferChanged.RemoveAll(this);
		Trade->OnTradeOfferChanged.AddUObject(this, &UNUInventoryWindowWidget::HandleTradeOfferViewChanged);
		Trade->OnTradeStateChanged.RemoveAll(this);
		Trade->OnTradeStateChanged.AddUObject(this, &UNUInventoryWindowWidget::HandleTradeStateViewChanged);
	}

	// 패드 집기 표시와 장치별 안내 문구
	if (Inventory.IsValid())
	{
		Inventory->OnPadHeldItemChanged.RemoveAll(this);
		Inventory->OnPadHeldItemChanged.AddUObject(this, &UNUInventoryWindowWidget::UpdateHeldHighlight);
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
		UI->OnGamepadModeChanged.AddUObject(this, &UNUInventoryWindowWidget::HandleGamepadModeChanged);
	}

	// 정렬 버튼은 창마다 타입이 다르다 --- 순정 Button(레거시), UNU_CloseButtonWidget, UNU_CommonButtonWidget
	if (UButton* SortButtonPure = Cast<UButton>(SortButton))
	{
		SortButtonPure->OnClicked.RemoveDynamic(this, &UNUInventoryWindowWidget::HandleSortClicked);
		SortButtonPure->OnClicked.AddDynamic(this, &UNUInventoryWindowWidget::HandleSortClicked);
	}
	else if (UNU_CloseButtonWidget* SortCloseButtonWidget = Cast<UNU_CloseButtonWidget>(SortButton))
	{
		SortCloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &UNUInventoryWindowWidget::HandleSortClicked);
		SortCloseButtonWidget->OnCloseClicked.AddDynamic(this, &UNUInventoryWindowWidget::HandleSortClicked);
	}
	else if (UNU_CommonButtonWidget* SortCommonButtonWidget = Cast<UNU_CommonButtonWidget>(SortButton))
	{
		SortCommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &UNUInventoryWindowWidget::HandleSortClicked);
		SortCommonButtonWidget->OnCommonClicked.AddDynamic(this, &UNUInventoryWindowWidget::HandleSortClicked);
	}
}

void UNUInventoryWindowWidget::HandleSortClicked()
{
	// 차단 사유(거래 중)와 완료·중단 안내는 컴포넌트가 배너로 알린다
	if (Inventory.IsValid())
	{
		Inventory->SortInventory();
	}
}

void UNUInventoryWindowWidget::HandleContainerSortClicked(int32 Container)
{
	// 차단 사유(거래 중·미등록 아이템)와 완료·중단 안내는 컴포넌트가 배너로 알린다
	if (Inventory.IsValid())
	{
		Inventory->SortContainer(Container);
	}
}

void UNUInventoryWindowWidget::NativeDestruct()
{
	if (Inventory.IsValid())
	{
		Inventory->OnStorageChanged.RemoveDynamic(this, &UNUInventoryWindowWidget::HandleStorageChanged);
		Inventory->OnPadHeldItemChanged.RemoveAll(this);
		Inventory->ClearPadHeldItem(); // 창이 닫히면 든 것도 없다
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
	}
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->OnTradeOfferChanged.RemoveAll(this);
		Trade->OnTradeStateChanged.RemoveAll(this);
	}
	Super::NativeDestruct(); // 드래그 비주얼 정리는 베이스가 한다
}

void UNUInventoryWindowWidget::HandleTradeOfferViewChanged()
{
	RequestRefresh();
}

void UNUInventoryWindowWidget::HandleTradeStateViewChanged(ENUTradeState NewState)
{
	RequestRefresh(); // 거래 시작/종료 시 금색 표시가 생기고 사라진다
}

UNUTradeSubsystem* UNUInventoryWindowWidget::GetTradeSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUTradeSubsystem>() : nullptr;
}

void UNUInventoryWindowWidget::ToggleTradeOffer(UNUTradeSubsystem& Trade, int32 InvSlotNo)
{
	const FNUTradeOffer& My = Trade.GetMyOffer();

	// 이미 올린 칸이면 내린다
	for (int32 Index = 0; Index < My.Slots.Num(); ++Index)
	{
		if (!My.Slots[Index].IsEmpty() && My.Slots[Index].InvSlotNo == InvSlotNo)
		{
			Trade.SetTradeItem(Index, -1);
			return;
		}
	}

	// 첫 빈 거래 칸에 올린다
	for (int32 Index = 0; Index < My.Slots.Num(); ++Index)
	{
		if (My.Slots[Index].IsEmpty())
		{
			Trade.SetTradeItem(Index, InvSlotNo);
			return;
		}
	}

	ShowNoticeMessage(NSLOCTEXT("NUInventory", "TradeSlotsFull", "거래 칸이 가득 찼습니다."));
}

void UNUInventoryWindowWidget::HandleStorageChanged(int32 Container)
{
	if (DisplayContainers.Contains(Container))
	{
		RequestRefresh();
	}
}

void UNUInventoryWindowWidget::SetDisplayContainers(const TArray<int32>& InContainers)
{
	if (InContainers.IsEmpty())
	{
		return;
	}
	DisplayContainers = InContainers;

	// 창고 창(전용 WBP_StorageWindow) --- 배치·제목·닫기 버튼은 WBP가 갖고, 여기서는 목록만 받아온다
	if (!DisplayContainers.Contains(NUContainer::Inventory))
	{
		// 정렬은 가방 전용 --- 창고 창은 패널마다 자기 정렬 버튼을 쓴다 (창고 WBP에는 창 머리 정렬 버튼이 없다)
		if (SortButton != nullptr)
		{
			SortButton->SetVisibility(ESlateVisibility::Collapsed);
		}

		// 서버에서 두 창고 목록을 받아온다 (응답이 오면 OnStorageChanged => 갱신)
		if (Inventory.IsValid())
		{
			for (const int32 Container : DisplayContainers)
			{
				Inventory->RequestContainerList(Container);
			}
		}
	}

	RefreshData();
}

void UNUInventoryWindowWidget::UseItem(FGuid InstanceId)
{
	if (Inventory.IsValid())
	{
		Inventory->ServerUseItem(InstanceId);
	}
}

void UNUInventoryWindowWidget::EquipItem(FGuid InstanceId)
{
	EquipItemChecked(InstanceId);
}

void UNUInventoryWindowWidget::DropItem(FGuid InstanceId)
{
	NUItemUI::RequestDrop(GetOwningPlayer(), InstanceId, ConfirmDialogClass);
}

void UNUInventoryWindowWidget::RefreshData()
{
	if (!ResolveComponents())
	{
		return;
	}

	bInBattle = Inventory->IsOwnerInBattle();

	EntriesByContainer.Reset();
	for (const int32 Container : DisplayContainers)
	{
		TArray<FNUSheetItemEntry>& Entries = EntriesByContainer.Add(Container);
		const TArray<FNUItemInstance>* Items = Inventory->GetContainerItems(Container);
		if (Items == nullptr)
		{
			continue;
		}
		const bool bIsInventory = Container == NUContainer::Inventory;
		for (const FNUItemInstance& Item : *Items)
		{
			const FNUItemDefRow* Def = Inventory->FindItemDef(Item.ItemRowName);
			FNUSheetItemEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.InstanceId = Item.InstanceId;
			Entry.SlotNo = Item.SlotNo;
			Entry.ItemRowName = Item.ItemRowName;
			Entry.DisplayName = Def ? Def->DisplayName : FText::FromName(Item.ItemRowName);
			Entry.Count = Item.Count;
			// 창고는 이동만 — 사용/장착/버리기는 인벤에서만
			Entry.bEquippable = bIsInventory && !bInBattle && Def != nullptr
				&& Def->ItemType == ENUItemType::Equipment && Def->EquipType != ENUEquipType::None;
			Entry.bUsable = bIsInventory && !bInBattle && Def != nullptr
				&& Def->ItemType == ENUItemType::Consumable && Def->bUsableInField;
			Entry.bDroppable = bIsInventory && !bInBattle && (Def == nullptr || NUItem::IsDroppable(Def->ItemType));
		}
	}

	// 레거시/BIE용 미러 — 첫 패널 기준 (인벤 창이면 곧 가방)
	const int32 FirstContainer = DisplayContainers[0];
	InventoryItems = EntriesByContainer.FindChecked(FirstContainer);
	MaxSlots = Inventory->GetContainerSlotCount(FirstContainer);
	UsedSlots = InventoryItems.Num();

	PopulateItemList();

	// 골드 줄은 소지품 창에만 --- 창고 창은 같은 WBP를 재사용하는데 크기 계산이 달라 자리가 안 맞는다
	const bool bShowGoldRow = DisplayContainers.Num() == 1 && DisplayContainers[0] == NUContainer::Inventory;
	if (GoldRow != nullptr)
	{
		GoldRow->SetVisibility(bShowGoldRow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	UpdateGoldText();

	OnInventoryListChanged();
}

void UNUInventoryWindowWidget::PopulateItemList()
{
	// 드래그가 끝났는데 담개가 화면에 남아 있으면 여기서 치운다.
	// 드롭을 다른 창이 삼키거나 종료 델리게이트가 유실되면 옮기기 전 자리에 아이콘이 남는다
	if (!FSlateApplication::Get().IsDragDropping())
	{
		ClearDragVisual();
	}

	// 다시 지으면 칸 위젯이 새것이라 포커스(패드)가 사라진다 --- 어느 칸이었는지 기억했다 되돌린다 (다른 창의 포커스는 건드리지 않는다)
	HideFocusTooltip();
	const UNUItemCellWidget* FocusedCell = FindFocusedCell();
	const FIntPoint FocusedCoords = FocusedCell != nullptr ? FIntPoint(FocusedCell->GetContainer(), FocusedCell->GetSlotNo()) : FIntPoint(INDEX_NONE, INDEX_NONE);

	// 다시 지으면 스크롤이 0으로 튄다 --- 위치를 기억했다 되살린다 (아이템 옮길 때마다 내용이 튀는 원인)
	UScrollBox* ScrollPanel = Cast<UScrollBox>(FindPanelByName(TEXT("ItemListBox")));
	const float SavedScrollOffset = ScrollPanel != nullptr ? ScrollPanel->GetScrollOffset() : 0.f;

	PanelRow->ClearChildren();
	CellByCoord.Reset();

	if (PanelWidgetClass == nullptr || GetDefault<UNUUISettings>()->ItemCellClass.IsNull())
	{
		UE_LOG(LogNUInventory, Warning, TEXT("PanelWidgetClass(WBP_InventoryWindow 기본값) 또는 ItemCellClass(프로젝트 세팅 NU UI) 미지정 --- 저장소를 그릴 수 없습니다"));
		return;
	}

	// 저장소 패널들 --- 인벤 창은 1개, 창고 창은 나란히 2개 (좌 계정 | 우 캐릭터)
	for (int32 Index = 0; Index < DisplayContainers.Num(); ++Index)
	{
		UPanelSlot* PanelSlot = PanelRow->AddChild(BuildContainerPanel(DisplayContainers[Index]));
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			BoxSlot->SetPadding(FMargin(Index > 0 ? 16.f : 0.f, 0.f, 0.f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Top);
		}
	}

	UpdateHintText();
	UpdateHeldHighlight();

	if (ScrollPanel != nullptr)
	{
		ScrollPanel->SetScrollOffset(SavedScrollOffset);
	}

	ResizeWindowToContent();

	if (FocusedCoords.Y != INDEX_NONE)
	{
		const TWeakObjectPtr<UNUItemCellWidget>* Same = CellByCoord.Find(FocusedCoords);
		if (Same != nullptr && Same->IsValid())
		{
			(*Same)->SetFocus();
		}
		else
		{
			FocusForPad();
		}
	}
}

void UNUInventoryWindowWidget::UpdateHintText()
{
	if (HintText == nullptr)
	{
		return;
	}
	const UNUTradeSubsystem* Trade = GetTradeSubsystem();
	const bool bTrading = Trade != nullptr && Trade->GetState() == ENUTradeState::Active;
	const bool bShopOpen = Inventory.IsValid() && Inventory->GetOpenShopId() > 0;
	const bool bStorageOpen = Inventory.IsValid() && Inventory->IsStorageOpen();
	const bool bIsStorageWindow = !DisplayContainers.Contains(NUContainer::Inventory);
	FText Hint;
	if (IsGamepadMode())
	{
		Hint = bTrading
			? NSLOCTEXT("NUInventory", "TradeHintPad", "RT: 거래에 올리기/내리기 (흐린 칸 = 올린 칸) · A로 집어 거래 칸에 놓기 · X: 아이템 메뉴")
			: bShopOpen
				? NSLOCTEXT("NUInventory", "ShopHintPad", "RT: 판매 · A: 집기/놓기 · X: 아이템 메뉴")
				: bIsStorageWindow
					? NSLOCTEXT("NUInventory", "StorageHintPad", "A: 집기/놓기 · RT: 소지품으로 · X: 아이템 메뉴 · LB/RB: 창 넘기기")
					: bStorageOpen
						? NSLOCTEXT("NUInventory", "GridHintPadStorage", "A: 집기/놓기 · RT: 캐릭터 창고로 (LT+RT: 계정 창고로) · X: 아이템 메뉴 · LB/RB: 창 넘기기")
						: NSLOCTEXT("NUInventory", "GridHintPad", "A: 집기/놓기 · RT: 사용/장착 · X: 아이템 메뉴 · LB/RB: 창 넘기기");
	}
	else
	{
		Hint = bTrading
			? NSLOCTEXT("NUInventory", "TradeHint", "우클릭: 거래에 올리기/내리기 (흐린 칸 = 올린 칸) · 거래 창에 드래그해도 올라간다")
			: bShopOpen
				? NSLOCTEXT("NUInventory", "ShopHint", "우클릭: 판매 · 드래그: 칸 이동/교환 · Ctrl+우클릭: 메뉴")
				: bIsStorageWindow
					? NSLOCTEXT("NUInventory", "StorageHint", "드래그로 옮기기 · 우클릭: 소지품으로 · Ctrl+우클릭: 메뉴")
					: NSLOCTEXT("NUInventory", "GridHint", "드래그: 칸 이동/교환 (창 밖에 놓으면 버리기) · 우클릭: 사용/장착 (창고 열림: 캐릭터 창고로, Alt+우클릭: 계정 창고로) · Ctrl+우클릭: 메뉴");
	}
	HintText->SetText(Hint);
}

void UNUInventoryWindowWidget::UpdateHeldHighlight()
{
	FNUPadHeldItem Held;
	if (Inventory.IsValid())
	{
		Held = Inventory->GetPadHeldItem();
	}
	for (const TPair<FIntPoint, TWeakObjectPtr<UNUItemCellWidget>>& Pair : CellByCoord)
	{
		if (UNUItemCellWidget* Cell = Pair.Value.Get())
		{
			Cell->SetHeld(Held.IsSet() && Pair.Key == FIntPoint(Held.Container, Held.SlotNo));
		}
	}
}

void UNUInventoryWindowWidget::HandleGamepadModeChanged(bool bGamepad)
{
	UpdateHintText();
	if (!bGamepad)
	{
		// 마우스로 돌아오면 집은 것과 포커스 툴팁은 뜻이 없다 (드래그·호버가 대신한다)
		HideFocusTooltip();
		if (Inventory.IsValid())
		{
			Inventory->ClearPadHeldItem();
		}
	}
}

void UNUInventoryWindowWidget::FocusForPad()
{
	for (const int32 Container : DisplayContainers)
	{
		const TWeakObjectPtr<UNUItemCellWidget>* First = CellByCoord.Find(FIntPoint(Container, 0));
		if (First != nullptr && First->IsValid())
		{
			(*First)->SetFocus();
			return;
		}
	}
	Super::FocusForPad();
}

FName UNUInventoryWindowWidget::GetCellItemRowName(int32 Container, int32 SlotNo) const
{
	const FNUSheetItemEntry* Entry = FindEntry(Container, SlotNo);
	return Entry != nullptr ? Entry->ItemRowName : NAME_None;
}

void UNUInventoryWindowWidget::ResizeWindowToContent()
{
	// 창고 창은 SetDisplayContainers가 크기를 따로 잡는다 (패널 2개가 나란히 들어간다)
	if (DisplayContainers.Num() != 1 || !Inventory.IsValid())
	{
		return;
	}

	// 칸 수가 그대로면 다시 잡지 않는다. 가방에 분류 구간이 있으면 구간 하나(패널 하나)가 보이는 높이로 잡고 나머지는 스크롤한다
	const FNUBagSection* Section = DisplayContainers[0] == NUContainer::Inventory ? Inventory->GetBagSection(NUBagCategory::Equipment) : nullptr;
	const int32 TotalSlots = Section != nullptr ? Section->Count : Inventory->GetContainerSlotCount(DisplayContainers[0]);
	if (TotalSlots <= 0 || TotalSlots == LastLayoutSlotCount)
	{
		return;
	}

	UPanelWidget* ListPanel = FindPanelByName(TEXT("ItemListBox"));
	UCanvasPanelSlot* ListSlot = ListPanel != nullptr ? Cast<UCanvasPanelSlot>(ListPanel->Slot) : nullptr;
	UCanvasPanelSlot* BackgroundSlot = Cast<UCanvasPanelSlot>(WindowBG->Slot);
	if (ListSlot == nullptr || BackgroundSlot == nullptr)
	{
		return; // 캔버스에 놓인 WBP가 아니면 손대지 않는다
	}

	// 내용을 실측하지 않고 칸 수로 계산한다.
	// 재서 딱 맞추면 안내 문구가 한 줄과 두 줄을 오갈 때 스크롤 막대가 생겼다 사라지며 창이 떨린다
	const int32 Rows = FMath::DivideAndRoundUp(TotalSlots, FMath::Max(1, GridColumns));
	const float HeaderHeight = 32.f;                       // 머리글 "가방 14 / 30"
	const float HintHeight = 56.f;                         // 하단 조작 안내 (두 줄까지 여유)
	const float GridHeight = Rows * (CellSize + 4.f) + 8.f;
	const float ListHeight = HeaderHeight + GridHeight + HintHeight;

	const FVector2D ListSize = ListSlot->GetSize();
	const FVector2D BackgroundSize = BackgroundSlot->GetSize();
	const float TopMargin = ListSlot->GetPosition().Y - BackgroundSlot->GetPosition().Y; // 제목·정렬 버튼 자리
	const float GoldRowHeight = 30.f; // 하단 골드 줄 (동전 22px + 위아래 여백)

	ListSlot->SetSize(FVector2D(ListSize.X, ListHeight));
	BackgroundSlot->SetSize(FVector2D(BackgroundSize.X, TopMargin + ListHeight + GoldRowHeight + 8.f));

	// 골드 줄은 안내 문구 바로 아래 --- 창 높이가 칸 수 따라 변하므로 여기서 같이 옮긴다
	// (WBP의 고정 좌표는 조정된 창 밖으로 밀린다)
	const float GoldY = ListSlot->GetPosition().Y + ListHeight + 2.f;
	if (UCanvasPanelSlot* GoldSlot = GoldRow != nullptr ? Cast<UCanvasPanelSlot>(GoldRow->Slot) : nullptr)
	{
		GoldSlot->SetPosition(FVector2D(GoldSlot->GetPosition().X, GoldY));
	}
	LastLayoutSlotCount = TotalSlots;
}

UWidget* UNUInventoryWindowWidget::BuildContainerPanel(int32 Container)
{
	// 가방에 분류 구간이 있으면 구간마다 패널 하나를 세로로 쌓는다 (머리글 "장비 11 / 30") --- 통합 메뉴의 분류 탭과 같은 순서
	if (Container == NUContainer::Inventory && Inventory.IsValid() && Inventory->GetBagSection(NUBagCategory::Equipment) != nullptr)
	{
		const FText SectionNames[NUBagCategory::Num] = {
			NSLOCTEXT("NUInventory", "BagEquipment", "장비"), NSLOCTEXT("NUInventory", "BagConsumable", "소모품"),
			NSLOCTEXT("NUInventory", "BagMaterial", "재료"), NSLOCTEXT("NUInventory", "BagOther", "기타") };
		UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
		for (int32 Category = 0; Category < NUBagCategory::Num; ++Category)
		{
			const FNUBagSection* Section = Inventory->GetBagSection(Category);
			if (UVerticalBoxSlot* StackSlot = Stack->AddChildToVerticalBox(BuildSlotPanel(Container, SectionNames[Category], Section->Start, Section->Count)))
			{
				StackSlot->SetPadding(FMargin(0.f, Category > 0 ? 8.f : 0.f, 0.f, 0.f));
			}
		}
		return Stack;
	}

	const FText ContainerName = Container == NUContainer::Inventory
		? NSLOCTEXT("NUInventory", "Bag", "가방")
		: Container == NUContainer::CharStorage
			? NSLOCTEXT("NUInventory", "CharStorage", "캐릭터 창고")
			: NSLOCTEXT("NUInventory", "AccountStorage", "계정 창고");

	// 창고인데 아직 목록을 못 받았으면 (미로그인 포함) 0칸으로 넘겨 안내만 띄운다
	const bool bLoaded = Inventory.IsValid() && Inventory->IsContainerLoaded(Container);
	return BuildSlotPanel(Container, ContainerName, 0, bLoaded ? Inventory->GetContainerSlotCount(Container) : 0);
}

UNUItemContainerPanelWidget* UNUInventoryWindowWidget::BuildSlotPanel(int32 Container, const FText& PanelName, int32 FirstSlotNo, int32 SlotCount)
{
	UNUItemContainerPanelWidget* Panel = CreateWidget<UNUItemContainerPanelWidget>(this, PanelWidgetClass);
	Panel->SetContainer(Container);
	// 패널 폭을 칸 수로 못 박는다 --- 머리글 숫자 폭이나 로딩 문구 길이에 폭이 흔들리면 가운데 정렬인 창이 흔들려 보인다
	Panel->SetPanelWidth(FMath::Max(1, GridColumns) * (CellSize + 4.f) + 8.f);

	if (SlotCount <= 0)
	{
		Panel->SetLoading(PanelName);
		return Panel;
	}

	// 저장소별 정렬 버튼은 창고 창에만 --- 가방 창은 창 머리의 정렬 버튼 하나로 충분하다
	Panel->SetSortButtonVisible(!DisplayContainers.Contains(NUContainer::Inventory));
	Panel->OnSortClicked.BindUObject(this, &UNUInventoryWindowWidget::HandleContainerSortClicked);

	// 칸 그리드 --- FirstSlotNo부터 SlotCount칸 전부 (빈 칸 포함)
	int32 UsedCount = 0;
	for (int32 Offset = 0; Offset < SlotCount; ++Offset)
	{
		const FNUSheetItemEntry* Entry = FindEntry(Container, FirstSlotNo + Offset);
		UsedCount += Entry != nullptr ? 1 : 0;
		if (UNUItemCellWidget* Cell = BuildGridCell(Container, FirstSlotNo + Offset, Entry))
		{
			Panel->AddCell(Cell, Offset, GridColumns);
		}
	}
	Panel->SetHeader(PanelName, UsedCount, SlotCount);
	return Panel;
}

UNUItemCellWidget* UNUInventoryWindowWidget::BuildGridCell(int32 Container, int32 SlotNo, const FNUSheetItemEntry* Entry)
{
	UNUItemCellWidget* Cell = MakeItemCell(CellSize);
	if (Cell == nullptr)
	{
		return nullptr;
	}
	Cell->SetCoords(Container, SlotNo);
	Cell->OnCellMouseDown.BindUObject(this, &UNUInventoryWindowWidget::HandleCellMouseDown);
	CellByCoord.Add(FIntPoint(Container, SlotNo), Cell);

	if (Entry == nullptr)
	{
		Cell->SetEmpty(FText::GetEmpty());
		return Cell;
	}

	// 거래에 올린 칸인지 --- 확정 전까지 아이템은 인벤에 남아 있어 표시로만 구분한다
	bool bOffered = false;
	if (Container == NUContainer::Inventory)
	{
		const UNUTradeSubsystem* Trade = GetTradeSubsystem();
		bOffered = Trade != nullptr && Trade->IsInventorySlotOffered(SlotNo);
	}
	// 아이콘이 없는 아이템은 이름이 아이콘 노릇 --- 어느 쪽이든 툴팁이 실제 정보원
	Cell->SetItem(LoadItemIconTexture(Entry->ItemRowName), Entry->Count, Entry->DisplayName, bOffered);
	if (UWidget* Tooltip = BuildItemTooltip(Entry->ItemRowName))
	{
		Cell->SetToolTip(Tooltip);
	}
	return Cell;
}

const FNUSheetItemEntry* UNUInventoryWindowWidget::FindEntry(int32 Container, int32 SlotNo) const
{
	if (SlotNo == INDEX_NONE)
	{
		return nullptr;
	}
	const TArray<FNUSheetItemEntry>* Entries = EntriesByContainer.Find(Container);
	if (Entries == nullptr)
	{
		return nullptr;
	}
	return Entries->FindByPredicate(
		[SlotNo](const FNUSheetItemEntry& Entry) { return Entry.SlotNo == SlotNo; });
}

void UNUInventoryWindowWidget::ShowSplitPrompt(int32 Container, int32 SlotNo, int32 StackCount)
{
	// 전부 떼는 건 나누기가 아니라 이동이라 상한은 하나 적다
	const int32 MaxSplit = StackCount - 1;
	if (MaxSplit < 1 || !Inventory.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UNUInventoryComponent> WeakInventory = Inventory;
	FOnNUSplitConfirmed OnConfirmed;
	OnConfirmed.BindLambda([WeakInventory, Container, SlotNo](int32 Count)
	{
		if (UNUInventoryComponent* Inv = WeakInventory.Get())
		{
			Inv->SplitStack(Container, SlotNo, Count);
		}
	});

	UNUSplitStackDialog::Show(this, MaxSplit, OnConfirmed);
}

void UNUInventoryWindowWidget::QuickMove(int32 FromContainer, int32 FromSlotNo, int32 ToContainer)
{
	if (!Inventory.IsValid())
	{
		return;
	}
	const int32 FreeSlot = Inventory->FindFreeSlotInContainer(ToContainer, GetCellItemRowName(FromContainer, FromSlotNo));
	if (FreeSlot == INDEX_NONE)
	{
		UE_LOG(LogNUInventory, Log, TEXT("빠른 이동 실패 — 저장소 %d가 가득이거나 미로드"), ToContainer);
		ShowNoticeMessage(ToContainer == NUContainer::Inventory
			? NSLOCTEXT("NUInventory", "BagFull", "가방이 가득 찼습니다.")
			: NSLOCTEXT("NUInventory", "StorageFull", "창고가 가득 찼습니다."));
		return;
	}
	Inventory->ServerMoveItemBetween(FromContainer, FromSlotNo, ToContainer, FreeSlot);
}

FReply UNUInventoryWindowWidget::HandleCellMouseDown(int32 Container, int32 SlotNo, const FPointerEvent& MouseEvent)
{
	const FNUSheetItemEntry* Entry = FindEntry(Container, SlotNo);

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Entry != nullptr)
	{
		// Ctrl + 우클릭 = 아이템 메뉴 (패드 X와 같은 목록). 우클릭 단독은 상황별 빠른 동작
		if (MouseEvent.IsControlDown())
		{
			FVector2D PixelPos, ViewportPos;
			USlateBlueprintLibrary::AbsoluteToViewport(this, MouseEvent.GetScreenSpacePosition(), PixelPos, ViewportPos);
			OpenItemMenu(Container, SlotNo, ViewportPos);
		}
		else
		{
			QuickAction(Container, SlotNo, MouseEvent.IsAltDown());
		}
		return FReply::Handled();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Entry != nullptr)
	{
		// Shift + 좌클릭 = 스택 나누기 (드래그보다 먼저 본다) — 몇 개를 뗄지 물어본다
		if (MouseEvent.IsShiftDown() && Entry->Count > 1 && Inventory.IsValid())
		{
			ShowSplitPrompt(Container, SlotNo, Entry->Count);
			return FReply::Handled();
		}

		// 드래그 후보 등록 — 실제 시작(OnDragDetected)은 커서가 움직여야 온다
		PendingDragContainer = Container;
		PendingDragSlotNo = SlotNo;
		return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return FReply::Handled(); // 빈 칸/기타 버튼 — 창 드래그로 번지지 않게만 소비
}

void UNUInventoryWindowWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (Inventory.IsValid())
	{
		Inventory->ClearPadHeldItem(); // 마우스로 끌기 시작하면 패드로 집은 것은 버린다
	}

	const FNUSheetItemEntry* Entry = FindEntry(PendingDragContainer, PendingDragSlotNo);
	if (Entry == nullptr)
	{
		return;
	}

	UNUItemDragOperation* Operation = NewObject<UNUItemDragOperation>(this);
	Operation->SourceContainer = PendingDragContainer;
	Operation->SourceSlotNo = PendingDragSlotNo;
	// 드롭을 다른 창(인벤<=>창고)이 받으면 이 창의 NativeOnDrop이 안 불린다 —
	// 어느 쪽에서 끝나든 비주얼이 정리되게 오퍼레이션 종료 델리게이트에 건다
	Operation->OnDrop.AddDynamic(this, &UNUInventoryWindowWidget::HandleDragEnded);
	Operation->OnDragCancelled.AddDynamic(this, &UNUInventoryWindowWidget::HandleDragEnded);
	OutOperation = Operation;

	// 커서를 따라가는 담개 --- 칸과 같은 WBP로 만들어 아이템이 끌려오는 느낌을 준다 (거래 표시 없이 선명하게)
	if (UNUItemCellWidget* Visual = MakeItemCell(CellSize))
	{
		Visual->SetItem(LoadItemIconTexture(Entry->ItemRowName), Entry->Count, Entry->DisplayName, false);
		ShowDragVisual(Visual, CellSize);
	}
}

bool UNUInventoryWindowWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UNUItemDragOperation* Operation = Cast<UNUItemDragOperation>(InOperation);
	if (Operation == nullptr)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	PendingDragSlotNo = INDEX_NONE;
	ClearDragVisual();

	// 거래 칸에서 끌어온 것을 인벤에 놓으면 = 내리기 (칸 지정은 서버가 원래 자리로 돌려준다)
	if (Operation->bFromTradeOffer)
	{
		if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
		{
			Trade->SetTradeItem(Operation->SourceSlotNo, -1);
		}
		return true;
	}

	const FVector2D ScreenPos = InDragDropEvent.GetScreenSpacePosition();

	// 스크롤로 잘려 안 보이는 칸은 대상이 아니다 --- 칸의 자리는 잘리기 전 그대로라 제목·골드 줄 밑에 깔린 칸이 잡힌다
	const UPanelWidget* ListPanel = FindPanelByName(TEXT("ItemListBox"));
	if (ListPanel != nullptr && !ListPanel->GetCachedGeometry().IsUnderLocation(ScreenPos))
	{
		return true;
	}

	// 셀 위 드롭 => 이동/합치기/교환 (출발이 다른 창/패널이어도 된다 — 인벤<=>창고, 계정<=>캐릭터)
	for (const TPair<FIntPoint, TWeakObjectPtr<UNUItemCellWidget>>& Pair : CellByCoord)
	{
		const UNUItemCellWidget* Cell = Pair.Value.Get();
		if (Cell != nullptr && Cell->GetCachedGeometry().IsUnderLocation(ScreenPos))
		{
			const int32 TargetContainer = Pair.Key.X;
			const int32 TargetSlotNo = Pair.Key.Y;
			const bool bSameSlot = Operation->SourceContainer == TargetContainer && Operation->SourceSlotNo == TargetSlotNo;
			if (!bSameSlot && Inventory.IsValid())
			{
				Inventory->ServerMoveItemBetween(Operation->SourceContainer, Operation->SourceSlotNo, TargetContainer, TargetSlotNo);
			}
			return true;
		}
	}

	return true; // 창 안 기타 영역(제목·여백) — 아무 일도 없음
}

void UNUInventoryWindowWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	PendingDragSlotNo = INDEX_NONE;
	ClearDragVisual();

	// 인벤 창 밖(아무 위젯도 안 받는 곳)에 놓으면 버리기 — 확인 창이 최종 관문.
	// 창고 발 드래그는 버리기 없음 (버리기 서버 패킷이 아직 없어 인벤 로컬 흐름만 지원)
	if (const UNUItemDragOperation* Operation = Cast<UNUItemDragOperation>(InOperation))
	{
		if (Operation->SourceContainer == NUContainer::Inventory)
		{
			const FNUSheetItemEntry* Entry = FindEntry(NUContainer::Inventory, Operation->SourceSlotNo);
			if (Entry != nullptr && Entry->bDroppable)
			{
				DropItem(Entry->InstanceId);
			}
		}
	}
}

void UNUInventoryWindowWidget::HandleDragEnded(UDragDropOperation* InOperation)
{
	PendingDragSlotNo = INDEX_NONE;
	ClearDragVisual();
}

void UNUInventoryWindowWidget::CloseSelf()
{
	if (!Inventory.IsValid())
	{
		return;
	}
	if (DisplayContainers.Contains(NUContainer::Inventory))
	{
		Inventory->ToggleInventoryUI();
	}
	else
	{
		Inventory->ToggleStorageUI(); // 열림 상태 토글 = 닫기
	}
}

FReply UNUInventoryWindowWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (const UNUItemCellWidget* Cell = FindFocusedCell())
	{
		const int32 Container = Cell->GetContainer();
		const int32 SlotNo = Cell->GetSlotNo();
		if (Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			PadPickOrPlace(Container, SlotNo);
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_FaceButton_Left)
		{
			// 메뉴는 칸 오른쪽에 (화면 밖이면 메뉴가 안쪽으로 당긴다)
			FVector2D FocusPos = FVector2D::ZeroVector, FocusSize = FVector2D::ZeroVector;
			if (Cell->GetCachedWidget().IsValid())
			{
				NUUIGeometry::GetViewportRect(this, *Cell->GetCachedWidget(), FocusPos, FocusSize);
			}
			OpenItemMenu(Container, SlotNo, FocusPos + FVector2D(FocusSize.X + 4.f, 0.f));
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_RightTrigger)
		{
			const APlayerController* PC = GetOwningPlayer();
			QuickAction(Container, SlotNo, PC != nullptr && PC->IsInputKeyDown(EKeys::Gamepad_LeftTrigger));
			return FReply::Handled();
		}
	}
	// B = 든 것 내려놓기 (창 닫기보다 먼저). 든 것이 없으면 흘려보내 PC가 창을 닫는다
	if (Key == EKeys::Gamepad_FaceButton_Right && Inventory.IsValid() && Inventory->GetPadHeldItem().IsSet())
	{
		Inventory->ClearPadHeldItem();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNUInventoryWindowWidget::QuickAction(int32 Container, int32 SlotNo, bool bAlt)
{
	const FNUSheetItemEntry* Entry = FindEntry(Container, SlotNo);
	if (Entry == nullptr || !Inventory.IsValid())
	{
		return;
	}
	if (Container != NUContainer::Inventory)
	{
		QuickMove(Container, SlotNo, NUContainer::Inventory); // 창고 칸 = 소지품으로
		return;
	}
	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade != nullptr && Trade->GetState() == ENUTradeState::Active)
	{
		ToggleTradeOffer(*Trade, SlotNo); // 거래 중 = 거래 칸 올리기/내리기 (사용/장착보다 우선)
	}
	else if (Inventory->GetOpenShopId() > 0)
	{
		OpenSellCountDialog(Entry->InstanceId, Inventory->GetOpenShopId()); // 상점이 열려 있는 동안 = 판매
	}
	else if (Inventory->IsStorageOpen())
	{
		QuickMove(Container, SlotNo, bAlt ? NUContainer::AccountStorage : NUContainer::CharStorage); // 창고 열림 = 캐릭터 창고로, Alt/LT = 계정 창고로
	}
	else if (Entry->bUsable)
	{
		UseItem(Entry->InstanceId); // 성공 시 복제 갱신이 RefreshData를 부른다
	}
	else if (Entry->bEquippable)
	{
		EquipItem(Entry->InstanceId);
	}
}

void UNUInventoryWindowWidget::PadPickOrPlace(int32 Container, int32 SlotNo)
{
	if (!Inventory.IsValid())
	{
		return;
	}
	const FNUPadHeldItem Held = Inventory->GetPadHeldItem();
	if (!Held.IsSet())
	{
		if (const FNUSheetItemEntry* Entry = FindEntry(Container, SlotNo))
		{
			Inventory->SetPadHeldItem(Container, SlotNo, Entry->InstanceId);
		}
		return;
	}
	if (Held.Container == NUContainer::TradeOffer)
	{
		// 거래 창에서 집어 온 칸 = 거래에서 내리기 (거래 칸을 끌어 소지품에 놓기와 같다)
		if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
		{
			Trade->SetTradeItem(Held.SlotNo, -1);
		}
		Inventory->ClearPadHeldItem();
		return;
	}
	if (Held.Container != Container || Held.SlotNo != SlotNo)
	{
		Inventory->ServerMoveItemBetween(Held.Container, Held.SlotNo, Container, SlotNo); // 빈칸 = 이동, 같은 아이템 = 합치기, 그 밖의 찬칸 = 교환 (드롭과 같다)
	}
	Inventory->ClearPadHeldItem();
}

void UNUInventoryWindowWidget::OpenItemMenu(int32 Container, int32 SlotNo, const FVector2D& ScreenPos)
{
	const FNUSheetItemEntry* Entry = FindEntry(Container, SlotNo);
	if (Entry == nullptr || !Inventory.IsValid())
	{
		return;
	}
	const FGuid InstanceId = Entry->InstanceId;
	TArray<FNUItemMenuEntry> Entries;
	const auto Add = [&Entries](const FText& Label, FSimpleDelegate Action)
	{
		Entries.Add({ Label, MoveTemp(Action) });
	};
	const auto MoveTo = [this, Container, SlotNo](int32 ToContainer)
	{
		return FSimpleDelegate::CreateWeakLambda(this, [this, Container, SlotNo, ToContainer]() { QuickMove(Container, SlotNo, ToContainer); });
	};

	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Container != NUContainer::Inventory)
	{
		Add(NSLOCTEXT("NUInventory", "MenuToBag", "소지품으로"), MoveTo(NUContainer::Inventory));
	}
	else if (Trade != nullptr && Trade->GetState() == ENUTradeState::Active)
	{
		Add(Trade->IsInventorySlotOffered(SlotNo)
				? NSLOCTEXT("NUInventory", "MenuTradeRemove", "거래에서 내리기")
				: NSLOCTEXT("NUInventory", "MenuTradeAdd", "거래에 올리기"),
			FSimpleDelegate::CreateWeakLambda(this, [this, SlotNo]()
			{
				if (UNUTradeSubsystem* Inner = GetTradeSubsystem())
				{
					ToggleTradeOffer(*Inner, SlotNo);
				}
			}));
	}
	else
	{
		if (Inventory->GetOpenShopId() > 0)
		{
			Add(NSLOCTEXT("NUInventory", "MenuSell", "판매"), FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId]()
			{
				if (Inventory.IsValid())
				{
					OpenSellCountDialog(InstanceId, Inventory->GetOpenShopId());
				}
			}));
		}
		if (Inventory->IsStorageOpen())
		{
			Add(NSLOCTEXT("NUInventory", "MenuToCharStorage", "캐릭터 창고로"), MoveTo(NUContainer::CharStorage));
			Add(NSLOCTEXT("NUInventory", "MenuToAccountStorage", "계정 창고로"), MoveTo(NUContainer::AccountStorage));
		}
		if (Entry->bUsable)
		{
			Add(NSLOCTEXT("NUInventory", "MenuUse", "사용"), FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId]() { UseItem(InstanceId); }));
		}
		if (Entry->bEquippable)
		{
			Add(NSLOCTEXT("NUInventory", "MenuEquip", "장착"), FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId]() { EquipItem(InstanceId); }));
		}
	}
	if (Entry->Count > 1)
	{
		Add(NSLOCTEXT("NUInventory", "MenuSplit", "나누기"), FSimpleDelegate::CreateWeakLambda(this, [this, Container, SlotNo]()
		{
			if (const FNUSheetItemEntry* Current = FindEntry(Container, SlotNo))
			{
				ShowSplitPrompt(Container, SlotNo, Current->Count);
			}
		}));
	}
	if (Entry->bDroppable)
	{
		Add(NSLOCTEXT("NUInventory", "MenuDrop", "버리기"), FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId]() { DropItem(InstanceId); }));
	}
	UNUItemContextMenu::Show(this, MoveTemp(Entries), ScreenPos);
}
