#include "WorldTransition/World/NUFieldLoadingSubsystem.h"

#include "Field/Net/FieldSyncSubsystem.h"
#include "PlayerController/NUPlayerController.h"
#include "WorldTransition/Data/NUWorldTransitionSettings.h"
#include "WorldTransition/Net/NULocationRestoreSubsystem.h"
#include "UI/NUUISettings.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceLevelStreaming.h"
#include "Net/EmberServerManager.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUFieldLoading, Log, All);

namespace
{
	const FName InputBlockReason(TEXT("FieldLoading"));
}

void UNUFieldLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	if (GEngine != nullptr)
	{
		GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
}

void UNUFieldLoadingSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (GEngine != nullptr)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	FinishLoading();
	Super::Deinitialize();
}

void UNUFieldLoadingSubsystem::BeginLoading(const FString& FieldMapName)
{
	if (FieldMapName.IsEmpty())
	{
		return;
	}

	FinishLoading();
	// 화면과 이 서브시스템의 입력 차단만 끄고, 위치 복원에 쓰는 준비 판정은 유지한다.
	if (!GetDefault<UNUWorldTransitionSettings>()->bEnableFieldLoading)
	{
		return;
	}

	TargetMapName = FieldMapName;
	LoadingStartedAt = FPlatformTime::Seconds();
	LoadingTimeoutSeconds = FMath::Max(1.0f, GetDefault<UNUWorldTransitionSettings>()->FieldLoadingTimeoutSeconds);
	bLoading = true;
	UpdateLoadingStage(TEXT("Map"));
	ShowLoadingScreen();
	// 월드 교체와 일시정지 중에도 실패 제한 시간을 검사한다.
	ReadyCheckTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickLoading), LevelInspectionInterval);

	if (UWorld* World = GetWorld())
	{
		HandleFieldControllerReady(Cast<ANUPlayerController>(World->GetFirstPlayerController()));
	}
}

void UNUFieldLoadingSubsystem::HandleFieldControllerReady(ANUPlayerController* Controller)
{
	if ((!bLoading && !bFailed) || !IsValid(Controller) || !Controller->IsLocalController())
	{
		return;
	}

	if (BlockedController.IsValid() && BlockedController.Get() != Controller)
	{
		ReleaseController();
	}

	if (BlockedController.Get() != Controller)
	{
		BlockedController = Controller;
		Controller->RequestGameInputBlock(InputBlockReason, true, this);
		Controller->SetIgnoreLookInput(true);
		if (bFailed)
		{
			Controller->RequestCursor(InputBlockReason, true, this);
		}
		FInputModeUIOnly InputMode;
		if (bFailed)
		{
			InputMode.SetWidgetToFocus(ReturnButton);
		}
		Controller->SetInputMode(InputMode);
	}
}

void UNUFieldLoadingSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if ((!bLoading && !bFailed) || !IsValid(LoadedWorld) || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	if (UGameplayStatics::GetCurrentLevelName(LoadedWorld, true) != TargetMapName)
	{
		FinishLoading();
		return;
	}

	FieldWorld = LoadedWorld;
	if (bLoading)
	{
		PrioritizeLevelLoading(LoadedWorld);
	}
	if (BlockedController.IsValid() && BlockedController->GetWorld() != LoadedWorld)
	{
		ReleaseController();
	}
	// 맵 전환 중 뷰포트가 위젯을 유지해도 중복 없이 새 월드 위에 다시 붙인다.
	if (bOverlayAdded && LoadingWidget.IsValid() && LoadingViewport.IsValid())
	{
		LoadingViewport->RemoveViewportWidgetContent(LoadingWidget.ToSharedRef());
	}
	bOverlayAdded = false;
	ShowLoadingScreen();
	HandleFieldControllerReady(Cast<ANUPlayerController>(LoadedWorld->GetFirstPlayerController()));
	CheckFieldReady();
}

void UNUFieldLoadingSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (bLoading && IsValid(World) && World->GetGameInstance() == GetGameInstance())
	{
		FailLoading(FString::Printf(TEXT("TravelFailure=%d %s"), static_cast<int32>(FailureType), *ErrorString));
	}
}

bool UNUFieldLoadingSubsystem::TickLoading(float DeltaSeconds)
{
	if (!bLoading && !bFailed)
	{
		return false;
	}
	ShowLoadingScreen();
	if (UWorld* World = GetWorld())
	{
		HandleFieldControllerReady(Cast<ANUPlayerController>(World->GetFirstPlayerController()));
	}
	if (bFailed)
	{
		return true;
	}
	const UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		FailLoading(TEXT("Server session ended during map travel"));
		return true;
	}
	CheckFieldReady();
	if (bLoading && FPlatformTime::Seconds() - LoadingStartedAt >= LoadingTimeoutSeconds)
	{
		FailLoading(FString::Printf(TEXT("Timeout after %.2fs: %s"), LoadingTimeoutSeconds, *LoadingStage));
	}
	return bLoading || bFailed;
}

void UNUFieldLoadingSubsystem::CheckFieldReady()
{
	UWorld* World = FieldWorld.Get();
	if (!bLoading || !IsValid(World) || !World->HasBegunPlay())
	{
		return;
	}

	// 로그인 설정이 선택 화면을 가리키는 경우에는 필드 로딩 조건을 적용하지 않는다.
	if (APlayerController* LocalController = World->GetFirstPlayerController())
	{
		if (!LocalController->IsA<ANUPlayerController>())
		{
			FinishLoading();
			return;
		}
	}
	const UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		FailLoading(TEXT("Server session ended during field entry"));
		return;
	}
	const UNULocationRestoreSubsystem* Restore = GetGameInstance()->GetSubsystem<UNULocationRestoreSubsystem>();
	const UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>();
	if ((Restore != nullptr && Restore->HasFieldEntryFailed()) || (Sync != nullptr && Sync->HasFieldEntryFailed()))
	{
		FailLoading(TEXT("LocationRestore or FieldEnter failed"));
		return;
	}

	ANUPlayerController* Controller = Cast<ANUPlayerController>(World->GetFirstPlayerController());
	if (!IsValid(Controller) || !IsValid(Controller->GetPawn()))
	{
		UpdateLoadingStage(TEXT("Pawn"));
		return;
	}
	HandleFieldControllerReady(Controller);

	const FNUFieldLevelReadiness Levels = InspectLevelInstances(World);
	if (!Levels.FailedInstance.IsEmpty())
	{
		FailLoading(FString::Printf(TEXT("Level load failed: %s"), *Levels.FailedInstance));
		return;
	}
	if (!Levels.IsReady())
	{
		UpdateLoadingStage(FString::Printf(TEXT("Levels %d/%d; pending=%s"), Levels.InstanceCount - Levels.PendingInstances.Num(), Levels.InstanceCount, *FString::Join(Levels.PendingInstances, TEXT(","))));
		return;
	}
	if (Restore != nullptr && !Restore->IsFieldEntryReady())
	{
		UpdateLoadingStage(TEXT("LocationRestore"));
		return;
	}
	// 레벨 표시 후 실제 도착 위치의 캡슐과 보행 가능한 바닥까지 확인한다.
	const ACharacter* Character = Cast<ACharacter>(Controller->GetPawn());
	const UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
	FFindFloorResult Floor;
	if (Movement != nullptr && Character->GetCapsuleComponent()->IsRegistered())
	{
		const float FloorDistance = Movement->MaxStepHeight + UCharacterMovementComponent::MAX_FLOOR_DIST;
		Movement->ComputeFloorDist(Character->GetActorLocation(), FloorDistance, FloorDistance, Floor, Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
	}
	if (!Floor.IsWalkableFloor() || Floor.HitResult.bStartPenetrating)
	{
		UpdateLoadingStage(TEXT("ArrivalCollision"));
		return;
	}
	if (Sync == nullptr || !Sync->HasConfirmedFieldEntry())
	{
		UpdateLoadingStage(TEXT("FieldEnter"));
		return;
	}

	UpdateLoadingStage(FString::Printf(TEXT("Ready; instances=%d"), Levels.InstanceCount));
	FinishLoading();
}

FNUFieldLevelReadiness UNUFieldLoadingSubsystem::InspectLevelInstances(UWorld* World) const
{
	FNUFieldLevelReadiness Result;
	if (!IsValid(World) || !IsValid(World->PersistentLevel))
	{
		Result.FailedInstance = TEXT("Invalid world");
		return Result;
	}
	const double CurrentTime = FPlatformTime::Seconds();
	// 준비 완료 결과는 재사용하지 않아 뒤늦게 추가된 중첩 레벨을 놓치지 않는다.
	if (InspectedWorld.Get() == World && !PendingLevelReadiness.IsReady() && CurrentTime < NextLevelInspectionAt)
	{
		return PendingLevelReadiness;
	}
	TSet<ULevel*> Levels;
	Levels.Add(World->PersistentLevel);
	for (ULevel* Level : World->GetLevels())
	{
		Levels.Add(Level);
	}
	// 아직 표시되지 않은 부모 레벨 안의 중첩 인스턴스도 검사 대상에 포함한다.
	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (IsValid(Streaming) && Streaming->ShouldBeLoaded() && !Streaming->GetIsRequestingUnloadAndRemoval())
		{
			// 실패한 레벨은 아직 액터의 GetLevelStreaming에 등록되지 않았을 수 있다.
			if (Streaming->IsA<ULevelStreamingLevelInstance>() && Streaming->GetLevelStreamingState() == ELevelStreamingState::FailedToLoad)
			{
				Result.FailedInstance = Streaming->GetWorldAssetPackageName();
			}
			Levels.Add(Streaming->GetLoadedLevel());
		}
	}
	for (ULevel* Level : Levels)
	{
		if (!IsValid(Level))
		{
			continue;
		}
		for (AActor* Actor : Level->Actors)
		{
			const ALevelInstance* Instance = Cast<ALevelInstance>(Actor);
			if (!IsValid(Instance) || !Instance->IsLoadingEnabled() || !Instance->IsWorldAssetValid())
			{
				continue;
			}
			++Result.InstanceCount;
			const ULevelStreaming* Streaming = Instance->GetLevelStreaming();
			if (Streaming != nullptr && Streaming->GetLevelStreamingState() == ELevelStreamingState::FailedToLoad)
			{
				Result.FailedInstance = Instance->GetPathName();
			}
			if (!Instance->IsLoaded() || Streaming == nullptr || !Streaming->IsLevelVisible())
			{
				Result.PendingInstances.Add(Instance->GetName());
			}
		}
	}
	Result.PendingInstances.Sort();
	InspectedWorld = World;
	PendingLevelReadiness = Result;
	NextLevelInspectionAt = CurrentTime + LevelInspectionInterval;
	return Result;
}

void UNUFieldLoadingSubsystem::ShowLoadingScreen()
{
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (!IsValid(Viewport))
	{
		return;
	}
	if (bOverlayAdded && LoadingViewport.Get() != Viewport)
	{
		bOverlayAdded = false;
	}

	if (!LoadingWidget.IsValid())
	{
		const TWeakObjectPtr<UNUFieldLoadingSubsystem> WeakThis(this);
		LoadingWidget = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("BlackBrush"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->bFailed ? NSLOCTEXT("NUFieldLoading", "Failed", "입장에 실패했습니다") : NSLOCTEXT("NUFieldLoading", "Loading", "필드를 불러오는 중..."); })
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 20.0f)
				[
					SAssignNew(ReturnButton, SButton)
					.Visibility_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->bFailed ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(GetDefault<UNUUISettings>()->LoginLevel.IsNull() ? NSLOCTEXT("NUFieldLoading", "Quit", "종료") : NSLOCTEXT("NUFieldLoading", "Return", "초기화면으로"))
					.OnClicked(FOnClicked::CreateUObject(this, &ThisClass::ReturnToLogin))
				]
			];
	}

	if (!bOverlayAdded)
	{
		Viewport->AddViewportWidgetContent(LoadingWidget.ToSharedRef(), 10000);
		LoadingViewport = Viewport;
		bOverlayAdded = true;
	}
}

void UNUFieldLoadingSubsystem::FinishLoading()
{
	FTSTicker::GetCoreTicker().RemoveTicker(ReadyCheckTicker);
	ReadyCheckTicker.Reset();
	RestoreLevelLoadingPriority();
	ReleaseController();

	if (bOverlayAdded && LoadingWidget.IsValid() && LoadingViewport.IsValid())
	{
		LoadingViewport->RemoveViewportWidgetContent(LoadingWidget.ToSharedRef());
	}

	bLoading = false;
	bFailed = false;
	bOverlayAdded = false;
	TargetMapName.Reset();
	FieldWorld.Reset();
	InspectedWorld.Reset();
	PendingLevelReadiness = {};
	NextLevelInspectionAt = 0.0;
	BlockedController.Reset();
	LoadingViewport.Reset();
	LoadingWidget.Reset();
	ReturnButton.Reset();
	LoadingStage.Reset();
}

void UNUFieldLoadingSubsystem::ReleaseController()
{
	if (ANUPlayerController* Controller = BlockedController.Get())
	{
		Controller->RequestGameInputBlock(InputBlockReason, false, this);
		Controller->SetIgnoreLookInput(false);
		// 기존 UI의 커서 요청을 다시 반영해 다른 창의 입력 모드를 덮어쓰지 않는다.
		Controller->RequestCursor(InputBlockReason, false, this, true);
	}
	BlockedController.Reset();
}

void UNUFieldLoadingSubsystem::PrioritizeLevelLoading(UWorld* World)
{
	AWorldSettings* WorldSettings = IsValid(World) ? World->GetWorldSettings() : nullptr;
	if (PrioritizedWorldSettings.Get() == WorldSettings)
	{
		return;
	}
	RestoreLevelLoadingPriority();
	if (IsValid(WorldSettings))
	{
		PrioritizedWorldSettings = WorldSettings;
		bPreviousHighPriorityLoading = WorldSettings->bHighPriorityLoadingLocal;
		WorldSettings->bHighPriorityLoadingLocal = true;
	}
}

void UNUFieldLoadingSubsystem::RestoreLevelLoadingPriority()
{
	if (AWorldSettings* WorldSettings = PrioritizedWorldSettings.Get())
	{
		WorldSettings->bHighPriorityLoadingLocal = bPreviousHighPriorityLoading;
	}
	PrioritizedWorldSettings.Reset();
	bPreviousHighPriorityLoading = false;
}

void UNUFieldLoadingSubsystem::UpdateLoadingStage(const FString& Stage)
{
	if (LoadingStage == Stage)
	{
		return;
	}
	LoadingStage = Stage;
	const double Elapsed = FPlatformTime::Seconds() - LoadingStartedAt;
	UE_LOG(LogNUFieldLoading, Log, TEXT("FieldLoading map=%s elapsed=%.3fs stage=%s"), *TargetMapName, Elapsed, *Stage);
}

void UNUFieldLoadingSubsystem::FailLoading(const FString& Reason)
{
	if (!bLoading)
	{
		return;
	}
	bLoading = false;
	bFailed = true;
	RestoreLevelLoadingPriority();
	UE_LOG(LogNUFieldLoading, Warning, TEXT("필드 입장 실패 map=%s elapsed=%.3fs reason=%s"), *TargetMapName, FPlatformTime::Seconds() - LoadingStartedAt, *Reason);
	if (UNULocationRestoreSubsystem* Restore = GetGameInstance()->GetSubsystem<UNULocationRestoreSubsystem>())
	{
		Restore->CancelFieldEntry();
	}
	if (UWorld* World = FieldWorld.Get())
	{
		if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
		{
			Sync->LeaveField();
		}
	}
	// 실패는 정상 입장이 아니다. 필드 입력은 막고 초기화면으로 나갈 UI만 활성화한다.
	ShowLoadingScreen();
	if (ANUPlayerController* Controller = BlockedController.Get())
	{
		Controller->RequestCursor(InputBlockReason, true, this);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ReturnButton);
		Controller->SetInputMode(InputMode);
	}
	if (ReturnButton.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(ReturnButton, EFocusCause::SetDirectly);
	}
}

FReply UNUFieldLoadingSubsystem::ReturnToLogin()
{
	if (!bFailed)
	{
		return FReply::Handled();
	}
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (Settings->LoginLevel.IsNull())
	{
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		return FReply::Handled();
	}
	// 기존 초기화면 경로와 같은 로그인 레벨 및 연결 정리 규칙을 사용한다.
	if (UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>())
	{
		Server->Disconnect();
	}
	if (UNULocationRestoreSubsystem* Restore = GetGameInstance()->GetSubsystem<UNULocationRestoreSubsystem>())
	{
		Restore->ResetFieldEntry();
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Settings->LoginLevel);
	return FReply::Handled();
}
