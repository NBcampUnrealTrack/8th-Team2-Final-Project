#include "UI/Controller/NUUIController.h"

#include "Engine/LocalPlayer.h"
#include "UI/NUUIManager.h"

void UNUUIController::InitializeController(UNUUIManager* InManager)
{
	Manager = InManager;
	OnInitialize();
}

void UNUUIController::DeinitializeController()
{
	OnDeinitialize();
	Manager = nullptr;
}

APlayerController* UNUUIController::GetOwningPlayerController() const
{
	const UNUUIManager* UI = GetManager();
	const ULocalPlayer* LP = UI ? UI->GetLocalPlayer<ULocalPlayer>() : nullptr;
	return LP ? LP->GetPlayerController(LP->GetWorld()) : nullptr;
}
