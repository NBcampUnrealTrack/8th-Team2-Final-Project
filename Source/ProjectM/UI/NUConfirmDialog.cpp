// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/NUConfirmDialog.h"

#include "UI/NUUISettings.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Inventory/NUInventoryTypes.h" // LogNUInventory
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"

UNUConfirmDialog* UNUConfirmDialog::Show(
	APlayerController* OwningPlayer,
	TSubclassOf<UNUConfirmDialog> DialogClass,
	const FText& InMessage,
	const FText& InConfirmLabel,
	FSimpleDelegate InOnConfirmed,
	FSimpleDelegate InOnCancelled,
	const FText& InCancelLabel,
	ENUConfirmIntent InIntent,
	bool bNoticeOnly)
{
	if (OwningPlayer == nullptr || DialogClass == nullptr)
	{
		return nullptr;
	}

	UNUConfirmDialog* Dialog = CreateWidget<UNUConfirmDialog>(OwningPlayer, DialogClass);
	if (Dialog == nullptr)
	{
		return nullptr;
	}

	// NativeConstruct(AddToViewport 시점)에서 읽으므로 먼저 채운다
	Dialog->Message = InMessage;
	Dialog->ConfirmLabel = InConfirmLabel;
	Dialog->CancelLabel = InCancelLabel;
	Dialog->Intent = InIntent;
	Dialog->bNoticeOnly = bNoticeOnly;
	Dialog->OnConfirmed = InOnConfirmed;
	Dialog->OnCancelled = InOnCancelled;

	// Modal 레이어 — ESC는 자체 처리라 매니저 ESC 대상에서 뺀다
	if (UNUUIManager* UI = UNUUIManager::Get(OwningPlayer))
	{
		FNUUIWindowParams Params;
		Params.Layer = NUUILayer::Modal;
		Params.bCloseOnEscape = false;
		UI->OpenWindow(Dialog, Params);
	}
	else
	{
		Dialog->AddToViewport(NUUILayer::Modal);
	}
	return Dialog;
}

void UNUConfirmDialog::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyMessageToWidget();
	// ConfirmButton/CancelButton 은 순정 Button(레거시), UNU_CloseButtonWidget, UNU_CommonButtonWidget
	// (WBP_CloseButton / WBP_CommonButton, 신규 공용 부품) 셋 다 지원한다.
	if (UButton* ConfirmButtonPure = Cast<UButton>(ConfirmButton))
	{
		ConfirmButtonPure->OnClicked.RemoveDynamic(this, &UNUConfirmDialog::HandleConfirmClicked);
		ConfirmButtonPure->OnClicked.AddDynamic(this, &UNUConfirmDialog::HandleConfirmClicked);

		// 색이 뜻과 어긋나면 눌러야 할 버튼을 취소로 오해한다 --- 파괴는 빨강, 생성·수락은 초록
		// (SetBackgroundColor는 UButton 전용 API라 다른 두 타입에는 아직 적용하지 않는다)
		if (!bUseIntentStyles)
		{
			const UNUUISettings* Palette = GetDefault<UNUUISettings>();
			ConfirmButtonPure->SetBackgroundColor(Intent == ENUConfirmIntent::Positive
				? Palette->PositiveColor
				: Palette->DestructiveColor);
		}
		else if (Intent == ENUConfirmIntent::Positive || bNoticeOnly)
		{
			ConfirmButtonPure->SetStyle(PositiveConfirmStyle);
			if (ConfirmLabelText != nullptr)
			{
				ConfirmLabelText->SetColorAndOpacity(PositiveLabelColor);
			}
		}
	}
	if (bUseIntentStyles)
	{
		const bool bPositive = Intent == ENUConfirmIntent::Positive || bNoticeOnly;
		if (UImage* Icon = Cast<UImage>(GetWidgetFromName(TEXT("IntentIconDestructive"))))
		{
			Icon->SetVisibility(bPositive ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		if (UImage* Icon = Cast<UImage>(GetWidgetFromName(TEXT("IntentIconPositive"))))
		{
			Icon->SetVisibility(bPositive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	else if (UNU_CloseButtonWidget* ConfirmCloseButtonWidget = Cast<UNU_CloseButtonWidget>(ConfirmButton))
	{
		ConfirmCloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &UNUConfirmDialog::HandleConfirmClicked);
		ConfirmCloseButtonWidget->OnCloseClicked.AddDynamic(this, &UNUConfirmDialog::HandleConfirmClicked);
	}
	else if (UNU_CommonButtonWidget* ConfirmCommonButtonWidget = Cast<UNU_CommonButtonWidget>(ConfirmButton))
	{
		ConfirmCommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &UNUConfirmDialog::HandleConfirmClicked);
		ConfirmCommonButtonWidget->OnCommonClicked.AddDynamic(this, &UNUConfirmDialog::HandleConfirmClicked);
	}

	if (UButton* CancelButtonPure = Cast<UButton>(CancelButton))
	{
		CancelButtonPure->OnClicked.RemoveDynamic(this, &UNUConfirmDialog::HandleCancelClicked);
		CancelButtonPure->OnClicked.AddDynamic(this, &UNUConfirmDialog::HandleCancelClicked);
	}
	else if (UNU_CloseButtonWidget* CancelCloseButtonWidget = Cast<UNU_CloseButtonWidget>(CancelButton))
	{
		CancelCloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &UNUConfirmDialog::HandleCancelClicked);
		CancelCloseButtonWidget->OnCloseClicked.AddDynamic(this, &UNUConfirmDialog::HandleCancelClicked);
	}
	else if (UNU_CommonButtonWidget* CancelCommonButtonWidget = Cast<UNU_CommonButtonWidget>(CancelButton))
	{
		CancelCommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &UNUConfirmDialog::HandleCancelClicked);
		CancelCommonButtonWidget->OnCommonClicked.AddDynamic(this, &UNUConfirmDialog::HandleCancelClicked);
	}
	// 안내 창은 고를 것이 없으므로 취소 버튼을 접는다
	if (bNoticeOnly && CancelButton != nullptr)
	{
		CancelButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 라벨 위젯은 선택 --- 버튼이 글자를 스스로 들고 있는 WBP도 있다
	if (ConfirmLabelText != nullptr)
	{
		ConfirmLabelText->SetText(ConfirmLabel);
	}
	if (CancelLabelText != nullptr && !CancelLabel.IsEmpty())
	{
		CancelLabelText->SetText(CancelLabel);
	}

	OnMessageSet();

	if (bPlayOpenMotion)
	{
		OpenMotion.Add(GetWidgetFromName(TEXT("Dim")), 0.f, 0.18f, FVector2D::ZeroVector);
		OpenMotion.Add(GetWidgetFromName(TEXT("Card")), 0.02f, 0.24f, FVector2D(0.f, 8.f), 0.f, 0.96f);
		OpenMotion.Start();
	}

	// ESC를 받으려면 포커스가 필요하다 (안 잡으면 PIE에서 ESC가 플레이 종료로 간다)
	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UNUConfirmDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	OpenMotion.Advance(InDeltaTime);
}

void UNUConfirmDialog::SetMessage(const FText& InMessage)
{
	Message = InMessage;

	// 아직 NativeConstruct 전이면 그때 한 번에 반영된다
	if (!IsConstructed())
	{
		return;
	}
	ApplyMessageToWidget();
	OnMessageSet();
}

void UNUConfirmDialog::ApplyMessageToWidget()
{
	MessageText->SetText(Message);
}

FReply UNUConfirmDialog::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC 또는 패드 B = 취소
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		Cancel();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNUConfirmDialog::Confirm()
{
	UNUSFXSubsystem::PlayCommon(this, NU::SFX::UI::Confirm);

	// 콜백이 창을 다시 띄울 수 있으므로 먼저 떼어내고 실행한다
	FSimpleDelegate Pending = OnConfirmed;
	OnConfirmed.Unbind();
	OnCancelled.Unbind();
	Close();
	Pending.ExecuteIfBound();
}

void UNUConfirmDialog::Cancel()
{
	UNUSFXSubsystem::PlayCommon(this, NU::SFX::UI::Cancel);

	FSimpleDelegate Pending = OnCancelled;
	OnConfirmed.Unbind();
	OnCancelled.Unbind();
	Close();
	Pending.ExecuteIfBound();
}

void UNUConfirmDialog::Dismiss()
{
	OnConfirmed.Unbind();
	OnCancelled.Unbind();
	Close();
}

void UNUConfirmDialog::HandleConfirmClicked()
{
	Confirm();
}

void UNUConfirmDialog::HandleCancelClicked()
{
	Cancel();
}

void UNUConfirmDialog::Close()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this); // 미등록이면 무시 — 아래 RemoveFromParent와 겹쳐도 안전
	}
	RemoveFromParent();
}
