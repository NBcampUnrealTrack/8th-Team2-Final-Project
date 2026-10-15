#include "Shop/UI/NUShopEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Shop/NUShopTypes.h"
#include "UI/Common/NU_CommonButtonWidget.h"

void UNUShopEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 한 번만 묶는다 --- 줄은 목록을 다시 그릴 때마다 새로 만들어지므로 재구독 걱정이 없다
	BuyButton->OnCommonClicked.AddDynamic(this, &UNUShopEntryWidget::HandleBuyClicked);
	SellButton->OnCommonClicked.AddDynamic(this, &UNUShopEntryWidget::HandleSellClicked);
}

void UNUShopEntryWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnEntryFocusChanged.ExecuteIfBound(this, true);
}

void UNUShopEntryWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	OnEntryFocusChanged.ExecuteIfBound(this, false);
}

void UNUShopEntryWidget::SetBuyEntry(const FNUShopItemRow& Item, const FText& ItemName, UTexture2D* Icon, int32 MyLevel)
{
	ShopItemId = Item.ShopItemId;
	NameText->SetText(ItemName);
	PriceText->SetText(bPriceNumberOnly ? FText::AsNumber(Item.Price)
		: FText::Format(NSLOCTEXT("NUShop", "BuyPriceLine", "{0}골드"), FText::AsNumber(Item.Price)));
	SetCommon(Icon, false);

	// 기획 5절: 레벨 미달은 잠김, 재고 0은 회색 처리하되 목록에 남긴다. 둘이 겹치면 잠김을 먼저 보여 준다.
	// 잠김·품절 줄은 버튼을 꺼서 수량 창까지 가지 않게 한다 (서버도 2206으로 거절한다)
	const bool bLocked = MyLevel > 0 && MyLevel < Item.RequiredLevel;
	SetLockAndStock(bLocked, Item.RequiredLevel, Item.Stock);
	BuyButton->SetIsEnabled(!bLocked && Item.Stock != 0);
}

void UNUShopEntryWidget::SetSellEntry(const FGuid& InInstanceId, const FText& ItemName, int32 Count, int32 UnitPrice, UTexture2D* Icon)
{
	InstanceId = InInstanceId;
	NameText->SetText(FText::Format(NSLOCTEXT("NUShop", "SellNameLine", "{0} x{1}"), ItemName, FText::AsNumber(Count)));
	PriceText->SetText(bPriceNumberOnly ? FText::AsNumber(UnitPrice)
		: FText::Format(NSLOCTEXT("NUShop", "SellPriceLine", "개당 {0}골드"), FText::AsNumber(UnitPrice)));
	SetCommon(Icon, true);
	SetLockAndStock(false, 0, -1);
}

void UNUShopEntryWidget::SetCommon(UTexture2D* Icon, bool bSell)
{
	// 그림이 없는 아이템은 칸만 비워 둔다 --- 이름이 옆에 있어 글자 폴백까지는 필요 없다
	if (Icon != nullptr)
	{
		IconImage->SetBrushFromTexture(Icon);
	}
	IconImage->SetVisibility(Icon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	BuyButton->SetVisibility(bSell ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	SellButton->SetVisibility(bSell ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UNUShopEntryWidget::SetLockAndStock(bool bLocked, int32 RequiredLevel, int32 Stock)
{
	if (LockedBox != nullptr)
	{
		LockedBox->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LockText != nullptr)
	{
		LockText->SetText(FText::Format(NSLOCTEXT("NUShop", "LockedLevel", "Lv. {0} 이상"), FText::AsNumber(RequiredLevel)));
		LockText->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StockText != nullptr)
	{
		// 무한(-1)은 표시하지 않는다. 잠긴 줄의 품절은 잠김 표시에 가려 쓰므로 접는다
		const bool bShowStock = Stock >= 0 && !(bLocked && Stock == 0);
		if (bShowStock)
		{
			StockText->SetText(Stock == 0
				? NSLOCTEXT("NUShop", "SoldOut", "품절")
				: FText::Format(NSLOCTEXT("NUShop", "StockLeft", "재고 {0}"), FText::AsNumber(Stock)));
		}
		StockText->SetVisibility(bShowStock ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUShopEntryWidget::SetSelected(bool bSelected)
{
	if (SelectedHighlight != nullptr)
	{
		SelectedHighlight->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUShopEntryWidget::SetSubText(const FText& InText)
{
	if (SubText != nullptr)
	{
		SubText->SetText(InText);
		SubText->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UNUShopEntryWidget::HandleBuyClicked()
{
	OnBuyClicked.ExecuteIfBound(ShopItemId);
}

void UNUShopEntryWidget::HandleSellClicked()
{
	OnSellClicked.ExecuteIfBound(InstanceId);
}
