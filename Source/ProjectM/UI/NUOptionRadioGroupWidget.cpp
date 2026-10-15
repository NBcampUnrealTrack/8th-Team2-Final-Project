#include "UI/NUOptionRadioGroupWidget.h"

#include "Components/Overlay.h"
#include "UI/Common/NU_CommonButtonWidget.h"

TArray<UNU_CommonButtonWidget*, TInlineAllocator<UNUOptionRadioGroupWidget::MaxOptions>> UNUOptionRadioGroupWidget::GetButtons() const
{
	return { Option0.Get(), Option1.Get(), Option2.Get(), Option3.Get(), Option4.Get() };
}

TArray<UWidget*, TInlineAllocator<UNUOptionRadioGroupWidget::MaxOptions>> UNUOptionRadioGroupWidget::GetSelectedMarks() const
{
	return { SelectedMark0.Get(), SelectedMark1.Get(), SelectedMark2.Get(), SelectedMark3.Get(), SelectedMark4.Get() };
}

void UNUOptionRadioGroupWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서도 보이게 여기서 --- 공용 버튼이 값을 기억하므로 한 번이면 된다
	if (LabelFontSize > 0)
	{
		for (UNU_CommonButtonWidget* Button : GetButtons())
		{
			if (Button != nullptr)
			{
				Button->SetLabelFontSize(LabelFontSize);
			}
		}
	}
}

void UNUOptionRadioGroupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Option0->OnCommonClicked.AddUniqueDynamic(this, &UNUOptionRadioGroupWidget::HandleOption0Clicked);
	Option1->OnCommonClicked.AddUniqueDynamic(this, &UNUOptionRadioGroupWidget::HandleOption1Clicked);
	Option2->OnCommonClicked.AddUniqueDynamic(this, &UNUOptionRadioGroupWidget::HandleOption2Clicked);
	Option3->OnCommonClicked.AddUniqueDynamic(this, &UNUOptionRadioGroupWidget::HandleOption3Clicked);
	Option4->OnCommonClicked.AddUniqueDynamic(this, &UNUOptionRadioGroupWidget::HandleOption4Clicked);
}

void UNUOptionRadioGroupWidget::SetOptions(const TArray<FString>& Labels)
{
	int32 Index = 0;
	for (UNU_CommonButtonWidget* Button : GetButtons())
	{
		const bool bUsed = Labels.IsValidIndex(Index);
		if (bUsed)
		{
			Button->SetLabelText(FText::FromString(Labels[Index]));
		}
		// 칸(버튼과 표시를 겹친 Overlay)째로 접어야 가로 줄에서 자리가 빠진다
		if (UOverlay* Cell = Cast<UOverlay>(Button->GetParent()))
		{
			Cell->SetVisibility(bUsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		else
		{
			Button->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		++Index;
	}
	RefreshVisual();
}

void UNUOptionRadioGroupWidget::SetSelectedIndex(int32 Index)
{
	SelectedIndex = (Index >= 0 && Index < MaxOptions) ? Index : INDEX_NONE;
	RefreshVisual();
}

void UNUOptionRadioGroupWidget::HandleClicked(int32 Index)
{
	SetSelectedIndex(Index);
	OnSelected.ExecuteIfBound(Index);
}

void UNUOptionRadioGroupWidget::RefreshVisual()
{
	// 표시의 생김새(바탕·밑줄·색)는 WBP 소유 --- 고른 칸의 표시만 켠다. 클릭은 아래 버튼이 받게 HitTestInvisible
	int32 Index = 0;
	for (UWidget* Mark : GetSelectedMarks())
	{
		Mark->SetVisibility(Index == SelectedIndex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		++Index;
	}
}
