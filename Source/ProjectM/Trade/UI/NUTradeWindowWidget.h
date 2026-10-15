// 1:1 거래 창 --- 내 제안 칸과 상대 제안 칸을 나란히 보여주고 잠금·확정을 처리한다.
//
// 팀 패턴: C++는 로직/계약만, 모양은 WBP가 담당한다.
//   - 이름 계약: MyOfferBox / PartnerOfferBox (칸이 들어갈 패널), LockButton, ConfirmButton
//   - 선택 계약: PartnerNameText, PartnerOfferTitle, LockLabel(잠금 버튼 글자),
//               MyLockCover / PartnerLockCover(잠금 덮개 --- C++는 보임/숨김만 바꾼다)
//   - 칸은 WBP_ItemCell(프로젝트 세팅 ItemCellClass)을 제안 칸 수만큼 만들어 패널에 붙인다 (인벤 그리드와 같은 칸)
#pragma once

#include "CoreMinimal.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "Trade/NUTradeTypes.h"
#include "NUTradeWindowWidget.generated.h"

class UNUTradeSubsystem;
class UNUTradeUIController;
class UWidget;

UCLASS(Abstract)
class PROJECTM_API UNUTradeWindowWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()

public:
	// WBP가 버튼에 물리거나, 이름 계약이 있으면 C++가 자동으로 묶는다
	UFUNCTION(BlueprintCallable, Category = "NU|Trade")
	void ToggleLock();

	UFUNCTION(BlueprintCallable, Category = "NU|Trade")
	void ConfirmTrade();

	// 패드 LB/RB로 건너오거나 창이 열릴 때 --- 내 제안 첫 칸
	virtual void FocusForPad() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshData() override;
	virtual void CloseSelf() override;

	// 인벤·거래 칸에서 끌어온 것을 받는다 (놓은 칸에 배치, 칸 밖이면 첫 빈 칸)
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// 내 거래 칸을 끌어 인벤으로 내리거나 다른 칸으로 옮긴다
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	// 패드 --- 내 제안 칸: A 집기/놓기(소지품에서 집어 온 것은 그 칸에 올림), RT 내리기, X 메뉴. 상대 칸은 툴팁만. B는 집은 것 내려놓기
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 포커스 툴팁용 --- 내 제안·상대 제안 칸의 아이템
	virtual FName GetCellItemRowName(int32 Container, int32 SlotNo) const override;

	// WBP가 직접 그릴 때 쓰는 통지
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Trade")
	void OnTradeViewChanged();

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	FText PartnerName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	bool bMyLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	bool bPartnerLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	bool bMyConfirmed = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	bool bPartnerConfirmed = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	bool bCanConfirm = false;

	// 칸 픽셀 크기 (WBP 클래스 기본값에서 조정)
	UPROPERTY(EditAnywhere, Category = "NU|Trade", meta = (ClampMin = 32.f))
	float CellSize = 72.f;

	// 제안 그리드 열 수 (6칸 = 3x2)
	UPROPERTY(EditAnywhere, Category = "NU|Trade", meta = (ClampMin = 1))
	int32 GridColumns = 3;

private:
	// 명명 계약: 순정 Button(레거시)과 UNU_CommonButtonWidget(신규 공용 부품) 둘 다 지원해야 해서
	// BindWidget 타입을 UWidget으로 열어두고 .cpp에서 캐스팅해 알맞은 델리게이트/함수로 다룬다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> LockButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> ConfirmButton;

	UFUNCTION()
	void HandleOfferChanged();

	UFUNCTION()
	void HandleLockClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	// 이름 계약 패널에 칸을 다시 그린다
	void PopulateOffers();
	void BuildOfferInto(const TCHAR* PanelName, const FNUTradeOffer& Offer, bool bMine);

	// 칸에 아이템을 채운다 (아이콘·개수, 아이콘 없으면 이름). 드래그 비주얼도 같은 것을 쓴다
	void FillOfferCell(class UNUItemCellWidget* Cell, const FNUTradeSlot& OfferSlot) const;

	// 드래그가 어디서 끝나든(인벤 창 드롭 포함) 비주얼 정리
	UFUNCTION()
	void HandleDragEnded(UDragDropOperation* InOperation);

	// 내 거래 칸 클릭 --- 우클릭은 내리기, 좌클릭은 드래그 시작
	FReply HandleOfferCellMouseDown(int32 Container, int32 TradeSlot, const FPointerEvent& MouseEvent);

	// 내 거래 칸에 놓기 --- 마우스 드롭과 패드 A가 같이 쓴다. 출발이 거래 칸이면 자리 이동·교환,
	// 소지품 칸이면 그 칸을 올린다 (DropSlot이 INDEX_NONE이면 첫 빈 칸)
	void PlaceIntoOffer(UNUTradeSubsystem& Trade, bool bFromOffer, int32 SourceSlot, int32 DropSlot);

	// 거래 칸 하나 내리기 --- 우클릭·패드 RT·메뉴
	void TakeBackOffer(int32 TradeSlot);

	// 패드 A --- 든 것이 없으면 이 칸을 집고, 들고 있으면 여기 놓는다
	void PadPickOrPlaceOffer(int32 TradeSlot);

	// 패드로 집은 거래 칸 표시 (집은 상태는 인벤 컴포넌트가 갖는다 --- 소지품 창과 공유)
	void UpdateHeldHighlight();

	// 놓인 지점의 내 거래 칸 번호 (칸 밖이면 INDEX_NONE)
	int32 FindOfferSlotAt(const FVector2D& ScreenPos) const;

	// 조작이 막혔을 때 화면에 알린다
	void ShowTradeNotice(const FText& Message) const;

	// 잠긴 쪽 덮개를 보이거나 숨긴다 (덮개 자체는 WBP 소유)
	void UpdateLockCovers();

	// 잠금 라벨과 확정 버튼 활성 상태를 지금 상태에 맞춘다
	void UpdateActionButtons();

	UNUTradeSubsystem* GetTradeSubsystem() const;
	UNUTradeUIController* GetTradeController() const;

	// 드롭 판정용 --- 내 거래 칸 번호로 찾는다. 다시 그릴 때마다 재구성
	TMap<int32, TWeakObjectPtr<class UNUItemCellWidget>> MyCellBySlot;

	// 드래그 출발 칸 (마우스다운에 기록 => OnDragDetected에서 소비)
	int32 PendingDragTradeSlot = INDEX_NONE;

	// 창이 열린 뒤 첫 새로고침에서 내 첫 칸에 포커스 (칸은 다음 틱 새로고침에서 생긴다)
	bool bFocusOfferOnRefresh = false;
};
