// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUShopWindowWidget.generated.h"

struct FNUShopRow;

// 상점 창 --- 구매/판매 탭 + 품목 목록. NPC 상호작용(임시: nu.Shop.Open)이 열고 인벤 창이 같이 뜬다.
// 표 기반 범용 창이라 어느 상점(Shop_Key)이든 SetShop 하나로 연다. 수량은 나누기 창을 재사용한다.
//
// 명명 계약: TitleText(상점 이름) / GoldText(소지금) / ListBox(품목 목록이 붙는 패널) --- 이름으로 찾는다.
// 위젯 계약(BindWidget): BuyTabButton / SellTabButton (WBP_CommonButton). 닫기는 베이스의 CloseButton(WBP_CloseButton).
// 선택: BuyTabIndicator / SellTabIndicator --- 고른 탭에만 보이는 표시(밑줄). 모양은 WBP, C++는 켜고 끄기만.
// 품목 줄은 EntryWidgetClass(WBP_ShopEntry, UNUShopEntryWidget)로 만든다 --- 줄 모양은 그 WBP 것
UCLASS(Abstract)
class PROJECTM_API UNUShopWindowWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()

public:
	// 컴포넌트(OpenShopUI)가 부른다. 열려 있는 창에 다시 부르면 다른 상점으로 갈아탄다
	void SetShop(int32 InShopId);

	// 인벤 우클릭 판매가 "지금 열린 상점"의 매각가를 쓰기 위해 조회한다
	int32 GetShopId() const { return ShopId; }

	// 패드 LB/RB로 건너오거나 창이 열릴 때 --- 목록 첫 줄의 버튼 (잠김·품절 줄은 버튼이 꺼져 건너뛴다)
	virtual void FocusForPad() override;

	// 상점을 여는 쪽(인벤 컴포넌트 OpenShopUI)이 창 클래스 기본값에서 읽는다
	bool ShouldOpenInventory() const { return bOpenInventoryWithShop; }
	bool ShouldHideFieldHUD() const { return bHideFieldHUDWhileOpen; }
	bool ShouldBlockGameInput() const { return bBlockGameInputWhileOpen; }

protected:
	// ---- 모양 선택 (기본값이면 지금 창 그대로) ----

	// 상점을 열 때 소지품 창도 같이 띄운다. 판매를 이 창 안(판매 탭)에서 하는 WBP는 끈다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Shop|Look")
	bool bOpenInventoryWithShop = true;

	// 열려 있는 동안 필드 HUD를 접고, 캐릭터 이동을 막는다 (대화창과 같은 흐름)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Shop|Look")
	bool bHideFieldHUDWhileOpen = false;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Shop|Look")
	bool bBlockGameInputWhileOpen = false;

	// ---- 고른 품목 칸 (선택). DetailPanel이 있으면 줄의 구매·판매 버튼은 그 품목을 고르기만 하고,
	// 수량(-/+/최대, 방향키 좌우)과 실행(ConfirmButton, F)은 이 칸에서 한다 --- 나누기 창을 띄우지 않는다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DetailPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> DetailIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> DetailNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> DetailDescText;

	// "보유 N · 재고 N" / "보유 N · 개당 N골드"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> DetailInfoText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> QuantityMinusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> QuantityPlusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> QuantityMaxButton;

	// 합계 (숫자만 --- 동전 그림은 WBP)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> TotalText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> ConfirmButton;

	// "구매하기" / "판매하기"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> ConfirmLabel;

	virtual void NativeConstruct() override; // 탭 버튼 바인딩
	virtual void RefreshData() override;     // 인벤·골드가 바뀔 때마다 (베이스가 구독) 목록을 다시 그린다
	virtual void CloseSelf() override;

	// 패드 LT = 구매 탭, RT = 판매 탭 (LB/RB는 창 넘기기라서)
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void PopulateList();

	UFUNCTION()
	void HandleBuyTabClicked();

	UFUNCTION()
	void HandleSellTabClicked();

	void HandleBuyClicked(int32 ShopItemId);

	// 줄에 패드 포커스가 서면 그 줄 아이템 툴팁 (마우스는 줄에 걸어 둔 호버 툴팁을 쓴다)
	void HandleEntryFocusChanged(class UNUShopEntryWidget* Entry, bool bFocused, FName ItemRowName);
	void HandleSellClicked(FGuid InstanceId);
	void HandleBuyConfirmed(int32 Count, int32 ShopItemId);

	const FNUShopRow* GetShopRow() const;

	// ---- 고른 품목 칸 ----
	bool UsesDetailPanel() const { return DetailPanel != nullptr; }
	void SelectBuy(int32 InShopItemId);
	void SelectSell(const FGuid& InInstanceId);
	// 목록을 다시 그린 뒤: 고른 품목이 아직 있으면 표시를 되살리고, 없으면 첫 줄을 고른다
	void RestoreSelection();
	void RefreshDetail();
	int32 GetSelectedMaxCount() const;
	void ChangeQuantity(int32 Delta);
	void ConfirmSelected();

	UFUNCTION()
	void HandleQuantityMinus();

	UFUNCTION()
	void HandleQuantityPlus();

	UFUNCTION()
	void HandleQuantityMax();

	UFUNCTION()
	void HandleConfirmClicked();

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNUShopEntryWidget>> Entries;

	int32 SelectedShopItemId = 0;   // 구매 탭에서 고른 품목
	FGuid SelectedInstanceId;       // 판매 탭에서 고른 아이템
	int32 SelectedCount = 1;

	// 기획 표 Shop_ID (0 = 미지정). RowName 대신 번호를 쓴다 --- NPC 표·서버 패킷과 같은 키
	int32 ShopId = 0;
	bool bSellTab = false;

	// 품목 줄 WBP (UNUShopEntryWidget 파생). WBP_ShopWindow 기본값에서 지정한다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Shop")
	TSubclassOf<class UNUShopEntryWidget> EntryWidgetClass;

	// 명명 계약: 순정 Button(레거시)과 UNU_CommonButtonWidget(신규 공용 부품) 둘 다 지원해야 해서
	// BindWidget 타입을 UWidget으로 열어두고 .cpp에서 캐스팅해 알맞은 델리게이트/함수로 다룬다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> BuyTabButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> SellTabButton;

	// 선택: 고른 탭에만 보이는 표시(밑줄 등). 모양은 WBP, C++는 보임/숨김만 바꾼다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> BuyTabIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> SellTabIndicator;
};
