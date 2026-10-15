#include "Inventory/UI/NUItemContainerPanelWidget.h"

#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "UI/Common/NU_CommonButtonWidget.h"

void UNUItemContainerPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 한 번만 묶는다 --- 패널은 목록을 다시 그릴 때마다 새로 만들어진다
	if (SortButton != nullptr)
	{
		SortButton->OnCommonClicked.AddDynamic(this, &UNUItemContainerPanelWidget::HandleSortClicked);
	}
}

void UNUItemContainerPanelWidget::SetPanelWidth(float Width)
{
	PanelSizeBox->SetWidthOverride(Width);
}

void UNUItemContainerPanelWidget::SetHeader(const FText& ContainerName, int32 Used, int32 SlotCount)
{
	const FText Line = FText::Format(NSLOCTEXT("NUInventory", "PanelHeader", "{0}  {1} / {2}"),
		ContainerName, FText::AsNumber(Used), FText::AsNumber(SlotCount));

	// 가득 차면 FullText(붉은색은 WBP) 쪽만 --- FullText를 두지 않은 WBP면 HeaderText에 그대로 쓴다
	const bool bUseFull = Used >= SlotCount && FullText != nullptr;
	HeaderText->SetText(Line);
	HeaderText->SetVisibility(bUseFull ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (FullText != nullptr)
	{
		FullText->SetText(Line);
		FullText->SetVisibility(bUseFull ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LoadingText != nullptr)
	{
		LoadingText->SetVisibility(ESlateVisibility::Collapsed);
	}
	Grid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UNUItemContainerPanelWidget::SetLoading(const FText& ContainerName)
{
	HeaderText->SetText(ContainerName);
	HeaderText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (FullText != nullptr)
	{
		FullText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (LoadingText != nullptr)
	{
		LoadingText->SetText(NSLOCTEXT("NUInventory", "StorageLoading", "불러오는 중... (서버 로그인 필요)"));
		LoadingText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Grid->ClearChildren();
	Grid->SetVisibility(ESlateVisibility::Collapsed);
	SetSortButtonVisible(false);
}

void UNUItemContainerPanelWidget::SetSortButtonVisible(bool bVisible)
{
	if (SortButton != nullptr)
	{
		SortButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UNUItemContainerPanelWidget::AddCell(UWidget* Cell, int32 Index, int32 Columns)
{
	const int32 ColumnCount = FMath::Max(1, Columns);
	if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Cell, Index / ColumnCount, Index % ColumnCount))
	{
		// 칸이 균등 격자를 채우며 늘어나지 않게 --- 칸 크기 그대로 정사각형 유지
		GridSlot->SetHorizontalAlignment(HAlign_Center);
		GridSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UNUItemContainerPanelWidget::HandleSortClicked()
{
	OnSortClicked.ExecuteIfBound(Container);
}
