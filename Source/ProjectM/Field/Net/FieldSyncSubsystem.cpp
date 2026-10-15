#include "Field/Net/FieldSyncSubsystem.h"

#include "Net/EmberServerManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NULocationDefinition.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"
#include "WorldTransition/Net/NULocationRestoreSubsystem.h"
#include "SRevGame/GameMessage.h"

DEFINE_LOG_CATEGORY_STATIC(LogFieldSync, Log, All);

namespace
{
	// 이동 중 안전망 주기. **정지 중에는 아무것도 보내지 않는다.**
	// 없으면 직선으로 오래 걸을 때 남들 화면에서 출발점에 서 있다가 순간이동한다.
	constexpr float HEARTBEAT_SEC = 0.2f;

	// 이동 중으로 보는 속도 임계 (로컬 캐릭터 UpdateAnimation과 같은 기준).
	constexpr float MOVING_SPEED_SQ = 25.0f;
	
	// 이만큼 거리가 벌어지면 보고함
	constexpr float POS_TOLERANCE_CM = 20.0f;

	// 같은 지역이거나 한쪽이 다른 쪽의 상위 지역이면 포탈 없이 걸어서 오가는 이동으로 본다.
	// 마을 안에 놓인 조우지(상위 지역 = 마을)가 여기에 해당한다.
	bool IsWalkableAreaChange(const UNUWorldDataSubsystem& Data, int32 FromAreaId, int32 ToAreaId)
	{
		if (FromAreaId == ToAreaId)
		{
			return true;
		}

		const FNUAreaDefinition* From = Data.FindAreaDefinition(FromAreaId);
		const FNUAreaDefinition* To = Data.FindAreaDefinition(ToAreaId);
		return (To != nullptr && To->ParentAreaId == FromAreaId)
			|| (From != nullptr && From->ParentAreaId == ToAreaId);
	}

	const UNUWorldDataSubsystem* GetWorldData(const UWorld* World)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		return GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldDataSubsystem>() : nullptr;
	}
}

// ── 서브시스템 ────────────────────────────────────────────────────────────

void UFieldSyncSubsystem::Deinitialize()
{
	UnbindWorldLocation();
	ResetOutdoorLocationMove();
	DesiredOutdoorLocationId = 0;
	bOutdoorMismatchRetried = false;
	ConfirmedAreaId = 0;
	ConfirmedLocationId = 0;
	bConfirmedIndoor = false;

	// 필드 월드가 내려가는 모든 경로(캐릭터 선택으로 이동 등)에서 서버에 퇴장을 알린다.
	// 서버는 필드에 있는 동안 캐릭터 재선택을 411로 거부하므로, 이걸 안 보내면 다시 들어올 수 없다.
	// LeaveField와 달리 원격 액터는 Destroy하지 않는다 --- 월드 해체 중이라 함께 사라진다
	if (bInField)
	{
		bInField = false;
		if (UEmberServerManager* Server = GetServer())
		{
			Server->SendFieldLeave();
		}
	}
	Remotes.Empty(); // 액터는 월드와 함께 사라진다
	PositionReportBlockers.Reset();
	Super::Deinitialize();
}

TStatId UFieldSyncSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFieldSyncSubsystem, STATGROUP_Tickables);
}

bool UFieldSyncSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFieldSyncSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasConfirmedFieldEntry())
	{
		TickReportMyPosition(DeltaSeconds);
	}
}

// ── 게임 코드용 API ───────────────────────────────────────────────────────

void UFieldSyncSubsystem::EnterField()
{
	EnterFieldInternal(0);
}

bool UFieldSyncSubsystem::EnterFieldAtLocation(int32 LocationId)
{
	return LocationId > 0 && EnterFieldInternal(LocationId);
}

bool UFieldSyncSubsystem::EnterFieldInternal(int32 LocationIdOverride)
{
	UEmberServerManager* Server = GetServer();
	AFieldPlayerCharacter* Me = GetMyCharacter();
	if (Server == nullptr || Me == nullptr || (LocationIdOverride > 0 && !Server->IsLoggedIn()))
	{
		UE_LOG(LogFieldSync, Warning, TEXT("EnterField 실패 - %s"),
			Server == nullptr ? TEXT("서버 매니저 없음") : Me == nullptr ? TEXT("내 캐릭터 없음") : TEXT("서버 연결 없음"));
		return false;
	}

	const APlayerState* PlayerState = Me->GetPlayerState();
	const UNUWorldLocationComponent* WorldLocation = IsValid(PlayerState)
		? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
	int32 LocationId = LocationIdOverride > 0 ? LocationIdOverride : (WorldLocation != nullptr ? WorldLocation->GetCurrentLocationId() : 0);

	// 로그인 복원에서만 저장 장소를 우선한다. 부활은 현재 침대 위치의 장소를 명시한다.
	if (LocationIdOverride <= 0)
	{
		if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
		{
			if (const UNULocationRestoreSubsystem* LocationRestore = GameInstance->GetSubsystem<UNULocationRestoreSubsystem>())
			{
				LocationRestore->GetFieldEnterLocationId(LocationId);
			}
		}
	}

	bFieldEntryConfirmed = false;
	bFieldEntryFailed = false;
	Server->SendFieldEnter(Me->GetActorLocation(), static_cast<int32>(Me->Facing), LocationId);

	BindWorldLocation();
	ResetOutdoorLocationMove();
	DesiredOutdoorLocationId = 0;
	bOutdoorMismatchRetried = false;
	ConfirmedAreaId = 0;
	ConfirmedLocationId = 0;
	bConfirmedIndoor = false;
	bInField = true;
	bWasMoving = false;
	SentFacing = Me->Facing;
	SentLocation = Me->GetActorLocation();
	TimeSinceSend = 0.0f;

	// FieldEnter는 강제 재동기화라 서버가 상태를 리셋한다.
	// 값이 안 바뀌었어도 정령을 다시 알려야 한다 (SetSpirit은 바뀔 때만 보내므로 직접 호출함)
	MySpiritId = GetMySpiritId();
	Server->SendFieldSpirit(MySpiritId);

	UE_LOG(LogFieldSync, Log, TEXT("필드 입장 (정령 %d)"), MySpiritId);
	return true;
}

void UFieldSyncSubsystem::HandleEnterResponse(int32 ResultCode, const FString& ResultMessage, int32 AreaId, int32 LocationId, bool bIndoor)
{
	// 실패 후 퇴장한 월드에 늦게 도착한 응답으로 입장을 다시 확정하지 않는다.
	if (!bInField)
	{
		return;
	}
	bFieldEntryConfirmed = false;
	if (ResultCode != 200)
	{
		bFieldEntryFailed = true;
		bInField = false;
		UnbindWorldLocation();
		ResetOutdoorLocationMove();
		UE_LOG(LogFieldSync, Warning, TEXT("FieldEnter 거절 (%d): %s"), ResultCode, *ResultMessage);
		return;
	}
	if (AreaId <= 0 || LocationId <= 0)
	{
		bFieldEntryFailed = true;
		bInField = false;
		UnbindWorldLocation();
		ResetOutdoorLocationMove();
		UE_LOG(LogFieldSync, Warning, TEXT("FieldEnter 성공 응답에 확정 공간이 없습니다. area=%d location=%d"),
			AreaId, LocationId);
		return;
	}

	const UNULocationRestoreSubsystem* Restore = GetWorld()->GetGameInstance()->GetSubsystem<UNULocationRestoreSubsystem>();
	if (Restore && !Restore->IsRecoveryConfirmationValid(AreaId, LocationId, bIndoor))
	{
		bFieldEntryFailed = true;
		bInField = false;
		UnbindWorldLocation();
		ResetOutdoorLocationMove();
		UE_LOG(LogFieldSync, Error, TEXT("FieldEnter가 복구한 공간과 다른 공간을 확정했습니다. area=%d location=%d indoor=%d"), AreaId, LocationId, bIndoor);
		return;
	}

	ApplyConfirmedSpace(AreaId, LocationId, bIndoor);
	bFieldEntryConfirmed = true;
	UE_LOG(LogFieldSync, Log, TEXT("필드 공간 확정 area=%d location=%d indoor=%d"),
		ConfirmedAreaId, ConfirmedLocationId, bConfirmedIndoor ? 1 : 0);
	EvaluateOutdoorLocation();
}

void UFieldSyncSubsystem::LeaveField()
{
	bFieldEntryConfirmed = false;
	if (bInField == false)
	{
		return;
	}
	bInField = false;
	UnbindWorldLocation();
	ResetOutdoorLocationMove();
	DesiredOutdoorLocationId = 0;
	bOutdoorMismatchRetried = false;
	ConfirmedAreaId = 0;
	ConfirmedLocationId = 0;
	bConfirmedIndoor = false;

	if (UEmberServerManager* Server = GetServer())
	{
		Server->SendFieldLeave();
	}

	for (const TPair<int64, TObjectPtr<AFieldRemoteCharacter>>& Pair : Remotes)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->Destroy();
		}
		OnRemotePlayerDespawned.Broadcast(Pair.Key);
	}
	Remotes.Empty();

	UE_LOG(LogFieldSync, Log, TEXT("필드 퇴장"));
}

bool UFieldSyncSubsystem::GetConfirmedSpace(int32& OutAreaId, int32& OutLocationId, bool& bOutIndoor) const
{
	if (ConfirmedAreaId <= 0 || ConfirmedLocationId <= 0)
	{
		return false;
	}

	OutAreaId = ConfirmedAreaId;
	OutLocationId = ConfirmedLocationId;
	bOutIndoor = bConfirmedIndoor;
	return true;
}

void UFieldSyncSubsystem::ApplyConfirmedSpace(int32 AreaId, int32 LocationId, bool bIndoor)
{
	if (AreaId <= 0 || LocationId <= 0)
	{
		return;
	}

	ConfirmedAreaId = AreaId;
	ConfirmedLocationId = LocationId;
	bConfirmedIndoor = bIndoor;
}

bool UFieldSyncSubsystem::IsLocationMoveReady(int32 LocalLocationId) const
{
	return !bInField || (ConfirmedLocationId > 0 && !bOutdoorLocationMovePending
		&& (LocalLocationId <= 0 || LocalLocationId == ConfirmedLocationId));
}

void UFieldSyncSubsystem::EvaluateOutdoorLocation()
{
	HandleLocalLocationChanged();
}

void UFieldSyncSubsystem::BindWorldLocation()
{
	AFieldPlayerCharacter* Character = GetMyCharacter();
	APlayerState* PlayerState = Character != nullptr ? Character->GetPlayerState() : nullptr;
	UNUWorldLocationComponent* Location = PlayerState != nullptr
		? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;

	if (BoundWorldLocation.Get() == Location)
	{
		return;
	}

	UnbindWorldLocation();
	BoundWorldLocation = Location;

	if (Location != nullptr)
	{
		Location->OnLocationChanged.AddUniqueDynamic(this, &ThisClass::HandleLocalLocationChanged);
	}
}

void UFieldSyncSubsystem::UnbindWorldLocation()
{
	if (BoundWorldLocation.IsValid())
	{
		BoundWorldLocation->OnLocationChanged.RemoveDynamic(this, &ThisClass::HandleLocalLocationChanged);
	}

	BoundWorldLocation.Reset();
}

void UFieldSyncSubsystem::HandleLocalLocationChanged()
{
	if (!bInField || ConfirmedAreaId <= 0 || ConfirmedLocationId <= 0 || bConfirmedIndoor
		|| IsPositionReportingSuspended() || !BoundWorldLocation.IsValid())
	{
		return;
	}

	const int32 LocalLocationId = BoundWorldLocation->GetCurrentLocationId();

	if (LocalLocationId <= 0 || LocalLocationId == ConfirmedLocationId)
	{
		DesiredOutdoorLocationId = 0;
		bOutdoorMismatchRetried = false;
		return;
	}

	const UNUWorldDataSubsystem* Data = GetWorldData(GetWorld());
	const FNULocationDefinition* Target = Data != nullptr ? Data->FindLocationDefinition(LocalLocationId) : nullptr;

	// 지역 포탈과 실내 출입은 각 전환 경로가 확정한다. 여기서는 걸어서 오가는 야외 구역만 동기화한다.
	// 마을 안 조우지처럼 같은 레벨에 놓인 하위 지역은 포탈이 없으므로 여기서 보낸다.
	if (Target == nullptr || Target->bIndoor || !IsWalkableAreaChange(*Data, ConfirmedAreaId, Target->AreaId))
	{
		return;
	}

	if (DesiredOutdoorLocationId != LocalLocationId)
	{
		DesiredOutdoorLocationId = LocalLocationId;
		bOutdoorMismatchRetried = false;
	}

	TrySendOutdoorLocationMove();
}

void UFieldSyncSubsystem::TrySendOutdoorLocationMove()
{
	if (bOutdoorLocationMovePending || DesiredOutdoorLocationId <= 0
		|| DesiredOutdoorLocationId == ConfirmedLocationId || bConfirmedIndoor)
	{
		return;
	}

	AFieldPlayerCharacter* Character = GetMyCharacter();
	UEmberServerManager* Server = GetServer();

	const UNUWorldDataSubsystem* Data = GetWorldData(GetWorld());
	const FNULocationDefinition* Target = Data != nullptr ? Data->FindLocationDefinition(DesiredOutdoorLocationId) : nullptr;

	if (Character == nullptr || Server == nullptr || !Server->IsLoggedIn() || Target == nullptr)
	{
		return;
	}

	PendingOutdoorFromLocationId = ConfirmedLocationId;
	PendingOutdoorToLocationId = DesiredOutdoorLocationId;
	// 하위 지역으로 걸어 들어가면 서버가 확정하는 지역도 바뀐다. 응답은 도착 장소의 지역으로 대조한다.
	PendingOutdoorAreaId = Target->AreaId;
	PendingOutdoorMutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	PendingOutdoorLocation = Character->GetActorLocation();
	PendingOutdoorFacing = static_cast<int32>(Character->Facing);
	bOutdoorLocationMovePending = true;

	UE_LOG(LogFieldSync, Log,
		TEXT("야외 장소 동기화 요청 from=%d to=%d mutation=%s pos=%s facing=%d"),
		PendingOutdoorFromLocationId, PendingOutdoorToLocationId, *PendingOutdoorMutationId,
		*PendingOutdoorLocation.ToCompactString(), PendingOutdoorFacing);
	Server->SendLocationMove(PendingOutdoorFromLocationId, PendingOutdoorToLocationId, 0,
		PendingOutdoorMutationId, PendingOutdoorLocation, PendingOutdoorFacing);
}

bool UFieldSyncSubsystem::HandleLocationMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response)
{
	if (!bOutdoorLocationMovePending)
	{
		return false;
	}

	if (Response == nullptr)
	{
		UE_LOG(LogFieldSync, Warning, TEXT("야외 LocationMove 응답 본문이 없습니다. code=%d message=%s"),
			ResultCode, *ResultMessage);
		ResetOutdoorLocationMove();
		return true;
	}

	if (Response->MutationId != PendingOutdoorMutationId)
	{
		return false;
	}

	const FVector ConfirmedLocation(Response->PosX, Response->PosY, Response->PosZ);
	const bool bValidSuccess = ResultCode == 200
		&& Response->FromLocationId == PendingOutdoorFromLocationId
		&& Response->ToLocationId == PendingOutdoorToLocationId
		&& Response->_area_id.AreaId == PendingOutdoorAreaId
		&& Response->_location_id.LocationId == PendingOutdoorToLocationId
		&& !Response->_is_indoor.IsIndoor
		&& Response->Facing >= static_cast<int32>(EFieldFacing::Down)
		&& Response->Facing <= static_cast<int32>(EFieldFacing::Right)
		&& !ConfirmedLocation.ContainsNaN()
		&& FVector::DistSquared(ConfirmedLocation, PendingOutdoorLocation) <= FMath::Square(50.0f);

	if (bValidSuccess)
	{
		ApplyConfirmedSpace(Response->_area_id.AreaId, Response->_location_id.LocationId, false);
		UE_LOG(LogFieldSync, Log, TEXT("야외 장소 동기화 완료 area=%d location=%d"),
			ConfirmedAreaId, ConfirmedLocationId);
		ResetOutdoorLocationMove();
		HandleLocalLocationChanged();
		return true;
	}

	if (ResultCode == 2402)
	{
		if (ReconcileLocationMoveFailure(ResultCode, ResultMessage, Response))
		{
			const int32 RetryTarget = DesiredOutdoorLocationId;
			const int32 ServerLocationId = ConfirmedLocationId;
			const bool bCanRetry = !bOutdoorMismatchRetried && RetryTarget > 0 && RetryTarget != ServerLocationId;
			ResetOutdoorLocationMove();
			DesiredOutdoorLocationId = RetryTarget;
			bOutdoorMismatchRetried = true;

			UE_LOG(LogFieldSync, Warning,
				TEXT("야외 장소 기준 재동기화 server=%d target=%d retry=%d"),
				ServerLocationId, RetryTarget, bCanRetry ? 1 : 0);

			if (bCanRetry && !bConfirmedIndoor)
			{
				TrySendOutdoorLocationMove();
			}

			return true;
		}
	}

	UE_LOG(LogFieldSync, Warning,
		TEXT("야외 LocationMove 거절 또는 응답 검증 실패. code=%d from=%d to=%d mutation=%s message=%s"),
		ResultCode, PendingOutdoorFromLocationId, PendingOutdoorToLocationId,
		*PendingOutdoorMutationId, *ResultMessage);
	ResetOutdoorLocationMove();
	return true;
}

bool UFieldSyncSubsystem::ReconcileLocationMoveFailure(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response)
{
	if (ResultCode != 2402)
	{
		return false;
	}

	const int32 ServerLocationId = ResolveServerLocationId(ResultMessage, Response);
	UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUWorldDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldDataSubsystem>() : nullptr;
	const FNULocationDefinition* ServerLocation = Data != nullptr ? Data->FindLocationDefinition(ServerLocationId) : nullptr;
	const int32 ResponseAreaId = Response != nullptr ? Response->_area_id.AreaId : 0;
	const int32 ServerAreaId = ResponseAreaId > 0
		? ResponseAreaId : (ServerLocation != nullptr ? ServerLocation->AreaId : 0);

	if (ServerLocationId <= 0 || ServerAreaId <= 0)
	{
		return false;
	}

	const bool bServerIndoor = ServerLocation != nullptr
		? ServerLocation->bIndoor : (Response != nullptr && Response->_is_indoor.IsIndoor);
	ApplyConfirmedSpace(ServerAreaId, ServerLocationId, bServerIndoor);
	return true;
}

void UFieldSyncSubsystem::ResetOutdoorLocationMove()
{
	bOutdoorLocationMovePending = false;
	PendingOutdoorFromLocationId = 0;
	PendingOutdoorToLocationId = 0;
	PendingOutdoorAreaId = 0;
	PendingOutdoorMutationId.Reset();
	PendingOutdoorLocation = FVector::ZeroVector;
	PendingOutdoorFacing = 0;
}

int32 UFieldSyncSubsystem::ResolveServerLocationId(const FString& ResultMessage, const FGrpcRevGameLocationMove* Response) const
{
	if (Response != nullptr && Response->_location_id.LocationId > 0)
	{
		return Response->_location_id.LocationId;
	}

	const FString Prefix(TEXT("Server has location "));
	int32 LocationId = 0;
	return ResultMessage.StartsWith(Prefix)
		&& LexTryParseString(LocationId, *ResultMessage.RightChop(Prefix.Len())) ? LocationId : 0;
}

void UFieldSyncSubsystem::SetActivity(EFieldActivity NewActivity)
{
	if (MyActivity == NewActivity)
	{
		return; // 바뀔 때만 보낸다
	}
	MyActivity = NewActivity;
	
	UE_LOG(LogFieldSync, Log, TEXT("내 활동 상태 -> %d"), (int32)NewActivity);

	if (bInField)
	{
		if (UEmberServerManager* Server = GetServer())
		{
			Server->SendFieldActivity(static_cast<int32>(NewActivity));
		}
	}
}

void UFieldSyncSubsystem::SetSpirit(int32 NewSpiritId)
{
	if (MySpiritId == NewSpiritId)
	{
		return;
	}
	MySpiritId = NewSpiritId;

	UE_LOG(LogFieldSync, Log, TEXT("내 정령 -> %d"), NewSpiritId);

	if (bInField)
	{
		if (UEmberServerManager* Server = GetServer())
		{
			Server->SendFieldSpirit(NewSpiritId);
		}
	}
}

void UFieldSyncSubsystem::RefreshMySpirit()
{
	SetSpirit(GetMySpiritId());
}

EFieldActivity UFieldSyncSubsystem::GetMyActivity() const
{
	return MyActivity;
}

AFieldRemoteCharacter* UFieldSyncSubsystem::FindRemotePlayer(int64 PlayerId) const
{
	const TObjectPtr<AFieldRemoteCharacter>* Found = Remotes.Find(PlayerId);
	return Found != nullptr ? Found->Get() : nullptr;
}

AFieldRemoteCharacter* UFieldSyncSubsystem::FindRemoteByCharacterIdx(int64 CharacterIdx) const
{
	// 캐릭터 미선택이면 0으로 옴
	if (CharacterIdx == 0)
	{
		return nullptr;
	}

	for (const TPair<int64, TObjectPtr<AFieldRemoteCharacter>>& Pair : Remotes)
	{
		if (Pair.Value != nullptr && Pair.Value->CharacterIdx == CharacterIdx)
		{
			return Pair.Value.Get();
		}
	}

	return nullptr;
}

// ── 내 위치 보고 ──────────────────────────────────────────────────────────
// 보내는 계기는 4가지뿐이다: 이동 시작 / 방향 전환 / 정지 / 이동 중 안전망.
// 가만히 서 있으면 아무것도 보내지 않는다.

void UFieldSyncSubsystem::TickReportMyPosition(float DeltaSeconds)
{
	if (IsPositionReportingSuspended())
	{
		return;
	}
	AFieldPlayerCharacter* Me = GetMyCharacter();
	UEmberServerManager* Server = GetServer();
	if (Me == nullptr || Server == nullptr)
	{
		return;
	}

	TimeSinceSend += DeltaSeconds;

	const FVector MyLocation = Me->GetActorLocation();
	const bool bMoving = Me->GetVelocity().SizeSquared2D() > MOVING_SPEED_SQ;
	const EFieldFacing Facing = Me->Facing;

	EFieldMoveReason Reason;
	if (bMoving && bWasMoving == false)
	{
		Reason = EFieldMoveReason::Start;
	}
	else if (bMoving == false && bWasMoving)
	{
		Reason = EFieldMoveReason::Stop; // 정확한 정지 좌표를 1발
	}
	else if (bMoving && Facing != SentFacing)
	{
		Reason = EFieldMoveReason::FacingChange;
	}
	else if (bMoving && TimeSinceSend >= HEARTBEAT_SEC)
	{
		Reason = EFieldMoveReason::Heartbeat;
	}
	else if (FVector::DistSquared(MyLocation, SentLocation) > FMath::Square(POS_TOLERANCE_CM))
	{
		// 낙하/착지 처리, 수평 좌표는 처리 안하기 때문에 위에서 안걸림
		Reason = EFieldMoveReason::Heartbeat;
	}
	else
	{
		bWasMoving = bMoving;
		return; // 보낼 이유 없음
	}

	Server->SendFieldMove(Me->GetActorLocation(), static_cast<int32>(Facing), static_cast<int32>(Reason));

	SentFacing = Facing;
	SentLocation = MyLocation;
	TimeSinceSend = 0.0f;
	bWasMoving = bMoving;
}

void UFieldSyncSubsystem::SuspendPositionReporting(const UObject* Requester)
{
	if (!IsValid(Requester) || PositionReportBlockers.Contains(Requester))
	{
		return;
	}
	// 이전 위치에서 정지했음을 알리고 전환 중 좌표는 송신하지 않는다.
	if (!IsPositionReportingSuspended() && bInField && bWasMoving)
	{
		AFieldPlayerCharacter* Me = GetMyCharacter();
		UEmberServerManager* Server = GetServer();
		if (Me != nullptr && Server != nullptr)
		{
			Server->SendFieldMove(Me->GetActorLocation(), static_cast<int32>(Me->Facing), static_cast<int32>(EFieldMoveReason::Stop));
		}
	}
	PositionReportBlockers.Add(Requester);
}

void UFieldSyncSubsystem::ResumePositionReporting(const UObject* Requester, bool bReportCurrentPosition)
{
	if (PositionReportBlockers.Remove(Requester) == 0 || IsPositionReportingSuspended())
	{
		return;
	}
	bWasMoving = false;
	TimeSinceSend = 0.0f;
	AFieldPlayerCharacter* Me = GetMyCharacter();
	if (Me != nullptr)
	{
		SentLocation = Me->GetActorLocation();
		SentFacing = Me->Facing;
		UEmberServerManager* Server = GetServer();
		// 기존 로컬 전환은 최종 위치 1회 보고. 서버 확정 위치 적용 경로에서는 false를 사용한다.
		if (bReportCurrentPosition && bInField && Server != nullptr)
		{
			Server->SendFieldMove(SentLocation, static_cast<int32>(SentFacing), static_cast<int32>(EFieldMoveReason::Stop));
		}
	}
}

bool UFieldSyncSubsystem::IsPositionReportingSuspended() const
{
	for (const TWeakObjectPtr<const UObject>& Requester : PositionReportBlockers)
	{
		if (Requester.IsValid())
		{
			return true;
		}
	}
	return false;
}

// ── 서버 푸시 수신 ────────────────────────────────────────────────────────

void UFieldSyncSubsystem::OnAppeared(int64 PlayerId, const FNUFieldAppearance& Data)
{
	UE_LOG(LogFieldSync, Log, TEXT("[진단] 원격 등장 %lld @ (%.0f, %.0f, %.0f) activity=%d charIdx=%lld"),
		PlayerId, Data.Location.X, Data.Location.Y, Data.Location.Z,
		(int32)Data.Activity, Data.CharacterIdx);

	// 캐릭터 Id 는 10억부터 시작하므로 이를 확인
	if (Data.CharacterIdx != 0 && Data.CharacterIdx < NUField::CHARACTER_IDX_START)
	{
		UE_LOG(LogFieldSync, Warning,
			TEXT("원격 %lld 의 character_idx 가 범위 밖입니다 (%lld) - player_id를 넣지 않았는지 확인하세요"),
			PlayerId, Data.CharacterIdx);
	}

	if (TObjectPtr<AFieldRemoteCharacter>* Existing = Remotes.Find(PlayerId))
	{
		if (*Existing != nullptr) // 이미 있다 - 상태만 갱신
		{
			(*Existing)->ApplyServerState(Data.Location, Data.Facing, Data.Activity);
			(*Existing)->SetSpirit(Data.SpiritId);
			(*Existing)->SetCharacterName(Data.CharacterName);
			(*Existing)->SetAppearance(Data.RaceId, Data.Gender, Data.HairId, Data.FaceId, Data.JobId);
			(*Existing)->CharacterIdx = Data.CharacterIdx;
			
			// 갱신에도 정령,이름,캐릭터Id 등을 확인해야 함, 변경됐을 수 있음
		}
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFieldRemoteCharacter* Remote = World->SpawnActor<AFieldRemoteCharacter>(
		AFieldRemoteCharacter::StaticClass(), Data.Location, FRotator::ZeroRotator, Params);
	if (Remote == nullptr)
	{
		UE_LOG(LogFieldSync, Warning, TEXT("[진단] 원격 캐릭터 스폰 실패 %lld"), PlayerId);
		return;
	}

	UE_LOG(LogFieldSync, Log, TEXT("[진단] 원격 캐릭터 스폰 완료 %lld (현재 %d명)"),
		PlayerId, Remotes.Num() + 1);

	Remote->PlayerId = PlayerId;
	Remote->CharacterIdx = Data.CharacterIdx;
	Remote->SetCharacterName(Data.CharacterName);
	Remote->TeleportToServerState(Data.Location, Data.Facing, Data.Activity);
	Remote->SetSpirit(Data.SpiritId);
	Remote->SetAppearance(Data.RaceId, Data.Gender, Data.HairId, Data.FaceId, Data.JobId);
	Remotes.Add(PlayerId, Remote);

	OnRemotePlayerSpawned.Broadcast(Remote);
}

void UFieldSyncSubsystem::OnDisappeared(int64 PlayerId)
{
	TObjectPtr<AFieldRemoteCharacter> Remote = nullptr;
	if (Remotes.RemoveAndCopyValue(PlayerId, Remote) == false)
	{
		return;
	}
	if (Remote != nullptr)
	{
		Remote->Destroy();
	}
	OnRemotePlayerDespawned.Broadcast(PlayerId);
}

void UFieldSyncSubsystem::OnMoved(int64 PlayerId, const FVector& Location, EFieldFacing Facing)
{
	TObjectPtr<AFieldRemoteCharacter>* Found = Remotes.Find(PlayerId);
	if (Found == nullptr || *Found == nullptr)
	{
		// **모르는 id의 이동은 무시한다 - 여기서 액터를 만들면 안 된다.**
		// (사라진 뒤 늦게 도착한 이동 패킷으로 유령 액터가 남는 것을 막는다)
		return;
	}
	(*Found)->ApplyServerState(Location, Facing, (*Found)->Activity);
}

void UFieldSyncSubsystem::OnActivityChanged(int64 PlayerId, EFieldActivity Activity)
{
	if (AFieldRemoteCharacter* Remote = FindRemotePlayer(PlayerId))
	{
		Remote->SetActivity(Activity);
	}
}

void UFieldSyncSubsystem::OnSpiritChanged(int64 PlayerId, int32 SpiritId)
{
	if (AFieldRemoteCharacter* Remote = FindRemotePlayer(PlayerId))
	{
		Remote->SetSpirit(SpiritId);
	}
}

// ── 헬퍼 ──────────────────────────────────────────────────────────────────

int32 UFieldSyncSubsystem::GetMySpiritId() const
{
	// 활성 정령이 없으면 0 (proto 규약: 0 = 정령 없음, 음수는 서버가 401로 거부)
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	const ANUPlayerState* PS = PC ? PC->GetPlayerState<ANUPlayerState>() : nullptr;
	const ANUSpiritState* SpiritState = PS ? PS->GetActiveSpiritState() : nullptr;

	// 꺼내지 않았으면 정령이 없는 것으로 보고함, 남의 화면에서도 사라져야 하기 때문
	if (!SpiritState || !PS->IsNaiaSummoned())
	{
		return 0;
	}

	return SpiritState->GetSpiritId();
}

AFieldPlayerCharacter* UFieldSyncSubsystem::GetMyCharacter() const
{
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			return Cast<AFieldPlayerCharacter>(PC->GetPawn());
		}
	}
	return nullptr;
}

UEmberServerManager* UFieldSyncSubsystem::GetServer() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UEmberServerManager>();
		}
	}
	return nullptr;
}
