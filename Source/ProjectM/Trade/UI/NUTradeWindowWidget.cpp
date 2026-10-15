#include "Trade/UI/NUTradeWindowWidget.h"

#include "UI/NUUISettings.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUInventoryWindowWidget.h" // UNUItemDragOperation
#include "Inventory/UI/NUItemCellWidget.h"
#include "Trade/NUTradeSubsystem.h"
#include "Trade/UI/NUTradeUIController.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUItemContextMenu.h"
#include "UI/NUUIGeometry.h"
#include "UI/NUUIManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUTradeWindow, Log, All);

namespace
{
	// 상대 제안 칸 좌표의 저장소 자리 --- 툴팁이 어느 쪽 제안인지 가르는 데만 쓴다 (내 칸은 NUContainer::TradeOffer)
	constexpr int32 PartnerOfferContainer = -2;

	// 명명 계약: 순정 Button(레거시)과 UNU_CommonButtonWidget(신규 공용 부품) 둘 다 지원해야 해서
	// BindWidget 타입을 UWidget으로 열어두고 여기서 캐스팅해 알맞은 델리게이트/함수로 다룬다 (다른 창들과 동일 패턴)
	void BindTradeButtonClicked(UWidget* Widget, UObject* Target, FName FunctionName)
	{
		FScriptDelegate Handler;
		Handler.BindUFunction(Target, FunctionName);
		if (UButton* AsButton = Cast<UButton>(Widget))
		{
			AsButton->OnClicked.AddUnique(Handler);
		}
		else if (UNU_CommonButtonWidget* AsCommon = Cast<UNU_CommonButtonWidget>(Widget))
		{
			AsCommon->OnCommonClicked.AddUnique(Handler);
		}
	}

	void UnbindTradeButtonClicked(UWidget* Widget, UObject* Target, FName FunctionName)
	{
		FScriptDelegate Handler;
		Handler.BindUFunction(Target, FunctionName);
		if (UButton* AsButton = Cast<UButton>(Widget))
		{
			AsButton->OnClicked.Remove(Handler);
		}
		else if (UNU_CommonButtonWidget* AsCommon = Cast<UNU_CommonButtonWidget>(Widget))
		{
			AsCommon->OnCommonClicked.Remove(Handler);
		}
	}

	// LockLabel/ConfirmLabel은 순정 Button 시절엔 버튼의 자식 TextBlock을 이름으로 찾아 갱신했다.
	// UNU_CommonButtonWidget은 그 자식이 내부 WBP 트리 안에 있어 이름으로 안 닿으니 SetLabelText로 대신한다
	void SetTradeButtonLabel(UWidget* Widget, const FText& InText)
	{
		if (UNU_CommonButtonWidget* AsCommon = Cast<UNU_CommonButtonWidget>(Widget))
		{
			AsCommon->SetLabelText(InText);
		}
	}
}

void UNUTradeWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->OnTradeOfferChanged.AddUObject(this, &UNUTradeWindowWidget::HandleOfferChanged);
	}

	// 재오픈 중복 구독 방지 --- 해제 선행 후 구독
	UnbindTradeButtonClicked(LockButton, this, GET_FUNCTION_NAME_CHECKED(UNUTradeWindowWidget, HandleLockClicked));
	BindTradeButtonClicked(LockButton, this, GET_FUNCTION_NAME_CHECKED(UNUTradeWindowWidget, HandleLockClicked));
	UnbindTradeButtonClicked(ConfirmButton, this, GET_FUNCTION_NAME_CHECKED(UNUTradeWindowWidget, HandleConfirmClicked));
	BindTradeButtonClicked(ConfirmButton, this, GET_FUNCTION_NAME_CHECKED(UNUTradeWindowWidget, HandleConfirmClicked));

	// 패드 --- 집은 거래 칸 표시, 마우스로 돌아오면 툴팁·집은 것 정리, 열리면 내 첫 칸에 포커스
	if (Inventory.IsValid())
	{
		Inventory->OnPadHeldItemChanged.RemoveAll(this);
		Inventory->OnPadHeldItemChanged.AddUObject(this, &UNUTradeWindowWidget::UpdateHeldHighlight);
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
		UI->OnGamepadModeChanged.AddWeakLambda(this, [this](bool bGamepad)
		{
			if (!bGamepad)
			{
				HideFocusTooltip();
				if (Inventory.IsValid())
				{
					Inventory->ClearPadHeldItem();
				}
			}
		});
	}
	bFocusOfferOnRefresh = IsGamepadMode();

	RequestRefresh();
}

void UNUTradeWindowWidget::NativeDestruct()
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->OnTradeOfferChanged.RemoveAll(this);
	}
	if (Inventory.IsValid())
	{
		Inventory->OnPadHeldItemChanged.RemoveAll(this);
		if (Inventory->GetPadHeldItem().Container == NUContainer::TradeOffer)
		{
			Inventory->ClearPadHeldItem(); // 거래 창이 닫히면 집은 거래 칸도 없다
		}
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UNUTradeWindowWidget::RefreshData()
{
	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade == nullptr)
	{
		return;
	}

	PartnerName = FText::FromString(Trade->GetPartnerName());
	bMyLocked = Trade->GetMyOffer().bLocked;
	bPartnerLocked = Trade->GetPartnerOffer().bLocked;
	bMyConfirmed = Trade->GetMyOffer().bConfirmed;
	bPartnerConfirmed = Trade->GetPartnerOffer().bConfirmed;
	bCanConfirm = Trade->CanConfirm();

	if (UTextBlock* NameText = FindTextByName(TEXT("PartnerNameText")))
	{
		NameText->SetText(PartnerName);
	}

	// 어느 칸이 누구 것인지 제목으로 못 박는다
	if (UTextBlock* PartnerTitle = FindTextByName(TEXT("PartnerOfferTitle")))
	{
		PartnerTitle->SetText(PartnerName.IsEmpty()
			? NSLOCTEXT("NUTrade", "PartnerOfferDefault", "상대 제안")
			: FText::Format(NSLOCTEXT("NUTrade", "PartnerOfferNamed", "{0}의 제안"), PartnerName));
	}

	PopulateOffers();
	UpdateLockCovers();
	UpdateActionButtons();
	OnTradeViewChanged();
}

void UNUTradeWindowWidget::PopulateOffers()
{
	const UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade == nullptr)
	{
		return;
	}

	// 다시 지으면 칸 위젯이 새것이라 패드 포커스가 사라진다 --- 칸 좌표를 기억했다 되돌린다 (창이 열린 첫 새로고침은 내 첫 칸)
	HideFocusTooltip();
	const UNUItemCellWidget* Focused = FindFocusedCell();
	const int32 FocusContainer = Focused != nullptr ? Focused->GetContainer() : NUContainer::TradeOffer;
	const int32 FocusSlot = Focused != nullptr ? Focused->GetSlotNo() : (bFocusOfferOnRefresh ? 0 : INDEX_NONE);
	bFocusOfferOnRefresh = false;

	BuildOfferInto(TEXT("MyOfferBox"), Trade->GetMyOffer(), true);
	BuildOfferInto(TEXT("PartnerOfferBox"), Trade->GetPartnerOffer(), false);
	UpdateHeldHighlight();

	if (FocusSlot == INDEX_NONE)
	{
		return;
	}
	if (FocusContainer == NUContainer::TradeOffer)
	{
		const TWeakObjectPtr<UNUItemCellWidget>* Cell = MyCellBySlot.Find(FocusSlot);
		if (Cell != nullptr && Cell->IsValid())
		{
			(*Cell)->SetFocus();
		}
	}
	else if (UPanelWidget* PartnerPanel = FindPanelByName(TEXT("PartnerOfferBox")))
	{
		if (UWidget* Cell = PartnerPanel->GetChildAt(FocusSlot))
		{
			Cell->SetFocus();
		}
	}
}

void UNUTradeWindowWidget::BuildOfferInto(const TCHAR* PanelName, const FNUTradeOffer& Offer, bool bMine)
{
	UPanelWidget* Panel = FindPanelByName(PanelName);
	if (Panel == nullptr)
	{
		return;
	}
	Panel->ClearChildren();
	if (bMine)
	{
		MyCellBySlot.Reset();
	}

	// 이름 계약 패널이 그리드면 행/열로 배치, 아니면 순서대로 채운다
	UUniformGridPanel* Grid = Cast<UUniformGridPanel>(Panel);
	const int32 Columns = FMath::Max(1, GridColumns);

	for (int32 Index = 0; Index < Offer.Slots.Num(); ++Index)
	{
		const FNUTradeSlot& OfferSlot = Offer.Slots[Index];

		// 칸 모양(빈 칸 색·아이콘·개수 배지)은 WBP_ItemCell이 갖는다 --- 인벤 그리드와 같은 칸
		UNUItemCellWidget* Cell = MakeItemCell(CellSize);
		if (Cell == nullptr)
		{
			return; // 칸 WBP 미지정 --- 인벤 창이 같은 설정으로 경고를 낸다
		}

		// 내 칸만 우클릭·드래그를 받는다 (좌표의 칸 번호 = 거래 칸). 상대 칸은 보기만 --- 좌표는 패드 포커스 툴팁이 쓴다
		if (bMine)
		{
			Cell->SetCoords(NUContainer::TradeOffer, Index);
			Cell->OnCellMouseDown.BindUObject(this, &UNUTradeWindowWidget::HandleOfferCellMouseDown);
			MyCellBySlot.Add(Index, Cell);
		}
		else
		{
			Cell->SetCoords(PartnerOfferContainer, Index);
		}

		if (OfferSlot.IsEmpty())
		{
			Cell->SetEmpty(FText::GetEmpty());
		}
		else
		{
			FillOfferCell(Cell, OfferSlot);
			Cell->SetToolTip(BuildItemTooltip(OfferSlot.ItemRowName));
		}

		if (Grid != nullptr)
		{
			if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Cell, Index / Columns, Index % Columns))
			{
				// 셀이 균등 칸을 채우며 늘어나지 않게 (인벤과 동일)
				GridSlot->SetHorizontalAlignment(HAlign_Center);
				GridSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		else
		{
			Panel->AddChild(Cell);
		}
	}
}

void UNUTradeWindowWidget::FillOfferCell(UNUItemCellWidget* Cell, const FNUTradeSlot& OfferSlot) const
{
	const FNUItemDefRow* Def = Inventory.IsValid() ? Inventory->FindItemDef(OfferSlot.ItemRowName) : nullptr;
	Cell->SetItem(LoadItemIconTexture(OfferSlot.ItemRowName), OfferSlot.Count,
		Def != nullptr ? Def->DisplayName : FText::FromName(OfferSlot.ItemRowName), false);
}

void UNUTradeWindowWidget::UpdateLockCovers()
{
	// 덮개의 생김새(자물쇠·어두운 판)는 WBP가 가진다 --- C++는 어느 쪽이 잠겼는지만 알린다
	const auto Apply = [this](const TCHAR* CoverName, bool bLocked)
	{
		if (UWidget* Cover = GetWidgetFromName(CoverName))
		{
			// 클릭은 통과시킨다 --- 잠긴 뒤에도 올라온 아이템 툴팁은 볼 수 있어야 한다
			Cover->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	Apply(TEXT("MyLockCover"), bMyLocked);
	Apply(TEXT("PartnerLockCover"), bPartnerLocked);
}

void UNUTradeWindowWidget::UpdateActionButtons()
{
	// 잠금 버튼은 누를 때마다 뜻이 바뀐다 --- 라벨로 현재 상태를 알린다
	const FText LockLabelText = bMyLocked
		? NSLOCTEXT("NUTrade", "UnlockButton", "잠금 해제")
		: NSLOCTEXT("NUTrade", "LockButton", "잠금");
	if (UTextBlock* LockLabel = FindTextByName(TEXT("LockLabel")))
	{
		LockLabel->SetText(LockLabelText);
	}
	SetTradeButtonLabel(LockButton, LockLabelText);

	// 양쪽이 잠그기 전까지 확정은 눌리지 않는다. 내가 확정한 뒤에는 상대를 기다린다는 것을 알린다
	ConfirmButton->SetIsEnabled(bCanConfirm);
	const FText ConfirmLabelText = bMyConfirmed
		? NSLOCTEXT("NUTrade", "WaitingPartner", "상대 대기 중...")
		: NSLOCTEXT("NUTrade", "ConfirmButton", "확정");
	if (UTextBlock* ConfirmLabel = FindTextByName(TEXT("ConfirmLabel")))
	{
		ConfirmLabel->SetText(ConfirmLabelText);
	}
	SetTradeButtonLabel(ConfirmButton, ConfirmLabelText);

	// 잠금 덮개 글자로 확정까지 표시 (잠김 => 확정 완료)
	if (UTextBlock* MyLockText = FindTextByName(TEXT("MyLockText")))
	{
		MyLockText->SetText(bMyConfirmed
			? NSLOCTEXT("NUTrade", "CoverConfirmed", "확정 완료")
			: NSLOCTEXT("NUTrade", "CoverLocked", "잠김"));
	}
	if (UTextBlock* PartnerLockText = FindTextByName(TEXT("PartnerLockText")))
	{
		PartnerLockText->SetText(bPartnerConfirmed
			? NSLOCTEXT("NUTrade", "CoverConfirmed", "확정 완료")
			: NSLOCTEXT("NUTrade", "CoverLocked", "잠김"));
	}
}

FReply UNUTradeWindowWidget::HandleOfferCellMouseDown(int32 Container, int32 TradeSlot, const FPointerEvent& MouseEvent)
{
	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade == nullptr || Trade->GetState() != ENUTradeState::Active)
	{
		return FReply::Handled();
	}

	const FNUTradeOffer& My = Trade->GetMyOffer();
	const bool bHasItem = My.Slots.IsValidIndex(TradeSlot) && !My.Slots[TradeSlot].IsEmpty();

	// 우클릭 = 내리기 (인벤의 우클릭 빠른 이동과 같은 결)
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bHasItem)
	{
		TakeBackOffer(TradeSlot);
		return FReply::Handled();
	}

	// 좌클릭 = 드래그 후보 (인벤으로 끌면 내리기, 다른 칸으로 끌면 자리 이동)
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bHasItem)
	{
		PendingDragTradeSlot = TradeSlot;
		return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return FReply::Handled(); // 빈 칸 --- 창 드래그로 번지지 않게 소비
}

void UNUTradeWindowWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	const UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade == nullptr || !Trade->GetMyOffer().Slots.IsValidIndex(PendingDragTradeSlot))
	{
		return;
	}

	UNUItemDragOperation* Operation = NewObject<UNUItemDragOperation>(this);
	Operation->SourceContainer = NUContainer::Inventory;
	Operation->SourceSlotNo = PendingDragTradeSlot; // 거래 칸 번호
	Operation->bFromTradeOffer = true;
	// 드롭을 인벤 창이 받으면 이 창의 NativeOnDrop이 안 불린다 --- 어느 쪽에서 끝나든 비주얼 정리
	Operation->OnDrop.AddDynamic(this, &UNUTradeWindowWidget::HandleDragEnded);
	Operation->OnDragCancelled.AddDynamic(this, &UNUTradeWindowWidget::HandleDragEnded);
	OutOperation = Operation;

	// 커서를 따라가는 담개 --- 칸과 같은 WBP (인벤 창과 같은 방식)
	if (UNUItemCellWidget* Visual = MakeItemCell(CellSize))
	{
		FillOfferCell(Visual, Trade->GetMyOffer().Slots[PendingDragTradeSlot]);
		ShowDragVisual(Visual, CellSize);
	}
}

void UNUTradeWindowWidget::HandleDragEnded(UDragDropOperation* InOperation)
{
	ClearDragVisual();
}

int32 UNUTradeWindowWidget::FindOfferSlotAt(const FVector2D& ScreenPos) const
{
	for (const TPair<int32, TWeakObjectPtr<UNUItemCellWidget>>& Pair : MyCellBySlot)
	{
		const UNUItemCellWidget* Cell = Pair.Value.Get();
		if (Cell != nullptr && Cell->GetCachedGeometry().IsUnderLocation(ScreenPos))
		{
			return Pair.Key;
		}
	}
	return INDEX_NONE;
}

bool UNUTradeWindowWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UNUItemDragOperation* Operation = Cast<UNUItemDragOperation>(InOperation);
	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Operation == nullptr || Trade == nullptr || Trade->GetState() != ENUTradeState::Active)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	// 거래 칸끼리 옮기거나 소지품에서 올린다. 창고에서 끌어온 것은 거래에 못 올린다
	if (Operation->bFromTradeOffer || Operation->SourceContainer == NUContainer::Inventory)
	{
		PlaceIntoOffer(*Trade, Operation->bFromTradeOffer, Operation->SourceSlotNo, FindOfferSlotAt(InDragDropEvent.GetScreenSpacePosition()));
	}
	return true; // 창 안에서의 드롭은 삼킨다 (인벤 밖 드롭의 버리기로 번지지 않게)
}

void UNUTradeWindowWidget::PlaceIntoOffer(UNUTradeSubsystem& Trade, bool bFromOffer, int32 SourceSlot, int32 DropSlot)
{
	if (Trade.GetState() != ENUTradeState::Active)
	{
		return;
	}
	const FNUTradeOffer& My = Trade.GetMyOffer();

	// 잠금 중에는 어떤 배치 변경도 서버가 거절한다 --- 요청 여러 발 쏘기 전에 한 번만 알린다
	if (My.bLocked)
	{
		ShowTradeNotice(NSLOCTEXT("NUTrade", "LockedNoChange", "잠금을 해제해야 아이템을 바꿀 수 있습니다."));
		return;
	}

	// 거래 칸끼리 --- 빈 칸이면 자리 이동, 찬 칸이면 서로 교환
	if (bFromOffer)
	{
		if (DropSlot == INDEX_NONE || DropSlot == SourceSlot || !My.Slots.IsValidIndex(SourceSlot) || !My.Slots.IsValidIndex(DropSlot))
		{
			return; // 제자리거나 칸 밖
		}

		const int32 SourceInv = My.Slots[SourceSlot].InvSlotNo;
		const int32 TargetInv = My.Slots[DropSlot].IsEmpty() ? INDEX_NONE : My.Slots[DropSlot].InvSlotNo;

		// 같은 인벤 칸이 두 거래 칸에 걸치는 순간이 없도록 비우고 나서 올린다 (서버 1808 방지).
		// 요청 순서는 스트림이 보장하고, 반영은 SVR_Trade_Updated로 돌아온다
		Trade.SetTradeItem(SourceSlot, -1);
		Trade.SetTradeItem(DropSlot, SourceInv);
		if (TargetInv != INDEX_NONE)
		{
			Trade.SetTradeItem(SourceSlot, TargetInv); // 밀려난 것을 원래 칸으로
		}
		return;
	}

	// 소지품에서 온 것 --- 이미 올린 칸이면 무시 (서버 1808을 미리 거른다)
	for (const FNUTradeSlot& OfferSlot : My.Slots)
	{
		if (!OfferSlot.IsEmpty() && OfferSlot.InvSlotNo == SourceSlot)
		{
			return;
		}
	}

	// 놓은 칸에 올린다 (찬 칸이면 교체). 칸 밖이면 첫 빈 칸
	if (DropSlot != INDEX_NONE)
	{
		Trade.SetTradeItem(DropSlot, SourceSlot);
		return;
	}
	for (int32 Index = 0; Index < My.Slots.Num(); ++Index)
	{
		if (My.Slots[Index].IsEmpty())
		{
			Trade.SetTradeItem(Index, SourceSlot);
			return;
		}
	}
	ShowTradeNotice(NSLOCTEXT("NUTrade", "OfferFull", "거래 칸이 가득 찼습니다."));
}

void UNUTradeWindowWidget::TakeBackOffer(int32 TradeSlot)
{
	// 거래 상태·잠금 검사와 안내는 서브시스템이 한다
	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade != nullptr && Trade->GetMyOffer().Slots.IsValidIndex(TradeSlot) && !Trade->GetMyOffer().Slots[TradeSlot].IsEmpty())
	{
		Trade->SetTradeItem(TradeSlot, -1);
	}
}

void UNUTradeWindowWidget::PadPickOrPlaceOffer(int32 TradeSlot)
{
	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade == nullptr || !Inventory.IsValid() || !Trade->GetMyOffer().Slots.IsValidIndex(TradeSlot))
	{
		return;
	}
	const FNUPadHeldItem Held = Inventory->GetPadHeldItem();
	if (!Held.IsSet())
	{
		if (!Trade->GetMyOffer().Slots[TradeSlot].IsEmpty())
		{
			Inventory->SetPadHeldItem(NUContainer::TradeOffer, TradeSlot, FGuid());
		}
		return;
	}
	Inventory->ClearPadHeldItem();
	// 거래 칸끼리는 자리 이동·교환, 소지품에서 집어 온 것은 이 칸에 올린다 (끌어다 놓기와 같다). 창고 칸은 거래에 못 올린다
	if (Held.Container == NUContainer::TradeOffer || Held.Container == NUContainer::Inventory)
	{
		PlaceIntoOffer(*Trade, Held.Container == NUContainer::TradeOffer, Held.SlotNo, TradeSlot);
	}
}

void UNUTradeWindowWidget::UpdateHeldHighlight()
{
	const FNUPadHeldItem Held = Inventory.IsValid() ? Inventory->GetPadHeldItem() : FNUPadHeldItem();
	for (const TPair<int32, TWeakObjectPtr<UNUItemCellWidget>>& Pair : MyCellBySlot)
	{
		if (UNUItemCellWidget* Cell = Pair.Value.Get())
		{
			Cell->SetHeld(Held.Container == NUContainer::TradeOffer && Held.SlotNo == Pair.Key);
		}
	}
}

FReply UNUTradeWindowWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bCellKey = Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Gamepad_FaceButton_Left || Key == EKeys::Gamepad_RightTrigger;
	const UNUItemCellWidget* Cell = bCellKey ? FindFocusedCell() : nullptr;
	if (Cell != nullptr)
	{
		// 상대 칸은 보기만 한다 --- 키는 먹어서 창 뒤로 새지 않게
		const int32 TradeSlot = Cell->GetSlotNo();
		const UNUTradeSubsystem* Trade = GetTradeSubsystem();
		if (Cell->GetContainer() != NUContainer::TradeOffer || Trade == nullptr)
		{
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			PadPickOrPlaceOffer(TradeSlot);
		}
		else if (Key == EKeys::Gamepad_RightTrigger)
		{
			TakeBackOffer(TradeSlot);
		}
		else if (Trade->GetMyOffer().Slots.IsValidIndex(TradeSlot) && !Trade->GetMyOffer().Slots[TradeSlot].IsEmpty())
		{
			// X = 메뉴 (칸 오른쪽에, 화면 밖이면 메뉴가 안쪽으로 당긴다)
			FVector2D FocusPos = FVector2D::ZeroVector, FocusSize = FVector2D::ZeroVector;
			if (Cell->GetCachedWidget().IsValid())
			{
				NUUIGeometry::GetViewportRect(this, *Cell->GetCachedWidget(), FocusPos, FocusSize);
			}
			TArray<FNUItemMenuEntry> Entries;
			Entries.Add({ NSLOCTEXT("NUTrade", "MenuTakeBack", "거래에서 내리기"),
				FSimpleDelegate::CreateWeakLambda(this, [this, TradeSlot]() { TakeBackOffer(TradeSlot); }) });
			UNUItemContextMenu::Show(this, MoveTemp(Entries), FocusPos + FVector2D(FocusSize.X + 4.f, 0.f));
		}
		return FReply::Handled();
	}
	// B = 집은 것 내려놓기 (창 닫기 = 거래 취소보다 먼저). 든 것이 없으면 흘려보내 PC가 창을 닫는다
	if (Key == EKeys::Gamepad_FaceButton_Right && Inventory.IsValid() && Inventory->GetPadHeldItem().IsSet())
	{
		Inventory->ClearPadHeldItem();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNUTradeWindowWidget::FocusForPad()
{
	const TWeakObjectPtr<UNUItemCellWidget>* First = MyCellBySlot.Find(0);
	if (First != nullptr && First->IsValid())
	{
		(*First)->SetFocus();
		return;
	}
	Super::FocusForPad();
}

FName UNUTradeWindowWidget::GetCellItemRowName(int32 Container, int32 SlotNo) const
{
	const UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade == nullptr || (Container != NUContainer::TradeOffer && Container != PartnerOfferContainer))
	{
		return NAME_None;
	}
	const FNUTradeOffer& Offer = Container == NUContainer::TradeOffer ? Trade->GetMyOffer() : Trade->GetPartnerOffer();
	return Offer.Slots.IsValidIndex(SlotNo) && !Offer.Slots[SlotNo].IsEmpty() ? Offer.Slots[SlotNo].ItemRowName : NAME_None;
}

void UNUTradeWindowWidget::ToggleLock()
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->SetLock(!Trade->GetMyOffer().bLocked);
	}
}

void UNUTradeWindowWidget::ConfirmTrade()
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->ConfirmTrade();
	}
}

void UNUTradeWindowWidget::CloseSelf()
{
	UNUTradeUIController* Controller = GetTradeController();
	if (Controller == nullptr)
	{
		return;
	}

	// X 버튼·ESC --- 창만 닫는 게 아니라 거래 자체를 취소한다.
	// 올려둔 것이 있으면 실수로 날리지 않게 한 번 묻는다
	const UNUTradeSubsystem* Trade = GetTradeSubsystem();
	const bool bHasOffer = Trade != nullptr && Trade->GetMyOffer().Slots.ContainsByPredicate(
		[](const FNUTradeSlot& OfferSlot) { return !OfferSlot.IsEmpty(); });

	if (!bHasOffer)
	{
		Controller->CancelTrade();
		return;
	}

	RequestConfirm(
		NSLOCTEXT("NUTrade", "CancelConfirm", "올려둔 아이템이 있습니다. 거래를 취소할까요?"),
		NSLOCTEXT("NUTrade", "CancelConfirmYes", "취소하기"),
		FSimpleDelegate::CreateWeakLambda(this, [this]()
		{
			if (UNUTradeUIController* Ctrl = GetTradeController())
			{
				Ctrl->CancelTrade();
			}
		}));
}

void UNUTradeWindowWidget::HandleOfferChanged()
{
	RequestRefresh();
}

void UNUTradeWindowWidget::HandleLockClicked()
{
	ToggleLock();
}

void UNUTradeWindowWidget::HandleConfirmClicked()
{
	ConfirmTrade();
}

void UNUTradeWindowWidget::ShowTradeNotice(const FText& Message) const
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(Message);
	}
}

UNUTradeSubsystem* UNUTradeWindowWidget::GetTradeSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUTradeSubsystem>() : nullptr;
}

UNUTradeUIController* UNUTradeWindowWidget::GetTradeController() const
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	return UI ? UI->GetController<UNUTradeUIController>() : nullptr;
}
