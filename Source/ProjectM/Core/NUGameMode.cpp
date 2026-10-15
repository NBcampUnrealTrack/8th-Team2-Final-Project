#include "Core/NUGameMode.h"

#include "CharacterState/NUPlayerState.h"
#include "Engine/World.h"
#include "Field/FieldPlayerCharacter.h"
#include "PlayerController/NUPlayerController.h"


ANUGameMode::ANUGameMode()
{
	DefaultPawnClass = AFieldPlayerCharacter::StaticClass();
	PlayerControllerClass = ANUPlayerController::StaticClass(); 
	PlayerStateClass = ANUPlayerState::StaticClass(); 
}


void ANUGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

FVector ANUGameMode::GetArenaCenter() const
{
	return ArenaCenter;
}
