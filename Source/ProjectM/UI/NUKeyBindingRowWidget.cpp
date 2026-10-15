#include "UI/NUKeyBindingRowWidget.h"

#include "Components/TextBlock.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUKeyBindingsWindow.h" // NUKeyBindings::GetKeyText
#include "UI/NUUIManager.h" // FocusFirstButtonIn

namespace
{
	// 칸 하나 (키 글자 / 비었음 글자 / 바뀜 표시). 패드 칸의 고정 표기는 키 자리에 그 글자를 넣고 "비었음"은 안 켠다
	void ApplyKeyColumn(UTextBlock* KeyText, UTextBlock* EmptyText, UWidget* ChangedBox, const FKey& Key, bool bChanged, const FText& FixedText)
	{
		if (KeyText == nullptr)
		{
			return;
		}
		const bool bFixed = !FixedText.IsEmpty();
		const bool bEmpty = !bFixed && !Key.IsValid();
		KeyText->SetText(bFixed ? FixedText : (bEmpty ? FText::GetEmpty() : NUKeyBindings::GetKeyText(Key)));
		KeyText->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (EmptyText != nullptr)
		{
			EmptyText->SetVisibility(bEmpty ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (ChangedBox != nullptr)
		{
			ChangedBox->SetVisibility(bChanged ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UNUKeyBindingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 한 번만 묶는다 --- 줄은 목록을 다시 그릴 때마다 새로 만들어진다
	ChangeButton->OnCommonClicked.AddDynamic(this, &UNUKeyBindingRowWidget::HandleChangeClicked);
	if (PadChangeButton != nullptr)
	{
		PadChangeButton->OnCommonClicked.AddDynamic(this, &UNUKeyBindingRowWidget::HandlePadChangeClicked);
	}
}

void UNUKeyBindingRowWidget::SetBinding(FName InMappingName, const FText& InDisplayName,
	const FKey& InKey, bool bChanged,
	const FKey& InPadKey, bool bPadChanged, const FText& PadFixedText)
{
	MappingName = InMappingName;
	NameText->SetText(InDisplayName);

	// 키가 없는 상태는 글자를 비우는 대신 EmptyText를 켠다 --- 색으로만 구분하면 WBP가 못 바꾼다
	ApplyKeyColumn(KeyText, EmptyText, ChangedBox, InKey, bChanged, FText::GetEmpty());
	ApplyKeyColumn(PadKeyText, PadEmptyText, PadChangedBox, InPadKey, bPadChanged, PadFixedText);

	// 고정 표기 줄(이동)은 패드 칸을 못 바꾼다
	if (PadChangeButton != nullptr)
	{
		PadChangeButton->SetVisibility(PadFixedText.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	SetWaiting(false, false);
}

void UNUKeyBindingRowWidget::SetWaiting(bool bWaiting, bool bGamepad)
{
	if (WaitingBox != nullptr)
	{
		WaitingBox->SetVisibility(bWaiting && !bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (PadWaitingBox != nullptr)
	{
		PadWaitingBox->SetVisibility(bWaiting && bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 기다리는 동안에는 다른 줄의 버튼을 눌러 상태가 겹치지 않게 한다
	ChangeButton->SetIsEnabled(!bWaiting);
	if (PadChangeButton != nullptr)
	{
		PadChangeButton->SetIsEnabled(!bWaiting);
	}
}

void UNUKeyBindingRowWidget::FocusChangeButton(bool bGamepad)
{
	UNUUIManager::FocusFirstButtonIn(bGamepad && PadChangeButton != nullptr ? PadChangeButton : ChangeButton);
}

void UNUKeyBindingRowWidget::HandleChangeClicked()
{
	OnChangeRequested.ExecuteIfBound(MappingName, false);
}

void UNUKeyBindingRowWidget::HandlePadChangeClicked()
{
	OnChangeRequested.ExecuteIfBound(MappingName, true);
}
