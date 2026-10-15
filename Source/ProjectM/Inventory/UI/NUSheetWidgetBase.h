// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "Inventory/NUEquipmentTypes.h"
#include "NUSheetWidgetBase.generated.h"

class UNUAbilitySystemComponent;
class UNUConfirmDialog;
class UNUEquipmentComponent;
class UNUInventoryComponent;

// 장비 슬롯 1칸의 UI 뷰
USTRUCT(BlueprintType)
struct FNUSheetSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	ENUEquipSlot Slot = ENUEquipSlot::Head;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	FText SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	FText ItemName;

	// 툴팁 조회용 (빈 슬롯이면 NAME_None)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	FName ItemRowName;
};

// 인벤토리 항목 1칸의 UI 뷰
USTRUCT(BlueprintType)
struct FNUSheetItemEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	FGuid InstanceId;

	// 그리드 칸 번호 (0부터 — FNUItemInstance.SlotNo)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	int32 SlotNo = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	FName ItemRowName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	int32 Count = 0;

	// 장착 버튼 활성 조건 (장비 타입 + 필드)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	bool bEquippable = false;

	// 사용 버튼 활성 조건 (소모품 + 필드 사용 가능)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	bool bUsable = false;

	// 버리기 활성 조건 (퀘스트 아이템 제외)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	bool bDroppable = false;
};

// 커서를 따라다니는 드래그 비주얼 담개.
// 창 안에 두면 캔버스 요구 크기가 커서 위치마다 변해 창이 흔들리므로 뷰포트에 따로 올린다
UCLASS()
class UNUDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 내용을 담아 뷰포트 맨 위에 올리고 커서를 따라가게 한다 (커서가 그림 가운데). 끌기가 끝나면 부른 쪽이 RemoveFromParent로 내린다.
	// Size = 화면에 보일 한 변(뷰포트 단위) --- 내용의 제 크기가 이와 다르면 비율대로 맞춘다 (통합 메뉴의 칸은 셸이 줄여 그린다).
	// bDrawFrame = 내용 뒤에 바탕을 깐다 (바탕이 없는 옛 칸용. 제 바탕을 가진 내용은 끈다)
	static UNUDragVisualWidget* Show(APlayerController* Player, UWidget* InContent, float Size, bool bDrawFrame = true);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void FollowCursor();

	UPROPERTY(Transient)
	TObjectPtr<class UBorder> Frame;

	// 루트는 RebuildWidget에서 만들어진다 --- 그때까지 내용을 맡아 둔다
	UPROPERTY(Transient)
	TObjectPtr<UWidget> PendingContent;

	bool bDrawFrame = true;
};

// 인벤토리/캐릭터 정보 창 공통 베이스 — 컴포넌트 해석, 구독 수명, 갱신 코얼레싱을 담당.
// 자식은 RefreshData()에서 자기 데이터만 재구성하고 BIE를 발화한다.
// 팀 패턴: BlueprintReadOnly + BlueprintImplementableEvent, BindWidget 미사용. 임시 UI — 추후 UI 담당 교체 예정.
UCLASS(Abstract)
class PROJECTM_API UNUSheetWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	// X 버튼 => 자기 창 토글로 닫기 (커서/입력 모드 원복 포함)
	UFUNCTION(BlueprintCallable, Category = "NU|Sheet")
	void RequestClose();

	// 패드로 이 창에 들어올 때 포커스를 줄 곳 --- 창이 열릴 때·창을 연 뒤 패드를 잡을 때(매니저가 부른다)·LB/RB로 건너올 때.
	// 기본은 매니저 공용 규칙(첫 칸, 없으면 닫기가 아닌 첫 버튼), 목록 창은 첫 줄로 바꾼다
	virtual void FocusForPad();


protected:
	// 구독은 Construct / 해제는 Destruct 쌍 — 캐시된 위젯 재오픈 시 중복 구독 방지
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 아이템 드래그가 NU 창 위에서 끝나면 삼킨다 --- 아무도 안 받으면 출발 창(인벤)이
	// "창 밖 = 버리기"로 해석해서, 정보창·장비창 위에 놓았는데 버리기 확인이 뜨던 오해 방지.
	// 칸 배치 같은 실제 동작은 자식(인벤·거래)이 오버라이드로 먼저 처리한다
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// 창 드래그 이동 — 버튼 등이 처리하지 않은 창 영역 좌클릭 드래그로 창 전체를 옮긴다
	// (임시 UI라 타이틀바 구분 없이 전체 드래그. RenderTranslation이라 위젯 캐시와 함께 위치도 유지됨)
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// --- 패드 (9/16 3단계) ---
	// 패드 LB/RB = 열린 창 사이 포커스 넘기기 (화면 왼쪽부터). 자식은 자기 키(A·X·RT)를 먼저 보고 나머지를 Super로 넘긴다
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	bool IsGamepadMode() const;

	// 이 창 안에서 포커스를 가진 칸 (없으면 nullptr)
	class UNUItemCellWidget* FindFocusedCell() const;

	// 칸 (저장소, 칸 번호)의 아이템 행 이름 --- 포커스 툴팁용. 칸이 비었거나 모르는 창은 NAME_None
	virtual FName GetCellItemRowName(int32 Container, int32 SlotNo) const { return NAME_None; }

	// 패드로 칸에 포커스가 서면 그 칸 오른쪽에 툴팁을 띄운다 (마우스 호버 툴팁을 대신한다). MakeItemCell이 칸에 묶는다.
	// 알림은 포커스가 실제로 옮겨지기 전에 오므로(OnFocusChanging) 칸을 받아 쓴다 --- HasKeyboardFocus로는 아직 못 찾는다
	void HandleCellFocusChanged(class UNUItemCellWidget* Cell, bool bFocused);

	// 패드 모드에서 Anchor(칸·상점 줄) 오른쪽에 RowName 아이템 툴팁을 띄운다 (화면 밖이면 왼쪽). 먼저 HideFocusTooltip을 부를 것.
	// bPreferLeft = 왼쪽 먼저 (공방 줄처럼 오른쪽에 상세 패널이 붙어 있어 가리면 안 될 때), 왼쪽이 화면 밖이면 오른쪽
	void ShowFocusTooltip(UWidget* Anchor, FName RowName, bool bPreferLeft = false);
	void HideFocusTooltip();

	// 장착 --- 다른 직업 전용 장비는 보내지 않고 알린다 (서버는 직업 제한을 모른다). 인벤 우클릭·아이템 메뉴·장비창에 집어 놓기가 같이 쓴다
	void EquipItemChecked(const FGuid& InstanceId);

	// 조작이 막혔을 때 화면에 알린다 (로그만 남기면 사용자는 왜 안 되는지 모른다)
	void ShowNoticeMessage(const FText& Message) const;

	// 아이템 드래그 비주얼 (UNUDragVisualWidget). 내용은 호출부가 만든다.
	// 드래그가 끝나는 모든 경로(드롭·취소)에서 ClearDragVisual을 부를 것
	void ShowDragVisual(UWidget* InContent, float InCellSize);
	void ClearDragVisual();

	// 자식 구현: 데이터 재구성 + 자기 BIE 발화
	virtual void RefreshData() {}

	// 자식 구현: 자기 창의 토글 호출
	virtual void CloseSelf() {}

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Sheet")
	void OnBattleStateChanged();

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	bool bInBattle = false;

	bool ResolveComponents();

	// 내 캐릭터 레벨 (성장 컴포넌트, 서버 값). 0 = 아직 모름 --- 호출부는 0이면 판정을 보류한다
	int32 GetMyLevel() const;

	// 같은 프레임의 다중 변경(스왑 = 인벤 2회 + 장비 1회 + 스탯 다수)을 1회 갱신으로 코얼레싱
	void RequestRefresh();

	// 창 초기 위치 오프셋 — 드래그 이동과 같은 RenderTranslation 경로라 이후 드래그와 자연스럽게 이어진다
	// (창고 창을 인벤 왼쪽에 띄우는 용도)
	void SetWindowTranslation(const FVector2D& InTranslation);

	// 닫기 버튼. 창마다 타입이 달라(순정 Button / WBP_CloseButton / WBP_CommonButton) UWidget으로 받아 캐스팅한다.
	// Optional인 이유: 상점 창처럼 닫기 버튼이 없는 WBP도 있다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CloseButton;

	// 이름이 고정되지 않은 위젯을 찾는 헬퍼 (칸 번호·부위 이름 등 실행 중에 정해지는 것).
	// 명명 계약: ItemListBox(인벤 목록), SlotListBox(장비 슬롯 목록), StatsText(스탯 텍스트)
	class UPanelWidget* FindPanelByName(FName WidgetName) const;
	class UTextBlock* FindTextByName(FName WidgetName) const;

	// 아이템 툴팁(WBP_ItemTooltip, 프로젝트 세팅 NU UI의 ItemTooltipClass)을 만들어 채운다. 정의나 클래스가 없으면 nullptr.
	// 인벤 칸·장비창·상점 줄·거래 창·남의 정보창이 같이 쓴다 --- 아이콘만으론 정보가 없으므로 툴팁이 유일한 정보원
	class UWidget* BuildItemTooltip(FName ItemRowName) const;

	// 같은 부위에 지금 끼고 있는 장비의 정의. 없으면 nullptr --- 툴팁 비교에 쓴다.
	// 정의에는 행 이름이 없어 이름 폴백용으로 함께 돌려준다
	const struct FNUItemDefRow* FindWornEquipDef(ENUEquipType EquipType, FName& OutRowName) const;

	// 내 직업 번호 (직업 컴포넌트, 없거나 아직 안 골랐으면 0) --- 툴팁의 직업 전용 줄 색에 쓴다
	int32 GetMyJobId() const;

	// 이름 규칙으로 아이콘 텍스처를 찾는다 (NUItemUI::LoadItemIcon).
	// 없으면 nullptr --- 호출부가 글자 표기로 폴백한다 (아이콘 미제작 아이템 대비)
	class UTexture2D* LoadItemIconTexture(FName ItemRowName) const;

	// 명명 계약 "GoldText"에 소지금 잔액을 쓴다 (인벤 하단·상점 창 공용).
	// 동전 아이콘은 WBP가 옆에 둔다 --- 그래서 숫자만 쓴다. 계약이 없는 창은 무시
	void UpdateGoldText();

	// 판매 수량 창을 띄우고 확정 시 판매한다 (상점 창의 [판매] 버튼과 인벤 우클릭 공용).
	// 판매 불가 품목은 배너만 띄우고 창을 열지 않는다
	void OpenSellCountDialog(const FGuid& InstanceId, int32 ShopId);

	// 아이템 칸 WBP(프로젝트 세팅 NU UI의 ItemCellClass) 하나를 주어진 크기로 만든다 --- 인벤·장비·거래·남 정보창 공용.
	// 칸 모양(빈 칸 색·아이콘·개수 배지)은 WBP_ItemCell이 갖는다. 클래스 미지정이면 nullptr (경고는 부르는 쪽이 필요할 때)
	class UNUItemCellWidget* MakeItemCell(float InCellSize);

	// 명명 계약 "CharacterPreviewBG" 패널 안에 캐릭터 미리보기 화면을 붙인다 (정보창·장비창 공용).
	// 파츠를 UMG에서 손으로 겹치지 않는다 --- 피벗·해상도·겹침 순서·흔들림을 다시 계산해야 하는데
	// 그 계산은 Paper2D가 필드에서 이미 정확히 한다. 그래서 같은 구성의 액터를 세워 찍은
	// 렌더 타깃을 그대로 보여준다 (ANUCharacterPreviewActor).
	// 액터는 창이 닫힐 때 대여소에 돌려준다 --- NativeDestruct가 대신 한다
	void PopulateCharacterPreview();

private:
	// LB/RB --- 같은 층에 열린 창 중 화면 순서로 이웃한 창에 포커스. 창이 하나면 false
	bool FocusAdjacentWindow(int32 Step);

	// 포커스된 칸 옆에 띄운 툴팁 (뷰포트에 직접 올린다 --- 창 밖으로 나가야 해서)
	UPROPERTY(Transient)
	TObjectPtr<UWidget> FocusTooltipWidget;
	int32 FocusTooltipRetries = 0; // 아직 안 그려져 다음 틱으로 미룬 횟수 --- 새로 지은 목록 줄은 두세 프레임 뒤에야 자리가 잡힌다

	// OpenSellCountDialog의 확정 콜백 --- 수량 창이 닫히는 사이 상점이 닫혔으면 조용히 무시된다
	void HandleSellCountConfirmed(int32 Count, FGuid InstanceId, int32 ShopId);

	UPROPERTY(Transient)
	TObjectPtr<UNUDragVisualWidget> DragVisualWidget;

	// 미리보기 액터 --- 창마다 하나, 클라 로컬이라 남에게 보이지 않는다
	UPROPERTY(Transient)
	TObjectPtr<class ANUCharacterPreviewActor> PreviewActor;

	// 렌더 타깃을 띄우는 이미지 --- 한 번 만들고 재사용한다
	UPROPERTY(Transient)
	TObjectPtr<class UImage> PreviewImage;

protected:

	// 되돌릴 수 없는 동작 앞에 확인 창을 세운다.
	// ConfirmDialogClass 미지정 시 경고만 남기고 즉시 실행 (개발 중 동작이 죽지 않도록)
	void RequestConfirm(const FText& InMessage, const FText& InConfirmLabel, FSimpleDelegate OnConfirmed);

	// 확인 창 WBP — 각 창 WBP의 클래스 기본값에서 지정. 정식 UI 교체 시 이 지정만 바꾸면 된다
	UPROPERTY(EditAnywhere, Category = "NU|Sheet")
	TSubclassOf<UNUConfirmDialog> ConfirmDialogClass;

	TWeakObjectPtr<UNUInventoryComponent> Inventory;
	TWeakObjectPtr<UNUEquipmentComponent> Equipment;
	TWeakObjectPtr<UNUAbilitySystemComponent> ASC;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleEquipmentChanged();

	UFUNCTION()
	void HandleCloseClicked();

	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleOwnerBattleChanged();
	static TArray<FGameplayAttribute> GetWatchedAttributes();

	bool bRefreshQueued = false;

	// 창 드래그 상태
	bool bDraggingWindow = false;
	FVector2D DragStartScreenPos = FVector2D::ZeroVector;
	FVector2D DragStartTranslation = FVector2D::ZeroVector;
	FVector2D WindowTranslation = FVector2D::ZeroVector;
};
