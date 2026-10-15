// 제작 창의 한 줄 --- 결과물 이름·아이콘과 재료 한 줄. 누르면 창이 오른쪽 상세 패널을 연다 (버튼은 상세 패널에 있다).
// 패드: 줄 자체가 포커스를 받는다(버튼이 없는 줄). 포커스가 서면 창이 그 레시피를 고르고, A는 상세 패널로 넘어가게 알린다.
// 모양은 WBP_CraftEntry가 갖고, C++는 이름 계약과 상태만 가진다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   IconImage    (Image)               결과물 아이콘
//   NameText     (TextBlock)           "하급 체력 포션 x3"
//   MaterialText (TextBlock)           "회복 물약 재료 1/3 · 광석 조각 0/1" 처럼 재료를 줄글로
//   GoldText     (TextBlock, 선택)     수수료. 0이면 접는다
//   SelectedBox  (아무 위젯, 선택)     고른 줄에만 보인다
//   LockedBox    (아무 위젯, 선택)     레벨 미달·도면 없음처럼 재료를 모아도 못 만드는 줄에만 보이는 덮개
//   LockText     (TextBlock, 선택)     그 이유 한 줄
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCraftEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FOnNUCraftEntrySelected, int32 /*RecipeId*/);
DECLARE_DELEGATE_TwoParams(FOnNUCraftEntryFocus, class UNUCraftEntryWidget* /*Entry*/, bool /*bFocused*/);

UCLASS(Abstract)
class PROJECTM_API UNUCraftEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 창이 줄을 만들고 바로 부른다. LockReason이 비어 있지 않으면 잠긴 줄로 보인다 (눌러서 상세를 보는 건 된다)
	void SetRecipe(int32 InRecipeId, const FText& InResultName, int32 ResultCount, UTexture2D* Icon,
		const FText& InMaterialText, int32 Gold, const FText& LockReason);

	void SetSelected(bool bSelected);

	int32 GetRecipeId() const { return RecipeId; }

	FOnNUCraftEntrySelected OnSelected;

	// 패드 A --- 상세 패널의 [만들기]로 넘어가 달라는 알림
	FOnNUCraftEntrySelected OnActivated;

	// 패드 포커스가 줄에 서고 빠질 때 (창이 레시피를 고르고 툴팁을 띄운다)
	FOnNUCraftEntryFocus OnEntryFocusChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MaterialText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockedBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LockText;

	int32 RecipeId = 0;
};
