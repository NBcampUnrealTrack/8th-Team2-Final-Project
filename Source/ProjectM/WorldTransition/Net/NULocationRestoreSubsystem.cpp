#include "WorldTransition/Net/NULocationRestoreSubsystem.h"
#include "WorldTransition/World/NUFieldLoadingSubsystem.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "Field/FieldPlayerCharacter.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"
#include "Net/EmberServerManager.h"
#include "Recovery/Data/NURecoverySettings.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NUWorldDataSettings.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"
#include "WorldLocation/World/NULocationTrigger.h"
#include "WorldTransition/Data/NUWorldPortalDefinition.h"
#include "WorldTransition/Data/NUWorldPortalDataSubsystem.h"
#include "WorldTransition/Data/NUWorldTransitionSettings.h"
#include "WorldTransition/Data/NUWorldTransitionTypes.h"
#include "WorldTransition/World/NUWorldTransitionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "WorldTransition/World/NUWorldEntryPoint.h"
#include "WorldTransition/World/NUWorldRestoreSafety.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogNULocationRestore, Log, All);

namespace
{
	constexpr int32 SuccessResultCode = 200;
	constexpr int32 PortalCancelledResultCode = 2407;
	constexpr double InteriorReadyTimeoutSeconds = 5.0;
}

void UNULocationRestoreSubsystem::BeginCharacterRestore(int64 CharacterIdx)
{
	// GameInstance 서브시스템이므로 이전 캐릭터 또는 재접속 시도의 상태가 남지 않게 초기화한다.
	Snapshot = {};
	bRecoveredLocation = false;
	ActiveCharacterIdx = CharacterIdx;
	InteriorReadyDeadline = 0.0;
	ApplyingCharacter.Reset();
	FieldEntryRetryDelegate.Unbind();
	State = ERestoreState::WaitingRestore;

	UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();

	if (CharacterIdx <= 0 || Server == nullptr || !Server->IsLoggedIn())
	{
		FailServerRestore(0, TEXT("LocationRestore 요청을 보낼 서버 세션이 없습니다."));
		return;
	}

	UE_LOG(LogNULocationRestore, Log, TEXT("LocationRestore 요청 CharacterIdx=%lld"), CharacterIdx);
	Server->SendLocationRestore(TEXT(""));
}

void UNULocationRestoreSubsystem::HandleLocationRestoreResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationRestore* Response)
{
	if (State != ERestoreState::WaitingRestore)
	{
		return;
	}

	FRestoreSnapshot Restored;

	if (ResultCode != SuccessResultCode || Response == nullptr || !ParseRestoreSnapshot(*Response, Restored))
	{
		FailServerRestore(ResultCode, ResultMessage.IsEmpty()
			? TEXT("LocationRestore 응답을 검증할 수 없습니다.") : ResultMessage);
		return;
	}

	Snapshot = MoveTemp(Restored);

	// 서버에 미완료 포탈 전환이 남아 있으면 현재 클라이언트에서 이어갈 수 없으므로 취소로 닫는다.
	if (Snapshot.TransitionState == 1)
	{
		if (Snapshot.TransitionId.IsEmpty())
		{
			FailServerRestore(ResultCode, TEXT("진행 중인 공간 이동에 transition_id가 없습니다."));
			return;
		}

		UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();

		if (Server == nullptr || !Server->IsLoggedIn())
		{
			FailServerRestore(0, TEXT("진행 중인 공간 이동을 취소할 서버 세션이 없습니다."));
			return;
		}

		State = ERestoreState::WaitingTransitionCancel;
		UE_LOG(LogNULocationRestore, Log,
			TEXT("LocationRestore가 진행 중 전환을 반환했습니다. 취소 보고 transition=%s"),
			*Snapshot.TransitionId);
		Server->SendPortalArrive(Snapshot.TransitionId, false,
			static_cast<int32>(ENUWorldTransitionFailure::Cancelled), Snapshot.Location, Snapshot.Facing);
		return;
	}

	CompleteServerRestore();
}

bool UNULocationRestoreSubsystem::HandlePortalArriveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGamePortalArrive* Response)
{
	if (State != ERestoreState::WaitingTransitionCancel)
	{
		return false;
	}

	const bool bValidResponse = Response != nullptr
		&& (ResultCode == SuccessResultCode || ResultCode == PortalCancelledResultCode)
		&& Response->TransitionId == Snapshot.TransitionId
		&& Response->_transition_state.TransitionState == 3
		&& Response->_area_id.AreaId > 0
		&& Response->_location_id.LocationId > 0
		&& Response->Facing >= static_cast<int32>(EFieldFacing::Down)
		&& Response->Facing <= static_cast<int32>(EFieldFacing::Right);

	if (!bValidResponse)
	{
		FailServerRestore(ResultCode, ResultMessage.IsEmpty()
			? TEXT("진행 중인 공간 이동의 취소 결과를 검증할 수 없습니다.") : ResultMessage);
		return true;
	}

	// 취소 응답이 돌려준 값이 전환 전 마지막 확정 위치이므로 최초 Restore 응답을 갱신한다.
	Snapshot.AreaId = Response->_area_id.AreaId;
	Snapshot.LocationId = Response->_location_id.LocationId;
	Snapshot.Location = FVector(Response->PosX, Response->PosY, Response->PosZ);
	Snapshot.Facing = Response->Facing;
	Snapshot.bIndoor = Response->_is_indoor.IsIndoor;
	Snapshot.bHasSavedLocation = Snapshot.Location != FVector::ZeroVector;
	Snapshot.TransitionState = 3;

	CompleteServerRestore();
	return true;
}

bool UNULocationRestoreSubsystem::PrepareFieldEntry(AFieldPlayerCharacter* Character, const FSimpleDelegate& RetryDelegate)
{
	if (State == ERestoreState::None)
	{
		return true;
	}

	if (State == ERestoreState::Applied)
	{
		return true;
	}

	if (State != ERestoreState::Ready || !IsValid(Character))
	{
		return false;
	}

	// 비동기 레벨 준비가 끝난 뒤 같은 PlayerController가 FieldEnter를 다시 시도하도록 보관한다.
	ApplyingCharacter = Character;
	FieldEntryRetryDelegate = RetryDelegate;
	UWorld* World = Character->GetWorld();

	// 컨트롤러가 먼저 시작된 경우 참가 컴포넌트 초기화를 기다린다.
	const UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
	if (!Character->HasActorBegunPlay() || (Participant != nullptr && !Participant->HasBegunPlay()))
	{
		UE_LOG(LogNULocationRestore, Verbose, TEXT("위치 복원 초기화 대기 character=%s actor_ready=%d participant_ready=%d"),
			*GetNameSafe(Character), Character->HasActorBegunPlay(), Participant == nullptr || Participant->HasBegunPlay());
		QueueFieldEntryRetry();
		return false;
	}

	const int32 CurrentAreaId = GetDefault<UNUWorldDataSettings>()->GetAreaIdForWorld(World);
	ReconcileRespawnLocation();
	FNUWorldDestination Destination;
	int32 EntryLocationId = 0;
	const bool bHasDestination = ResolveRestoreDestination(Destination, &EntryLocationId);
	// 다른 지역이거나 스트리밍 레벨에 속한 장소라면 좌표 적용 전에 목적 공간을 준비한다.
	const bool bNeedsSpacePreparation = Snapshot.AreaId != CurrentAreaId
		|| (bHasDestination && !Destination.IsPersistentLevelDestination());

	if (bNeedsSpacePreparation)
	{
		if (!bHasDestination)
		{
			UE_LOG(LogNULocationRestore, Error, TEXT("복원 목적지 바인딩 없음 area=%d location=%d"), Snapshot.AreaId, Snapshot.LocationId);
			HandleWorldRestoreFailed();
			return false;
		}

		UNUWorldTransitionSubsystem* Transition = World->GetSubsystem<UNUWorldTransitionSubsystem>();
		State = ERestoreState::Applying;
		FNUWorldRestoreTransitionRequest RestoreRequest;
		RestoreRequest.Destination = Destination;
		RestoreRequest.Arrival = Snapshot.Location;
		RestoreRequest.Facing = Snapshot.Facing;
		RestoreRequest.bUseSavedArrival = Snapshot.bHasSavedLocation;
		RestoreRequest.LocationId = Snapshot.LocationId;
		RestoreRequest.FallbackLocationId = EntryLocationId;
		RestoreRequest.bIndoor = Snapshot.bIndoor;
		for (const FNUWorldPortalEntryBinding& Binding : GetGameInstance()->GetSubsystem<UNUWorldPortalDataSubsystem>()->GetEntryBindings())
		{
			if (Binding.LocationId == Snapshot.LocationId && Binding.AreaId == Snapshot.AreaId)
			{
				RestoreRequest.ReturnDestination = Binding.ReturnDestination;
				RestoreRequest.ReturnDestination.ResolvedAreaId = Snapshot.AreaId;
				RestoreRequest.ReturnLocationId = Binding.ReturnLocationId;
				break;
			}
		}
		RestoreRequest.OnSucceeded = FSimpleDelegate::CreateUObject(this, &ThisClass::HandleWorldRestoreSucceeded);
		RestoreRequest.OnFailed = FSimpleDelegate::CreateUObject(this, &ThisClass::HandleWorldRestoreFailed);
		UE_LOG(LogNULocationRestore, Log, TEXT("복원 공간 준비 시작 actor=%s area=%d location=%d level=%s entry=%s"),
			*GetNameSafe(Character), Snapshot.AreaId, Snapshot.LocationId, *Destination.Level.ToSoftObjectPath().ToString(), *Destination.EntryPointId.ToString());

		if (Transition == nullptr || !Transition->RequestRestoreTransition(Character, RestoreRequest))
		{
			if (State == ERestoreState::Applying)
			{
				HandleWorldRestoreFailed();
			}
		}

		return false;
	}

	// 건물 충돌이 준비되기 전에 저장 좌표를 적용하거나 FieldEnter를 보내지 않는다.
	const UNUFieldLoadingSubsystem* Loading = GetGameInstance()->GetSubsystem<UNUFieldLoadingSubsystem>();
	if (Loading != nullptr)
	{
		const FNUFieldLevelReadiness Levels = Loading->InspectLevelInstances(World);
		if (!Levels.FailedInstance.IsEmpty())
		{
			HandleWorldRestoreFailed();
			return false;
		}
		if (!Levels.IsReady())
		{
			QueueFieldEntryRetry();
			return false;
		}
	}

	// 레벨 표시 후에도 실내 참가 컴포넌트가 등록될 때까지 기다린다.
	if (Snapshot.bIndoor && FindInterior(Snapshot.LocationId) == nullptr)
	{
		const double CurrentTime = World->GetTimeSeconds();

		if (InteriorReadyDeadline <= 0.0)
		{
			InteriorReadyDeadline = CurrentTime + InteriorReadyTimeoutSeconds;
			UE_LOG(LogNULocationRestore, Log, TEXT("실내 Level Instance 준비 대기 location=%d"), Snapshot.LocationId);
		}

		if (CurrentTime < InteriorReadyDeadline)
		{
			QueueFieldEntryRetry();
			return false;
		}

		HandleWorldRestoreFailed();
		return false;
	}

	InteriorReadyDeadline = 0.0;

	if (!ApplySnapshot(Character))
	{
		HandleWorldRestoreFailed();
		return false;
	}

	State = ERestoreState::Applied;
	ApplyingCharacter.Reset();
	FieldEntryRetryDelegate.Unbind();
	return true;
}

bool UNULocationRestoreSubsystem::GetFieldEnterLocationId(int32& OutLocationId) const
{
	if (State != ERestoreState::Applied || Snapshot.LocationId <= 0)
	{
		return false;
	}

	OutLocationId = Snapshot.LocationId;
	return true;
}

void UNULocationRestoreSubsystem::ForgetAppliedLocationAfterRespawn()
{
	if (State != ERestoreState::Applied) { return; }
	Snapshot = {};
	bRecoveredLocation = false;
	State = ERestoreState::None;
}

bool UNULocationRestoreSubsystem::IsRecoveryConfirmationValid(int32 AreaId, int32 LocationId, bool bIndoor) const
{
	return !bRecoveredLocation || State != ERestoreState::Applied
		|| (AreaId == Snapshot.AreaId && LocationId == Snapshot.LocationId && bIndoor == Snapshot.bIndoor);
}

bool UNULocationRestoreSubsystem::ParseRestoreSnapshot(const FGrpcRevGameLocationRestore& Response, FRestoreSnapshot& OutSnapshot) const
{
	const uint64 CharacterIdx = Response._character_idx.CharacterIdx;
	const FVector Location(Response._pos_x.PosX, Response._pos_y.PosY, Response._pos_z.PosZ);
	const int32 Facing = Response._facing.Facing;
	const int32 TransitionState = Response._transition_state.TransitionState;

	// 다른 캐릭터의 응답이나 클라이언트가 적용할 수 없는 공간, 방향 값은 필드 진입 전에 차단한다.
	if (CharacterIdx == 0 || CharacterIdx > static_cast<uint64>(MAX_int64)
		|| static_cast<int64>(CharacterIdx) != ActiveCharacterIdx
		|| Response._area_id.AreaId <= 0
		|| Response._location_id.LocationId <= 0
		|| Location.ContainsNaN()
		|| Facing < static_cast<int32>(EFieldFacing::Down)
		|| Facing > static_cast<int32>(EFieldFacing::Right)
		|| (TransitionState != 0 && TransitionState != 1 && TransitionState != 3))
	{
		return false;
	}

	OutSnapshot.CharacterIdx = static_cast<int64>(CharacterIdx);
	OutSnapshot.AreaId = Response._area_id.AreaId;
	OutSnapshot.LocationId = Response._location_id.LocationId;
	OutSnapshot.Location = Location;
	OutSnapshot.Facing = Facing;
	OutSnapshot.bIndoor = Response._is_indoor.IsIndoor;
	OutSnapshot.bHasSavedLocation = Location != FVector::ZeroVector;
	OutSnapshot.TransitionId = Response.TransitionId;
	OutSnapshot.TransitionState = TransitionState;
	return true;
}

bool UNULocationRestoreSubsystem::ApplySnapshot(AFieldPlayerCharacter* Character)
{
	if (!IsValid(Character))
	{
		return false;
	}

	Character->Facing = static_cast<EFieldFacing>(Snapshot.Facing);

	UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
	UNUWalkableInteriorComponent* RestoredInterior = nullptr;

	if (Snapshot.bIndoor)
	{
		RestoredInterior = FindInterior(Snapshot.LocationId);

		if (Participant == nullptr || RestoredInterior == nullptr)
		{
			return false;
		}
	}

	const FVector Candidate = Snapshot.bHasSavedLocation ? Snapshot.Location : Character->GetActorLocation();
	if (!NUWorldRestoreSafety::IsSafe(Character, RestoredInterior, Candidate))
	{
		FVector SafeArrival;
		int32 EntryLocationId = Snapshot.LocationId;
		if (RestoredInterior)
		{
			if (!NUWorldRestoreSafety::FindInteriorArrival(Character, RestoredInterior, SafeArrival))
			{
				UE_LOG(LogNULocationRestore, Error, TEXT("실내 복원 도착점이 안전하지 않습니다. location=%d saved=%s"), Snapshot.LocationId, *Candidate.ToCompactString());
				return false;
			}
		}
		else
		{
			FNUWorldDestination Destination;
			if (!ResolveRestoreDestination(Destination, &EntryLocationId)) { return false; }
			const UNUWorldTransitionSubsystem* Transition = Character->GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>();
			ULevel* Level = Transition ? Transition->GetActiveLevel(Character) : Character->GetWorld()->PersistentLevel.Get();
			if (!Level || (!Destination.Level.IsNull() && (!Transition || Destination.Level.ToSoftObjectPath().GetLongPackageName() != Transition->GetActiveMapPackageName(Character)))) { return false; }
			ANUWorldEntryPoint* Entry = nullptr;
			for (AActor* Actor : Level->Actors)
			{
				ANUWorldEntryPoint* Found = Cast<ANUWorldEntryPoint>(Actor);
				if (IsValid(Found) && Found->GetEntryPointId() == Destination.EntryPointId)
				{
					if (Entry != nullptr) { return false; }
					Entry = Found;
				}
			}
			if (!NUWorldRestoreSafety::FindEntryArrival(Character, RestoredInterior, Entry, SafeArrival))
			{
				UE_LOG(LogNULocationRestore, Error, TEXT("안전한 복원 진입점이 없습니다. entry=%s saved=%s"), *Destination.EntryPointId.ToString(), *Candidate.ToCompactString());
				return false;
			}
		}
		UE_LOG(LogNULocationRestore, Warning, TEXT("잘못된 복원 좌표를 진입점으로 복구합니다. old_location=%d new_location=%d old=%s new=%s; FieldEnter로 동기화"),
			Snapshot.LocationId, EntryLocationId, *Candidate.ToCompactString(), *SafeArrival.ToCompactString());
		bRecoveredLocation = true;
		Snapshot.Location = SafeArrival;
		Snapshot.LocationId = EntryLocationId;
		Snapshot.bHasSavedLocation = true;
	}
	const FVector* SavedLocation = Snapshot.bHasSavedLocation ? &Snapshot.Location : nullptr;

	// 저장 좌표에서 목적 크기로 충돌을 검사한 뒤 위치와 실내외 상태를 한 번에 적용한다.
	if (Participant != nullptr)
	{
		if (!Participant->ApplyRestoredState(RestoredInterior, SavedLocation))
		{
			return false;
		}
	}
	else if (Snapshot.bHasSavedLocation
		&& !Character->TeleportTo(Snapshot.Location, Character->GetActorRotation(), false, true))
	{
		return false;
	}

	APlayerState* PlayerState = Character->GetPlayerState();
	UNUWorldLocationComponent* WorldLocation = PlayerState != nullptr
		? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;

	if (WorldLocation != nullptr)
	{
		WorldLocation->RefreshLocation();
	}

	UE_LOG(LogNULocationRestore, Log,
		TEXT("복원 위치 적용 area=%d location=%d indoor=%d pos=%s facing=%d"),
		Snapshot.AreaId, Snapshot.LocationId, Snapshot.bIndoor ? 1 : 0,
		*Character->GetActorLocation().ToCompactString(), Snapshot.Facing);
	return true;
}

void UNULocationRestoreSubsystem::ReconcileRespawnLocation()
{
	if (!Snapshot.bHasSavedLocation || Snapshot.bIndoor || Snapshot.Location.ContainsNaN()) { return; }
	const UNURecoveryNetSubsystem* Recovery = GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>();
	if (Recovery == nullptr || !Recovery->HasState() || Recovery->GetState().RespawnState != 2 || Recovery->GetState().DeathId <= 0
		|| Recovery->GetState().EffectivePointId <= 0 || Recovery->GetState().EffectivePointId > MAX_int32) { return; }
	FNURecoveryPointDefinition Point;
	if (!GetDefault<UNURecoverySettings>()->TryGetRecoveryPoint(static_cast<int32>(Recovery->GetState().EffectivePointId), Point)) { return; }
	UWorld* World = GetGameInstance()->GetWorld();
	if (World == nullptr || !Point.LevelTransform.Equals(FTransform::Identity)
		|| Point.Map.ToSoftObjectPath().GetLongPackageName() != UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) { return; }
	const int32 RecoveryAreaId = GetDefault<UNUWorldDataSettings>()->GetAreaIdForWorld(World);
	if (RecoveryAreaId <= 0 || RecoveryAreaId == Snapshot.AreaId) { return; }
	const UNUWorldDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUWorldDataSubsystem>();
	const FNULocationDefinition* Selected = nullptr;
	int32 SelectedPriority = MIN_int32;
	for (TActorIterator<ANULocationTrigger> It(World); It; ++It)
	{
		const ANULocationTrigger* Trigger = *It;
		if (!IsValid(Trigger)) { continue; }
		const UBoxComponent* Bounds = Trigger->GetTriggerBounds();
		if (Trigger->GetLevel() != World->PersistentLevel || !IsValid(Bounds)) { continue; }
		const FVector Extent = Bounds->GetUnscaledBoxExtent();
		if (!FBox(-Extent, Extent).IsInsideOrOn(Bounds->GetComponentTransform().InverseTransformPosition(Snapshot.Location))) { continue; }
		int32 LocationId = 0;
		const FString Name = Trigger->GetLocationId().ToString();
		if (!LexTryParseString(LocationId, *Name) || LocationId <= 0 || Name != LexToString(LocationId)) { continue; }
		const FNULocationDefinition* Candidate = Data != nullptr ? Data->FindLocationDefinition(LocationId) : nullptr;
		if (Candidate == nullptr || Candidate->AreaId != RecoveryAreaId || Candidate->bIndoor) { continue; }
		if (Selected == nullptr || Trigger->GetLocationPriority() > SelectedPriority
			|| (Trigger->GetLocationPriority() == SelectedPriority && LocationId < Selected->LocationId))
		{
			Selected = Candidate;
			SelectedPriority = Trigger->GetLocationPriority();
		}
	}
	if (Selected == nullptr)
	{
		UE_LOG(LogNULocationRestore, Warning, TEXT("부활 뒤 서버 장소와 거점 맵이 다르지만 저장 좌표의 장소 트리거를 찾지 못했습니다. area=%d location=%d pos=%s"),
			Snapshot.AreaId, Snapshot.LocationId, *Snapshot.Location.ToCompactString());
		return;
	}
	UE_LOG(LogNULocationRestore, Warning, TEXT("사망 후 저장 좌표와 서버 장소 불일치 복구 area=%d location=%d -> area=%d location=%d pos=%s"),
		Snapshot.AreaId, Snapshot.LocationId, RecoveryAreaId, Selected->LocationId, *Snapshot.Location.ToCompactString());
	Snapshot.AreaId = RecoveryAreaId;
	Snapshot.LocationId = Selected->LocationId;
	bRecoveredLocation = true;
}

bool UNULocationRestoreSubsystem::ResolveRestoreDestination(FNUWorldDestination& OutDestination, int32* OutEntryLocationId) const
{
	const UNUWorldPortalDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUWorldPortalDataSubsystem>();
	const FNUWorldPortalEntryBinding* AreaFallback = nullptr;

	for (const FNUWorldPortalEntryBinding& Binding : Data->GetEntryBindings())
	{
		if (Binding.AreaId != Snapshot.AreaId)
		{
			continue;
		}

		if (AreaFallback == nullptr)
		{
			AreaFallback = &Binding;
		}

		if (Binding.LocationId == Snapshot.LocationId)
		{
			if (OutEntryLocationId) { *OutEntryLocationId = Binding.LocationId; }
			OutDestination = Binding.Destination;
			OutDestination.ResolvedAreaId = Snapshot.AreaId;
			return OutDestination.IsConfigured();
		}
	}

	// 장소별 바인딩이 아직 없는 지역은 같은 지역의 대표 진입점으로 레벨만 준비한다.
	if (AreaFallback != nullptr && !Snapshot.bIndoor)
	{
		if (OutEntryLocationId) { *OutEntryLocationId = AreaFallback->LocationId; }
		OutDestination = AreaFallback->Destination;
		OutDestination.ResolvedAreaId = Snapshot.AreaId;
		return OutDestination.IsConfigured();
	}

	return false;
}

UNUWalkableInteriorComponent* UNULocationRestoreSubsystem::FindInterior(int32 LocationId) const
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;

	for (TObjectIterator<UNUWalkableInteriorComponent> It; It; ++It)
	{
		UNUWalkableInteriorComponent* Interior = *It;

		if (IsValid(Interior) && Interior->GetWorld() == World && Interior->LocationId == LocationId)
		{
			return Interior;
		}
	}

	return nullptr;
}

void UNULocationRestoreSubsystem::CompleteServerRestore()
{
	// 캐릭터 선택 완료 이벤트를 여기서 허용해야 필드 레벨이 Restore 응답보다 먼저 열리지 않는다.
	State = ERestoreState::Ready;

	UE_LOG(LogNULocationRestore, Log,
		TEXT("LocationRestore 확정 character=%lld area=%d location=%d indoor=%d transition_state=%d pos=%s"),
		Snapshot.CharacterIdx, Snapshot.AreaId, Snapshot.LocationId, Snapshot.bIndoor ? 1 : 0,
		Snapshot.TransitionState, *Snapshot.Location.ToCompactString());

	if (UNUCharacterManager* Characters = GetGameInstance()->GetSubsystem<UNUCharacterManager>())
	{
		Characters->HandleLocationRestoreCompleted(true, SuccessResultCode, TEXT(""));
	}
}

void UNULocationRestoreSubsystem::FailServerRestore(int32 ResultCode, const FString& ResultMessage)
{
	State = ERestoreState::Failed;
	UE_LOG(LogNULocationRestore, Warning, TEXT("LocationRestore 실패 (%d): %s"), ResultCode, *ResultMessage);

	if (UNUCharacterManager* Characters = GetGameInstance()->GetSubsystem<UNUCharacterManager>())
	{
		Characters->HandleLocationRestoreCompleted(false, ResultCode, ResultMessage);
	}
}

void UNULocationRestoreSubsystem::HandleWorldRestoreSucceeded()
{
	if (State != ERestoreState::Applying)
	{
		return;
	}
	AFieldPlayerCharacter* Character = ApplyingCharacter.Get();

	if (!IsValid(Character))
	{
		UE_LOG(LogNULocationRestore, Error, TEXT("공간 전환 완료 후 복원 대상 캐릭터 참조가 없습니다."));
		HandleWorldRestoreFailed();
		return;
	}
	// 전환에서 검증한 결과를 보관한다. 원래의 잘못된 저장 좌표를 다시 적용하지 않는다.
	bRecoveredLocation = Snapshot.bHasSavedLocation && !Snapshot.Location.Equals(Character->GetActorLocation(), 1.0f);
	Snapshot.Location = Character->GetActorLocation();
	Snapshot.bHasSavedLocation = true;
	if (const UNUWorldTransitionSubsystem* Transition = Character->GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>())
	{
		Snapshot.LocationId = Transition->GetLastRestoreLocationId();
	}
	// 공간 로딩이 끝난 뒤 실내외 참가 상태와 최종 위치 판정을 한 번 더 확정한다.
	if (!ApplySnapshot(Character))
	{
		HandleWorldRestoreFailed();
		return;
	}

	State = ERestoreState::Applied;
	QueueFieldEntryRetry();
}

void UNULocationRestoreSubsystem::HandleWorldRestoreFailed()
{
	State = ERestoreState::Failed;
	InteriorReadyDeadline = 0.0;
	ApplyingCharacter.Reset();
	UE_LOG(LogNULocationRestore, Error,
		TEXT("복원 공간을 준비하거나 서버 확정 위치를 적용하지 못했습니다. area=%d location=%d"),
		Snapshot.AreaId, Snapshot.LocationId);
	FieldEntryRetryDelegate.Unbind();
}

void UNULocationRestoreSubsystem::CancelFieldEntry()
{
	const bool bWasApplying = State == ERestoreState::Applying;
	State = ERestoreState::Failed;
	InteriorReadyDeadline = 0.0;
	FieldEntryRetryDelegate.Unbind();
	ApplyingCharacter.Reset();
	// 실패 화면 뒤에서 복원 완료 콜백이 FieldEnter를 다시 보내지 못하게 한다.
	if (bWasApplying && GetWorld() != nullptr)
	{
		if (UNUWorldTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>())
		{
			Transition->CancelTransition();
		}
	}
}

void UNULocationRestoreSubsystem::ResetFieldEntry()
{
	CancelFieldEntry();
	Snapshot = {};
	bRecoveredLocation = false;
	ActiveCharacterIdx = 0;
	State = ERestoreState::None;
}

void UNULocationRestoreSubsystem::QueueFieldEntryRetry()
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;

	// 전환 완료 콜백 안에서 바로 FieldEnter를 보내지 않고 다음 틱에 월드 상태가 안정된 뒤 재시도한다.
	if (World != nullptr && FieldEntryRetryDelegate.IsBound())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ExecuteFieldEntryRetry);
	}
}

void UNULocationRestoreSubsystem::ExecuteFieldEntryRetry()
{
	// BeginPlay와 Possession이 각각 예약한 재시도가 진행 중인 복원 참조를 지우지 않게 한다.
	if (State != ERestoreState::Ready && State != ERestoreState::Applied)
	{
		return;
	}
	FSimpleDelegate RetryDelegate = FieldEntryRetryDelegate;
	FieldEntryRetryDelegate.Unbind();
	ApplyingCharacter.Reset();

	RetryDelegate.ExecuteIfBound();
}
