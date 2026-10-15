#include "Shop/UI/NUCraftCategoryWidget.h"

#include "Components/TextBlock.h"

void UNUCraftCategoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// UserWidget 기본 가시성은 SelfHitTestInvisible이라 클릭이 뒤로 빠진다 --- 머리줄 자체가 클릭 면이다
	SetVisibility(ESlateVisibility::Visible);
	// 패드 D패드가 머리줄에도 선다 (A로 접고 편다)
	SetIsFocusable(true);
}

void UNUCraftCategoryWidget::SetCategory(const FString& InKey, const FText& InTitle, int32 InCount, bool bExpanded)
{
	Key = InKey;
	TitleText->SetText(InTitle);
	if (CountText != nullptr)
	{
		CountText->SetText(FText::Format(NSLOCTEXT("NUCraft", "CategoryCount", "({0})"), FText::AsNumber(InCount)));
	}
	// 접힘/펼침은 색이 아니라 위젯을 켜고 끈다 (모양은 WBP)
	if (ExpandedBox != nullptr)
	{
		ExpandedBox->SetVisibility(bExpanded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CollapsedBox != nullptr)
	{
		CollapsedBox->SetVisibility(bExpanded ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

FReply UNUCraftCategoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnToggled.ExecuteIfBound(Key);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UNUCraftCategoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom)
	{
		OnToggled.ExecuteIfBound(Key);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FNavigationReply UNUCraftCategoryWidget::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	// 머리줄에서 오른쪽은 제자리 (고른 레시피가 없는 줄) --- 창 공용 규칙에 맡기면 위쪽 닫기 버튼으로 갔다
	if (InNavigationEvent.GetNavigationType() == EUINavigation::Right)
	{
		return FNavigationReply::Stop();
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}
