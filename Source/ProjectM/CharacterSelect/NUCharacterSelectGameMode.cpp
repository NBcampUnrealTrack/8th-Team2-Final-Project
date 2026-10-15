// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/NUCharacterSelectGameMode.h"

#include "Blueprint/UserWidget.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "CharacterSelect/UI/NUCharacterSelectWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "WorldTransition/World/NUFieldLoadingSubsystem.h"

ANUCharacterSelectGameMode::ANUCharacterSelectGameMode()
{
	// 선택창 레벨에는 조작할 폰이 없다 (NULoginGameMode 선례)
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
}

void ANUCharacterSelectGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNUCharacterManager* Manager = GI->GetSubsystem<UNUCharacterManager>())
		{
			Manager->OnCharacterSelected.AddDynamic(this, &ThisClass::HandleCharacterSelected);
		}
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		UE_LOG(LogNUCharSelect, Error,
			TEXT("로컬 PlayerController가 없어 선택창을 만들 수 없습니다. PIE Net Mode를 Play Standalone으로 두세요"));
		return;
	}

	PC->bShowMouseCursor = true;
	PC->SetInputMode(FInputModeUIOnly());

	if (!CharacterSelectWidgetClass)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("CharacterSelectWidgetClass가 지정되지 않았습니다"));
		return;
	}

	CharacterSelectWidget = CreateWidget<UNUCharacterSelectWidget>(PC, CharacterSelectWidgetClass);
	if (IsValid(CharacterSelectWidget))
	{
		CharacterSelectWidget->AddToViewport();
	}
}

void ANUCharacterSelectGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNUCharacterManager* Manager = GI->GetSubsystem<UNUCharacterManager>())
		{
			Manager->OnCharacterSelected.RemoveDynamic(this, &ThisClass::HandleCharacterSelected);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ANUCharacterSelectGameMode::HandleCharacterSelected(int64 CharacterIdx)
{
	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 선택 확정 Idx=%lld — 필드로 이동"), CharacterIdx);
	TravelToField();
}

void ANUCharacterSelectGameMode::TravelToCharacterCreate()
{
	if (CharacterCreateLevel.IsNull())
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("CharacterCreateLevel이 지정되지 않았습니다"));
		return;
	}

	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 생성 화면으로 이동 [%s]"), *CharacterCreateLevel.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, CharacterCreateLevel);
}

void ANUCharacterSelectGameMode::TravelToField()
{
	if (FieldLevel.IsNull())
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("FieldLevel이 지정되지 않았습니다"));
		return;
	}

	UE_LOG(LogNUCharSelect, Log, TEXT("필드로 이동 [%s]"), *FieldLevel.ToString());
	if (UNUFieldLoadingSubsystem* Loading = GetGameInstance()->GetSubsystem<UNUFieldLoadingSubsystem>())
	{
		Loading->BeginLoading(FieldLevel.ToSoftObjectPath().GetAssetName());
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, FieldLevel);
}
