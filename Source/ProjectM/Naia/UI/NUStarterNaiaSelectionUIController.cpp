#include "Naia/UI/NUStarterNaiaSelectionUIController.h"

#include "CharacterState/NUPlayerState.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/UI/NUStarterNaiaSelectionWidget.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

void UNUStarterNaiaSelectionUIController::OpenSelection()
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	UNUUIManager* UI = GetManager();
	ANUPlayerState* PlayerState = PlayerController != nullptr ? PlayerController->GetPlayerState<ANUPlayerState>() : nullptr;
	UNUNaiaRosterComponent* RosterComponent = PlayerState != nullptr ? PlayerState->GetNaiaRosterComponent() : nullptr;
	if (PlayerController == nullptr || UI == nullptr || RosterComponent == nullptr)
	{
		return;
	}
	if (SelectionWidget != nullptr && UI->IsWindowOpen(SelectionWidget))
	{
		return;
	}
	if (RosterComponent->HasSelectedStarterNaia())
	{
		UI->ShowNotice(NSLOCTEXT("NUStarterNaia", "AlreadySelected", "이미 시작 나이아를 선택했습니다"));
		return;
	}
	if (PlayerController->IsGameInputBlocked())
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUStarterNaiaSelectionWidget> WidgetClass = Settings != nullptr
		? Settings->StarterNaiaSelectionWidgetClass.LoadSynchronous()
		: nullptr;
	if (WidgetClass == nullptr)
	{
		WidgetClass = UNUStarterNaiaSelectionWidget::StaticClass();
	}

	GetOrCreateWidget(SelectionWidget, WidgetClass);
	if (SelectionWidget == nullptr)
	{
		return;
	}

	SelectionWidget->InitializeSelection(RosterComponent, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSelectionFinished));

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bDimBackground = true;
	Params.bCloseOnEscape = false;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	Params.bHideFieldHUD = SelectionWidget->ShouldHideFieldHUD();
	UI->OpenWindow(SelectionWidget, Params);
}

void UNUStarterNaiaSelectionUIController::CloseSelection()
{
	if (UNUUIManager* UI = GetManager(); UI != nullptr && SelectionWidget != nullptr)
	{
		UI->CloseWindow(SelectionWidget);
	}
}

void UNUStarterNaiaSelectionUIController::HandleSelectionFinished()
{
	CloseSelection();
	ANUPlayerController* PC = Cast<ANUPlayerController>(GetOwningPlayerController());
	const ANUPlayerState* State = PC != nullptr ? PC->GetPlayerState<ANUPlayerState>() : nullptr;
	const UNUNaiaRosterComponent* Roster = State != nullptr ? State->GetNaiaRosterComponent() : nullptr;
	if (Roster != nullptr && Roster->HasSelectedStarterNaia()) { PC->OpenDialogueEvent(TEXT("StarterNaiaConfirmed")); }
}

void UNUStarterNaiaSelectionUIController::OnPlayerControllerChanged(APlayerController* NewPC)
{
	CloseSelection();
	SelectionWidget = nullptr;
}

void UNUStarterNaiaSelectionUIController::OnDeinitialize()
{
	CloseSelection();
	SelectionWidget = nullptr;
}
