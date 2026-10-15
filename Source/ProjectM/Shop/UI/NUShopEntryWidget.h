// 상점 목록의 줄 하나 --- 모양은 WBP_ShopEntry가 갖고, C++는 자료와 상태만 넣는다 (선택창 칸 WBP_CharacterSlot과 같은 방식).
// 구매 탭과 판매 탭이 같은 줄을 쓴다: 구매 줄은 BuyButton, 판매 줄은 SellButton만 보인다 --- 색·글자·간격은 WBP 것.
// 버튼은 민기 님 공용 부품 WBP_CommonButton(UNU_CommonButtonWidget)이라 글자는 인스턴스의 LabelText로 디자이너가 넣는다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다 --- 이름이 바뀌면 그 자리에서 드러난다.
//   IconImage   (Image)      아이템 아이콘. 그림이 없으면 Hidden (자리는 남긴다)
//   NameText    (TextBlock)  이름 --- 판매 줄은 "이름 x수량"
//   PriceText   (TextBlock)  구매 줄 "N골드", 판매 줄 "개당 N골드"
//   BuyButton   (WBP_CommonButton) [구매] --- 구매 줄에서만 보인다. 잠김·품절이면 비활성
//   SellButton  (WBP_CommonButton) [판매] --- 판매 줄에서만 보인다
//   LockedBox   (아무 위젯, 선택)  레벨 미달일 때만 보이는 덮개 (기획 5절 잠김). 색·모양은 WBP
//   LockText    (TextBlock, 선택)  "Lv. N 이상" --- LockedBox 안에 두면 같이 보인다
//   StockText   (TextBlock, 선택)  "재고 N" / "품절". 무한(-1)이면 접는다. 지금은 표의 최대치 --- 남은 재고는 서버 조회가 생기면 갱신
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUShopEntryWidget.generated.h"

class UImage;
class UNU_CommonButtonWidget;
class UTextBlock;
class UTexture2D;
struct FNUShopItemRow;

DECLARE_DELEGATE_OneParam(FOnNUShopEntryBuy, int32 /*ShopItemId*/);
DECLARE_DELEGATE_OneParam(FOnNUShopEntrySell, FGuid /*InstanceId*/);
DECLARE_DELEGATE_TwoParams(FOnNUShopEntryFocus, class UNUShopEntryWidget* /*Entry*/, bool /*bFocused*/);

UCLASS(Abstract)
class PROJECTM_API UNUShopEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 구매 줄. 가격은 품목표 Price 그대로 (등급·지역 배수 반영 완료값).
	// MyLevel이 품목 RequiredLevel보다 낮으면 잠김 (0 = 레벨을 아직 모름 = 잠그지 않는다. 최종 판정은 서버 2206)
	void SetBuyEntry(const FNUShopItemRow& Item, const FText& ItemName, UTexture2D* Icon, int32 MyLevel);

	// 판매 줄. UnitPrice는 NUShop::GetSellPrice가 정한 개당 매각가
	void SetSellEntry(const FGuid& InInstanceId, const FText& ItemName, int32 Count, int32 UnitPrice, UTexture2D* Icon);

	FOnNUShopEntryBuy OnBuyClicked;
	FOnNUShopEntrySell OnSellClicked;

	// 패드 포커스가 이 줄(안의 구매·판매 버튼)에 들어오거나 나갈 때 --- 창이 툴팁을 띄운다.
	// 포커스가 실제로 옮겨지기 전에 오므로 줄 자신을 넘긴다 (아이템 칸 알림과 같은 이유)
	FOnNUShopEntryFocus OnEntryFocusChanged;

	// 창의 "고른 품목" 칸을 쓰는 WBP용 (선택 위젯이 없으면 아무 일도 안 한다)
	void SetSelected(bool bSelected);
	void SetSubText(const FText& InText);
	int32 GetShopItemId() const { return ShopItemId; }
	const FGuid& GetInstanceId() const { return InstanceId; }

protected:
	// 가격 글자를 숫자만으로 (동전 그림은 WBP가 옆에 둔다). 끄면 "N골드" / "개당 N골드"
	UPROPERTY(EditDefaultsOnly, Category = "NU|Shop|Look")
	bool bPriceNumberOnly = false;

	// 고른 줄에만 보이는 표시 (선택)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedHighlight;

	// 이름 아래 작은 글자 --- 창이 보유 수나 설명을 넣는다 (선택)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubText;

	virtual void NativeOnInitialized() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> BuyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> SellButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockedBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LockText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StockText;

	UFUNCTION()
	void HandleBuyClicked();

	UFUNCTION()
	void HandleSellClicked();

	// 아이콘과 구매/판매 버튼 표시 --- 두 Set 함수의 공통 부분
	void SetCommon(UTexture2D* Icon, bool bSell);

	// 잠김 덮개·재고 글자 상태. 판매 줄은 둘 다 접는다
	void SetLockAndStock(bool bLocked, int32 RequiredLevel, int32 Stock);

	int32 ShopItemId = 0;
	FGuid InstanceId;
};
