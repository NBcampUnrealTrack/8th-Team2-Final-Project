// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUInventoryWindowWidget.generated.h"

class UNUItemCellWidget;
class UNUItemContainerPanelWidget;
class UNUTradeSubsystem;
enum class ENUTradeState : uint8;

// 인벤토리 칸 드래그 페이로드 (출발 저장소/칸 — 인벤<=>창고 창 간 드래그를 지원한다)
UCLASS()
class UNUItemDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	int32 SourceContainer = 1;
	int32 SourceSlotNo = INDEX_NONE;

	// 거래 칸에서 끌어온 경우 --- SourceSlotNo는 거래 칸 번호(0~5)를 뜻한다.
	// 인벤 창에 놓으면 내리기, 다른 거래 칸에 놓으면 자리 이동
	bool bFromTradeOffer = false;
};

// 인벤토리/창고 창 — 칸 그리드 + 사용/장착/버리기/칸 이동. 캐릭터 정보창(P키)과 분리.
// 저장소 패널(WBP_ItemContainerPanel)과 칸(WBP_ItemCell)을 만들어 WBP의 PanelRow에 붙인다 --- 모양은 그 WBP들이 갖고 C++는 자료만 넣는다.
// 같은 클래스가 "저장소 패널 여러 개"를 나란히 그린다: 인벤 창 = {가방}, 창고 창 = {계정 창고 | 캐릭터 창고}.
// 창고 창은 전용 WBP_StorageWindow(같은 클래스)에 SetDisplayContainers({3,2})로 두 패널을 그린다 (창고는 서버 전용, 이동만 가능).
// 조작: 드래그 = 칸 이동(빈칸=이동, 같은 아이템=합치기, 그 밖의 찬칸=교환 — ItemMove(61)의 판정, 창 간·패널 간 드래그 가능),
//       인벤 칸 우클릭 = 사용/장착, 단 창고가 열려 있으면 캐릭터 창고로 빠른 이동(Alt = 계정 창고),
//       창고 칸 우클릭 = 소지품으로 빠른 이동, 인벤 창 밖에 놓으면 버리기(확인 창), Ctrl+우클릭 = 아이템 메뉴(할 수 있는 동작 목록).
// 패드(9/16): D패드로 칸 이동, A = 집기/놓기(드래그 대신 --- 집은 칸은 컴포넌트가 기억해 창고 창에서도 놓는다), X = 아이템 메뉴,
//       RT = 우클릭과 같은 빠른 동작(LT+RT = 계정 창고), LB/RB = 창 넘기기, B = 든 것 내려놓기(없으면 창 닫기), 포커스 칸 옆에 툴팁.
// 드래그 비주얼은 엔진 데코레이터(감지 위젯 위치에서 150ms 날아오는 보간이 하드코딩) 대신
// 뷰포트에 따로 올린 담개(UNUDragVisualWidget)가 틱마다 커서에 정확히 붙는다
UCLASS(Abstract)
class PROJECTM_API UNUInventoryWindowWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|Sheet")
	void UseItem(FGuid InstanceId);

	UFUNCTION(BlueprintCallable, Category = "NU|Sheet")
	void EquipItem(FGuid InstanceId);

	// 스택 전체 버리기 (임시 UI — 개수 선택 창은 정식 UI에서)
	UFUNCTION(BlueprintCallable, Category = "NU|Sheet")
	void DropItem(FGuid InstanceId);

	// 이 창이 보여줄 저장소 패널들 (왼쪽=>오른쪽 순). 컴포넌트의 ToggleStorageUI가 창고 창에 {3,2}를 준다
	void SetDisplayContainers(const TArray<int32>& InContainers);

	// 패드로 이 창에 오면 첫 패널의 첫 칸
	virtual void FocusForPad() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshData() override;
	virtual void CloseSelf() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Sheet")
	void OnInventoryListChanged();

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	TArray<FNUSheetItemEntry> InventoryItems;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	int32 UsedSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	int32 MaxSlots = 0;

	// 그리드 한 줄 칸 수 / 칸 픽셀 크기 — WBP 클래스 기본값에서 조정
	UPROPERTY(EditAnywhere, Category = "NU|Sheet", meta = (ClampMin = 1))
	int32 GridColumns = 8;

	UPROPERTY(EditAnywhere, Category = "NU|Sheet", meta = (ClampMin = 32.f))
	float CellSize = 72.f;

	// 칸 드래그 (UMG DnD — 셀 마우스다운이 창을 드래그 감지자로 등록한다)
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// 패드 A(집기/놓기)·X(메뉴)·RT(빠른 동작)·B(내려놓기). 나머지는 베이스(LB/RB)
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FName GetCellItemRowName(int32 Container, int32 SlotNo) const override;

private:
	// 가방 전체 정렬 버튼 (소지품 창 WBP에만 있다 --- 창고 창은 패널마다 자기 정렬 버튼). 타입은 WBP마다 다르다(순정 Button / WBP_CloseButton / WBP_CommonButton)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> SortButton;

	// 창의 보이는 몸체 --- 칸 수에 맞춰 창 크기를 다시 잡을 때 기준
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> WindowBG;

	// 저장소 패널들이 붙는 줄 (ItemListBox 안). 인벤 창은 1개, 창고 창은 계정|캐릭터 2개
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UPanelWidget> PanelRow;

	// 하단 조작 안내 --- 거래·상점·창고 상황에 따라 문구가 바뀐다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> HintText;

	// 골드 줄(동전 + 숫자 묶음). 소지품 창에만 보이고, 창 높이가 칸 수로 바뀌면 안내 문구 아래로 따라간다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> GoldRow;

	// 저장소 패널 WBP (UNUItemContainerPanelWidget 파생). WBP_InventoryWindow 기본값에서 지정한다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Sheet")
	TSubclassOf<class UNUItemContainerPanelWidget> PanelWidgetClass;

	// PanelRow에 저장소 패널(WBP_ItemContainerPanel)들을 채우고 하단 안내 문구를 고른다
	void PopulateItemList();
	// 저장소 하나의 패널. 가방에 분류 구간이 있으면 구간마다 패널 하나씩을 세로로 쌓은 묶음
	UWidget* BuildContainerPanel(int32 Container);
	// 칸 번호 FirstSlotNo부터 SlotCount칸을 그리는 패널 (0칸이면 불러오는 중 안내)
	class UNUItemContainerPanelWidget* BuildSlotPanel(int32 Container, const FText& PanelName, int32 FirstSlotNo, int32 SlotCount);
	class UNUItemCellWidget* BuildGridCell(int32 Container, int32 SlotNo, const FNUSheetItemEntry* Entry);
	const FNUSheetItemEntry* FindEntry(int32 Container, int32 SlotNo) const;
	FReply HandleCellMouseDown(int32 Container, int32 SlotNo, const FPointerEvent& MouseEvent);

	// 우클릭 빠른 이동 — 대상 저장소의 첫 빈 칸으로 (가방이면 그 아이템의 분류 구간에서, 가득이면 알림)
	void QuickMove(int32 FromContainer, int32 FromSlotNo, int32 ToContainer);

	// Shift + 좌클릭 — 몇 개를 뗄지 묻는 창을 띄우고, 확인하면 나누기를 요청한다
	void ShowSplitPrompt(int32 Container, int32 SlotNo, int32 StackCount);

	// 목록 높이에 맞춰 창을 줄인다 — 칸 수는 서버가 내려주고 확장도 되므로
	// WBP에 높이를 고정해두면 칸이 적을 때 아래가 비고 많을 때 잘린다.
	// 이름 계약 "ItemListBox"(내용)와 "WindowBG"(배경)의 캔버스 슬롯만 만진다
	void ResizeWindowToContent();

	// 우클릭·패드 RT의 상황별 빠른 동작 --- 창고 칸: 소지품으로, 거래 중: 올리기/내리기, 상점: 판매, 창고 열림: 창고로(bAlt = 계정 창고), 그 외: 사용/장착
	void QuickAction(int32 Container, int32 SlotNo, bool bAlt);

	// 패드 A --- 든 것이 없으면 이 칸을 집고, 있으면 여기에 놓는다 (빈칸 = 이동, 같은 아이템 = 합치기, 그 밖의 찬칸 = 교환, 같은 칸 = 내려놓기)
	void PadPickOrPlace(int32 Container, int32 SlotNo);

	// 패드 X·Ctrl+우클릭 --- 이 칸에서 할 수 있는 동작 목록 팝업 (ScreenPos = 뷰포트 좌표)
	void OpenItemMenu(int32 Container, int32 SlotNo, const FVector2D& ScreenPos);

	// 하단 안내 문구 --- 장치(패드/키보드)·거래·상점·창고 상황에 따라
	void UpdateHintText();

	// 집은 칸 표시를 칸들에 다시 칠한다 (집기·놓기·취소가 어느 창에서 일어나든 컴포넌트가 알린다)
	void UpdateHeldHighlight();
	void HandleGamepadModeChanged(bool bGamepad);

	// 거래 중 인벤 우클릭 = 거래 칸 올리기/내리기 토글 (반영은 서버 푸시로 돌아온다)
	void ToggleTradeOffer(UNUTradeSubsystem& Trade, int32 InvSlotNo);
	UNUTradeSubsystem* GetTradeSubsystem() const;

	// 거래 제안이 바뀌면 올라간 칸 표시(금색)를 다시 그린다
	void HandleTradeOfferViewChanged();
	void HandleTradeStateViewChanged(ENUTradeState NewState);

	UFUNCTION()
	void HandleStorageChanged(int32 Container);

	// 명명 계약 "SortButton" 클릭 => 컴포넌트 정렬 실행 (가방 창 전용)
	UFUNCTION()
	void HandleSortClicked();

	// 창고 창은 패널이 둘이라 WBP의 정렬 버튼 하나로는 대상이 모호하다.
	// 그래서 패널 머리글마다 버튼을 달고, 그 버튼이 자기 저장소를 실어 보낸다
	void HandleContainerSortClicked(int32 Container);

	// 드래그가 어디서 끝나든(다른 창 드롭 포함) 커서 비주얼 정리
	UFUNCTION()
	void HandleDragEnded(UDragDropOperation* InOperation);

	// 이 창이 보여주는 저장소들 (기본 {1}=인벤. 창고 창 인스턴스만 {3,2}로 바뀐다)
	TArray<int32> DisplayContainers = { 1 };

	// 컨테이너별 항목 뷰 (RefreshData가 재구성. 레거시 InventoryItems는 첫 컨테이너 미러)
	TMap<int32, TArray<FNUSheetItemEntry>> EntriesByContainer;

	// 창 높이를 마지막으로 맞춘 총 칸 수 --- 같은 칸 수면 다시 재지 않는다 (재는 순간 창이 떨린다)
	int32 LastLayoutSlotCount = INDEX_NONE;

	// 드래그 출발 (마우스다운 시 기록 => OnDragDetected에서 소비)
	int32 PendingDragContainer = 1;
	int32 PendingDragSlotNo = INDEX_NONE;

	// 드롭 판정용 캐시 — 키 (X=저장소, Y=칸). populate마다 재구성
	TMap<FIntPoint, TWeakObjectPtr<UNUItemCellWidget>> CellByCoord;
};
