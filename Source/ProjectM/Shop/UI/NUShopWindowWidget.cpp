// Fill out your copyright notice in the Description page of Project Settings.

#include "Shop/UI/NUShopWindowWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUSplitStackDialog.h"
#include "Shop/NUShopTypes.h"
#include "Styling/CoreStyle.h"
#include "Shop/UI/NUShopEntryWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"

namespace
{
	// 명명 계약: 순정 Button(레거시)과 UNU_CommonButtonWidget(신규 공용 부품) 둘 다 지원해야 해서
	// BindWidget 타입을 UWidget으로 열어두고 여기서 캐스팅해 알맞은 델리게이트/함수로 다룬다 (NUOptionsWindowWidget과 동일 패턴)
	void BindTabClicked(UWidget* Widget, UObject* Target, FName FunctionName)
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

	void UnbindTabClicked(UWidget* Widget, UObject* Target, FName FunctionName)
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
}

void UNUShopWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UnbindTabClicked(BuyTabButton, this, GET_FUNCTION_NAME_CHECKED(UNUShopWindowWidget, HandleBuyTabClicked));
	BindTabClicked(BuyTabButton, this, GET_FUNCTION_NAME_CHECKED(UNUShopWindowWidget, HandleBuyTabClicked));
	UnbindTabClicked(SellTabButton, this, GET_FUNCTION_NAME_CHECKED(UNUShopWindowWidget, HandleSellTabClicked));
	BindTabClicked(SellTabButton, this, GET_FUNCTION_NAME_CHECKED(UNUShopWindowWidget, HandleSellTabClicked));

	// 고른 품목 칸 (있는 WBP만)
	if (QuantityMinusButton != nullptr)
	{
		QuantityMinusButton->OnClicked.AddUniqueDynamic(this, &UNUShopWindowWidget::HandleQuantityMinus);
	}
	if (QuantityPlusButton != nullptr)
	{
		QuantityPlusButton->OnClicked.AddUniqueDynamic(this, &UNUShopWindowWidget::HandleQuantityPlus);
	}
	if (QuantityMaxButton != nullptr)
	{
		QuantityMaxButton->OnClicked.AddUniqueDynamic(this, &UNUShopWindowWidget::HandleQuantityMax);
	}
	if (ConfirmButton != nullptr)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &UNUShopWindowWidget::HandleConfirmClicked);
	}
}

void UNUShopWindowWidget::SetShop(int32 InShopId)
{
	ShopId = InShopId;
	bSellTab = false;
	RefreshData();
}

void UNUShopWindowWidget::RefreshData()
{
	const FNUShopRow* Shop = GetShopRow();
	if (Shop == nullptr || !ResolveComponents())
	{
		return; // SetShop 전(창 생성 직후)이거나 인벤이 아직 없다
	}

	if (UTextBlock* Title = FindTextByName(TEXT("TitleText")))
	{
		Title->SetText(Shop->DisplayName);
	}

	// 판매 탭은 일반 상점에만 있다 (기획 5절)
	const bool bCanSell = Shop->ShopType == ENUShopType::Shop;
	SellTabButton->SetVisibility(bCanSell ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bCanSell)
	{
		bSellTab = false;
	}

	// 고른 탭의 표시만 켠다 --- 색·모양은 WBP 것. 안 고른 쪽은 자리를 남겨(Hidden) 줄 높이가 흔들리지 않게 하고,
	// 판매 탭이 없는 상점은 표시도 접어(Collapsed) 구매 탭이 줄을 다 쓰게 한다
	if (BuyTabIndicator != nullptr)
	{
		BuyTabIndicator->SetVisibility(bSellTab ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
	if (SellTabIndicator != nullptr)
	{
		SellTabIndicator->SetVisibility(!bCanSell ? ESlateVisibility::Collapsed
			: bSellTab ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	UpdateGoldText();

	// 다시 지으면 줄 위젯이 새것이라 패드 포커스가 사라진다 --- 몇 번째 줄이었는지 기억했다 되돌린다 (줄이 줄었으면 마지막 줄)
	UPanelWidget* List = FindPanelByName(TEXT("ListBox"));
	int32 FocusedIndex = INDEX_NONE;
	for (int32 Index = 0; List != nullptr && Index < List->GetChildrenCount(); ++Index)
	{
		if (List->GetChildAt(Index)->HasFocusedDescendants())
		{
			FocusedIndex = Index;
			break;
		}
	}
	HideFocusTooltip();
	PopulateList();
	RestoreSelection();
	if (FocusedIndex != INDEX_NONE && List != nullptr && List->GetChildrenCount() > 0)
	{
		UWidget* Target = UNUUIManager::FindFirstFocusableIn(List->GetChildAt(FMath::Min(FocusedIndex, List->GetChildrenCount() - 1)));
		if (Target != nullptr)
		{
			Target->SetFocus();
		}
		else
		{
			FocusForPad(); // 그 줄이 품절·잠김이 됐다
		}
	}
}

void UNUShopWindowWidget::PopulateList()
{
	UPanelWidget* Panel = FindPanelByName(TEXT("ListBox"));
	const FNUShopRow* Shop = GetShopRow();
	if (Panel == nullptr || Shop == nullptr || !Inventory.IsValid())
	{
		return;
	}
	Panel->ClearChildren();
	Entries.Reset();
	if (EntryWidgetClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[상점] EntryWidgetClass 미지정 --- 품목 줄을 그릴 수 없습니다 (WBP_ShopWindow 기본값 확인)"));
		return;
	}

	// 줄 하나를 만들어 패널에 붙인다. 툴팁은 베이스 공용(인벤 칸과 같은 내용). 줄 사이 간격은 WBP_ShopEntry가 갖는다
	const auto AddEntry = [this, Panel](FName ItemRowName)
	{
		UNUShopEntryWidget* Entry = CreateWidget<UNUShopEntryWidget>(this, EntryWidgetClass);
		if (UWidget* Tooltip = BuildItemTooltip(ItemRowName))
		{
			Entry->SetToolTip(Tooltip);
		}
		Entry->OnEntryFocusChanged.BindUObject(this, &UNUShopWindowWidget::HandleEntryFocusChanged, ItemRowName);
		Panel->AddChild(Entry);
		Entries.Add(Entry);
		return Entry;
	};

	if (!bSellTab)
	{
		// 구매 --- 품목 표에서 이 상점 것만 (SlotIndex 순). 레벨 미달 잠김은 줄이 판단한다 (최종 판정은 서버 2206)
		const int32 MyLevel = GetMyLevel();
		for (const FNUShopItemRow* Item : NUShop::GetShopItems(Shop->ShopId))
		{
			const FName ItemRowName = Inventory->FindRowNameByNumericId(Item->RefId);
			const FNUItemDefRow* Def = Inventory->FindItemDef(ItemRowName);
			if (Def == nullptr)
			{
				continue; // 품목 표 Ref_ID가 아이템·장비 표에 없다 (변환 스크립트가 경고하는 항목)
			}
			UNUShopEntryWidget* Entry = AddEntry(ItemRowName);
			Entry->SetBuyEntry(*Item, Def->DisplayName, LoadItemIconTexture(ItemRowName), MyLevel);
			const int32 Owned = Inventory->GetCountByItemId(ItemRowName);
			Entry->SetSubText(Owned > 0
				? FText::Format(NSLOCTEXT("NUShop", "OwnedLine", "보유 {0}"), FText::AsNumber(Owned))
				: Def->Description);
			Entry->OnBuyClicked.BindUObject(this, &UNUShopWindowWidget::HandleBuyClicked);
		}
		return;
	}

	// 판매 --- 내 인벤에서 팔 수 있는 것만 (퀘스트템·열쇠템·도면·품목 표에 없는 아이템 제외)
	for (const FNUItemInstance& Item : Inventory->GetItems())
	{
		const FNUItemDefRow* Def = Inventory->FindItemDef(Item.ItemRowName);
		const int32 UnitPrice = Def != nullptr ? NUShop::GetSellPrice(*Def, *Shop) : 0;
		if (Def == nullptr || !NUItem::IsSellable(Def->ItemType) || UnitPrice <= 0)
		{
			continue;
		}
		UNUShopEntryWidget* Entry = AddEntry(Item.ItemRowName);
		Entry->SetSellEntry(Item.InstanceId, Def->DisplayName, Item.Count, UnitPrice, LoadItemIconTexture(Item.ItemRowName));
		Entry->SetSubText(Def->Description);
		Entry->OnSellClicked.BindUObject(this, &UNUShopWindowWidget::HandleSellClicked);
	}
}

void UNUShopWindowWidget::HandleBuyTabClicked()
{
	if (bSellTab)
	{
		bSellTab = false;
		RefreshData();
	}
}

void UNUShopWindowWidget::HandleSellTabClicked()
{
	if (!bSellTab)
	{
		bSellTab = true;
		RefreshData();
	}
}

void UNUShopWindowWidget::HandleBuyClicked(int32 ShopItemId)
{
	if (UsesDetailPanel())
	{
		SelectBuy(ShopItemId);   // 수량·구매는 고른 품목 칸에서
		return;
	}
	const FNUShopItemRow* Item = NUShop::FindShopItemRow(ShopItemId);
	if (GetShopRow() == nullptr || Item == nullptr || !Inventory.IsValid())
	{
		return;
	}

	const FName ItemRowName = Inventory->FindRowNameByNumericId(Item->RefId);
	const FNUItemDefRow* Def = Inventory->FindItemDef(ItemRowName);
	const int32 UnitPrice = Item->Price; // 표의 값이 곧 구매가 (등급·지역 배수 반영 완료)
	if (Def == nullptr || UnitPrice <= 0)
	{
		return; // 아이템 표에 없거나 가격 미정 품목 (표의 Price가 비어 있다)
	}

	// 지금 살 수 있는 최대 = min(가방이 받을 수 있는 수, 골드로 살 수 있는 수)
	const int32 MaxRoom = Inventory->GetReceivableCount(ItemRowName);
	const int32 MaxAfford = static_cast<int32>(FMath::Min<int64>(Inventory->GetGold() / UnitPrice, MAX_int32));
	const int32 MaxCount = FMath::Min(MaxRoom, MaxAfford);
	if (MaxCount < 1)
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
		{
			UI->ShowNotice(MaxRoom < 1
				? NSLOCTEXT("NUShop", "NotEnoughRoom", "가방에 자리가 부족합니다.")
				: NSLOCTEXT("NUShop", "NotEnoughGold", "골드가 부족합니다."));
		}
		return;
	}

	UNUSplitStackDialog::ShowCount(this, MaxCount, 1,
		FText::Format(NSLOCTEXT("NUShop", "BuyPrompt", "[{0}] 몇 개를 구매할까요? (최대 {1}개)"),
			Def->DisplayName, FText::AsNumber(MaxCount)),
		NSLOCTEXT("NUShop", "BuyAction", "구매"), UnitPrice,
		FOnNUSplitConfirmed::CreateUObject(this, &UNUShopWindowWidget::HandleBuyConfirmed, ShopItemId));
}

void UNUShopWindowWidget::HandleEntryFocusChanged(UNUShopEntryWidget* Entry, bool bFocused, FName ItemRowName)
{
	HideFocusTooltip();
	if (bFocused && UsesDetailPanel() && Entry != nullptr)
	{
		// 키·패드로 줄을 옮기면 고른 품목도 따라간다 (설명은 고른 품목 칸이 보여 주므로 툴팁은 띄우지 않는다)
		if (bSellTab)
		{
			SelectSell(Entry->GetInstanceId());
		}
		else
		{
			SelectBuy(Entry->GetShopItemId());
		}
		return;
	}
	if (bFocused)
	{
		ShowFocusTooltip(Entry, ItemRowName);
	}
}

void UNUShopWindowWidget::FocusForPad()
{
	if (UWidget* First = UNUUIManager::FindFirstFocusableIn(FindPanelByName(TEXT("ListBox"))))
	{
		First->SetFocus();
		return;
	}
	Super::FocusForPad(); // 빈 목록 --- 탭·닫기 버튼으로
}

FReply UNUShopWindowWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_RightTrigger)
	{
		const bool bToSell = Key == EKeys::Gamepad_RightTrigger;
		// 판매 탭이 없는 상점(일반 상점이 아닌 곳)에서는 RT가 아무 일도 안 한다
		if (bSellTab != bToSell && (!bToSell || SellTabButton->IsVisible()))
		{
			if (bToSell)
			{
				HandleSellTabClicked();
			}
			else
			{
				HandleBuyTabClicked();
			}
			FocusForPad(); // 탭을 바꾸면 새 목록의 첫 줄부터
		}
		return FReply::Handled();
	}
	if (UsesDetailPanel())
	{
		// 고른 품목 칸: 좌우 = 수량, F / 패드 X = 실행 (목록은 위아래로만 움직인다)
		if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
		{
			ChangeQuantity(-1);
			return FReply::Handled();
		}
		if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
		{
			ChangeQuantity(1);
			return FReply::Handled();
		}
		if (Key == EKeys::F || Key == EKeys::Gamepad_FaceButton_Left)
		{
			ConfirmSelected();
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNUShopWindowWidget::HandleSellClicked(FGuid InstanceId)
{
	if (UsesDetailPanel())
	{
		SelectSell(InstanceId);   // 수량·판매는 고른 품목 칸에서
		return;
	}
	// 수량 창·실행은 베이스 공용 --- 인벤 우클릭 판매와 같은 흐름을 탄다
	OpenSellCountDialog(InstanceId, ShopId);
}

void UNUShopWindowWidget::HandleBuyConfirmed(int32 Count, int32 ShopItemId)
{
	const FNUShopRow* Shop = GetShopRow();
	const FNUShopItemRow* Item = NUShop::FindShopItemRow(ShopItemId);
	if (Shop != nullptr && Item != nullptr && Inventory.IsValid())
	{
		Inventory->ShopBuyItem(*Item, Count, *Shop); // 성공하면 인벤 변경 통지로 창이 다시 그려진다
	}
}

const FNUShopRow* UNUShopWindowWidget::GetShopRow() const
{
	return ShopId > 0 ? NUShop::FindShopRow(ShopId) : nullptr;
}

// ======================================================================== 고른 품목 칸
void UNUShopWindowWidget::SelectBuy(int32 InShopItemId)
{
	if (InShopItemId != SelectedShopItemId)
	{
		SelectedShopItemId = InShopItemId;
		SelectedCount = 1;
	}
	SelectedInstanceId.Invalidate();
	for (UNUShopEntryWidget* Entry : Entries)
	{
		Entry->SetSelected(Entry->GetShopItemId() == SelectedShopItemId);
	}
	RefreshDetail();
}

void UNUShopWindowWidget::SelectSell(const FGuid& InInstanceId)
{
	if (InInstanceId != SelectedInstanceId)
	{
		SelectedInstanceId = InInstanceId;
		SelectedCount = 1;
	}
	SelectedShopItemId = 0;
	for (UNUShopEntryWidget* Entry : Entries)
	{
		Entry->SetSelected(Entry->GetInstanceId() == SelectedInstanceId);
	}
	RefreshDetail();
}

void UNUShopWindowWidget::RestoreSelection()
{
	if (!UsesDetailPanel())
	{
		return;
	}
	const bool bStillThere = Entries.ContainsByPredicate([this](const UNUShopEntryWidget* Entry)
	{
		return bSellTab ? (SelectedInstanceId.IsValid() && Entry->GetInstanceId() == SelectedInstanceId)
			: (SelectedShopItemId != 0 && Entry->GetShopItemId() == SelectedShopItemId);
	});
	if (bStillThere)
	{
		bSellTab ? SelectSell(SelectedInstanceId) : SelectBuy(SelectedShopItemId);
		return;
	}
	SelectedShopItemId = 0;
	SelectedInstanceId.Invalidate();
	SelectedCount = 1;
	if (Entries.IsEmpty())
	{
		RefreshDetail();   // 빈 목록 --- 칸을 접는다
		return;
	}
	if (bSellTab)
	{
		SelectSell(Entries[0]->GetInstanceId());
		return;
	}
	// 처음 고를 품목은 지금 살 수 있는 첫 줄 (레벨 잠김 줄은 건너뛴다). 다 잠겼으면 첫 줄
	const int32 MyLevel = GetMyLevel();
	UNUShopEntryWidget* First = Entries[0];
	for (UNUShopEntryWidget* Entry : Entries)
	{
		const FNUShopItemRow* Item = NUShop::FindShopItemRow(Entry->GetShopItemId());
		if (Item != nullptr && (MyLevel <= 0 || MyLevel >= Item->RequiredLevel) && Item->Stock != 0)
		{
			First = Entry;
			break;
		}
	}
	SelectBuy(First->GetShopItemId());
}

int32 UNUShopWindowWidget::GetSelectedMaxCount() const
{
	if (!Inventory.IsValid())
	{
		return 0;
	}
	if (bSellTab)
	{
		const FNUItemInstance* Instance = SelectedInstanceId.IsValid() ? Inventory->FindInstance(SelectedInstanceId) : nullptr;
		return Instance != nullptr ? Instance->Count : 0;
	}
	const FNUShopItemRow* Item = SelectedShopItemId != 0 ? NUShop::FindShopItemRow(SelectedShopItemId) : nullptr;
	if (Item == nullptr || Item->Price <= 0 || Item->Stock == 0)
	{
		return 0;
	}
	const int32 MyLevel = GetMyLevel();
	if (MyLevel > 0 && MyLevel < Item->RequiredLevel)
	{
		return 0;
	}
	const FName RowName = Inventory->FindRowNameByNumericId(Item->RefId);
	int32 Max = FMath::Min(Inventory->GetReceivableCount(RowName),
		static_cast<int32>(FMath::Min<int64>(Inventory->GetGold() / Item->Price, MAX_int32)));
	if (Item->Stock > 0)
	{
		Max = FMath::Min(Max, Item->Stock);
	}
	return FMath::Max(Max, 0);
}

void UNUShopWindowWidget::RefreshDetail()
{
	if (!UsesDetailPanel())
	{
		return;
	}
	const FNUShopRow* Shop = GetShopRow();
	FName RowName;
	const FNUItemDefRow* Def = nullptr;
	int32 UnitPrice = 0;
	FText Info;
	if (Inventory.IsValid() && Shop != nullptr)
	{
		if (!bSellTab && SelectedShopItemId != 0)
		{
			if (const FNUShopItemRow* Item = NUShop::FindShopItemRow(SelectedShopItemId))
			{
				RowName = Inventory->FindRowNameByNumericId(Item->RefId);
				Def = Inventory->FindItemDef(RowName);
				UnitPrice = Item->Price;
				const FText Owned = FText::AsNumber(Inventory->GetCountByItemId(RowName));
				const int32 MyLevel = GetMyLevel();
				if (MyLevel > 0 && MyLevel < Item->RequiredLevel)
				{
					Info = FText::Format(NSLOCTEXT("NUShop", "DetailLocked", "보유 {0} · Lv. {1}부터 살 수 있습니다"), Owned, FText::AsNumber(Item->RequiredLevel));
				}
				else if (Item->Stock == 0)
				{
					Info = FText::Format(NSLOCTEXT("NUShop", "DetailSoldOut", "보유 {0} · 품절"), Owned);
				}
				else if (Item->Stock > 0)
				{
					Info = FText::Format(NSLOCTEXT("NUShop", "DetailStock", "보유 {0} · 재고 {1}"), Owned, FText::AsNumber(Item->Stock));
				}
				else
				{
					Info = FText::Format(NSLOCTEXT("NUShop", "DetailOwned", "보유 {0}"), Owned);
				}
			}
		}
		else if (bSellTab && SelectedInstanceId.IsValid())
		{
			if (const FNUItemInstance* Instance = Inventory->FindInstance(SelectedInstanceId))
			{
				RowName = Instance->ItemRowName;
				Def = Inventory->FindItemDef(RowName);
				UnitPrice = Def != nullptr ? NUShop::GetSellPrice(*Def, *Shop) : 0;
				Info = FText::Format(NSLOCTEXT("NUShop", "DetailSell", "보유 {0} · 개당 {1}골드"),
					FText::AsNumber(Instance->Count), FText::AsNumber(UnitPrice));
			}
		}
	}

	DetailPanel->SetVisibility(Def != nullptr ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Def == nullptr)
	{
		return;
	}
	const int32 Max = GetSelectedMaxCount();
	SelectedCount = FMath::Clamp(SelectedCount, 1, FMath::Max(Max, 1));

	if (DetailNameText != nullptr) { DetailNameText->SetText(Def->DisplayName); }
	if (DetailDescText != nullptr) { DetailDescText->SetText(Def->Description); }
	if (DetailInfoText != nullptr) { DetailInfoText->SetText(Info); }
	if (DetailIcon != nullptr)
	{
		UTexture2D* Icon = LoadItemIconTexture(RowName);
		DetailIcon->SetVisibility(Icon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (Icon != nullptr)
		{
			DetailIcon->SetBrushResourceObject(Icon);    // 크기는 WBP 값 그대로
		}
	}
	if (QuantityText != nullptr) { QuantityText->SetText(FText::AsNumber(SelectedCount)); }
	if (TotalText != nullptr) { TotalText->SetText(FText::AsNumber(static_cast<int64>(UnitPrice) * SelectedCount)); }
	if (QuantityMinusButton != nullptr) { QuantityMinusButton->SetIsEnabled(Max >= 1 && SelectedCount > 1); }
	if (QuantityPlusButton != nullptr) { QuantityPlusButton->SetIsEnabled(SelectedCount < Max); }
	if (QuantityMaxButton != nullptr) { QuantityMaxButton->SetIsEnabled(SelectedCount < Max); }
	if (ConfirmButton != nullptr) { ConfirmButton->SetIsEnabled(Max >= 1); }
	if (ConfirmLabel != nullptr)
	{
		ConfirmLabel->SetText(bSellTab ? NSLOCTEXT("NUShop", "SellConfirm", "판매하기") : NSLOCTEXT("NUShop", "BuyConfirm", "구매하기"));
	}
}

void UNUShopWindowWidget::ChangeQuantity(int32 Delta)
{
	const int32 Max = GetSelectedMaxCount();
	const int32 Next = FMath::Clamp(SelectedCount + Delta, 1, FMath::Max(Max, 1));
	if (Next != SelectedCount)
	{
		SelectedCount = Next;
		RefreshDetail();
	}
}

void UNUShopWindowWidget::ConfirmSelected()
{
	const FNUShopRow* Shop = GetShopRow();
	if (Shop == nullptr || !Inventory.IsValid())
	{
		return;
	}
	const int32 Max = GetSelectedMaxCount();
	if (Max < 1)
	{
		// 잠김·품절은 칸에 이미 쓰여 있다 --- 자리·골드 부족만 알린다
		if (!bSellTab)
		{
			if (const FNUShopItemRow* Item = NUShop::FindShopItemRow(SelectedShopItemId))
			{
				const FName RowName = Inventory->FindRowNameByNumericId(Item->RefId);
				if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()); UI != nullptr && Item->Price > 0 && Item->Stock != 0)
				{
					UI->ShowNotice(Inventory->GetReceivableCount(RowName) < 1
						? NSLOCTEXT("NUShop", "NotEnoughRoom", "가방에 자리가 부족합니다.")
						: NSLOCTEXT("NUShop", "NotEnoughGold", "골드가 부족합니다."));
				}
			}
		}
		return;
	}
	const int32 Count = FMath::Clamp(SelectedCount, 1, Max);
	SelectedCount = 1;     // 다음 거래는 다시 1개부터 (인벤 변경 통지로 칸이 다시 그려진다)
	if (bSellTab)
	{
		Inventory->ShopSellItem(SelectedInstanceId, Count, *Shop);
	}
	else if (const FNUShopItemRow* Item = NUShop::FindShopItemRow(SelectedShopItemId))
	{
		Inventory->ShopBuyItem(*Item, Count, *Shop);
	}
}

void UNUShopWindowWidget::HandleQuantityMinus()
{
	ChangeQuantity(-1);
}

void UNUShopWindowWidget::HandleQuantityPlus()
{
	ChangeQuantity(1);
}

void UNUShopWindowWidget::HandleQuantityMax()
{
	ChangeQuantity(GetSelectedMaxCount());
}

void UNUShopWindowWidget::HandleConfirmClicked()
{
	ConfirmSelected();
}

void UNUShopWindowWidget::CloseSelf()
{
	if (Inventory.IsValid())
	{
		Inventory->CloseShopUI();
	}
}
