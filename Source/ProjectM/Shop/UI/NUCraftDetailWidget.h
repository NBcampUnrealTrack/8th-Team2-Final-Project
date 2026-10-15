// 제작 창 오른쪽의 상세 패널 --- 고른 레시피의 결과물 아이콘, 재료 칸들, 만들 개수, 소지금과 비용, [만들기].
// 창(UNUCraftWindowWidget) 안에 붙어 있고 줄을 고르면 펼쳐진다. 세로는 창과 같고 가로는 WBP가 정한다.
// 재료 개수·최대 횟수 같은 계산은 창이 인벤에서 해서 넘겨준다 --- 이 패널은 넘겨받은 값과 만들 개수만 안다.
// 패드: 포커스가 이 패널 안에 있으면 D패드 위아래 1씩·좌우 10씩 개수를 바꾸고(수량 창과 같은 규칙), B는 목록으로 돌아간다.
// 패드 포커스는 [만들기]에만 선다 --- 개수 칸(SpinBox)은 포커스를 받으면 글자 입력 상태가 되어 표시가 멈추므로 매니저가 이동 후보에서 뺀다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   ResultIcon     (Image)               결과물 아이콘 (상단 가운데)
//   MaterialBox    (PanelWidget)         재료 칸(WBP_CraftMaterial)들이 가로로 채워진다
//   QuantitySpin   (SpinBox)             만들 개수. 1 ~ 최대 횟수
//   CraftButton    (WBP_CommonButton)    [만들기] --- 못 만들면 꺼진다
//   ResultNameText (TextBlock, 선택)     결과물 이름 (x개수 포함)
//   ResultLabelText(TextBlock, 선택)     아이콘이 없을 때 이름을 아이콘 자리에
//   OwnedGoldText  (TextBlock, 선택)     내 소지금
//   CostGoldText   (TextBlock, 선택)     이번에 드는 골드 (수수료 x 개수)
//   MaxCountText   (TextBlock, 선택)     "최대 N개"
//   ReasonText     (TextBlock, 선택)     못 만드는 이유 한 줄 (레벨 미달 / 도면 없음 / 재료 부족)
//   PadHintText    (TextBlock, 선택)     패드일 때만 보이는 조작 안내 (문구는 WBP)
//   MaterialCellClass (EditDefaultsOnly) 재료 칸 WBP
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCraftDetailWidget.generated.h"

class UImage;
class UNU_CommonButtonWidget;
class UNUCraftMaterialWidget;
class UPanelWidget;
class USpinBox;
class UTextBlock;
class UTexture2D;

// 창이 인벤에서 계산해 넘겨주는 값들 (아이콘은 넘겨받는 즉시 위젯에 꽂아 두고 따로 들고 있지 않는다)
struct FNUCraftMaterialView
{
	FText Name;
	UTexture2D* Icon = nullptr;
	int32 Have = 0;
	int32 Need = 0; // 1회 제작 기준
};

struct FNUCraftDetailData
{
	int32 RecipeId = 0;
	FText ResultName;
	UTexture2D* ResultIcon = nullptr;
	int32 ResultCount = 1;      // 1회 제작으로 나오는 개수
	TArray<FNUCraftMaterialView> Materials;
	int32 GoldPerCraft = 0;
	int64 OwnedGold = 0;
	int32 MaxCount = 0;         // 재료·골드로 만들 수 있는 최대 횟수
	FText Reason;               // 비어 있지 않으면 지금은 못 만든다 (레벨·도면·재료)
};

DECLARE_DELEGATE_TwoParams(FOnNUCraftRequested, int32 /*RecipeId*/, int32 /*Times*/);

UCLASS(Abstract)
class PROJECTM_API UNUCraftDetailWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 같은 레시피를 다시 넘기면(인벤 변화로 갱신) 만들 개수는 유지하고, 다른 레시피면 1로 돌아간다
	void SetData(const FNUCraftDetailData& Data);

	// 패드: 목록 줄에서 A를 누르면 창이 부른다. 꺼진 버튼(못 만드는 레시피)은 포커스를 못 받는다 --- 이유는 이미 패널에 보인다
	void FocusCraftButton();

	FOnNUCraftRequested OnCraftRequested;

	// 패드 B, 또는 포커스가 있던 [만들기]가 꺼졌을 때 --- 창이 목록의 고른 줄로 포커스를 돌린다
	FSimpleDelegate OnBackRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;

private:
	UFUNCTION()
	void HandleQuantityChanged(float NewValue);

	UFUNCTION()
	void HandleCraftClicked();

	// 패드 조작 안내(PadHintText)는 패드일 때만 보인다
	void HandleGamepadModeChanged(bool bGamepad);

	// 만들 개수에 따라 재료 필요 수·비용·버튼 상태를 다시 쓴다
	void RefreshForQuantity();
	int32 GetQuantity() const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ResultIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> MaterialBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USpinBox> QuantitySpin;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> CraftButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultLabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnedGoldText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CostGoldText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReasonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PadHintText;

	// 재료 칸 WBP (UNUCraftMaterialWidget 파생). WBP_CraftDetail 기본값에서 지정한다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Craft")
	TSubclassOf<UNUCraftMaterialWidget> MaterialCellClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUCraftMaterialWidget>> MaterialCells;

	// 넘겨받은 값 중 개수가 바뀔 때 다시 계산에 쓰는 것들
	int32 RecipeId = 0;
	int32 GoldPerCraft = 0;
	int64 OwnedGold = 0;
	int32 MaxCount = 0;
	bool bHardLocked = false; // 레벨·도면처럼 개수와 무관하게 막힌 상태
	TArray<int32> MaterialHave;
	TArray<int32> MaterialNeed;
};
