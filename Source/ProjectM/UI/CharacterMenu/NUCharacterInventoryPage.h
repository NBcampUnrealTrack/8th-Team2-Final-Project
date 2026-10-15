// 통합 캐릭터 메뉴의 인벤토리 페이지 (WBP_CharacterInventoryPage, 민기 님 디자인) --- 가방 아이템을 분류별로 보여 주고
// 고른 아이템을 오른쪽 상세 카드에 띄운다. 페이지는 내용만 맡는다: 열고 닫기·탭 전환·입력 막기는 셸(UNUCharacterMenuBase)의 일.
// 칸의 모양은 칸 WBP(CellClass = WBP_CharacterInventoryCell)가 갖는다 --- 페이지는 가방 칸 수만큼 만들어 InventoryGrid에 놓고
// 칸 안의 위젯을 이름으로 찾아 아이콘·글자·보임만 바꾼다. 디자이너에서는 PreviewCellCount만큼 미리보기 칸을 놓는다
// (디자이너는 보임 값과 상관없이 칸의 모든 층을 그리므로 게임 화면의 빈 칸 모양과는 다르다).
//
// 이름 계약 (없는 위젯은 그 기능만 빠진다):
//   InventoryGrid(UniformGridPanel, 필수) --- 칸을 놓는 격자. 칸 사이 간격과 칸 크기는 격자와 칸 WBP의 값 그대로
//   칸 WBP 안: Cell... (FNUCharacterMenuSlot 참고)
//       CellButton은 아이템이 든 칸의 몸체라 빈 칸이면 접고, CellEmpty·CellEmptyMark는 빈 칸일 때만 보인다
//       CellLevel에는 장비 "Lv. N"·묶음 "xN"을 적는다 (아이콘이 없는 아이템은 이름)
//       CellRarity는 등급이 있는 아이템만 등급 색으로 보인다
//       CellHeld는 패드로 집은 칸일 때만 보인다
//       CellEquippedBadge·CellEquippedText는 쓰지 않는다 (장착하면 가방에서 빠진다) --- 칸 WBP에 접힌 채로 있다
//   분류 탭: Category{Equipment,Consumable,Material,Other}Button(Button) + {..}Label(TextBlock), CategorySelectedRule(고른 탭 밑줄)
//   필터 탭: Filter{All,Weapon,Armor,Accessory}Button + {..}Label --- 장비 분류에서만 보인다
//   SortButton(Button) + SortLabel(TextBlock) --- 누르면 정렬 기준 목록 SortOptions를 폈다 접는다
//   SortOptions(아무 위젯, 처음에는 접혀 있다) 안의 SortOption{Kind,Grade}Button(Button) + {..}Label(TextBlock) --- 고르면 그 기준으로 바로 정렬한다.
//       목록의 자리·크기·모양은 WBP가 정렬 상자에 맞춰 갖는다 (코드는 폈다 접기와 글자만). 목록·정렬 상자 밖을 누르거나 패드 포커스가 밖으로 나가거나 B·ESC면 접힌다.
//       SortOptions가 없는 WBP면 정렬 상자가 예전처럼 바로 종류 순으로 정렬한다
//   InventoryCount(TextBlock "22 / 30") / InventoryScroll(ScrollBox)
//   ViewLargeButton·ViewCompactButton(Button) --- 큰 칸 / 작은 칸 보기. 고른 모양·보통 모양은 WBP의 처음 상태(큰 칸이 골라져 있음)에서 읽는다.
//       작은 칸은 칸 WBP 뿌리(SizeBox)의 크기만 줄이고 안의 그림은 칸 WBP의 ScaleBox가 줄여 그린다. 고른 보기는 게임 설정에 남는다
//   DismantleButton·LockButton --- 기능이 없어 늘 꺼 둔다
//   상세 카드: FNUCharacterMenuItemCard 참고
// 고른 탭·보통 탭의 모양은 WBP에 그려진 처음 상태(각 묶음의 첫 탭이 골라져 있음)에서 읽어 서로 바꿔 끼운다.
//
// 조작: 칸 클릭 = 고르기, [장착]/[사용] 버튼, 칸 우클릭 = 바로 장착·사용, Ctrl+우클릭 = 아이템 메뉴(나누기·버리기 포함),
//       칸을 끌어 다른 칸에 놓기 = 그 자리로 옮기기 (빈 칸 = 이동, 같은 아이템 = 합치기, 그 밖의 찬 칸 = 맞바꾸기 --- 옛 소지품 창과 같은
//       ItemMove 61의 판정). 칸 밖에 놓으면 아무 일도 없다.
// 패드 (옛 소지품 창과 같은 자리): D패드로 칸 이동(빈 칸에도 선다), A = 집기/놓기(끌어 놓기 대신 --- 같은 칸에 다시 누르면 내려놓기),
//       B = 든 것 내려놓기(없으면 셸이 메뉴를 닫는다), RT = 바로 장착·사용, X = 아이템 메뉴. 포커스가 선 칸의 아이템이 상세 카드에 뜬다.
//       집은 칸은 인벤 컴포넌트가 기억한다(FNUPadHeldItem) --- 분류 탭을 바꿔 다른 칸에 놓을 수 있고, 페이지가 내려가면 비운다.
//       가방에 분류 구간이 있으면 다른 분류의 탭에는 못 놓는다 (안내만 하고 든 채로 둔다).
//
// 칸이 가리키는 것: 가방에 분류 구간이 있으면(UNUInventoryComponent::GetBagSection) 이 탭 구간의 칸 전부 --- 보이는 칸 수가 탭의 칸 수다.
// 구간이 없으면(한 통 가방) 가방 칸 순서대로 이 분류의 아이템이 든 칸과 빈 칸이고, 다른 분류의 아이템이 차지한 칸은 건너뛴다.
// 어느 쪽이든 보이는 빈 칸은 진짜 빈 가방 칸이다 (놓은 아이템이 그 자리에 남는다). 필터는 구간 안에서 아이템이 든 칸만 거른다
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/CharacterMenu/NUCharacterMenuItemCard.h"
#include "UI/NUPadFocusTarget.h"
#include "NUCharacterInventoryPage.generated.h"

class UNUDragVisualWidget;
class UNUInventoryComponent;
class UScrollBox;
class UUniformGridPanel;

UCLASS(Abstract)
class PROJECTM_API UNUCharacterInventoryPage : public UUserWidget, public INUPadFocusTarget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void RefreshPage();

	// 패드 첫 자리 = 첫 칸 (옛 소지품 창과 같다). 키보드나 전체 메뉴로 이 페이지를 열어도 왼쪽 메뉴가 아니라 칸에 선다
	virtual UWidget* GetPadFocusTarget() const override;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 칸 위의 마우스 누름은 터널 단계에서 받는다 --- 버튼은 우클릭을 안 받고(그 위의 스크롤 상자가 끌어서 스크롤로 먼저 삼킨다),
	// 좌클릭은 버튼이 먹으면 끌기를 시작할 수 없다
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 패드 A는 터널 단계에서 받는다 --- 아이템 칸은 안의 버튼이 포커스를 받고, 버튼은 A를 누름으로 먼저 삼킨다
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 패드로 칸에 서면 그 아이템을 상세 카드에 띄운다
	virtual void NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

	// 칸 끌기 (누름이 끌기 감지를 걸어 둔다). 옛 소지품 창과 같은 페이로드(UNUItemDragOperation)를 쓴다
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	FNUCharacterMenuSlotStyle SlotStyle;

	// 칸 하나의 WBP. 비우면 칸이 안 그려진다 (경고)
	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu")
	TSubclassOf<UUserWidget> CellClass;

	// 한 줄에 놓는 칸 수
	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu", meta = (ClampMin = 1))
	int32 CellColumns = 5;

	// 작은 칸 보기의 한 줄 칸 수와 칸 한 변 --- 기본값이면 한 탭 30칸이 스크롤 없이 든다 (큰 칸 보기는 CellColumns와 칸 WBP에 그려진 크기)
	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu", meta = (ClampMin = 1))
	int32 CompactCellColumns = 7;

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu", meta = (ClampMin = 1))
	float CompactCellSize = 85.f;

	// 디자이너에서 미리 보일 칸 수 (게임에서는 가방 칸 수만큼 만든다)
	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu", meta = (ClampMin = 0))
	int32 PreviewCellCount = 30;

private:
	// 칸 하나가 가리키는 가방 칸 (가방 칸 순). 빈 칸이면 InstanceId가 비어 있다
	struct FShownSlot
	{
		int32 SlotNo = INDEX_NONE;
		FGuid InstanceId;
		FName RowName;
		int32 Count = 0;
	};

	// 탭 묶음 (분류 / 필터)
	struct FTabGroup
	{
		TArray<TWeakObjectPtr<UButton>, TInlineAllocator<4>> Buttons;
		TArray<TWeakObjectPtr<UTextBlock>, TInlineAllocator<4>> Labels;
		FButtonStyle SelectedStyle;
		FButtonStyle IdleStyle;
		FSlateColor SelectedLabelColor;
		FSlateColor IdleLabelColor;
		// 버튼 안의 그림(보기 전환 버튼의 격자 무늬)도 같이 바꿔 칠할 때만 --- 분류·필터 탭은 글자뿐이라 안 쓴다
		bool bTintImages = false;
		FSlateColor SelectedImageTint;
		FSlateColor IdleImageTint;
	};

	void BindTabGroup(FTabGroup& Group, std::initializer_list<const TCHAR*> Prefixes, void (UNUCharacterInventoryPage::*Handler)(int32), bool bTintImages = false);
	void ApplyTabGroup(const FTabGroup& Group, int32 SelectedIndex) const;

	// 고른 분류 탭 밑으로 밑줄을 옮기고, 필터 탭은 장비 분류에서만 보인다
	void UpdateTabs();

	// 칸이 Count개가 되게 모자란 만큼 만들어 격자에 놓는다 (줄지는 않는다)
	void EnsureCells(int32 Count);

	// 고른 보기(큰 칸 / 작은 칸)대로 칸 크기와 격자 자리를 맞춘다
	void ApplyCellView();

	// 칸 WBP 하나의 크기를 고른 보기에 맞춘다 (끌기 그림에도 쓴다)
	void SizeCell(UUserWidget& CellWidget);

	void FillCell(const FNUCharacterMenuSlot& Cell, const FShownSlot& Shown) const;

	// 고른 칸 표시와 상세 카드 (목록은 그대로)
	void UpdateSelection();

	const FShownSlot* FindShownItem(const FGuid& InstanceId) const;

	// 그 번호의 칸에 아이템이 들어 있는지
	bool HasItemAt(int32 Index) const;

	// 조건(마우스가 가리킨다)에 맞는 아이템 칸의 번호
	int32 FindCell(bool (UWidget::*Test)() const) const;

	// 포커스가 선 칸의 번호 (빈 칸 포함 --- 아이템 칸은 안의 버튼에, 빈 칸은 칸 자체에 포커스가 선다)
	int32 FindFocusedCell() const;

	// 패드 A --- 든 것이 없으면 이 칸을 집고, 있으면 여기에 놓는다 (같은 칸 = 내려놓기, 그 밖에는 끌어 놓기와 같은 이동)
	void PadPickOrPlace(int32 Index);

	// 패드로 집은 칸 표시를 다시 칠한다
	void UpdateHeld();

	bool IsGamepadMode() const;
	void HandleGamepadModeChanged(bool bGamepad);

	// 화면의 그 자리에 놓인 칸의 번호 (빈 칸 포함, 스크롤 상자 밖으로 잘린 칸은 제외)
	int32 FindCellAt(const FVector2D& ScreenPos) const;

	// 칸에 대고 하는 동작 (우클릭·패드 공용) --- 무엇에 손댔는지 보이게 그 칸을 고른 뒤 아이템 메뉴를 열거나 바로 장착·사용한다
	void ActOnCell(int32 Index, bool bOpenMenu, const FVector2D& MenuViewportPos);

	// 장비는 장착, 소모품은 사용 (전투 중이거나 쓸 수 없는 아이템이면 아무것도 안 한다)
	void ActivateItem(const FShownSlot& Item);
	bool CanActivate(const FNUItemDefRow* Def) const;
	bool IsInBattle() const;

	void OpenItemMenu(const FShownSlot& Item, const FVector2D& ViewportPos);

	// 끌기가 끝났다 (놓음·취소) --- 커서를 따라가던 그림을 내린다
	void ClearDrag();

	// 메뉴·수량 창을 띄울 때 주인으로 넘길 매니저 창 (이 페이지를 담은 셸). 셸 밖에서 단독으로 쓰이면 자기 자신
	UUserWidget* GetOwningWindow();

	void HandleCellClicked(int32 Index);
	void HandleCategoryClicked(int32 Index);
	void HandleFilterClicked(int32 Index);
	void HandleViewClicked(int32 Index);
	void HandleSortOptionClicked(int32 Index);

	// 정렬 기준 목록을 폈다 접는다. 패드는 펴면 첫 기준에, 접으면 정렬 상자에 선다.
	// bMovePadFocus가 거짓이면 포커스는 건드리지 않는다 (페이지를 다시 만들 때, 패드 포커스가 이미 목록 밖으로 나갔을 때)
	void SetSortOptionsOpen(bool bOpen, bool bMovePadFocus = true);
	void HandleBattleChanged();

	UFUNCTION()
	void HandleSortClicked();

	UFUNCTION()
	void HandleActionClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InventoryCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> InventoryScroll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SortButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SortLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SortOptions;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DismantleButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LockButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CategorySelectedRule;

	// 버튼마다 하나씩 (칸·분류·필터) --- 페이지가 살아 있는 동안 붙잡아 둔다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUIndexedButtonRelay>> ButtonRelays;

	TArray<FNUCharacterMenuSlot> Cells;
	FNUCharacterMenuItemCard Card;
	FTabGroup CategoryTabs;
	FTabGroup FilterTabs;
	FTabGroup ViewTabs;
	FTabGroup SortOptionTabs; // 정렬 기준 목록의 버튼·글자 (번호 = ENUItemSortMode)

	// 작은 칸 보기인지 (UNUGameUserSettings에 남는다). 큰 칸의 크기는 칸 WBP와 격자에 그려진 값을 처음에 읽어 둔다
	bool bCompactView = false;
	float DesignCellSize = 0.f;
	FVector2D DesignSlotMin = FVector2D::ZeroVector;

	// 밑줄이 분류 탭 안에서 놓이는 자리 (WBP의 첫 탭에 그려진 값)
	FAnchorData CategoryRuleLayout;
	int32 CategoryRuleZOrder = 0;

	// 커서를 따라가는 끌기 그림 (끄는 동안만)
	UPROPERTY(Transient)
	TObjectPtr<UNUDragVisualWidget> DragVisual;

	// 끌기 출발 가방 칸 (누를 때 적어 두고 끌기가 감지되면 쓴다)
	int32 DragSourceSlotNo = INDEX_NONE;

	TArray<FShownSlot> ShownSlots;
	FGuid SelectedInstanceId;
	int32 Category = 0; // 분류 탭 번호 (NUBagCategory --- 0 장비 / 1 소모품 / 2 재료 / 3 기타)
	int32 Filter = 0;   // 필터 탭 번호 (0 전체 / 1 무기 / 2 방어구 / 3 장신구)

	TWeakObjectPtr<UNUInventoryComponent> Inventory;
};
