// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/NUCharacterCreateGameMode.h"

#include "Camera/CameraActor.h"
#include "CharacterSelect/NUCharacterCreatePreview.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "EngineUtils.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "CharacterSelect/UI/NUCharacterCreateWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

ANUCharacterCreateGameMode::ANUCharacterCreateGameMode()
{
	// 생성 레벨에도 조작할 폰이 없다 (선택창과 같다)
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
}

void ANUCharacterCreateGameMode::BeginPlay()
{
	Super::BeginPlay();

	bTravelling = false;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNUCharacterManager* Manager = GI->GetSubsystem<UNUCharacterManager>())
		{
			Manager->OnCharacterCreateResult.AddDynamic(this, &ThisClass::HandleCreateResult);
		}
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		UE_LOG(LogNUCharSelect, Error,
			TEXT("로컬 PlayerController가 없어 생성 화면을 만들 수 없습니다. PIE Net Mode를 Play Standalone으로 두세요"));
		return;
	}

	// 레벨의 캐릭터를 눌러 종족을 고르므로 월드 클릭이 살아 있어야 한다.
	// UIOnly면 클릭이 위젯에서 멈춘다 --- GameAndUI로 두고 위젯에 포커스만 준다
	PC->bShowMouseCursor = true;
	PC->bEnableClickEvents = true;
	PC->bEnableMouseOverEvents = true;


	// 모양은 WBP_CharacterCreate가 갖는다 (코드 최소 UI 폴백은 9/6 제거) --- 미지정이면 화면을 못 만든다
	if (CharacterCreateWidgetClass == nullptr)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("CharacterCreateWidgetClass 미지정 --- 생성 레벨 게임모드에 WBP_CharacterCreate를 지정해야 한다"));
		return;
	}

	CharacterCreateWidget = CreateWidget<UNUCharacterCreateWidget>(PC, CharacterCreateWidgetClass);
	if (!IsValid(CharacterCreateWidget))
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("생성 화면을 만들지 못했습니다"));
		return;
	}

	FOnNUCharacterCreateConfirmed OnConfirmed;
	OnConfirmed.BindUObject(this, &ANUCharacterCreateGameMode::HandleCreateConfirmed);

	// NativeConstruct에서 읽으므로 뷰포트에 올리기 전에 채운다
	CharacterCreateWidget->Setup(OnConfirmed, ConfirmDialogClass);

	// 레벨의 종족별 자리를 전부 모아 넘긴다 --- 어디를 비출지는 위젯이 정한다.
	// 하나도 없으면 화면이 빌 뿐 생성은 그대로 된다 (액터 배치 전에도 동작해야 한다)
	TArray<ANUCharacterCreatePreview*> Stands;
	for (TActorIterator<ANUCharacterCreatePreview> It(GetWorld()); It; ++It)
	{
		Stands.Add(*It);
	}
	CharacterCreateWidget->SetPreviewStands(Stands);

	// 전경 카메라는 "Overview" 태그로 찾는다. 태그를 안 붙였으면 어느 자리도 물고 있지 않은
	// 카메라를 쓰고, 그것도 없으면 첫 카메라 (레벨을 대충 꾸며도 화면은 나와야 한다)
	ACameraActor* Overview = nullptr;
	ACameraActor* FirstCamera = nullptr;
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		ACameraActor* Camera = *It;
		if (FirstCamera == nullptr)
		{
			FirstCamera = Camera;
		}
		if (Camera->Tags.Contains(TEXT("Overview")))
		{
			Overview = Camera;
			break;
		}
		const bool bUsedByStand = Stands.ContainsByPredicate(
			[Camera](const ANUCharacterCreatePreview* Stand)
			{
				return Stand != nullptr && Stand->GetStandCamera() == Camera;
			});
		if (!bUsedByStand && Overview == nullptr)
		{
			Overview = Camera;
		}
	}
	CharacterCreateWidget->SetOverviewCamera(Overview != nullptr ? Overview : FirstCamera);
	CharacterCreateWidget->OnClosed.BindUObject(this, &ANUCharacterCreateGameMode::HandleWidgetClosed);
	CharacterCreateWidget->AddToViewport();

	// 뷰포트에 올린 뒤라야 포커스를 줄 수 있다 (ESC 취소가 위젯으로 들어가야 한다)
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(CharacterCreateWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
}

void ANUCharacterCreateGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNUCharacterManager* Manager = GI->GetSubsystem<UNUCharacterManager>())
		{
			Manager->OnCharacterCreateResult.RemoveDynamic(this, &ThisClass::HandleCreateResult);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ANUCharacterCreateGameMode::HandleCreateConfirmed(const FNUCharacterCreateParams& Params)
{
	UGameInstance* GI = GetGameInstance();
	UNUCharacterManager* Manager = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	if (Manager == nullptr)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("캐릭터 매니저가 없어 생성 요청을 보내지 못했습니다"));
		return;
	}

	Manager->CreateCharacter(Params); // 결과는 HandleCreateResult로 온다
}

void ANUCharacterCreateGameMode::HandleCreateResult(bool bSuccess, int32 ResultCode)
{
	if (bSuccess)
	{
		// 새 캐릭터를 골라 둔 상태로 선택 레벨에 돌아간다 (매니저가 Idx를 들고 있다)
		UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 생성 완료 --- 선택 화면으로 돌아간다"));
		TravelToCharacterSelect();
		return;
	}

	// 이름이 거절당한 것이면 화면을 유지한다 --- 나가면 고른 것이 전부 날아간다 (기획서 6장)
	if (IsValid(CharacterCreateWidget))
	{
		CharacterCreateWidget->ShowNameRejected(ResultCode);
	}
}

void ANUCharacterCreateGameMode::HandleWidgetClosed()
{
	// 취소 --- 화면은 이미 내려갔고 레벨만 되돌리면 된다
	TravelToCharacterSelect();
}

void ANUCharacterCreateGameMode::TravelToCharacterSelect()
{
	if (bTravelling)
	{
		return;
	}

	if (CharacterSelectLevel.IsNull())
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("CharacterSelectLevel이 지정되지 않았습니다 --- 돌아갈 곳이 없다"));
		return;
	}

	bTravelling = true;
	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 선택 화면으로 이동 [%s]"), *CharacterSelectLevel.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, CharacterSelectLevel);
}
