#include "Inventory/UI/NUItemCellWidget.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	// 선택 위젯 보이기/접기 --- WBP가 뺀 위젯(nullptr)은 건너뛴다. 보일 때도 클릭은 칸(UserWidget)이 받게 HitTestInvisible
	void ShowOptional(UWidget* Widget, bool bShow)
	{
		if (Widget != nullptr)
		{
			Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	void SetOptionalText(UTextBlock* Text, const FText& Value, bool bShow)
	{
		if (Text != nullptr && bShow)
		{
			Text->SetText(Value);
		}
		ShowOptional(Text, bShow);
	}
}

void UNUItemCellWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// UserWidget 기본값(SelfHitTestInvisible)은 마우스를 안 받는다 --- 칸은 클릭·드래그의 출발점이라 스스로 받아야 한다
	SetVisibility(ESlateVisibility::Visible);
	// 패드 D패드가 칸 사이를 오가려면 칸이 포커스를 받아야 한다 (마우스 클릭은 포커스를 안 준다 --- 창이 Handled로 끝낸다)
	SetIsFocusable(true);
}

void UNUItemCellWidget::SetCoords(int32 InContainer, int32 InSlotNo)
{
	Container = InContainer;
	SlotNo = InSlotNo;
}

void UNUItemCellWidget::SetCellSize(float Size)
{
	CellSizeBox->SetWidthOverride(Size);
	CellSizeBox->SetHeightOverride(Size);
}

void UNUItemCellWidget::SetItem(UTexture2D* Icon, int32 Count, const FText& Label, bool bOffered)
{
	if (Icon != nullptr)
	{
		IconImage->SetBrushFromTexture(Icon);
	}
	ShowOptional(IconImage, Icon != nullptr);
	// 아이콘이 없는 아이템은 이름이 아이콘 노릇을 한다 (아이콘 미제작 대비)
	SetOptionalText(LabelText, Label, Icon == nullptr);
	SetOptionalText(CountText, FText::Format(NSLOCTEXT("NUInventory", "CellCount", "x{0}"), FText::AsNumber(Count)), Count > 1);
	ShowOptional(EmptyBox, false);
	ShowOptional(OfferedBox, bOffered);
	ShowOptional(HeldBox, false);
}

void UNUItemCellWidget::SetEmpty(const FText& Label)
{
	ShowOptional(IconImage, false);
	SetOptionalText(LabelText, Label, !Label.IsEmpty());
	ShowOptional(CountText, false);
	ShowOptional(EmptyBox, true);
	ShowOptional(OfferedBox, false);
	ShowOptional(HeldBox, false);
}

void UNUItemCellWidget::SetHeld(bool bHeld)
{
	ShowOptional(HeldBox, bHeld);
}

FReply UNUItemCellWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (OnCellMouseDown.IsBound())
	{
		return OnCellMouseDown.Execute(Container, SlotNo, InMouseEvent);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UNUItemCellWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnCellFocusChanged.ExecuteIfBound(this, true);
}

void UNUItemCellWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	OnCellFocusChanged.ExecuteIfBound(this, false);
}
