#include "WorldTransition/World/NUWorldTransitionSubsystem.h"
#include "WorldTransition/Data/NUWorldTransitionSettings.h"
#include "WorldTransition/Data/NUWorldPortalDataSubsystem.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Net/EmberServerManager.h"
#include "UI/NUUIManager.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Field/FieldPlayerCharacter.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"
#include "SRevGame/GameMessage.h"

#include "WorldTransition/World/NUWorldEntryPoint.h"
#include "WorldTransition/World/NUWorldRestoreSafety.h"
#include "WorldTransition/World/NUWorldPortal.h"
#include "WorldTransition/World/NUWorldStreamingSubsystem.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NUWorldDataSettings.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "PlayerController/NUPlayerController.h"
#include "Recovery/World/NURecoveryPoint.h"

#include "AbilitySystemInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Subsystems/SubsystemCollection.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUWorldTransition, Log, All);

#if WITH_EDITOR
// 오프라인 PIE의 이동 테스트에만 사용하며 서버 승인과 목적지 안전 검사는 유지한다.
static TAutoConsoleVariable<int32> CVarPortalTestIgnoreRequirements(
	TEXT("nu.Portal.TestIgnoreRequirements"), 0,
	TEXT("오프라인 PIE 포탈 입장 조건 우회: 1 켜기, 0 끄기"), ECVF_Cheat);
#endif

const FName UNUWorldTransitionSubsystem::InputBlockReason(TEXT("WorldTransition"));

void UNUWorldTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Streaming = Collection.InitializeDependency<UNUWorldStreamingSubsystem>();
}

// 월드 종료 시 타이머와 페이드를 정리하고 입력을 복구한다.
void UNUWorldTransitionSubsystem::Deinitialize()
{
	GetWorld()->GetTimerManager().ClearTimer(PreparationTimer);
	GetWorld()->GetTimerManager().ClearTimer(FadeTimer);
	GetWorld()->GetTimerManager().ClearTimer(LocationRefreshTimer);
	if (bOwnsFade && TransitionController.IsValid())
	{
		TransitionController->ClearWorldTransitionFade();
	}
	if (State == ENUWorldTransitionState::AwaitingServerApproval)
	{
		ResetPortalRequest();
	}
	else if (IsTransitionInProgress())
	{
		ReportPortalPreparationFailure(ENUWorldTransitionFailure::Cancelled);
		RestoreCharacterState(!bDestinationCommitted);
	}
	if (IsValid(Streaming))
	{
		Streaming->ReleaseSpace(PendingSpace);
		Streaming->ReleaseSpace(ActiveSpace);
	}
	State = ENUWorldTransitionState::Idle;

	if (bRestoreRequestActive)
	{
		CompleteRestoreRequest(false);
	}

	ResetPortalRequest();
	Super::Deinitialize();
}

bool UNUWorldTransitionSubsystem::CanRequestTransition(const AActor* Interactor) const
{
	const ACharacter* Character = Cast<ACharacter>(Interactor);
	if (!IsValid(Character) || !Character->IsLocallyControlled() || IsTransitionInProgress())
	{
		return false;
	}
	const ANUPlayerController* Controller = Cast<ANUPlayerController>(Character->GetController());
	return (!IsValid(Controller) || !Controller->IsGameInputBlocked()) && !IsBattleActive(Character);
}

bool UNUWorldTransitionSubsystem::RequestPortalTransition(AActor* Interactor, const ANUWorldPortal* Portal)
{
	if (IsTransitionInProgress())
	{
		return false;
	}
	ACharacter* Character = Cast<ACharacter>(Interactor);
	if (!IsValid(Portal) || Portal->GetWorld() != GetWorld() || !CanRequestTransition(Interactor))
	{
		LastFailure = ENUWorldTransitionFailure::InvalidRequest;
		return false;
	}
	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	const UNUWorldPortalDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldPortalDataSubsystem>() : nullptr;
	const APlayerState* PlayerState = Character->GetPlayerState();
	const int32 ResolvedAreaId = GetActiveAreaId(Character);
	const int32 CurrentAreaId = ResolvedAreaId > 0 ? ResolvedAreaId : GetDefault<UNUWorldDataSettings>()->GetAreaIdForMap(GetActiveMapPackageName(Character));
	FNUWorldPortalRoute Route;
	FString Error;
	if (Data == nullptr || !Data->ResolvePortal(Portal->GetPortalId(), Portal->GetPlacementSide(), Route, Error))
	{
		LastFailure = ENUWorldTransitionFailure::PortalDataInvalid;
		UE_LOG(LogNUWorldTransition, Warning, TEXT("PortalId=%s: %s"), *Portal->GetPortalId().ToString(), *Error);
		return false;
	}
	const FNUWorldDestination& Source = Route.Source.Destination;
	const bool bSourceMatches = Source.IsPersistentLevelDestination()
		? GetActiveLevel(Character) == GetWorld()->PersistentLevel
		: Source.Level.ToSoftObjectPath().GetLongPackageName() == GetActiveMapPackageName(Character) && Source.LevelTransform.Equals(ActiveDestination.LevelTransform);
	if (!bSourceMatches || (CurrentAreaId > 0 && CurrentAreaId != Route.Source.AreaId) || Portal->GetLevel() != GetActiveLevel(Character) || FindEntryPoint(Portal->GetLevel(), Source.EntryPointId) == nullptr)
	{
		LastFailure = ENUWorldTransitionFailure::PortalDataInvalid;
		UE_LOG(LogNUWorldTransition, Warning, TEXT("PortalId=%s: 현재 공간과 출발 배치가 일치하지 않거나 출발 EntryPoint가 없습니다."), *Portal->GetPortalId().ToString());
		return false;
	}
	UEmberServerManager* Server = GameInstance->GetSubsystem<UEmberServerManager>();
	if (Server != nullptr && Server->IsLoggedIn())
	{
		int32 NumericPortalId = 0;
		if (!LexTryParseString(NumericPortalId, *Portal->GetPortalId().ToString()) || NumericPortalId <= 0)
		{
			LastFailure = ENUWorldTransitionFailure::PortalDataInvalid;
			return false;
		}

		// 서버 응답 전에는 목적지 로딩을 시작하지 않는다. 같은 오버랩에서 중복 요청되는 것도 상태로 차단한다.
		PendingPortalRoute = Route;
		PendingPortalMutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		PendingServerTransitionId.Reset();
		TransitionCharacter = Character;
		bPortalRequestActive = true;
		bPortalArrivalSent = false;
		State = ENUWorldTransitionState::AwaitingServerApproval;
		LastFailure = ENUWorldTransitionFailure::None;
		Server->SendPortalMove(NumericPortalId, Route.SourceSide, PendingPortalMutationId,
			Route.Target.LocationId, Route.Target.AreaId, Route.Target.Destination.EntryPointId.ToString());
		return true;
	}
	if (Server != nullptr && !Server->GetUserId().IsEmpty())
	{
		LastFailure = ENUWorldTransitionFailure::ServerContractUnavailable;
		UE_LOG(LogNUWorldTransition, Warning, TEXT("PortalId=%s: 서버 연결이 준비되지 않았습니다."), *Portal->GetPortalId().ToString());
		return false;
	}
	const UNUProgressionComponent* Progression = IsValid(PlayerState) ? PlayerState->FindComponentByClass<UNUProgressionComponent>() : nullptr;
	const UNUQuestComponent* Quest = IsValid(PlayerState) ? PlayerState->FindComponentByClass<UNUQuestComponent>() : nullptr;
	const UNUInventoryComponent* Inventory = IsValid(PlayerState) ? PlayerState->FindComponentByClass<UNUInventoryComponent>() : nullptr;
	const FNUWorldPortalDefinition& Definition = Route.Definition;
	const bool bQuestMet = Definition.UnlockQuestId == 0 || (Quest != nullptr && Quest->HasCompletedQuests({ FName(*LexToString(Definition.UnlockQuestId)) }));
	const int32 RequiredItemCount = Definition.RequiredItemId > 0 && Inventory != nullptr ? Inventory->CountItemsByNumericId(Definition.RequiredItemId) : 0;
	bool bIgnoreRequirements = false;
#if WITH_EDITOR
	bIgnoreRequirements = GetWorld()->WorldType == EWorldType::PIE && GetWorld()->GetNetMode() == NM_Standalone && CVarPortalTestIgnoreRequirements.GetValueOnGameThread() != 0;
#endif

	if (!bIgnoreRequirements && !Definition.AreRequirementsMet(Progression != nullptr ? Progression->GetLevel() : 0, bQuestMet, RequiredItemCount))
	{
		LastFailure = ENUWorldTransitionFailure::PortalRequirementsNotMet;
		if (UNUUIManager* UI = UNUUIManager::Get(Cast<APlayerController>(Character->GetController())))
		{
			UI->ShowNotice(Definition.GetRequirementFailureText(Progression != nullptr ? Progression->GetLevel() : 0, bQuestMet, RequiredItemCount));
		}
		UE_LOG(LogNUWorldTransition, Warning, TEXT("PortalId=%d: 비활성 상태이거나 입장 조건을 충족하지 못했습니다."), Definition.PortalId);
		return false;
	}
	FNUWorldDestination Destination = Route.Target.Destination;
	Destination.bUseFade = Definition.bLoadingScreen;
	Destination.ResolvedAreaId = Route.Target.AreaId;
	return RequestTransition(Interactor, Destination);
}

bool UNUWorldTransitionSubsystem::ResolveLocationPortal(const ANUWorldPortal* Portal, FNUWorldDestination& OutDestination, int32& OutLocationId, bool& bOutIndoor) const
{
	if (!IsValid(Portal)) { return false; }
	if (Portal->ReturnsToEntrance())
	{
		if (ActiveReturnRoutes.IsEmpty() || Portal->GetLevel() != Streaming->GetPreparedLevel(ActiveSpace)) { return false; }
		const FNUWorldReturnRoute& Route = ActiveReturnRoutes.Last();
		OutDestination = Route.Destination;
		OutLocationId = Route.LocationId;
		bOutIndoor = Route.bIndoor;
	}
	else
	{
		OutDestination = Portal->GetDestination();
		OutLocationId = Portal->GetDestinationLocationId();
		bOutIndoor = Portal->IsDestinationIndoor();
	}
	return OutDestination.IsConfigured();
}

bool UNUWorldTransitionSubsystem::CanRequestLocationTransition(const AActor* Interactor, const ANUWorldPortal* Portal) const
{
	if (!CanRequestTransition(Interactor) || !IsValid(Portal) || Portal->GetWorld() != GetWorld()) { return false; }
	FNUWorldDestination Destination;
	int32 TargetLocationId = 0;
	bool bTargetIndoor = false;
	if (!ResolveLocationPortal(Portal, Destination, TargetLocationId, bTargetIndoor)) { return false; }
	const UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>();
	if (!Server || Server->GetUserId().IsEmpty()) { return true; }
	if (!Server->IsLoggedIn()) { return false; }
	const UNUWorldDataSubsystem* Data = GetWorld()->GetGameInstance()->GetSubsystem<UNUWorldDataSubsystem>();
	const FNULocationDefinition* Target = Data ? Data->FindLocationDefinition(TargetLocationId) : nullptr;
	return Target && Target->bIndoor == bTargetIndoor;
}

bool UNUWorldTransitionSubsystem::RequestLocationTransition(AActor* Interactor, const ANUWorldPortal* Portal)
{
	if (!CanRequestTransition(Interactor) || !IsValid(Portal) || Portal->GetWorld() != GetWorld()) { return false; }
	ACharacter* Character = CastChecked<ACharacter>(Interactor);
	FNUWorldDestination Destination;
	int32 TargetLocationId = 0;
	bool bTargetIndoor = false;
	if (!ResolveLocationPortal(Portal, Destination, TargetLocationId, bTargetIndoor)) { return false; }
	UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>();
	const UNUWorldDataSubsystem* Data = GetWorld()->GetGameInstance()->GetSubsystem<UNUWorldDataSubsystem>();
	const FNULocationDefinition* Target = Data ? Data->FindLocationDefinition(TargetLocationId) : nullptr;
	if (Server && !Server->IsLoggedIn() && !Server->GetUserId().IsEmpty())
	{
		LastFailure = ENUWorldTransitionFailure::ServerContractUnavailable;
		return false;
	}
	// ID가 없는 기존 건물은 오프라인 배치 확인만 허용한다.
	if (Server && Server->IsLoggedIn() && (!Target || Target->bIndoor != bTargetIndoor))
	{
		LastFailure = ENUWorldTransitionFailure::PortalDataInvalid;
		UE_LOG(LogNUWorldTransition, Warning, TEXT("출입 목적 장소 ID 또는 실내외 정의가 없습니다. Portal=%s"), *Portal->GetName());
		return false;
	}
	APlayerState* PlayerState = Character->GetPlayerState();
	const UNUWorldLocationComponent* Location = PlayerState ? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
	UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();
	int32 SourceLocation = Location ? Location->GetCurrentLocationId() : 0;
	int32 SourceArea = GetActiveAreaId(Character);
	const UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
	bool bSourceIndoor = Participant && Participant->GetCurrentInterior();
	if (Server && Server->IsLoggedIn() && Sync)
	{
		if (!Sync->IsLocationMoveReady(SourceLocation)) { Sync->EvaluateOutdoorLocation(); return false; }
		Sync->GetConfirmedSpace(SourceArea, SourceLocation, bSourceIndoor);
	}
	if (SourceArea <= 0) { SourceArea = GetDefault<UNUWorldDataSettings>()->GetAreaIdForWorld(GetWorld()); }
	// 같은 층으로 향하는 반대편 계단 트리거는 이동을 다시 요청하지 않는다.
	if (SourceLocation == TargetLocationId && bSourceIndoor == bTargetIndoor) { return false; }
	PendingReturnRoutes = ActiveReturnRoutes;
	if (Portal->ReturnsToEntrance()) { PendingReturnRoutes.Pop(); }
	else if (!Portal->GetReturnEntryPointId().IsNone())
	{
		FNUWorldReturnRoute Route;
		Route.Destination = ActiveDestination;
		Route.Destination.EntryPointId = Portal->GetReturnEntryPointId();
		Route.Destination.ResolvedAreaId = SourceArea;
		Route.LocationId = SourceLocation;
		Route.bIndoor = bSourceIndoor;
		if (!FindEntryPoint(Portal->GetLevel(), Route.Destination.EntryPointId)) { PendingReturnRoutes.Reset(); return false; }
		PendingReturnRoutes.Add(Route);
	}
	Destination.ResolvedAreaId = Target ? Target->AreaId : SourceArea;
	bLocationRequestActive = true;
	bPendingIndoor = bTargetIndoor;
	PendingSourceLocationId = SourceLocation;
	PendingTargetLocationId = TargetLocationId;
	PendingGateId = Portal->GetGateId();
	LocationFadeOutSeconds = FMath::Max(0.0f, Portal->GetLocationFadeOutDuration());
	LocationFadeInSeconds = FMath::Max(0.0f, Portal->GetLocationFadeInDuration());
	PendingLocationMutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	const bool bStarted = RequestTransition(Interactor, Destination);
	if (!bStarted && State == ENUWorldTransitionState::Idle) { ResetPortalRequest(); }
	return bStarted;
}

bool UNUWorldTransitionSubsystem::PrepareLocationArrival()
{
	PendingInterior.Reset();
	if (!TransitionCharacter.IsValid() || !TransitionCharacter->FindComponentByClass<UNUInteriorParticipantComponent>()) { return false; }
	if (!bPendingIndoor) { return true; }
	ULevel* Level = Streaming->GetPreparedLevel(PendingSpace);
	if (!Level) { return false; }
	for (AActor* Actor : Level->Actors)
	{
		UNUWalkableInteriorComponent* Interior = IsValid(Actor) ? Actor->FindComponentByClass<UNUWalkableInteriorComponent>() : nullptr;
		if (Interior && (Interior->LocationId == PendingTargetLocationId || Interior->LocationId == 0))
		{
			if (PendingInterior.IsValid()) { return false; }
			PendingInterior = Interior;
		}
	}
	if (PendingInterior.IsValid()) { PendingInterior->LocationId = PendingTargetLocationId; }
	return PendingInterior.IsValid();
}

bool UNUWorldTransitionSubsystem::HandleLocationMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response)
{
	if (!bLocationRequestActive || !bLocationMoveSent || State != ENUWorldTransitionState::AwaitingServerConfirmation) { return false; }
	// 다른 이동이나 늦게 도착한 이전 요청의 응답은 소비하지 않는다.
	if (Response && Response->MutationId != PendingLocationMutationId) { return false; }
	GetWorld()->GetTimerManager().ClearTimer(LocationConnectionTimer);
	if (ResultCode != 200)
	{
		FailTransition(ENUWorldTransitionFailure::ServerRejected, *ResultMessage);
		return true;
	}
	const FVector Arrival = Response ? FVector(Response->PosX, Response->PosY, Response->PosZ) : FVector::ZeroVector;
	if (!Response || Response->FromLocationId != PendingSourceLocationId || Response->ToLocationId != PendingTargetLocationId
		|| Response->_location_id.LocationId != PendingTargetLocationId || Response->_area_id.AreaId != PendingDestination.ResolvedAreaId
		|| Response->_is_indoor.IsIndoor != bPendingIndoor || Arrival.ContainsNaN() || !Arrival.Equals(PendingArrival, 1.0f))
	{
		// 성공 응답의 내용이 잘못되면 서버 확정 여부를 추측해 계속 이동하지 않는다.
		if (UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>()) { Server->Disconnect(); }
		FailTransition(ENUWorldTransitionFailure::ServerResponseInvalid, TEXT("건물 출입 확정값을 검증할 수 없습니다. 재접속이 필요합니다."));
		return true;
	}
	CommitConfirmedTransition(Arrival);
	return true;
}

void UNUWorldTransitionSubsystem::CheckLocationConnection()
{
	if (!bLocationRequestActive || !bLocationMoveSent || State != ENUWorldTransitionState::AwaitingServerConfirmation) { return; }
	UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>();
	if (!Server || !Server->IsLoggedIn() || FPlatformTime::Seconds() - LocationRequestSentAt >= GetDefault<UNUWorldTransitionSettings>()->LocationMoveTimeoutSeconds)
	{
		// 단일 단계 요청은 서버에서 이미 확정됐을 수 있으므로 재접속 복원으로 정합성을 회복한다.
		if (Server && Server->IsLoggedIn()) { Server->Disconnect(); }
		FailTransition(ENUWorldTransitionFailure::ServerContractUnavailable, TEXT("출입 응답을 받지 못했습니다. 재접속 후 위치를 복원합니다."));
	}
}

void UNUWorldTransitionSubsystem::HandlePortalMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGamePortalMove* Response)
{
	if (!bPortalRequestActive || State != ENUWorldTransitionState::AwaitingServerApproval)
	{
		return;
	}

	if (ResultCode != 200)
	{
		RejectPortalRequest(ENUWorldTransitionFailure::ServerRejected,
			ResultMessage.IsEmpty() ? TEXT("서버가 포탈 이동을 거절했습니다.") : ResultMessage);
		return;
	}

	if (Response != nullptr && !Response->_transition_id.TransitionId.IsEmpty())
	{
		PendingServerTransitionId = Response->_transition_id.TransitionId;
	}

	const bool bValidResponse = Response != nullptr
		&& Response->MutationId == PendingPortalMutationId
		&& !PendingServerTransitionId.IsEmpty()
		&& Response->_transition_state.TransitionState == 1
		&& Response->ToAreaId == PendingPortalRoute.Target.AreaId
		&& Response->ToLocationId == PendingPortalRoute.Target.LocationId
		&& FName(*Response->EntryPointId) == PendingPortalRoute.Target.Destination.EntryPointId;
	if (!bValidResponse)
	{
		RejectPortalRequest(ENUWorldTransitionFailure::ServerResponseInvalid,
			TEXT("서버 포탈 승인 결과가 로컬 포탈 연결과 일치하지 않습니다."));
		return;
	}

	ACharacter* Character = TransitionCharacter.Get();
	if (!IsValid(Character))
	{
		RejectPortalRequest(ENUWorldTransitionFailure::Cancelled, TEXT("포탈 승인 대기 중 캐릭터가 사라졌습니다."));
		return;
	}

	FNUWorldDestination Destination = PendingPortalRoute.Target.Destination;
	Destination.EntryPointId = FName(*Response->EntryPointId);
	Destination.ResolvedAreaId = Response->ToAreaId;
	Destination.bUseFade = PendingPortalRoute.Definition.bLoadingScreen;

	// RequestTransition이 공통 로딩/입력 차단 상태를 설정할 수 있도록 승인 대기 상태만 해제한다.
	State = ENUWorldTransitionState::Idle;
	TransitionCharacter.Reset();
	if (!RequestTransition(Character, Destination))
	{
		// AcquireSpace 이후 실패한 경우 FailTransition이 입력 복구와 페이드 인을 이미 진행한다.
		if (IsTransitionInProgress())
		{
			return;
		}
		ReportPortalPreparationFailure(LastFailure);
		RejectPortalRequest(LastFailure, TEXT("서버 승인 후 목적지 준비를 시작하지 못했습니다."));
	}
}

void UNUWorldTransitionSubsystem::HandlePortalArriveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGamePortalArrive* Response)
{
	if (!bPortalRequestActive || !bPortalArrivalSent || State != ENUWorldTransitionState::AwaitingServerConfirmation)
	{
		return;
	}

	if (ResultCode != 200)
	{
		FailTransition(ENUWorldTransitionFailure::ServerRejected,
			ResultMessage.IsEmpty() ? TEXT("서버가 포탈 도착 확정을 거절했습니다.") : *ResultMessage);
		return;
	}

	const FVector ConfirmedArrival = Response != nullptr
		? FVector(Response->PosX, Response->PosY, Response->PosZ) : FVector::ZeroVector;
	const bool bValidResponse = Response != nullptr
		&& Response->TransitionId == PendingServerTransitionId
		&& Response->_transition_state.TransitionState == 2
		&& Response->_area_id.AreaId == PendingPortalRoute.Target.AreaId
		&& Response->_location_id.LocationId == PendingPortalRoute.Target.LocationId
		&& !ConfirmedArrival.ContainsNaN()
		&& FVector::DistSquared(ConfirmedArrival, PendingArrival) <= FMath::Square(50.0f);
	if (!bValidResponse)
	{
		FailTransition(ENUWorldTransitionFailure::ServerResponseInvalid,
			TEXT("서버 포탈 도착 확정값을 검증할 수 없습니다."));
		return;
	}

	PendingDestination.ResolvedAreaId = Response->_area_id.AreaId;
	if (UFieldSyncSubsystem* FieldSync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>())
	{
		FieldSync->ApplyConfirmedSpace(Response->_area_id.AreaId,
			Response->_location_id.LocationId, Response->_is_indoor.IsIndoor);
	}
	CommitConfirmedTransition(ConfirmedArrival);
}

bool UNUWorldTransitionSubsystem::RequestTransition(AActor* Interactor, const FNUWorldDestination& Destination)
{
	return StartTransition(Interactor, Destination, false);
}

bool UNUWorldTransitionSubsystem::StartTransition(AActor* Interactor, const FNUWorldDestination& Destination, bool bInternalRestore)
{
	// 중복 요청은 진행 중인 전환 상태를 변경하지 않는다.
	if (IsTransitionInProgress())
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(Interactor);
	if (!IsValid(Character) || Character->GetWorld() != GetWorld() || !Character->IsLocallyControlled() || !Destination.IsConfigured() || !IsValid(Character->GetCharacterMovement()))
	{
		LastFailure = ENUWorldTransitionFailure::InvalidRequest;
		return false;
	}

	ANUPlayerController* Controller = Cast<ANUPlayerController>(Character->GetController());
	// 내부 복원은 로딩 입력 차단을 유지한 채 진행한다. 일반 이동은 차단을 따른다.
	if (!bInternalRestore && IsValid(Controller) && Controller->IsGameInputBlocked())
	{
		LastFailure = ENUWorldTransitionFailure::InputBlocked;
		return false;
	}
	if (IsBattleActive(Character))
	{
		LastFailure = ENUWorldTransitionFailure::BattleActive;
		return false;
	}

	if (ActiveCharacter.Get() != Character)
	{
		Streaming->ReleaseSpace(ActiveSpace);
		ActiveDestination = {};
		ActiveCharacter = Character;
	}

	// 출발 Portal 해제에 대비해 목적지와 복구 상태를 복사한다.
	PendingDestination = Destination;
	TransitionCharacter = Character;
	TransitionController = Controller;
	OriginalCharacterTransform = Character->GetActorTransform();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	OriginalMovementMode = Movement->MovementMode;
	OriginalCustomMovementMode = Movement->CustomMovementMode;
	bDestinationCommitted = false;
	bOwnsFade = false;
	State = ENUWorldTransitionState::PreparingDestination;
	LastFailure = ENUWorldTransitionFailure::None;

	// 전환 중 이동과 입력을 차단한다.
	SuspendedFieldSync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();
	if (SuspendedFieldSync.IsValid())
	{
		SuspendedFieldSync->SuspendPositionReporting(this);
	}
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	if (IsValid(Controller))
	{
		Controller->RequestGameInputBlock(InputBlockReason, true, this);
		bInputBlockRequested = true;
	}

	ENUWorldTransitionFailure Failure;
	// 목적지 로딩과 암전을 진행하고, 둘 다 완료된 후 이동한다.
	if (!Streaming->AcquireSpace(PendingDestination, PendingSpace, Failure))
	{
		FailTransition(Failure, TEXT("목적 레벨 준비 요청이 거절되었습니다."));
		return false;
	}

	// 사용자가 탄 포탈 이동에서만 --- 건물 출입은 문 소리가 맡고, 접속 복원·부활 이동은 장소 요청 플래그를 켠 채 들어온다
	if (!bLocationRequestActive)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Field::PortalDepart);
	}

	const UNUWorldTransitionSettings* Settings = GetDefault<UNUWorldTransitionSettings>();
	// 건물 출입의 짧은 페이드는 배치 액터에서 편집하며 필드 이동 시퀀스와 구분한다.
	const bool bLocationFade = bLocationRequestActive && !bRestoreRequestActive;
	const float FadeOutSeconds = bLocationFade ? LocationFadeOutSeconds : FMath::Max(0.0f, Settings->FadeOutSeconds);
	FadeInSeconds = bLocationFade ? LocationFadeInSeconds : FMath::Max(0.0f, Settings->FadeInSeconds);
	const bool bEnableFade = Settings->bEnableFade && PendingDestination.bUseFade;
	FadeInSequence = bEnableFade && !bLocationFade ? Settings->FadeInSequence.LoadSynchronous() : nullptr;
	ULevelSequence* FadeOutSequence = bEnableFade && !bLocationFade ? Settings->FadeOutSequence.LoadSynchronous() : nullptr;
	bOwnsFade = bEnableFade && IsValid(Controller) && IsValid(Controller->PlayerCameraManager) && (IsValid(FadeOutSequence) || IsValid(FadeInSequence) || FadeOutSeconds > 0.0f || FadeInSeconds > 0.0f);
	if (bOwnsFade)
	{
		State = ENUWorldTransitionState::FadingOut;
		if (Controller->PlayWorldTransitionSequence(FadeOutSequence, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleFadeOutFinished), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleFadeInterrupted)))
		{
			return true;
		}

		// 시퀀스 재생 실패 시 기본 페이드로 대체한다.
		Controller->SetWorldTransitionFade(1.0f, FadeOutSeconds);
		if (FadeOutSeconds > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(FadeTimer, this, &ThisClass::HandleFadeOutFinished, FadeOutSeconds, false);
			return true;
		}
	}
	HandleFadeOutFinished();
	return LastFailure == ENUWorldTransitionFailure::None;
}

bool UNUWorldTransitionSubsystem::RequestRestoreTransition(AActor* Interactor, const FNUWorldRestoreTransitionRequest& Request)
{
	// 일반 포탈 전환과 같은 로딩, 페이드 흐름을 사용하되 서버 포탈 요청은 새로 보내지 않는다.
	if (bRestoreRequestActive || IsTransitionInProgress() || !Request.Destination.IsConfigured() || Request.Arrival.ContainsNaN()
		|| Request.RecoveryPointId < 0
		|| Request.Facing < static_cast<int32>(EFieldFacing::Down)
		|| Request.Facing > static_cast<int32>(EFieldFacing::Right))
	{
		UE_LOG(LogNUWorldTransition, Warning, TEXT("복원 요청 거절: active=%d transitioning=%d entry=%s arrival=%s facing=%d"),
			bRestoreRequestActive, IsTransitionInProgress(), *Request.Destination.EntryPointId.ToString(), *Request.Arrival.ToCompactString(), Request.Facing);
		return false;
	}

	bRestoreRequestActive = true;
	bLocationRequestActive = true;
	bPendingIndoor = Request.bIndoor;
	PendingTargetLocationId = Request.LocationId;
	PendingReturnRoutes.Reset();
	if (Request.ReturnDestination.IsConfigured())
	{
		FNUWorldReturnRoute Route;
		Route.Destination = Request.ReturnDestination;
		Route.LocationId = Request.ReturnLocationId;
		PendingReturnRoutes.Add(Route);
	}
	LastRestoreLocationId = Request.LocationId;
	RestoreFallbackLocationId = Request.FallbackLocationId;
	PendingRecoveryPointId = Request.RecoveryPointId;
	bRestoreUsesSavedArrival = Request.bUseSavedArrival;
	PendingRestoreArrival = Request.Arrival;
	PendingRestoreFacing = Request.Facing;
	RestoreSucceededDelegate = Request.OnSucceeded;
	RestoreFailedDelegate = Request.OnFailed;

	if (!StartTransition(Interactor, Request.Destination, true))
	{
		UE_LOG(LogNUWorldTransition, Warning, TEXT("복원 전환 시작 실패: Failure=%d actor=%s entry=%s"), static_cast<int32>(LastFailure), *GetNameSafe(Interactor), *Request.Destination.EntryPointId.ToString());
		if (bRestoreRequestActive && State == ENUWorldTransitionState::Idle)
		{
			ResetPortalRequest();
			CompleteRestoreRequest(false);
		}

		return false;
	}

	return true;
}

// 목적지 준비가 끝날 때까지 암전을 유지한다.
void UNUWorldTransitionSubsystem::HandleFadeOutFinished()
{
	if (bOwnsFade && TransitionController.IsValid())
	{
		TransitionController->SetWorldTransitionFade(1.0f, 0.0f);
	}
	State = ENUWorldTransitionState::PreparingDestination;
	ContinueTransition();
	if (State == ENUWorldTransitionState::PreparingDestination)
	{
		GetWorld()->GetTimerManager().SetTimer(PreparationTimer, this, &ThisClass::ContinueTransition, 0.05f, true);
	}
}

void UNUWorldTransitionSubsystem::HandleFadeInterrupted()
{
	if (State == ENUWorldTransitionState::FadingOut)
	{
		FailTransition(ENUWorldTransitionFailure::Cancelled, TEXT("출입 암전 시퀀스 재생이 중단되었습니다."));
	}
	else if (State == ENUWorldTransitionState::FadingIn)
	{
		FinishTransition();
	}
}

void UNUWorldTransitionSubsystem::BeginFadeIn()
{
	GetWorld()->GetTimerManager().ClearTimer(FadeTimer);
	const bool bUseSequence = State != ENUWorldTransitionState::FadingOut;
	State = ENUWorldTransitionState::FadingIn;
	if (bOwnsFade && TransitionController.IsValid())
	{
		TransitionController->StopWorldTransitionSequence();
		if (bUseSequence && TransitionController->PlayWorldTransitionSequence(FadeInSequence, FSimpleDelegate::CreateUObject(this, &ThisClass::FinishTransition), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleFadeInterrupted)))
		{
			return;
		}
	}
	if (bOwnsFade && TransitionController.IsValid() && FadeInSeconds > 0.0f)
	{
		TransitionController->SetWorldTransitionFade(0.0f, FadeInSeconds);
		GetWorld()->GetTimerManager().SetTimer(FadeTimer, this, &ThisClass::FinishTransition, FadeInSeconds, false);
		return;
	}
	FinishTransition();
}

void UNUWorldTransitionSubsystem::FinishTransition()
{
	GetWorld()->GetTimerManager().ClearTimer(FadeTimer);
	if (bOwnsFade && TransitionController.IsValid())
	{
		TransitionController->ClearWorldTransitionFade();
	}
	bOwnsFade = false;
	FadeInSequence = nullptr;
	RestoreCharacterState(false);
	State = ENUWorldTransitionState::Idle;
	if (bDestinationCommitted)
	{
		GetWorld()->GetTimerManager().ClearTimer(LocationRefreshTimer);
		// 도착 위치의 Overlap 갱신 후 장소를 판정한다.
		LocationRefreshTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshWorldLocation);
	}
	else if (bRestoreRequestActive)
	{
		CompleteRestoreRequest(false);
	}

	ResetPortalRequest();
}

void UNUWorldTransitionSubsystem::ContinueTransition()
{
	if (State != ENUWorldTransitionState::PreparingDestination)
	{
		return;
	}
	if (!TransitionCharacter.IsValid())
	{
		FailTransition(ENUWorldTransitionFailure::Cancelled, TEXT("전환 중 캐릭터가 사라졌습니다."));
		return;
	}
	ENUWorldTransitionFailure Failure;
	const ENUWorldSpacePreparation Preparation = Streaming->GetPreparation(PendingSpace, Failure);
	if (Preparation == ENUWorldSpacePreparation::Pending)
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(PreparationTimer);
	if (Preparation == ENUWorldSpacePreparation::Failed)
	{
		FailTransition(Failure, TEXT("목적 레벨 준비에 실패했습니다."));
		return;
	}

	if (bLocationRequestActive && !PrepareLocationArrival())
	{
		FailTransition(ENUWorldTransitionFailure::InvalidRequest, TEXT("목적 실내가 없거나 중복입니다."));
		return;
	}

	if (bRestoreRequestActive)
	{
		AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(TransitionCharacter.Get());

		if (!IsValid(FieldCharacter))
		{
			FailTransition(ENUWorldTransitionFailure::InvalidRequest,
				TEXT("복원 위치를 적용할 필드 캐릭터가 없습니다."));
			return;
		}

		FieldCharacter->Facing = static_cast<EFieldFacing>(PendingRestoreFacing);
		if (PendingRecoveryPointId > 0)
		{
			const ULevel* Level = Streaming->GetPreparedLevel(PendingSpace);
			const ANURecoveryPoint* Point = nullptr;
			if (Level != nullptr)
			{
				for (AActor* Actor : Level->Actors)
				{
					const ANURecoveryPoint* Candidate = Cast<ANURecoveryPoint>(Actor);
					if (IsValid(Candidate) && Candidate->LocationId == PendingRecoveryPointId)
					{
						if (Point != nullptr)
						{
							FailTransition(ENUWorldTransitionFailure::InvalidRequest, TEXT("부활 거점 액터가 중복되었습니다."));
							return;
						}
						Point = Candidate;
					}
				}
			}
			const FVector Arrival = Point != nullptr ? Point->GetRecoveryTransform().GetLocation() : FVector::ZeroVector;
			if (Point == nullptr || !NUWorldRestoreSafety::IsSafe(FieldCharacter, PendingInterior.Get(), Arrival))
			{
				FailTransition(ENUWorldTransitionFailure::UnsafeEntryPoint, TEXT("부활 거점 액터가 없거나 도착 위치가 안전하지 않습니다."));
				return;
			}
			State = ENUWorldTransitionState::Moving;
			CommitConfirmedTransition(Arrival);
			return;
		}

		// 저장 좌표가 없으면 아래의 기존 EntryPoint 검증과 안전 위치 계산을 그대로 사용한다.
		if (bRestoreUsesSavedArrival && NUWorldRestoreSafety::IsSafe(FieldCharacter, PendingInterior.Get(), PendingRestoreArrival))
		{
			State = ENUWorldTransitionState::Moving;
			CommitConfirmedTransition(PendingRestoreArrival);
			return;
		}
		if (bPendingIndoor)
		{
			FVector SafeArrival;
			if (!NUWorldRestoreSafety::FindInteriorArrival(FieldCharacter, PendingInterior.Get(), SafeArrival))
			{
				FailTransition(ENUWorldTransitionFailure::UnsafeEntryPoint, TEXT("안전한 실내 복원 도착점이 없습니다."));
				return;
			}
			UE_LOG(LogNUWorldTransition, Warning, TEXT("실내 복원 도착점 사용 old=%s new=%s location=%d"),
				*PendingRestoreArrival.ToCompactString(), *SafeArrival.ToCompactString(), PendingTargetLocationId);
			State = ENUWorldTransitionState::Moving;
			CommitConfirmedTransition(SafeArrival);
			return;
		}
	}

	ANUWorldEntryPoint* EntryPoint = FindEntryPoint(Streaming->GetPreparedLevel(PendingSpace), PendingDestination.EntryPointId);
	if (!IsValid(EntryPoint))
	{
		FailTransition(ENUWorldTransitionFailure::EntryPointMissing, TEXT("목적 레벨의 진입점이 없거나 중복되었습니다."));
		return;
	}
	if (bRestoreRequestActive)
	{
		FVector SafeArrival;
		if (RestoreFallbackLocationId <= 0 
			|| (bPendingIndoor && RestoreFallbackLocationId != PendingTargetLocationId)
			|| !NUWorldRestoreSafety::FindEntryArrival(TransitionCharacter.Get(), PendingInterior.Get(), EntryPoint, SafeArrival))
		{
			FailTransition(ENUWorldTransitionFailure::UnsafeEntryPoint, TEXT("저장 좌표와 복원 진입점 모두 안전하지 않습니다."));
			return;
		}
		
		UE_LOG(LogNUWorldTransition, Warning, TEXT("복원 진입점 사용 old=%s new=%s location=%d; FieldEnter로 동기화"), 
		*PendingRestoreArrival.ToCompactString(), 
		*SafeArrival.ToCompactString(), RestoreFallbackLocationId
	);
		LastRestoreLocationId = RestoreFallbackLocationId;
		PendingTargetLocationId = RestoreFallbackLocationId;
		State = ENUWorldTransitionState::Moving;
		CommitConfirmedTransition(SafeArrival);
		
		return;
	}
	State = ENUWorldTransitionState::Moving;
	CompleteTransition(EntryPoint);
}

// 목적 레벨에서만 검색하며, ID가 중복되면 실패한다.
ANUWorldEntryPoint* UNUWorldTransitionSubsystem::FindEntryPoint(ULevel* Level, FName EntryPointId) const
{
	ANUWorldEntryPoint* Found = nullptr;
	if (!IsValid(Level))
	{
		return nullptr;
	}
	for (AActor* Actor : Level->Actors)
	{
		ANUWorldEntryPoint* EntryPoint = Cast<ANUWorldEntryPoint>(Actor);
		if (IsValid(EntryPoint) && EntryPoint->GetEntryPointId() == EntryPointId)
		{
			if (Found != nullptr)
			{
				return nullptr;
			}
			Found = EntryPoint;
		}
	}
	return Found;
}

bool UNUWorldTransitionSubsystem::FindSafeArrival(ACharacter* Character, const ANUWorldEntryPoint* EntryPoint, FVector& OutLocation) const
{
	if (EntryPoint->UsesFeetLocation())
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
		const FVector Scale = bLocationRequestActive && Participant ? Participant->GetTransitionScale(PendingInterior.Get()) : Character->GetActorScale3D();
		const float ShapeScale = (Capsule->GetComponentScale() * Scale / Character->GetActorScale3D()).GetAbsMin();
		const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight() * ShapeScale;
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetUnscaledCapsuleRadius() * ShapeScale, HalfHeight);
		const FVector Center = EntryPoint->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight);
		const FVector Range(0.0f, 0.0f, Movement->MaxStepHeight);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(LocationArrival), false, Character);
		const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
		FHitResult Floor;
		if (!GetWorld()->SweepSingleByChannel(Floor, Center + Range, Center - Range, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses)
			|| Floor.bStartPenetrating || !Movement->IsWalkable(Floor))
		{
			UE_LOG(LogNUWorldTransition, Warning, TEXT("도착 바닥 검사 실패 Entry=%s Feet=%s Hit=%s Blocking=%d Penetrating=%d Normal=%s"), *EntryPoint->GetName(), *EntryPoint->GetActorLocation().ToCompactString(), *GetNameSafe(Floor.GetActor()), Floor.bBlockingHit, Floor.bStartPenetrating, *Floor.ImpactNormal.ToCompactString());
			return false;
		}
		OutLocation = Floor.Location + FVector(0.0f, 0.0f, UCharacterMovementComponent::MIN_FLOOR_DIST);
		return !GetWorld()->OverlapBlockingTestByChannel(OutLocation, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses);
	}
	// 캡슐 중심 기준으로 충돌과 보행 가능한 바닥을 검사한다.
	OutLocation = EntryPoint->GetActorLocation();
	if (OutLocation.ContainsNaN() || !GetWorld()->FindTeleportSpot(Character, OutLocation, OriginalCharacterTransform.Rotator()))
	{
		return false;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	FFindFloorResult Floor;
	const float FloorDistance = Movement->MaxStepHeight + UCharacterMovementComponent::MAX_FLOOR_DIST;
	Movement->ComputeFloorDist(OutLocation, FloorDistance, FloorDistance, Floor, Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
	return Floor.IsWalkableFloor() && !Floor.HitResult.bStartPenetrating;
}

void UNUWorldTransitionSubsystem::CompleteTransition(ANUWorldEntryPoint* EntryPoint)
{
	ACharacter* Character = TransitionCharacter.Get();
	FVector Arrival;
	if (!IsValid(Character) || !IsValid(EntryPoint))
	{
		FailTransition(ENUWorldTransitionFailure::InvalidRequest, TEXT("이동 직전에 캐릭터 또는 진입점이 사라졌습니다."));
		return;
	}
	if (!FindSafeArrival(Character, EntryPoint, Arrival))
	{
		FailTransition(ENUWorldTransitionFailure::UnsafeEntryPoint, TEXT("진입점의 캡슐 공간 또는 걸을 수 있는 바닥이 없습니다."));
		return;
	}

	if (bLocationRequestActive && !bRestoreRequestActive)
	{
		UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>();
		if (Server && Server->IsLoggedIn())
		{
			PendingArrival = Arrival;
			bLocationMoveSent = true;
			State = ENUWorldTransitionState::AwaitingServerConfirmation;
			LocationRequestSentAt = FPlatformTime::Seconds();
			GetWorld()->GetTimerManager().SetTimer(LocationConnectionTimer, this, &ThisClass::CheckLocationConnection, 0.1f, true);
			Server->SendLocationMove(PendingSourceLocationId, PendingTargetLocationId, PendingGateId, PendingLocationMutationId, Arrival, GetTransitionFacing());
			return;
		}
		if (Server && !Server->GetUserId().IsEmpty())
		{
			FailTransition(ENUWorldTransitionFailure::ServerContractUnavailable, TEXT("실내 준비 중 서버 연결이 종료되었습니다."));
			return;
		}
	}
	if (bPortalRequestActive && !PendingServerTransitionId.IsEmpty())
	{
		// 서버가 공간을 바꾸기 전까지는 로컬 캐릭터도 출발 위치에 둔다.
		PendingArrival = Arrival;
		bPortalArrivalSent = true;
		State = ENUWorldTransitionState::AwaitingServerConfirmation;
		if (UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>())
		{
			Server->SendPortalArrive(PendingServerTransitionId, true, 0, PendingArrival, GetTransitionFacing());
			return;
		}
		FailTransition(ENUWorldTransitionFailure::ServerContractUnavailable, TEXT("포탈 도착을 확정할 서버 연결이 없습니다."));
		return;
	}

	CommitConfirmedTransition(Arrival);
}

void UNUWorldTransitionSubsystem::CommitConfirmedTransition(const FVector& Arrival)
{
	ACharacter* Character = TransitionCharacter.Get();
	if (!IsValid(Character))
	{
		FailTransition(ENUWorldTransitionFailure::InvalidRequest, TEXT("이동 확정 직전에 캐릭터가 사라졌습니다."));
		return;
	}

	// 기존 회전을 유지한 채 검사된 위치로 텔레포트한다.
	UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
	const bool bMoved = bLocationRequestActive && Participant
		? Participant->ApplyRestoredState(PendingInterior.Get(), &Arrival)
		: Character->TeleportTo(Arrival, OriginalCharacterTransform.Rotator(), false, true);
	if (!bMoved)
	{
		if (bLocationMoveSent)
		{
			if (UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>()) { Server->Disconnect(); }
		}
		FailTransition(ENUWorldTransitionFailure::TeleportFailed, TEXT("캐릭터 이동에 실패했습니다."));
		return;
	}

	if (bLocationRequestActive && bLocationMoveSent)
	{
		if (UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>()) { Sync->ApplyConfirmedSpace(PendingDestination.ResolvedAreaId, PendingTargetLocationId, bPendingIndoor); }
	}
	// 텔레포트의 이동 모드 변경에 대비해 연출 종료까지 다시 고정한다.
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->GetCharacterMovement()->DisableMovement();

	// 이번 갱신에서만 카메라 지연을 끄고 도착 위치에 즉시 맞춘다.
	if (USpringArmComponent* Arm = Character->FindComponentByClass<USpringArmComponent>())
	{
		const bool bLocationLag = Arm->bEnableCameraLag;
		const bool bRotationLag = Arm->bEnableCameraRotationLag;
		Arm->bEnableCameraLag = false;
		Arm->bEnableCameraRotationLag = false;
		Arm->TickComponent(0.0f, LEVELTICK_All, nullptr);
		Arm->bEnableCameraLag = bLocationLag;
		Arm->bEnableCameraRotationLag = bRotationLag;
	}
	if (APlayerController* Controller = Cast<APlayerController>(Character->GetController()))
	{
		if (IsValid(Controller->PlayerCameraManager))
		{
			Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
			Controller->PlayerCameraManager->UpdateCamera(0.0f);
		}
	}

	SuppressArrivalPortals(Character);
	FNUWorldSpaceHandle PreviousSpace = ActiveSpace;
	ActiveSpace = PendingSpace;
	PendingSpace = {};
	ActiveDestination = PendingDestination;
	ActiveReturnRoutes = MoveTemp(PendingReturnRoutes);
	ActiveCharacter = Character;
	LocationRefreshCharacter = Character;
	bDestinationCommitted = true;
	LastFailure = ENUWorldTransitionFailure::None;

	// 이동 성공 후 출발 공간의 참조를 해제한다.
	Streaming->ReleaseSpace(PreviousSpace);
	if (!bLocationRequestActive)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Field::PortalArrive);
	}
	BeginFadeIn();
}

void UNUWorldTransitionSubsystem::FailTransition(ENUWorldTransitionFailure Failure, const TCHAR* Detail)
{
	UE_LOG(LogNUWorldTransition, Warning, TEXT("공간 이동 실패. Failure=%d Detail=%s"), static_cast<int32>(Failure), Detail);

	GetWorld()->GetTimerManager().ClearTimer(LocationConnectionTimer);
	GetWorld()->GetTimerManager().ClearTimer(PreparationTimer);
	ReportPortalPreparationFailure(Failure);
	// 실패한 목적지를 정리하고 출발 위치로 복구한다.
	Streaming->ReleaseSpace(PendingSpace);
	if (ACharacter* Character = TransitionCharacter.Get())
	{
		Character->SetActorTransform(OriginalCharacterTransform, false, nullptr, ETeleportType::TeleportPhysics);
		SuppressArrivalPortals(Character);
	}
	LastFailure = Failure;
	BeginFadeIn();
}

void UNUWorldTransitionSubsystem::ReportPortalPreparationFailure(ENUWorldTransitionFailure Failure)
{
	if (!bPortalRequestActive || bPortalArrivalSent || PendingServerTransitionId.IsEmpty())
	{
		return;
	}

	if (UEmberServerManager* Server = GetWorld()->GetGameInstance()->GetSubsystem<UEmberServerManager>())
	{
		bPortalArrivalSent = true;
		Server->SendPortalArrive(PendingServerTransitionId, false, static_cast<int32>(Failure),
			OriginalCharacterTransform.GetLocation(), GetTransitionFacing());
	}
}

void UNUWorldTransitionSubsystem::RejectPortalRequest(ENUWorldTransitionFailure Failure, const FString& Detail)
{
	UE_LOG(LogNUWorldTransition, Warning, TEXT("포탈 서버 요청 실패. Failure=%d Detail=%s"),
		static_cast<int32>(Failure), *Detail);
	// 승인을 받은 뒤 로컬 검증에서 실패했다면 열려 있는 서버 전환도 즉시 닫는다.
	ReportPortalPreparationFailure(Failure);
	LastFailure = Failure;
	State = ENUWorldTransitionState::Idle;
	if (ANUPlayerController* Controller = Cast<ANUPlayerController>(TransitionCharacter.IsValid()
		? TransitionCharacter->GetController() : nullptr))
	{
		if (UNUUIManager* UI = UNUUIManager::Get(Controller))
		{
			UI->ShowNotice(FText::FromString(Detail));
		}
	}
	TransitionCharacter.Reset();
	ResetPortalRequest();
}

void UNUWorldTransitionSubsystem::ResetPortalRequest()
{
	GetWorld()->GetTimerManager().ClearTimer(LocationConnectionTimer);
	bLocationRequestActive = false;
	bLocationMoveSent = false;
	bPendingIndoor = false;
	PendingSourceLocationId = 0;
	PendingTargetLocationId = 0;
	PendingGateId = 0;
	PendingLocationMutationId.Reset();
	PendingInterior.Reset();
	PendingReturnRoutes.Reset();
	bPortalRequestActive = false;
	bPortalArrivalSent = false;
	PendingPortalMutationId.Reset();
	PendingServerTransitionId.Reset();
	PendingArrival = FVector::ZeroVector;
	PendingPortalRoute = {};
}

int32 UNUWorldTransitionSubsystem::GetTransitionFacing() const
{
	const AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(TransitionCharacter.Get());
	return FieldCharacter != nullptr ? static_cast<int32>(FieldCharacter->Facing) : 0;
}

void UNUWorldTransitionSubsystem::RestoreCharacterState(bool bRestoreOriginalTransform)
{
	ACharacter* Character = TransitionCharacter.Get();
	if (IsValid(Character))
	{
		if (bRestoreOriginalTransform)
		{
			Character->SetActorTransform(OriginalCharacterTransform, false, nullptr, ETeleportType::TeleportPhysics);
			SuppressArrivalPortals(Character);
		}
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(OriginalMovementMode, OriginalCustomMovementMode);
		}
	}

	// 이 전환이 요청한 입력 차단만 해제한다.
	if (SuspendedFieldSync.IsValid())
	{
		SuspendedFieldSync->ResumePositionReporting(this);
	}
	SuspendedFieldSync.Reset();
	if (bInputBlockRequested && TransitionController.IsValid())
	{
		TransitionController->RequestGameInputBlock(InputBlockReason, false, this);
	}
	bInputBlockRequested = false;
	TransitionCharacter.Reset();
	TransitionController.Reset();
}

// 도착한 출구를 벗어날 때까지 자동 재진입을 차단한다.
void UNUWorldTransitionSubsystem::SuppressArrivalPortals(ACharacter* Character)
{
	TArray<AActor*> Portals;
	Character->GetCapsuleComponent()->GetOverlappingActors(Portals, ANUWorldPortal::StaticClass());
	for (AActor* Actor : Portals)
	{
		CastChecked<ANUWorldPortal>(Actor)->SuppressUntilExit(Character);
	}
}

// 지역과 장소 표시, 퀘스트 도달 판정 갱신.
void UNUWorldTransitionSubsystem::RefreshWorldLocation()
{
	ACharacter* Character = LocationRefreshCharacter.Get();
	LocationRefreshCharacter.Reset();
	APlayerState* PlayerState = IsValid(Character) ? Character->GetPlayerState() : nullptr;
	UNUWorldLocationComponent* Location = IsValid(PlayerState) ? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;

	if (IsValid(Location))
	{
		Location->RefreshLocation();
	}

	if (bRestoreRequestActive)
	{
		CompleteRestoreRequest(true);
	}
}

void UNUWorldTransitionSubsystem::CompleteRestoreRequest(bool bSuccess)
{
	FSimpleDelegate Completion = bSuccess ? RestoreSucceededDelegate : RestoreFailedDelegate;

	// 콜백이 다음 FieldEnter 시도를 시작할 수 있으므로 내부 상태부터 정리한다.
	bRestoreRequestActive = false;
	bRestoreUsesSavedArrival = false;
	PendingRecoveryPointId = 0;
	PendingRestoreArrival = FVector::ZeroVector;
	PendingRestoreFacing = 0;
	RestoreSucceededDelegate.Unbind();
	RestoreFailedDelegate.Unbind();

	Completion.ExecuteIfBound();
}

bool UNUWorldTransitionSubsystem::IsBattleActive(const ACharacter* Character) const
{
	const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Character);
	const UNUAbilitySystemComponent* AbilitySystem = AbilityOwner != nullptr ? Cast<UNUAbilitySystemComponent>(AbilityOwner->GetAbilitySystemComponent()) : nullptr;
	if (AbilitySystem != nullptr && AbilitySystem->GetBattleEncounter() != nullptr)
	{
		return true;
	}
	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	const UNUBattleNetSubsystem* BattleNet = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	return BattleNet != nullptr && BattleNet->IsInBattle();
}

// 화면 복귀 중 취소하면 현재 위치에서 연출만 종료한다.
void UNUWorldTransitionSubsystem::CancelTransition()
{
	if (State == ENUWorldTransitionState::AwaitingServerApproval)
	{
		RejectPortalRequest(ENUWorldTransitionFailure::Cancelled, TEXT("포탈 승인 요청이 취소되었습니다."));
		return;
	}
	// 도착 보고 뒤에는 서버 확정 응답을 적용해야 양쪽 공간 상태가 갈라지지 않는다.
	if (State == ENUWorldTransitionState::AwaitingServerConfirmation)
	{
		return;
	}
	if (State == ENUWorldTransitionState::FadingIn)
	{
		FinishTransition();
		return;
	}
	if (IsTransitionInProgress())
	{
		FailTransition(ENUWorldTransitionFailure::Cancelled, TEXT("요청이 취소되었습니다."));
	}
}

bool UNUWorldTransitionSubsystem::IsTransitionInProgress() const
{
	return State != ENUWorldTransitionState::Idle;
}

ENUWorldTransitionState UNUWorldTransitionSubsystem::GetTransitionState() const
{
	return State;
}

ENUWorldTransitionFailure UNUWorldTransitionSubsystem::GetLastFailure() const
{
	return LastFailure;
}

ULevelStreaming* UNUWorldTransitionSubsystem::GetActiveStreamingLevel() const
{
	return Streaming->GetStreamingLevel(ActiveSpace);
}

ULevel* UNUWorldTransitionSubsystem::GetActiveLevel(const APawn* Pawn) const
{
	return ActiveCharacter.Get() == Pawn ? Streaming->GetPreparedLevel(ActiveSpace) : GetWorld()->PersistentLevel.Get();
}

FString UNUWorldTransitionSubsystem::GetActiveMapPackageName(const APawn* Pawn) const
{
	return ActiveCharacter.Get() == Pawn && !ActiveSpace.IsPersistent() ? ActiveDestination.Level.ToSoftObjectPath().GetLongPackageName() : UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
}

int32 UNUWorldTransitionSubsystem::GetActiveAreaId(const APawn* Pawn) const
{
	return IsValid(Pawn) && ActiveCharacter.Get() == Pawn ? ActiveDestination.ResolvedAreaId : 0;
}
