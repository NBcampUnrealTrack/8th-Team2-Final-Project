#include "Core/NULoginWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Core/NULoginGameMode.h"
#include "Engine/World.h"

void UNULoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ANULoginGameMode* GameMode = GetLoginGameMode();
	if (GameMode != nullptr)
	{
		GameMode->OnLoginStatusChanged.AddUniqueDynamic(this, &ThisClass::HandleStatusChanged);
	}
	if (GoogleLoginButton != nullptr)
	{
		GoogleLoginButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGoogleClicked);
	}
	if (GoogleCancelButton != nullptr)
	{
		GoogleCancelButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGoogleCancelClicked);
		GoogleCancelButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DevLoginButton != nullptr)
	{
		DevLoginButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDevLoginClicked);
	}
	if (UserIdInput != nullptr)
	{
		UserIdInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleUserIdCommitted);
	}

	const bool bDevVisible = GameMode != nullptr && GameMode->IsDevNameLoginVisible();
	const bool bGoogleReady = GameMode != nullptr && GameMode->IsGoogleLoginAvailable();
	if (DevLoginBox != nullptr)
	{
		DevLoginBox->SetVisibility(bDevVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (GoogleSetupHint != nullptr)
	{
		// 출시 빌드에서 설정이 비었으면 버튼만 잠기고 안내는 개발용과 함께 보인다
		GoogleSetupHint->SetVisibility(!bGoogleReady && bDevVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	ApplyBusy(false);

	if (bPlayIntro)
	{
		IntroMotion.Add(GetWidgetFromName(TEXT("IntroTree")), 0.f, 1.1f, FVector2D(0.f, 18.f), 0.f, 0.97f);
		IntroMotion.Add(GetWidgetFromName(TEXT("IntroTitle")), 0.35f, 0.9f, FVector2D(0.f, 12.f), 0.f, 1.04f);
		IntroMotion.Add(GetWidgetFromName(TEXT("IntroBar")), 0.55f, 0.8f, FVector2D::ZeroVector, 0.f, 0.9f);
		IntroMotion.Add(GetWidgetFromName(TEXT("IntroMenu")), 0.8f, 0.6f, FVector2D(0.f, 24.f));
		IntroMotion.Start();
	}
}

void UNULoginWidget::NativeDestruct()
{
	if (ANULoginGameMode* GameMode = GetLoginGameMode())
	{
		GameMode->OnLoginStatusChanged.RemoveDynamic(this, &ThisClass::HandleStatusChanged);
	}
	IntroMotion.Finish();
	Super::NativeDestruct();
}

void UNULoginWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	IntroMotion.Advance(InDeltaTime);

	// 브라우저에서 계정을 고르는 동안만 "취소"를 보인다 (게임모드에 상태를 묻는다 --- 이벤트를 하나 더 두지 않으려고)
	const ANULoginGameMode* GameMode = GetLoginGameMode();
	const bool bPending = GameMode != nullptr && GameMode->IsGoogleSignInPending();
	if (bPending != bLastPending)
	{
		bLastPending = bPending;
		if (GoogleCancelButton != nullptr)
		{
			GoogleCancelButton->SetVisibility(bPending ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

ANULoginGameMode* UNULoginWidget::GetLoginGameMode() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetAuthGameMode<ANULoginGameMode>() : nullptr;
}

void UNULoginWidget::ApplyBusy(bool bInBusy)
{
	bBusy = bInBusy;
	const ANULoginGameMode* GameMode = GetLoginGameMode();
	if (GoogleLoginButton != nullptr)
	{
		GoogleLoginButton->SetIsEnabled(!bBusy && GameMode != nullptr && GameMode->IsGoogleLoginAvailable());
	}
	if (DevLoginButton != nullptr)
	{
		DevLoginButton->SetIsEnabled(!bBusy);
	}
	if (UserIdInput != nullptr)
	{
		UserIdInput->SetIsEnabled(!bBusy);
	}
}

void UNULoginWidget::HandleStatusChanged(const FText& InStatusText, bool bInBusy)
{
	if (StatusText != nullptr)
	{
		StatusText->SetText(InStatusText);
	}
	ApplyBusy(bInBusy);
}

void UNULoginWidget::HandleGoogleClicked()
{
	if (ANULoginGameMode* GameMode = GetLoginGameMode(); GameMode != nullptr && !bBusy)
	{
		GameMode->RequestGoogleLogin();
	}
}

void UNULoginWidget::HandleGoogleCancelClicked()
{
	if (ANULoginGameMode* GameMode = GetLoginGameMode())
	{
		GameMode->CancelGoogleLogin();
	}
}

void UNULoginWidget::HandleDevLoginClicked()
{
	if (ANULoginGameMode* GameMode = GetLoginGameMode(); GameMode != nullptr && UserIdInput != nullptr && !bBusy)
	{
		GameMode->RequestLogin(UserIdInput->GetText().ToString());
	}
}

void UNULoginWidget::HandleUserIdCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleDevLoginClicked();
	}
}
