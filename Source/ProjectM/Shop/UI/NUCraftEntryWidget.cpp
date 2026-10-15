#include "Shop/UI/NUCraftEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UNUCraftEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// UserWidget 기본 가시성은 SelfHitTestInvisible이라 클릭이 뒤로 빠진다 --- 줄 자체가 클릭 면이다
	SetVisibility(ESlateVisibility::Visible);
	// 줄 안에 버튼이 없어 패드 D패드가 설 곳은 줄 자체다
	SetIsFocusable(true);
}

void UNUCraftEntryWidget::SetRecipe(int32 InRecipeId, const FText& InResultName, int32 ResultCount, UTexture2D* Icon,
	const FText& InMaterialText, int32 Gold, const FText& LockReason)
{
	RecipeId = InRecipeId;

	NameText->SetText(ResultCount > 1
		? FText::Format(NSLOCTEXT("NUCraft", "ResultCount", "{0} x{1}"), InResultName, FText::AsNumber(ResultCount))
		: InResultName);
	MaterialText->SetText(InMaterialText);

	// 아이콘이 없는 아이템은 빈 칸으로 둔다 --- 이름이 이미 옆에 있다
	IconImage->SetBrushFromTexture(Icon);
	IconImage->SetVisibility(Icon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

	if (GoldText != nullptr)
	{
		GoldText->SetText(FText::AsNumber(Gold));
		GoldText->SetVisibility(Gold > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 막힌 이유는 색이 아니라 위젯으로 보인다 (색은 WBP 몫)
	const bool bLocked = !LockReason.IsEmpty();
	if (LockedBox != nullptr)
	{
		LockedBox->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LockText != nullptr)
	{
		LockText->SetText(LockReason);
		LockText->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUCraftEntryWidget::SetSelected(bool bSelected)
{
	if (SelectedBox != nullptr)
	{
		SelectedBox->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FReply UNUCraftEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnSelected.ExecuteIfBound(RecipeId);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UNUCraftEntryWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnEntryFocusChanged.ExecuteIfBound(this, true);
}

void UNUCraftEntryWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	OnEntryFocusChanged.ExecuteIfBound(this, false);
}

FReply UNUCraftEntryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom)
	{
		OnActivated.ExecuteIfBound(RecipeId);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FNavigationReply UNUCraftEntryWidget::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	// 오른쪽(스틱·D패드)은 A와 같이 상세의 [만들기]로. 못 만드는 레시피면 제자리 ---
	// 창 공용 규칙(그 방향 가장 가까운 조작부)에 맡기면 위쪽 닫기 버튼으로 갔다 (닫기는 맨 위에서 위로)
	if (InNavigationEvent.GetNavigationType() == EUINavigation::Right)
	{
		OnActivated.ExecuteIfBound(RecipeId);
		return FNavigationReply::Stop();
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}
