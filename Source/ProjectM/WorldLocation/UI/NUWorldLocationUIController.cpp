#include "WorldLocation/UI/NUWorldLocationUIController.h"

#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"
#include "WorldLocation/UI/NUWorldLocationWidget.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UNUWorldLocationUIController::OnInitialize()
{
	if (UNUUIManager* UI = GetManager())
	{
		UI->OnFieldHUDVisibilityChanged.AddUObject(this, &ThisClass::ApplyHUDVisibility);
	}
	BindPlayerController(GetOwningPlayerController());
}

void UNUWorldLocationUIController::OnDeinitialize()
{
	if (UNUUIManager* UI = GetManager())
	{
		UI->OnFieldHUDVisibilityChanged.RemoveAll(this);
	}
	BindPlayerController(nullptr);
}

void UNUWorldLocationUIController::ApplyHUDVisibility()
{
	UNUUIManager* UI = GetManager();
	if (LocationWidget && UI)
	{
		UI->PresentFieldHUDWidget(LocationWidget, ESlateVisibility::HitTestInvisible);
	}
}

void UNUWorldLocationUIController::OnPlayerControllerChanged(APlayerController* NewPC)
{
	BindPlayerController(NewPC);
}

void UNUWorldLocationUIController::BindPlayerController(APlayerController* PlayerController)
{
	if (BoundPlayerController.IsValid())
	{
		BoundPlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	if (LocationComponent.IsValid())
	{
		LocationComponent->OnLocationChanged.RemoveDynamic(this, &ThisClass::HandleLocationChanged);
	}
	LocationComponent = nullptr;
	LastNoticeLocationId = 0;
	if (UNUUIManager* UI = GetManager())
	{
		UI->ForgetFieldHUDWidget(LocationWidget);
		UI->CloseWindow(LocationWidget);
	}
	LocationWidget = nullptr;
	BoundPlayerController = IsValid(PlayerController) && PlayerController->IsLocalController() ? PlayerController : nullptr;
	if (BoundPlayerController.IsValid())
	{
		BoundPlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		RefreshPlayerStateBinding();
	}
}

void UNUWorldLocationUIController::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	RefreshPlayerStateBinding();
}

void UNUWorldLocationUIController::RefreshPlayerStateBinding()
{
	const APlayerState* PlayerState = BoundPlayerController.IsValid() ? BoundPlayerController->GetPlayerState<APlayerState>() : nullptr;
	UNUWorldLocationComponent* NewComponent = PlayerState != nullptr ? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
	if (LocationComponent.Get() != NewComponent)
	{
		if (LocationComponent.IsValid())
		{
			LocationComponent->OnLocationChanged.RemoveDynamic(this, &ThisClass::HandleLocationChanged);
		}
		LocationComponent = NewComponent;
		LastNoticeLocationId = 0;
		if (NewComponent != nullptr)
		{
			NewComponent->OnLocationChanged.AddUniqueDynamic(this, &ThisClass::HandleLocationChanged);
		}
	}
	HandleLocationChanged();
}

void UNUWorldLocationUIController::HandleLocationChanged()
{
	UNUUIManager* UI = GetManager();
	if (UI == nullptr)
	{
		return;
	}
	if (!BoundPlayerController.IsValid() || !LocationComponent.IsValid() || !LocationComponent->HasTrackedPawn())
	{
		UI->ForgetFieldHUDWidget(LocationWidget);
		UI->CloseWindow(LocationWidget);
		LastNoticeLocationId = 0;
		return;
	}
	if (LocationWidget == nullptr)
	{
		const UNUUISettings* Settings = GetDefault<UNUUISettings>();
		TSubclassOf<UNUWorldLocationWidget> WidgetClass = Settings->WorldLocationWidgetClass.LoadSynchronous();
		if (WidgetClass == nullptr)
		{
			WidgetClass = UNUWorldLocationWidget::StaticClass();
		}
		if (GetOrCreateWidget(LocationWidget, WidgetClass) == nullptr)
		{
			return;
		}
	}
	const UGameInstance* GameInstance = BoundPlayerController->GetGameInstance();
	const UNUWorldDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldDataSubsystem>() : nullptr;
	const int32 CurrentLocationId = LocationComponent->GetCurrentLocationId();
	const FNUAreaDefinition* Area = Data != nullptr ? Data->FindAreaDefinition(LocationComponent->GetLastKnownAreaId()) : nullptr;
	const FNULocationDefinition* Location = Data != nullptr ? Data->FindLocationDefinition(CurrentLocationId) : nullptr;
	LocationWidget->SetLocationNames(Area != nullptr ? Area->DisplayName : FText::GetEmpty(), Location != nullptr ? Location->DisplayName : FText::GetEmpty());
	ApplyHUDVisibility();
	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::HUD;
	Params.bCloseOnEscape = false;
	Params.bRequestCursor = false;
	Params.bBlockGameInput = false;
	UI->OpenWindow(LocationWidget, Params);
	// The native fallback builds its root during AddToViewport; register that root as well.
	ApplyHUDVisibility();
	const bool bLocationChanged = CurrentLocationId != LastNoticeLocationId;
	LastNoticeLocationId = CurrentLocationId;
	if (bLocationChanged && UI->IsFieldHUDVisible() && Location != nullptr && LocationComponent->ShouldShowEntryNotice())
	{
		UI->ShowNotice(FText::Format(NSLOCTEXT("NUWorldLocation", "LocationEntered", "{0}에 진입했습니다."), Location->DisplayName), ENUNoticeType::LocationEntry);
	}
}
