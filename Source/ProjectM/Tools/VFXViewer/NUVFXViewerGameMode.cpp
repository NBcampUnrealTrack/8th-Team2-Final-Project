// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/VFXViewer/NUVFXViewerGameMode.h"

#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "Tools/VFXViewer/NUVFXViewerDirector.h"
#include "Tools/VFXViewer/NUVFXViewerPlayerController.h"

ANUVFXViewerGameMode::ANUVFXViewerGameMode()
{
	DefaultPawnClass = nullptr;
	// 비워 두면 HUD 스폰이 "no class" 경고를 남김, 빈 기본 HUD 로 둠
	HUDClass = AHUD::StaticClass();
	PlayerControllerClass = ANUVFXViewerPlayerController::StaticClass();
}

void ANUVFXViewerGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Director = World->SpawnActor<ANUVFXViewerDirector>(SpawnParams);

	if (ANUVFXViewerPlayerController* Controller = Cast<ANUVFXViewerPlayerController>(World->GetFirstPlayerController()))
	{
		Controller->SetDirector(Director);
	}
}
