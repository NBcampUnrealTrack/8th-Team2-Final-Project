#include "Recovery/Net/NURecoveryNetSubsystem.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Naia/Net/NUNaiaNetSubsystem.h"
#include "Net/EmberServerManager.h"

#include "SRevGame/GameMessage.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

void UNURecoveryNetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UEmberServerManager>();
	Collection.InitializeDependency<UNUNaiaNetSubsystem>();
	GetServer()->OnStateChanged.AddDynamic(this, &ThisClass::HandleConnectionChanged);
	GetGameInstance()->GetSubsystem<UNUNaiaNetSubsystem>()->OnRosterNetChanged.AddUObject(this, &ThisClass::CaptureNaiaRatios);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 1.0f);
}

void UNURecoveryNetSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	if (UEmberServerManager* Server = GetServer()) { Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleConnectionChanged); }
	if (UNUNaiaNetSubsystem* Naia = GetGameInstance()->GetSubsystem<UNUNaiaNetSubsystem>()) { Naia->OnRosterNetChanged.RemoveAll(this); }
	Super::Deinitialize();
}

UEmberServerManager* UNURecoveryNetSubsystem::GetServer() const
{
	return GetGameInstance()->GetSubsystem<UEmberServerManager>();
}

bool UNURecoveryNetSubsystem::IsActive() const
{
	return bCharacterSelected && ActiveCharacterId > 0 && GetServer() && GetServer()->IsLoggedIn();
}

// 캐릭터 선택 / 요청 송신
void UNURecoveryNetSubsystem::BeginCharacterSession(int64 CharacterId)
{
	if (CharacterId <= 0) { return; }

	// 같은 캐릭터 재접속이면 이전 요청의 결과가 불명확할 수 있으므로 UUID를 유지한다.
	if (CharacterId != ActiveCharacterId)
	{
		PendingRequest = ENURecoveryRequest::None;
		PendingMutationId.Reset();
		PendingLocationId = 0;
		LastPresentedDeathId = 0;
	}
	ActiveCharacterId = CharacterId;
	bCharacterSelected = true;
	State = FNURecoveryServerState();
	bHasState = false;
	bAwaitingRespawn = false;
	PendingPresentationDeathId = 0;
	bStatusPending = false;
	bCanRetry = true;
	NaiaRatios.Reset();
	OnStateChanged.Broadcast();
	RequestStatus();
}

bool UNURecoveryNetSubsystem::RequestService(int32 NpcId, bool bRest)
{
	if (!IsActive() || NpcId <= 0 || bAwaitingRespawn) { return false; }
	const ENURecoveryRequest Request = bRest ? ENURecoveryRequest::Rest : ENURecoveryRequest::Heal;
	if (PendingRequest != ENURecoveryRequest::None && (PendingRequest != Request || PendingNpcId != NpcId || !bCanRetry)) { return false; }

	// 재전송은 기존 UUID를 사용해야 서버에서 골드를 중복 차감하지 않는다.
	if (PendingRequest == ENURecoveryRequest::None) { PendingMutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower); }
	PendingRequest = Request;
	PendingNpcId = NpcId;
	bCanRetry = false;
	RequestStarted = FPlatformTime::Seconds();
	if (bRest)
	{
		GetServer()->SendNpcRest(NpcId, PendingMutationId);
	}
	else
	{
		GetServer()->SendNpcHeal(NpcId, PendingMutationId);
	}
	return true;
}

bool UNURecoveryNetSubsystem::RequestStatus()
{
	if (!IsActive() || bStatusPending) { return false; }
	bStatusPending = true;
	StatusStarted = FPlatformTime::Seconds();
	GetServer()->SendRecoveryStatus();
	return true;
}

bool UNURecoveryNetSubsystem::RequestRegisterPoint(int32 LocationId)
{
	if (!IsActive() || LocationId <= 0 || bAwaitingRespawn) { return false; }
	if (PendingRequest != ENURecoveryRequest::None
		&& (PendingRequest != ENURecoveryRequest::RegisterPoint || PendingLocationId != LocationId || !bCanRetry)) { return false; }

	// 응답이 유실되면 같은 ID로 재전송하여 서버의 멱등 처리를 사용한다.
	if (PendingRequest == ENURecoveryRequest::None) { PendingMutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower); }
	PendingRequest = ENURecoveryRequest::RegisterPoint;
	PendingLocationId = LocationId;
	bCanRetry = false;
	RequestStarted = FPlatformTime::Seconds();
	GetServer()->SendRecoveryPointRegister(LocationId, PendingMutationId);
	return true;
}

// 서버 확정 응답 반영
void UNURecoveryNetSubsystem::HandleResponse(ENURecoveryRequest Request, int32 Code, const FString& Message, const FString& Json)
{
	if (!IsActive() || (Request != ENURecoveryRequest::Status && PendingRequest != Request)) { return; }
	if (Request == ENURecoveryRequest::Status) { bStatusPending = false; }
	if (Code != 200)
	{
		if (Request != ENURecoveryRequest::Status) { PendingRequest = ENURecoveryRequest::None; PendingMutationId.Reset(); PendingLocationId = 0; }
		const FText Error = Code == 2302 ? NSLOCTEXT("NURecoveryNet", "Gold", "골드가 부족합니다.")
			: Code == 2304 ? NSLOCTEXT("NURecoveryNet", "Cost", "회복 비용 데이터를 확인할 수 없습니다.")
			: Message.IsEmpty() ? NSLOCTEXT("NURecoveryNet", "Rejected", "회복 또는 거점 등록 요청을 처리할 수 없습니다.") : FText::FromString(Message);
		if (Request == ENURecoveryRequest::Status) { GetGameInstance()->GetSubsystem<UNUNaiaNetSubsystem>()->RequestNaiaList(); }
		OnRequestFailed.Broadcast(Code, Error);
		return;
	}

	// 파싱 실패나 요청 ID 불일치는 성공으로 처리하지 않는다. 기존 확정 상태를 유지한다.
	FNURecoveryServerState Parsed;
	if (!NURecoveryServer::ParseResponse(Request, Json, Parsed)
		|| (Request == ENURecoveryRequest::Status || Request == ENURecoveryRequest::RegisterPoint
			? Parsed.CharacterId != ActiveCharacterId : Parsed.MutationId != PendingMutationId)
		|| (Request == ENURecoveryRequest::RegisterPoint && Parsed.RegisteredPointId != PendingLocationId))
	{
		bCanRetry = true;
		if (Request != ENURecoveryRequest::Status) { RequestStatus(); }
		OnRequestFailed.Broadcast(0, NSLOCTEXT("NURecoveryNet", "InvalidResponse", "회복 또는 거점 등록 응답을 확인할 수 없습니다. 상태를 다시 조회해 주세요."));
		return;
	}

	// 190/191에 없는 거점·사망 정보는 직전 조회 결과를 유지한다.
	if (Request == ENURecoveryRequest::Heal || Request == ENURecoveryRequest::Rest)
	{
		Parsed.CharacterId = ActiveCharacterId;
		Parsed.RegisteredPointId = State.RegisteredPointId;
		Parsed.EffectivePointId = State.EffectivePointId;
		Parsed.DeathId = State.DeathId;
		PendingRequest = ENURecoveryRequest::None;
		PendingMutationId.Reset();
		// 게임 인스턴스 서브시스템이라 월드는 인스턴스에서 얻는다
		UNUSFXSubsystem::Play(GetGameInstance()->GetWorld(), Request == ENURecoveryRequest::Rest ? NU::SFX::Field::Rest : NU::SFX::Field::Heal);
	}
	const bool bRegistered = (Request == ENURecoveryRequest::RegisterPoint
		|| (Request == ENURecoveryRequest::Status && PendingRequest == ENURecoveryRequest::RegisterPoint && Parsed.RegisteredPointId == PendingLocationId));
	const int32 RegisteredLocationId = bRegistered ? PendingLocationId : 0;
	if (bRegistered)
	{
		PendingRequest = ENURecoveryRequest::None;
		PendingMutationId.Reset();
		PendingLocationId = 0;
	}

	// 195를 놓친 경우 현재 대기 중인 패배만 복구한다. 로그인 시 과거 사망 기록으로 이동하지 않는다.
	if (Request == ENURecoveryRequest::Status && bAwaitingRespawn && Parsed.RespawnState == 2 && Parsed.DeathId > 0 && Parsed.DeathId != DeathBeforeBattle && Parsed.DeathId != LastPresentedDeathId)
	{
		PendingPresentationDeathId = Parsed.DeathId;
	}
	State = Parsed;
	StateReceived = FPlatformTime::Seconds();
	PublishState();
	if (bRegistered) { OnPointRegistered.Broadcast(RegisteredLocationId); }
	// 등록 결과가 불명확했고 조회에도 반영되지 않았다면 같은 멱등 키로 다시 보낸다.
	if (Request == ENURecoveryRequest::Status && PendingRequest == ENURecoveryRequest::RegisterPoint && bCanRetry)
	{
		RequestRegisterPoint(PendingLocationId);
	}
	// 192 정산 뒤 전체 조회한다. 190/191의 reason 5는 파티만 오므로 보관함도 다시 받는다.
	GetGameInstance()->GetSubsystem<UNUNaiaNetSubsystem>()->RequestNaiaList();
}

void UNURecoveryNetSubsystem::PublishState()
{
	bHasState = true;
	++StateSerial;
	OnStateChanged.Broadcast();
	OnDisplayChanged.Broadcast();
}

// 사망 사건 및 전투 전환
void UNURecoveryNetSubsystem::HandleRespawn(const FGrpcRevGameSVR_Respawn_Result& Message)
{
	if (!IsActive() || Message.DeathId <= 0 || Message.DeathId == LastPresentedDeathId || Message.DeathId == PendingPresentationDeathId
		|| (bAwaitingRespawn ? Message.DeathId == DeathBeforeBattle : Message.DeathId == State.DeathId)
		|| Message.PlayerMaxHp <= 0 || Message.PlayerHp != Message.PlayerMaxHp || Message.ChargedGold < 0
		|| Message.RecoveryPointId <= 0 || Message.RecoveryPointId > MAX_int32) { return; }

	// 서버에서 부활과 차감은 이미 끝났다. 클라이언트는 확정 상태와 이동 대기만 갱신한다.
	State.CharacterId = ActiveCharacterId;
	State.PlayerHp = Message.PlayerHp;
	State.PlayerMaxHp = Message.PlayerMaxHp;
	State.Gold = Message.Gold;
	State.bHasGold = true;
	State.ChargedGold = Message.ChargedGold;
	State.DeathId = Message.DeathId;
	State.EffectivePointId = Message.RecoveryPointId;
	State.RespawnState = 2;
	State.RecoveryState = 0;
	State.ServerTime = Message.ServerTime;
	PendingPresentationDeathId = Message.DeathId;
	bAwaitingRespawn = true;
	PublishState();
	GetGameInstance()->GetSubsystem<UNUNaiaNetSubsystem>()->RequestNaiaList();
}

void UNURecoveryNetSubsystem::CompleteRespawnPresentation()
{
	LastPresentedDeathId = PendingPresentationDeathId;
	PendingPresentationDeathId = 0;
	bAwaitingRespawn = false;
}

void UNURecoveryNetSubsystem::HandleBattleStarted()
{
	State.RecoveryState = 0;
	OnDisplayChanged.Broadcast();
}

void UNURecoveryNetSubsystem::HandleBattleEnded(int32 Reason)
{
	if (!IsActive()) { return; }
	bAwaitingRespawn = Reason == 2;
	DeathBeforeBattle = State.DeathId;
	RespawnWaitStarted = FPlatformTime::Seconds();
	State.RecoveryState = 0;
	// 패배는 195가 확정값을 보낸다. 승리/도주는 필드 HP를 조회한다.
	if (Reason != 2) { RequestStatus(); }
}

// 화면 표시용 HP 보간
void UNURecoveryNetSubsystem::CaptureNaiaRatios()
{
	if (!IsActive()) { return; }
	const UNUNaiaNetSubsystem* Naia = GetGameInstance()->GetSubsystem<UNUNaiaNetSubsystem>();
	NaiaRatios.Reset();
	for (const FNUServerNaiaSlot& Slot : Naia->GetPartySlots()) { if (!Slot.IsEmpty()) { NaiaRatios.Add(Slot.Instance.InstanceId, Slot.Instance.HealthRatio); } }
	for (const FNUServerNaiaSlot& Slot : Naia->GetStorageSlots()) { if (!Slot.IsEmpty()) { NaiaRatios.Add(Slot.Instance.InstanceId, Slot.Instance.HealthRatio); } }
	OnDisplayChanged.Broadcast();
}

float UNURecoveryNetSubsystem::GetDisplayedPlayerHealth() const
{
	return IsResting() ? NURecoveryServer::InterpolateHealth(State.PlayerHp, State.PlayerMaxHp, FPlatformTime::Seconds() - StateReceived, State.FullRecoverySeconds) : State.PlayerHp;
}

float UNURecoveryNetSubsystem::GetDisplayedNaiaRatio(int64 InstanceId, float Fallback) const
{
	const float* Ratio = NaiaRatios.Find(InstanceId);
	return IsResting() && Ratio ? NURecoveryServer::InterpolateHealth(*Ratio, 1.0f, FPlatformTime::Seconds() - StateReceived, State.FullRecoverySeconds) : Fallback;
}

// 응답 유실 복구와 1초 표시 갱신
bool UNURecoveryNetSubsystem::Tick(float DeltaSeconds)
{
	if (!IsActive()) { return true; }
	const double Now = FPlatformTime::Seconds();

	// 응답 지연은 서버 실패를 뜻하지 않는다. 상태를 조회하면서 같은 요청의 재전송을 허용한다.
	if (PendingRequest != ENURecoveryRequest::None && !bCanRetry && Now - RequestStarted >= 10.0)
	{
		bCanRetry = true;
		RequestStatus();
		const FText Message = PendingRequest == ENURecoveryRequest::RegisterPoint
			? NSLOCTEXT("NURecoveryNet", "RegisterTimeout", "부활 거점 등록 결과를 확인 중입니다.")
			: NSLOCTEXT("NURecoveryNet", "Timeout", "회복 결과를 확인 중입니다. 같은 선택지를 다시 선택하면 기존 요청으로 재시도합니다.");
		OnRequestFailed.Broadcast(0, Message);
	}
	if (bStatusPending && Now - StatusStarted >= 10.0)
	{
		bStatusPending = false;
		OnRequestFailed.Broadcast(0, NSLOCTEXT("NURecoveryNet", "StatusTimeout", "회복 상태 조회 응답이 없습니다. 연결 상태를 확인해 주세요."));
	}
	if (PendingRequest == ENURecoveryRequest::RegisterPoint && bCanRetry && !bStatusPending && Now - StatusStarted >= 10.0)
	{
		RequestStatus();
	}

	// 패배 이후 부활 푸시가 도착하지 않으면 상태 조회로 처리 결과를 확인한다.
	if (bAwaitingRespawn && PendingPresentationDeathId == 0 && Now - RespawnWaitStarted >= 10.0)
	{
		RespawnWaitStarted = Now;
		RequestStatus();
	}
	if (IsResting()) { OnDisplayChanged.Broadcast(); }
	return true;
}

// 접속 종료 시 표시 상태를 비우되 결과 불명 요청의 식별자는 보존한다.
void UNURecoveryNetSubsystem::HandleConnectionChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn) { return; }
	bCharacterSelected = false;
	bHasState = false;
	bStatusPending = false;
	bAwaitingRespawn = false;
	PendingPresentationDeathId = 0;
	bCanRetry = true; // 같은 캐릭터 재선택 시 결과 불명 요청의 UUID를 재사용한다.
	OnStateChanged.Broadcast();
	OnDisplayChanged.Broadcast();
}
