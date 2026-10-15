// Fill out your copyright notice in the Description page of Project Settings.


#include "NUNaiaNetSubsystem.h"

#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Naia/Data/NUNaiaTypes.h"
#include "Net/EmberServerManager.h"
#include "SRevGame/GameMessage.h"
#include "Subsystems/SubsystemCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUNaiaNet, Log, All);

namespace
{
	ENUNaiaUpdateReason ConvertUpdateReason(int32 RawReason)
	{
		if (RawReason <= static_cast<int32>(ENUNaiaUpdateReason::None)
			|| RawReason >= static_cast<int32>(ENUNaiaUpdateReason::Count))
		{
			return ENUNaiaUpdateReason::None;
		}

		return static_cast<ENUNaiaUpdateReason>(RawReason);
	}
	
	constexpr int32 ROSTER_RC_SUCCESS = 200;
	constexpr int32 ROSTER_RC_STARTER_ALREADY = 2008;
}


void UNUNaiaNetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// UEmberServerManager 부터 초기화 보장
	Collection.InitializeDependency<UEmberServerManager>();

	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.AddDynamic(this, &ThisClass::HandleServerStateChanged);
	}

	// 디버그 명령어 추가
#if !UE_BUILD_SHIPPING
	RegisterDebugCommands();
#endif
}

void UNUNaiaNetSubsystem::Deinitialize()
{
	// 디버그 명렁어 제거
#if !UE_BUILD_SHIPPING
	UnregisterDebugCommands();
#endif

	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleServerStateChanged);
	}

	ResetRosterState();

	Super::Deinitialize();
}

void UNUNaiaNetSubsystem::RequestNaiaList()
{
	UEmberServerManager* Server = GetLoggedInServer(TEXT("나이아 로스터 조회"));
	if (!Server)
	{
		return;
	}

	Server->SendNaiaList();
}

void UNUNaiaNetSubsystem::RequestNaiaMove(int32 FromContainer, int32 FromSlot, int32 ToContainer, int32 ToSlot)
{
	if ((FromContainer != NUNaiaContainer::Party && FromContainer != NUNaiaContainer::Storage)
		|| (ToContainer != NUNaiaContainer::Party && ToContainer != NUNaiaContainer::Storage)
		|| FromSlot < 0 || ToSlot < 0)
	{
		UE_LOG(LogNUNaiaNet, Warning, TEXT("나이아 이동 실패 : 잘못된 인자 [%d/%d -> %d/%d]"),
			FromContainer, FromSlot, ToContainer, ToSlot);
		return;
	}

	UEmberServerManager* Server = GetLoggedInServer(TEXT("나이아 이동"));
	if (!Server)
	{
		return;
	}

	FNUServerNaiaMoveRequest Request;
	Request.FromContainer = FromContainer;
	Request.FromSlot = FromSlot;
	Request.ToContainer = ToContainer;
	Request.ToSlot = ToSlot;

	Server->SendNaiaMove(Request);
}

bool UNUNaiaNetSubsystem::RequestStarterNaiaContract(int32 NaiaId, FText& OutError)
{
	OutError = FText::GetEmpty();

	if (NaiaId <= 0)
	{
		OutError = NSLOCTEXT("NUNaia", "StarterInvalidId", "계약할 스타팅 나이아가 올바르지 않습니다.");
		return false;
	}

	UEmberServerManager* Server = GetLoggedInServer(TEXT("스타팅 나이아 계약"));
	if (!Server)
	{
		OutError = NSLOCTEXT("NUNaia", "StarterServerOffline", "서버에 접속한 뒤 다시 시도하세요.");
		return false;
	}

	if (!bRosterReceived)
	{
		OutError = NSLOCTEXT("NUNaia", "StarterRosterPending", "나이아 로스터 동기화가 끝난 뒤 다시 시도하세요.");
		RequestNaiaList();
		return false;
	}

	if (HasStarterNaiaContract())
	{
		OutError = NUNaiaServer::GetResultMessage(ROSTER_RC_STARTER_ALREADY);
		return false;
	}

	if (!PendingStarterNaiaMutationId.IsEmpty() && PendingStarterNaiaId != NaiaId)
	{
		OutError = NSLOCTEXT("NUNaia", "StarterRequestPending", "다른 스타팅 나이아의 계약 요청을 처리 중입니다.");
		return false;
	}

	if (PendingStarterNaiaMutationId.IsEmpty())
	{
		PendingStarterNaiaId = NaiaId;
		PendingStarterNaiaMutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	Server->SendNaiaStarterSelect(PendingStarterNaiaId, PendingStarterNaiaMutationId);
	UE_LOG(LogNUNaiaNet, Log, TEXT("스타팅 나이아 계약 요청 전송 NaiaId=%d"), PendingStarterNaiaId);
	return true;
}

void UNUNaiaNetSubsystem::HandleListResponse(int32 ResultCode, const FGrpcRevGameNaiaList& Message)
{
	if (ResultCode != ROSTER_RC_SUCCESS)
	{
		UE_LOG(LogNUNaiaNet, Warning, TEXT("나이아 로스터 조회 거절 [ResultCode=%d]"), ResultCode);
		OnMoveRejected.Broadcast(NUNaiaServer::GetResultMessage(ResultCode));
		return;
	}

	TArray<FNUServerNaiaSlot> ParsedSlots;
	ParsedSlots.Reserve(Message.Slots.Num());

	for (const FGrpcRevGameNaiaSlot& Source : Message.Slots)
	{
		FNUServerNaiaSlot Slot;
		FString Error;

		if (!NUNaiaServer::ConvertNaiaSlot(Source, Slot, Error))
		{
			UE_LOG(LogNUNaiaNet, Warning, TEXT("나이아 칸 변환 실패: %s"), *Error);
			return;
		}

		ParsedSlots.Add(MoveTemp(Slot));
	}

	if (Message.StarterNaiaInstanceId < 0)
	{
		UE_LOG(LogNUNaiaNet, Warning, TEXT("나이아 로스터 조회 거절: 음수 StarterNaiaInstanceId=%lld"), Message.StarterNaiaInstanceId);
		return;
	}

	PartySlotCount = Message.PartySlotCount;
	StorageSlotCount = Message.StorageSlotCount;
	bRosterReceived = true;

	// 전부 교체
	ApplySlots(ParsedSlots, true);

	if (Message.StarterNaiaInstanceId > 0)
	{
		StarterNaiaInstanceId = Message.StarterNaiaInstanceId;
		StarterNaiaId = 0;
		const bool bResolved = ResolveStarterNaiaContractFromSlots();
		ClearPendingStarterNaiaContract();

		if (!bResolved)
		{
			UE_LOG(LogNUNaiaNet, Warning, TEXT("스타팅 계약 개체가 로스터에 없음 InstanceId=%lld"), StarterNaiaInstanceId);
		}
	}
	else if (!IsStarterNaiaContractPending())
	{
		StarterNaiaInstanceId = 0;
		StarterNaiaId = 0;
	}

	UE_LOG(LogNUNaiaNet, Log, TEXT("나이아 로스터 조회 완료 Party=%d/%d Storage=%d/%d"),
		PartySlots.Num(), PartySlotCount, StorageSlots.Num(), StorageSlotCount);

	OnRosterNetChanged.Broadcast();
}

void UNUNaiaNetSubsystem::HandleUpdated(const FGrpcRevGameSVR_Naia_Updated& Message)
{
	TArray<FNUServerNaiaSlot> ParsedSlots;
	ParsedSlots.Reserve(Message.Slots.Num());

	for (const FGrpcRevGameNaiaSlot& Source : Message.Slots)
	{
		FNUServerNaiaSlot Slot;
		FString Error;

		if (!NUNaiaServer::ConvertNaiaSlot(Source, Slot, Error))
		{
			UE_LOG(LogNUNaiaNet, Warning, TEXT("나이아 칸 변환 실패: %s"), *Error);
			return;
		}

		ParsedSlots.Add(MoveTemp(Slot));
	}

	const ENUNaiaUpdateReason Reason = ConvertUpdateReason(Message.Reason);

	// 부분만 교체
	ApplySlots(ParsedSlots, false);

	if (Reason == ENUNaiaUpdateReason::StarterContract)
	{
		bool bResolved = ResolveStarterNaiaContractFromSlots();
		if (!bResolved)
		{
			const FNUServerNaiaSlot* StarterSlot = PartySlots.FindByPredicate([](const FNUServerNaiaSlot& Slot)
			{
				return Slot.SlotNo == 0 && !Slot.IsEmpty();
			});

			if (StarterSlot != nullptr)
			{
				StarterNaiaInstanceId = StarterSlot->Instance.InstanceId;
				StarterNaiaId = StarterSlot->Instance.NaiaId;
				bResolved = true;
			}
		}

		if (bResolved)
		{
			ClearPendingStarterNaiaContract();
		}
		else
		{
			UE_LOG(LogNUNaiaNet, Warning, TEXT("StarterContract 푸시에 파티 0번 개체가 없음"));
		}
	}

	UE_LOG(LogNUNaiaNet, Log, TEXT("나이아 로스터 갱신 Reason=%s Slots=%d"),
		*UEnum::GetValueAsString(Reason), ParsedSlots.Num());

	OnRosterNetChanged.Broadcast();
	OnRosterUpdated.Broadcast(Reason);
}

void UNUNaiaNetSubsystem::HandleAck(int32 ResultCode, const TCHAR* RequestName)
{
	if (ResultCode == ROSTER_RC_SUCCESS)
	{
		UE_LOG(LogNUNaiaNet, Log, TEXT("%s 요청 승인"), RequestName);
		return;
	}

	UE_LOG(LogNUNaiaNet, Warning, TEXT("%s 요청 거절 [ResultCode=%d]"), RequestName, ResultCode);
	OnMoveRejected.Broadcast(NUNaiaServer::GetResultMessage(ResultCode));
}

void UNUNaiaNetSubsystem::HandleStarterSelectResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameNaiaStarterSelect& Message)
{
	if (ResultCode != ROSTER_RC_SUCCESS && ResultCode != ROSTER_RC_STARTER_ALREADY)
	{
		const FText Error = ResultMessage.IsEmpty() ? NUNaiaServer::GetResultMessage(ResultCode) : FText::FromString(ResultMessage);
		UE_LOG(LogNUNaiaNet, Warning, TEXT("스타팅 나이아 계약 거절 [ResultCode=%d] %s"), ResultCode, *Error.ToString());
		ClearPendingStarterNaiaContract();
		OnStarterNaiaContractRejected.Broadcast(Error);
		return;
	}

	if (Message.StarterNaiaInstanceId <= 0)
	{
		const FText Error = NSLOCTEXT("NUNaia", "StarterInvalidResponse", "서버가 스타팅 나이아의 영구 개체 ID를 보내지 않았습니다.");
		UE_LOG(LogNUNaiaNet, Error, TEXT("스타팅 나이아 계약 응답 오류 [ResultCode=%d InstanceId=%lld]"), ResultCode, Message.StarterNaiaInstanceId);
		ClearPendingStarterNaiaContract();
		OnStarterNaiaContractRejected.Broadcast(Error);
		return;
	}

	StarterNaiaInstanceId = Message.StarterNaiaInstanceId;
	StarterNaiaId = 0;
	if (ResolveStarterNaiaContractFromSlots())
	{
		ClearPendingStarterNaiaContract();
		OnRosterNetChanged.Broadcast();
	}

	UE_LOG(LogNUNaiaNet, Log, TEXT("스타팅 나이아 계약 응답 승인 ResultCode=%d InstanceId=%lld"), ResultCode, StarterNaiaInstanceId);
	RequestNaiaList();
}

bool UNUNaiaNetSubsystem::HasReceivedRoster() const
{
	return bRosterReceived;
}

bool UNUNaiaNetSubsystem::HasActiveServerSession() const
{
	const UEmberServerManager* Server = GetServer();
	return IsValid(Server) && Server->IsLoggedIn();
}

const TArray<FNUServerNaiaSlot>& UNUNaiaNetSubsystem::GetPartySlots() const
{
	return PartySlots;
}

const TArray<FNUServerNaiaSlot>& UNUNaiaNetSubsystem::GetStorageSlots() const
{
	return StorageSlots;
}

int32 UNUNaiaNetSubsystem::GetPartySlotCount() const
{
	return PartySlotCount;
}

int32 UNUNaiaNetSubsystem::GetStorageSlotCount() const
{
	return StorageSlotCount;
}

const FNUServerNaiaSlot* UNUNaiaNetSubsystem::FindSlot(int32 Container, int32 SlotNo) const
{
	const TArray<FNUServerNaiaSlot>& Slots = Container == NUNaiaContainer::Party 
		? PartySlots 
		: StorageSlots;

	return Slots.FindByPredicate([SlotNo](const FNUServerNaiaSlot& Slot)
	{
		return Slot.SlotNo == SlotNo;
	});
}

void UNUNaiaNetSubsystem::HandleServerStateChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn)
	{
		return;
	}

	if (!bRosterReceived && !IsStarterNaiaContractPending() && !HasStarterNaiaContract())
	{
		return;
	}

	UE_LOG(LogNUNaiaNet, Warning, TEXT("서버 연결 종료로 나이아 로스터 상태 초기화"));

	ResetRosterState();
}

UEmberServerManager* UNUNaiaNetSubsystem::GetServer() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
}

void UNUNaiaNetSubsystem::ResetRosterState()
{
	PartySlots.Reset();
	StorageSlots.Reset();
	PartySlotCount = 0;
	StorageSlotCount = 0;
	bRosterReceived = false;
	StarterNaiaInstanceId = 0;
	StarterNaiaId = 0;
	ClearPendingStarterNaiaContract();

	OnRosterNetChanged.Broadcast();
}

bool UNUNaiaNetSubsystem::ResolveStarterNaiaContractFromSlots()
{
	if (StarterNaiaInstanceId <= 0)
	{
		return false;
	}

	const auto MatchesStarter = [this](const FNUServerNaiaSlot& Slot)
	{
		return !Slot.IsEmpty() && Slot.Instance.InstanceId == StarterNaiaInstanceId;
	};

	const FNUServerNaiaSlot* StarterSlot = PartySlots.FindByPredicate(MatchesStarter);
	if (StarterSlot == nullptr)
	{
		StarterSlot = StorageSlots.FindByPredicate(MatchesStarter);
	}
	if (StarterSlot == nullptr)
	{
		return false;
	}

	StarterNaiaId = StarterSlot->Instance.NaiaId;
	return StarterNaiaId > 0;
}

void UNUNaiaNetSubsystem::ClearPendingStarterNaiaContract()
{
	PendingStarterNaiaId = 0;
	PendingStarterNaiaMutationId.Reset();
}

void UNUNaiaNetSubsystem::ApplySlots(const TArray<FNUServerNaiaSlot>& Slots, bool bFullSnapshot)
{
	TArray<FNUServerNaiaSlot> IncomingParty;
	TArray<FNUServerNaiaSlot> IncomingStorage;

	for (const FNUServerNaiaSlot& Slot : Slots)
	{
		if (Slot.Container == NUNaiaContainer::Party)
		{
			IncomingParty.Add(Slot);
		}
		else
		{
			IncomingStorage.Add(Slot);
		}
	}

	if (bFullSnapshot)
	{
		// NaiaList — 빈 칸은 원래 안 실려 오므로 받은 것만으로 통째로 교체
		PartySlots = MoveTemp(IncomingParty);
		StorageSlots = MoveTemp(IncomingStorage);
		return;
	}

	// SVR_Naia_Updated — 파티는 항상 4칸이 전부 오므로 통째로 교체
	if (!IncomingParty.IsEmpty())
	{
		PartySlots = MoveTemp(IncomingParty);
	}

	// 보관함은 건드린 칸만 오므로 SlotNo 기준으로 병합, 빈 칸으로 온 항목은 제거
	for (const FNUServerNaiaSlot& Slot : IncomingStorage)
	{
		StorageSlots.RemoveAll([&Slot](const FNUServerNaiaSlot& Existing)
		{
			return Existing.SlotNo == Slot.SlotNo;
		});

		if (!Slot.IsEmpty())
		{
			StorageSlots.Add(Slot);
		}
	}
}

UEmberServerManager* UNUNaiaNetSubsystem::GetLoggedInServer(const TCHAR* Operation) const
{
	UEmberServerManager* Server = GetServer();

	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		UE_LOG(LogNUNaiaNet, Warning, TEXT("%s 실패: 서버 미접속"), Operation);
		return nullptr;
	}

	return Server;
}


// -------------------
//   콘솔 디버그 명령
// -------------------
#if !UE_BUILD_SHIPPING
void UNUNaiaNetSubsystem::RegisterDebugCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.NaiaNet.List"),
		TEXT("나이아 로스터 전체 조회를 요청합니다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			RequestNaiaList();
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.NaiaNet.StarterSelect"),
		TEXT("스타팅 나이아 계약을 요청합니다. 사용법: nu.NaiaNet.StarterSelect <NaiaId>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.IsEmpty())
			{
				UE_LOG(LogNUNaiaNet, Warning, TEXT("사용법: nu.NaiaNet.StarterSelect <NaiaId>"));
				return;
			}

			FText Error;
			if (!RequestStarterNaiaContract(FCString::Atoi(*Args[0]), Error))
			{
				UE_LOG(LogNUNaiaNet, Warning, TEXT("스타팅 나이아 계약 요청 실패: %s"), *Error.ToString());
			}
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.NaiaNet.Move"),
		TEXT("나이아 칸 이동을 요청합니다. 사용법: nu.NaiaNet.Move <fromContainer> <fromSlot> <toContainer> <toSlot>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 4)
			{
				UE_LOG(LogNUNaiaNet, Warning,
					TEXT("사용법: nu.NaiaNet.Move <fromContainer> <fromSlot> <toContainer> <toSlot>"));
				return;
			}

			RequestNaiaMove(
				FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]),
				FCString::Atoi(*Args[2]), FCString::Atoi(*Args[3]));
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.NaiaNet.Dump"),
		TEXT("클라이언트가 보관 중인 서버 나이아 로스터 상태를 출력합니다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			DumpDebugState();
		}),
		ECVF_Default));
}

void UNUNaiaNetSubsystem::UnregisterDebugCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();
	TArray<IConsoleObject*> StillRegistered;
	Console.ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda(
			[&StillRegistered](const TCHAR*, IConsoleObject* Object)
			{
				StillRegistered.Add(Object);
			}),
		TEXT("nu.NaiaNet."));

	for (IConsoleCommand* Command : DebugCommands)
	{
		if (Command && StillRegistered.Contains(Command))
		{
			Console.UnregisterConsoleObject(Command);
		}
	}

	DebugCommands.Reset();
}

void UNUNaiaNetSubsystem::DumpDebugState() const
{
	auto DumpSlots = [](const TArray<FNUServerNaiaSlot>& Slots)
	{
		for (const FNUServerNaiaSlot& Slot : Slots)
		{
			UE_LOG(LogNUNaiaNet, Log,
				TEXT("  Slot=%d Instance=%lld NaiaId=%d Lv=%d HP%%=%.2f Passive=%d Actives=%d"),
				Slot.SlotNo,
				Slot.Instance.InstanceId,
				Slot.Instance.NaiaId,
				Slot.Instance.Level,
				Slot.Instance.HealthRatio,
				Slot.Instance.PassiveSkillId,
				Slot.Instance.ActiveSkillIds.Num());
		}
	};

	UE_LOG(LogNUNaiaNet, Log, TEXT("========== NaiaNet 상태 =========="));
	UE_LOG(LogNUNaiaNet, Log, TEXT("Received=%s Party=%d/%d Storage=%d/%d"),
		bRosterReceived ? TEXT("true") : TEXT("false"),
		PartySlots.Num(), PartySlotCount, StorageSlots.Num(), StorageSlotCount);
	UE_LOG(LogNUNaiaNet, Log, TEXT("StarterInstance=%lld StarterNaiaId=%d Pending=%s PendingNaiaId=%d"),
		StarterNaiaInstanceId, StarterNaiaId,
		IsStarterNaiaContractPending() ? TEXT("true") : TEXT("false"), PendingStarterNaiaId);

	UE_LOG(LogNUNaiaNet, Log, TEXT("-- Party --"));
	DumpSlots(PartySlots);

	UE_LOG(LogNUNaiaNet, Log, TEXT("-- Storage --"));
	DumpSlots(StorageSlots);

	UE_LOG(LogNUNaiaNet, Log, TEXT("==================================="));
}
#endif
