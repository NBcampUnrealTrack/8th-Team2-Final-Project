// Development console command (NUA): nu.Battle.PressReaction presses the battle reaction key (parry / dodge) by
// injecting its input action through Enhanced Input, so the press takes the same path as a key press
// (ANUPlayerController::HandleBattleParry). Used by the capture harness (tools/ue_apply/pie_battle_run.py in the
// NiaUnbounds repository) to film a real parry or dodge. Not compiled into shipping builds.

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputActionValue.h"

static FAutoConsoleCommandWithWorld GNUPressReactionCmd(
	TEXT("nu.Battle.PressReaction"),
	TEXT("Presses the battle reaction key (parry / dodge) once by injecting the IA_Parry input action"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		ULocalPlayer* LocalPlayer = Controller != nullptr ? Controller->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
			? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		const UInputAction* Action = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Battle/IA_Parry.IA_Parry"));
		if (Input == nullptr || Action == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Battle.PressReaction] no local input subsystem or IA_Parry"));
			return;
		}
		Input->InjectInputForAction(Action, FInputActionValue(true), {}, {});
		UE_LOG(LogTemp, Log, TEXT("[nu.Battle.PressReaction] IA_Parry injected at world time %.3f"), World->GetTimeSeconds());
	}));

#endif
