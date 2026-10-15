// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleNetSubsystem.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Net/EmberServerManager.h"
#include "Party/NUPartySubsystem.h"
#include "Subsystems/SubsystemCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleNet, Log, All);

namespace
{
	bool ConvertBattleEvents(
		const TArray<FGrpcRevGameBattleEvent>& SourceEvents,
		TArray<FNUServerBattleEvent>& OutEvents,
		FString& OutError)
	{
		TArray<FNUServerBattleEvent> ParsedEvents;
		ParsedEvents.Reserve(SourceEvents.Num());

		for (const FGrpcRevGameBattleEvent& Source : SourceEvents)
		{
			FNUServerBattleEvent Event;

			if (!NUBattleServer::ConvertEvent(Source, Event, OutError))
			{
				return false;
			}

			// 배열 순서는 서버의 연출 순서이므로 그대로 보존함
			ParsedEvents.Add(MoveTemp(Event));
		}

		OutEvents = MoveTemp(ParsedEvents);
		OutError.Reset();

		return true;
	}

	// 종류마다 채우는 칸이 달라서, 공용 로그 줄에 없는 값만 뒤에 덧붙임
	// 포획은 Amount 대신 성공 여부·흔들림이, 상태이상은 번호·스택·남은 턴이 그 자리를 대신함
	// 포획은 bSuccess 만으론 왜 실패했는지 못 가리므로 흔들림 배열까지 같이 찍어야 shakes 가 비었는지 확인 가능
	FString FormatEventSuffix(const FNUServerBattleEvent& Event)
	{
		if (Event.Type == ENUBattleEventType::StatusApplied
			|| Event.Type == ENUBattleEventType::StatusStack
			|| Event.Type == ENUBattleEventType::StatusRemoved)
		{
			return FString::Printf(TEXT(" Status=%d Stacks=%d RemainingTurns=%d"),
				Event.StatusId, Event.Stacks, Event.RemainingTurns);
		}

		if (Event.Type == ENUBattleEventType::SelfCost)
		{
			return FString::Printf(TEXT(" HPAfter=%d"), Event.HealthAfter);
		}

		// 새 수치가 Amount 와 Stacks 중 어느 칸에 실리는지 로그로 가릴 수 있게 둘 다 찍음
		if (Event.Type == ENUBattleEventType::ResourceChanged)
		{
			return FString::Printf(TEXT(" Resource=%d Stacks=%d Max=%d RemainingTurns=%d"),
				Event.ResourceId, Event.Stacks, Event.ResourceMax, Event.RemainingTurns);
		}

		if (Event.Type == ENUBattleEventType::JobStateChanged)
		{
			return FString::Printf(TEXT(" State=%s Start=%d End=%d Resource=%d Open=%s"),
				*Event.StateName.ToString(), Event.StartTurn, Event.EndTurn, Event.ResourceId,
				Event.bSuccess ? TEXT("true") : TEXT("false"));
		}

		if (Event.Type == ENUBattleEventType::Damage && Event.ResourceId > 0)
		{
			return FString::Printf(TEXT(" Resource=%d"), Event.ResourceId);
		}

		if (Event.Type != ENUBattleEventType::Capture)
		{
			return FString();
		}

		FString Shakes;
		for (int32 Index = 0; Index < Event.CaptureShakes.Num(); ++Index)
		{
			if (Index > 0)
			{
				Shakes += TEXT(",");
			}
			Shakes += Event.CaptureShakes[Index] ? TEXT("O") : TEXT("X");
		}

		return FString::Printf(TEXT(" Success=%s Shakes=[%s]"),
			Event.bSuccess ? TEXT("true") : TEXT("false"),
			*Shakes);
	}
}


void UNUBattleNetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// UEmberServerManager 부터 초기화 보장
	Collection.InitializeDependency<UEmberServerManager>();
	
	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.AddDynamic(this, &ThisClass::HandleServerStateChanged);
	}

#if !UE_BUILD_SHIPPING
	RegisterDebugCommands();
#endif
}

void UNUBattleNetSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
	UnregisterDebugCommands();
#endif

	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleServerStateChanged);
	}
	
	ResetBattleState();
	
	Super::Deinitialize();
}

void UNUBattleNetSubsystem::RequestBattleStart(int32 NaiaId, int32 EncounterKind, int64 SymbolId)
{
	// 나이아 id가 잘못됐거나 이미 전투중이라면 반환
	if (NaiaId <= 0)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("전투 시작 실패 : 잘못된 NaiaId [%d]"), NaiaId);
		return;
	}
	
	if (IsInBattle())
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("전투 시작 실패 : 이미 전투 진행중 [%lld]"), BattleId);
		return;
	}
	
	UEmberServerManager* Server = GetLoggedInServer(TEXT("전투 시작"));
	if (!Server)
	{
		return;
	}
	
	// 서버 응답 전 일반, 퀘스트 전투의 중복 시작을 막는다.
	if (!TryReserveBattleStart())
	{
		return;
	}

	UE_LOG(LogNUBattleNet, Log, TEXT("전투 시작 요청 NaiaId=%d EncounterKind=%d SymbolId=%lld"),
		NaiaId, EncounterKind, SymbolId);

	Server->SendBattleStart(NaiaId, EncounterKind, SymbolId);
}

bool UNUBattleNetSubsystem::TryReserveBattleStart()
{
	UWorld* World = GetGameInstance()->GetWorld();
	UNUBattleNetStageSubsystem* Stage = World ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ANUPlayerState* Player = PC ? PC->GetPlayerState<ANUPlayerState>() : nullptr;
	if (World == nullptr || IsInBattle() || bBattleStartPending || (Stage && Stage->IsTransitionActive())
		|| (Player && Player->GetBattleEncounter() != nullptr) || GetLoggedInServer(TEXT("전투 시작")) == nullptr)
	{
		return false;
	}

	// 일반, 퀘스트 시작 요청이 같은 대기 상태를 사용한다.
	bBattleStartPending = true;
	bBattleStartAcknowledged = false;
	bBattleStartSyncPending = false;
	BattleStartWorld = World;
	return true;
}

void UNUBattleNetSubsystem::ClearBattleStartPending(bool bClearStage)
{
	if (bClearStage && BattleStartWorld.IsValid())
	{
		if (UNUBattleNetStageSubsystem* Stage = BattleStartWorld->GetSubsystem<UNUBattleNetStageSubsystem>())
		{
			Stage->SetPendingStageLevel(TSoftObjectPtr<UWorld>());
		}
	}
	BattleStartWorld.Reset();
	bBattleStartPending = false;
	bBattleStartAcknowledged = false;
	bBattleStartSyncPending = false;
}

void UNUBattleNetSubsystem::SubmitCommand(FNUServerBattleCommandRequest Command)
{
	UEmberServerManager* Server = GetActiveBattleServer(TEXT("전투 명령 제출"));
	if (!Server)
	{
		return;
	}
	
	// 호출부에선 BattleId를 몰라도 됨
	Command.BattleId = BattleId;

	// 응답이 늦게 와서 이미 다음 턴이 됐는지 나중에 가릴 수 있도록 보낸 시점의 턴을 기억함
	PendingCommandTurn = Turn;
	LastCommandRequest = Command;

	Server->SendBattleCommand(Command);
}

void UNUBattleNetSubsystem::SubmitReaction(FNUServerBattleReactionRequest Reaction)
{
	UEmberServerManager* Server = GetActiveBattleServer(TEXT("전투 반응 제출"));
	if (!Server)
	{
		return;
	}
	
	Reaction.BattleId = BattleId;
	
	Server->SendBattleReaction(Reaction);
}

void UNUBattleNetSubsystem::DiscardReaction(int32 ReactionId)
{
	PendingReactionWindows.RemoveAll([ReactionId](const FNUServerBattleReactionWindow& Window)
	{
		return Window.ReactionId == ReactionId;
	});
}

void UNUBattleNetSubsystem::ReportReaction(int32 ReactionId, bool bSuccess, int32 PressTimeMs)
{
	const int32 Index = PendingReactionWindows.IndexOfByPredicate(
		[ReactionId](const FNUServerBattleReactionWindow& Window)
		{
			return Window.ReactionId == ReactionId;
		});

	if (!PendingReactionWindows.IsValidIndex(Index))
	{
		// 이미 보고했거나 지난 턴의 창, 다시 보내면 서버가 1915로 거절함
		UE_LOG(LogNUBattleNet, Warning, TEXT("보고할 반응 창이 없음 Id=%d"), ReactionId);
		return;
	}

	FNUServerBattleReactionRequest Request;
	Request.CombatantId = PendingReactionWindows[Index].DefenderId;
	Request.ReactionId = ReactionId;
	Request.bSuccess = bSuccess;
	Request.PressTimeMs = PressTimeMs;

	// 미러의 SP 는 여기서 건드리지 않음
	// 패링 소모는 회복으로 사라지는 값이므로 표시(재생 큐)에서만 선반영함

	// 전송 전에 삭제, 보내고 삭제하면 응답을 기다리는 사이 같은 창을 두 번 보고할 수 음
	PendingReactionWindows.RemoveAt(Index);

	UE_LOG(LogNUBattleNet, Log, TEXT("반응 보고 Id=%d 전투원=%d 성공=%s 경과=%dms"),
		ReactionId,
		Request.CombatantId,
		bSuccess ? TEXT("true") : TEXT("false"),
		PressTimeMs);

	SubmitReaction(Request);
}

void UNUBattleNetSubsystem::RequestNaiaSwap(int32 PartySlot)
{
	// 0번은 활성화상태, 1~3번 슬롯만 교체해야 함
	if (PartySlot < 1 || PartySlot > 3)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("나이아 교체 실패 : 잘못된 파티 슬롯 [%d]"), PartySlot);
		return;
	}

	UEmberServerManager* Server = GetActiveBattleServer(TEXT("나이아 교체"));
	if (!Server)
	{
		return;
	}
	
	Server->SendBattleNaiaSwap(BattleId, PartySlot);
}

void UNUBattleNetSubsystem::RequestSync()
{
	UEmberServerManager* Server = GetLoggedInServer(TEXT("전투 동기화"));
	if (!Server)
	{
		return;
	}
		
	if (bBattleStartPending)
	{
		// 승인 전 조회는 아직 처리되지 않은 시작 요청을 전투 없음으로 오판할 수 있다.
		if (!bBattleStartAcknowledged || bBattleStartSyncPending)
		{
			return;
		}
		bBattleStartSyncPending = true;
	}
	// 0이면 서버가 전투Id를 찾으므로 0도 유효함
	Server->SendBattleSync(BattleId);
}

void UNUBattleNetSubsystem::RequestLeave()
{
	UEmberServerManager* Server = GetActiveBattleServer(TEXT("전투 이탈"));
	if (!Server)
	{
		return;
	}
	
	Server->SendBattleLeave(BattleId);
}

void UNUBattleNetSubsystem::NotifyActionPlaybackFinished(int32 InTurn)
{
	// 전투가 이미 끝났으면(SVR_Battle_End로 BattleId가 0이 됨) 보내지 않음 - 서버가 1901로 거절함
	UEmberServerManager* Server = GetActiveBattleServer(TEXT("행동 연출 종료 보고"));
	if (!Server)
	{
		return;
	}

	Server->SendBattleActionAck(InTurn);
}

void UNUBattleNetSubsystem::HandleStarted(const FGrpcRevGameSVR_Battle_Started& Message)
{
	if (Message.BattleId <= 0 || Message.Turn < 0)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("SVR_Battle_Started 값이 유효하지 않음"));
		return;
	}
	
	if (IsInBattle() && BattleId != Message.BattleId)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("다른 전투의 Started 폐기 [현재=%lld, 수신=%lld]"),
			BattleId, Message.BattleId);
		return;
	}
	
	TMap<int32, FNUServerCombatant> ParsedCombatants;
	
	for (const FGrpcRevGameBattleCombatant& Source : Message.Combatants)
	{
		FNUServerCombatant Combatant;
		FString Error;
		
		if (!NUBattleServer::ConvertCombatant(Source, Combatant,Error))
		{
			UE_LOG(LogNUBattleNet, Warning, TEXT("전투원 변환 실패: %s"), *Error);
			return;
		}
		
		if (ParsedCombatants.Contains(Combatant.CombatantId))
		{
			UE_LOG(LogNUBattleNet, Warning, TEXT("중복 CombatantId: %d"), Combatant.CombatantId);
			return;
		}
		
		ParsedCombatants.Add(Combatant.CombatantId, MoveTemp(Combatant));
	}
	
	// 전투에 들어가기 전에 가려야 함, 들어간 뒤의 Started 는 나이아 교체 · 재동기화
	const bool bJoinedPartyBattle = !IsInBattle() && IsJoinedPartyBattle(Message);

	ClearBattleStartPending(false);
	BattleId = Message.BattleId;
	Turn = Message.Turn;
	LastTimeoutMs = 0;

	// 지난 전투의 보상이 이번 결과창에 남지 않도록 여기서 비워야 함
	LastReward = FNUServerBattleReward();
	bHasLastReward = false;
	bLastRewardApplied = false;
	
	Combatants = MoveTemp(ParsedCombatants);
	PendingCombatantIds.Reset();
	LastOrder.Reset();
	LastAck = FNUServerCommandAck();
	
	// 로그 메세지 블록
	{
		UE_LOG(LogNUBattleNet, Log, TEXT("서버 전투 시작 BattleId=%lld, Turn=%d, 전투원=%d, EncounterKind=%d, SymbolId=%lld, 시작=%lld"),
			BattleId, Turn, Combatants.Num(), Message.EncounterKind, Message.SymbolId, Message.InitiatorCharacterIdx);
		
		for (const TPair<int32, FNUServerCombatant>& Pair : Combatants)
		{
			const FNUServerCombatant& Combatant = Pair.Value;

			UE_LOG(LogNUBattleNet, Log,
				TEXT("전투원 Id=%d Side=%d Kind=%d Owner=%lld NaiaId=%d Instance=%lld HP=%d/%d 액티브=%d 패시브=%d 자원=%d 상태=%d 종족=%d 직업=%d"),
				Combatant.CombatantId,
				Combatant.Side,
				Combatant.Kind,
				Combatant.OwnerCharacterIdx,
				Combatant.NaiaId,
				Combatant.NaiaInstanceId,
				Combatant.Health,
				Combatant.MaxHealth,
				Combatant.ActiveSkillIds.Num(),
				Combatant.PassiveSkillIds.Num(),
				Combatant.Resources.Num(),
				Combatant.JobStates.Num(),
				Combatant.RaceId,
				Combatant.JobId);
		}
	}

	// 배경을 무대보다 먼저 정해야 하므로 상태 변화보다 먼저 알림
	if (bJoinedPartyBattle)
	{
		OnPartyBattleJoined.Broadcast(Message.SymbolId);
	}

	// 나이아 교체·재동기화도 이 스냅샷으로 오므로 전투 중에 다시 도착할 수 있음
	// 전투원 번호가 바뀌었을 수 있다는 것을 상태 변화보다 먼저 알림
	OnCombatantsReplaced.Broadcast();

	OnBattleStateChanged.Broadcast();
}

void UNUBattleNetSubsystem::HandleTurnBegin(const FGrpcRevGameSVR_Battle_TurnBegin& Message)
{
	if (!IsCurrentBattleMessage(Message.BattleId, TEXT("SVR_Battle_TurnBegin")))
	{
		return;
	}
	
	if (Message.Turn < 0 || Message.TimeoutMs < 0)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("TurnBegin 값이 유효하지 않음"));
		return;
	}
	
	Turn = Message.Turn;
	LastTimeoutMs = Message.TimeoutMs;
	PendingCombatantIds = Message.PendingCombatantIds;
	LastOrder.Reset();

	// 새 턴이 시작되면 ActionSeq 가 1부터 다시 매겨지므로 지난 턴의 창은 여기서 버림
	// 남겨두면 다음 턴 1번 행동의 창으로 잘못 뽑혀 엉뚱한 타격에 바가 뜰 수 있음
	PendingReactionWindows.Reset();

	// SP 순서 기록도 같은 이유로 비움, 안 비우면 새 턴 1번 행동이 지난 턴 마지막 행동보다
	// 작은 번호라 통째로 무시됨
	LastSPActionSeq.Reset();
	
	UE_LOG(LogNUBattleNet, Log, TEXT("턴 시작 Turn=%d, Timeout=%dms, Pending=%d"),
		Turn, LastTimeoutMs, PendingCombatantIds.Num());
	
	OnBattleStateChanged.Broadcast();
}

void UNUBattleNetSubsystem::HandleSubmit(const FGrpcRevGameSVR_Battle_Submit& Message)
{
	
	if (!IsCurrentBattleMessage(Message.BattleId, TEXT("SVR_Battle_Submit")))
	{
		return;
	}
	
	PendingCombatantIds = Message.PendingCombatantIds;
	
	UE_LOG(LogNUBattleNet, Log, TEXT("명령 제출 상태 Submitted=%d, Pending=%d"),
		Message.SubmittedCombatantIds.Num(), PendingCombatantIds.Num());
	
	OnBattleStateChanged.Broadcast();
}

void UNUBattleNetSubsystem::HandleOrder(const FGrpcRevGameSVR_Battle_Order& Message)
{
	if (!IsCurrentBattleMessage(Message.BattleId, TEXT("SVR_Battle_Order")))
	{
		return;
	}
	
	if (Message.Turn < 0)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("Turn이 유효하지 않음"));
		return;
	}
	
	Turn = Message.Turn;
	LastTimeoutMs = 0;
	PendingCombatantIds.Reset();
	LastOrder = Message.CombatantIds;	// 실행 순서 그대로
	
	UE_LOG(LogNUBattleNet, Log, TEXT("행동 순서 확정 Turn=%d, Count=%d"), Turn, LastOrder.Num());
	
	OnBattleStateChanged.Broadcast();
}

void UNUBattleNetSubsystem::HandleReaction(const FGrpcRevGameSVR_Battle_Reaction& Message)
{
	if (!IsCurrentBattleMessage(Message.BattleId, TEXT("SVR_Battle_Reaction")))
	{
		return;
	}

	FNUServerBattleReactionWindow Window;
	FString Error;

	if (!NUBattleServer::ConvertReactionWindow(Message, Window, Error))
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("반응 창 변환 실패: %s"), *Error);
		return;
	}

	// 같은 창이 두 번 오면 뒤에있는 창으로 덮어쓰기 (재동기화하면 미해결 창이 다시 옴)
	const int32 ExistingIndex = PendingReactionWindows.IndexOfByPredicate(
		[&Window](const FNUServerBattleReactionWindow& Other)
		{
			return Other.ReactionId == Window.ReactionId;
		});

	if (PendingReactionWindows.IsValidIndex(ExistingIndex))
	{
		PendingReactionWindows[ExistingIndex] = Window;
	}
	else
	{
		PendingReactionWindows.Add(Window);
	}

	UE_LOG(LogNUBattleNet, Log,
		TEXT("반응 창 Id=%d Seq=%d Hit=%d Attacker=%d Defender=%d Skill=%d Parryable=%s ")
		TEXT("MustReport=%s Duration=%d Success=%d~%d SP=%d"),
		Window.ReactionId,
		Window.ActionSeq,
		Window.HitIndex,
		Window.AttackerId,
		Window.DefenderId,
		Window.SkillId,
		Window.bParryable ? TEXT("true") : TEXT("false"),
		Window.bMustReport ? TEXT("true") : TEXT("false"),
		Window.ChallengeDurationMs,
		Window.SuccessStartMs,
		Window.SuccessEndMs,
		Window.SpCost);

	OnBattleReactionReceived.Broadcast(Window);
}

void UNUBattleNetSubsystem::CollectReactionWindows(int32 ActionSeq,
	TArray<FNUServerBattleReactionWindow>& OutWindows) const
{
	OutWindows.Reset();

	for (const FNUServerBattleReactionWindow& Window : PendingReactionWindows)
	{
		if (Window.ActionSeq == ActionSeq)
		{
			OutWindows.Add(Window);
		}
	}

	// 타격 순서대로 열려야 함, 서버가 보낸 순서를 믿지 않고 HitIndex 로 정렬한다
	OutWindows.Sort([](const FNUServerBattleReactionWindow& A, const FNUServerBattleReactionWindow& B)
	{
		return A.HitIndex < B.HitIndex;
	});
}

bool UNUBattleNetSubsystem::HasPendingReactionWindow() const
{
	return !PendingReactionWindows.IsEmpty();
}
void UNUBattleNetSubsystem::HandleAction(const FGrpcRevGameSVR_Battle_Action& Message)
{
	if (!IsCurrentBattleMessage(Message.BattleId, TEXT("SVR_Battle_Action")))
	{
		return;
	}
	
	TArray<FNUServerBattleEvent> Events;
	FString Error;
	
	if (!ConvertBattleEvents(Message.Events, Events, Error))
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("BattleAction 이벤트 변환 실패: %s"), *Error);
		return;
	}

	UE_LOG(LogNUBattleNet, Log, TEXT("행동 수신 Actor=%d Events=%d"), Message.ActorId, Events.Num());

	for (const FNUServerBattleEvent& Event : Events)
	{
		// 서버 배열 순서대로 적용
		ApplyEventToSnapshot(Event, Message.ActionSeq);

		UE_LOG(LogNUBattleNet, Log,
			TEXT("Event=%s Raw=%d Actor=%d Target=%d Amount=%d%s"),
			*UEnum::GetValueAsString(Event.Type),
			Event.RawType,
			Event.ActorId,
			Event.TargetId,
			Event.Amount,
			*FormatEventSuffix(Event));
	}

	OnBattleActionReceived.Broadcast(Message.ActorId, Message.ActionSeq, Events);
	OnBattleStateChanged.Broadcast();
}

void UNUBattleNetSubsystem::HandleTurnEnd(const FGrpcRevGameSVR_Battle_TurnEnd& Message)
{
	if (!IsCurrentBattleMessage(Message.BattleId, TEXT("SVR_Battle_TurnEnd")))
	{
		return;
	}

	if (Message.Turn < 0)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("TurnEnd의 Turn이 유효하지 않음"));
		return;
	}

	TArray<FNUServerBattleEvent> Events;
	FString Error;

	if (!ConvertBattleEvents(Message.Events, Events, Error))
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("TurnEnd 이벤트 변환 실패: %s"), *Error);
		return;
	}

	Turn = Message.Turn;
	LastTimeoutMs = 0;
	PendingCombatantIds.Reset();
	LastOrder.Reset();

	for (const FNUServerBattleEvent& Event : Events)
	{
		// 턴 종료 묶음(도트·만료)은 그 턴의 모든 행동 뒤다
		ApplyEventToSnapshot(Event, TNumericLimits<int32>::Max());

		UE_LOG(LogNUBattleNet, Log,
			TEXT("TurnEnd Event=%s Raw=%d Actor=%d Target=%d Amount=%d%s"),
			*UEnum::GetValueAsString(Event.Type),
			Event.RawType,
			Event.ActorId,
			Event.TargetId,
			Event.Amount,
			*FormatEventSuffix(Event));
	}

	OnBattleTurnEndReceived.Broadcast(Turn, Events);
	OnBattleStateChanged.Broadcast();
}

void UNUBattleNetSubsystem::HandleEnd(const FGrpcRevGameSVR_Battle_End& Message, const FString& ResponseData)
{
	if (!IsCurrentBattleMessage(Message.BattleId, TEXT("SVR_Battle_End")))
	{
		return;
	}

	// 승리가 아니면 보상이 비어 오므로 실패가 아니라 없는 것으로 다룸
	FString RewardError;
	bHasLastReward = NUBattleRewardServer::Parse(ResponseData, LastReward, RewardError);
	if (bHasLastReward)
	{
		UE_LOG(LogNUBattleNet, Log,
			TEXT("전투 보상 경험치=%lld 골드=%lld 레벨업=%s 아이템=%d"),
			LastReward.Experience,
			LastReward.Gold,
			LastReward.bHasGrowth && LastReward.Growth.IsLevelUp() ? TEXT("true") : TEXT("false"),
			LastReward.Items.Num());
	}
	else if (!RewardError.IsEmpty())
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("전투 보상을 읽지 못함: %s"), *RewardError);
	}
	
	TMap<int32, FNUServerCombatant> FinalCombatants;

	for (const FGrpcRevGameBattleCombatant& Source : Message.Combatants)
	{
		FNUServerCombatant Combatant;
		FString Error;

		if (!NUBattleServer::ConvertCombatant(Source, Combatant, Error))
		{
			UE_LOG(LogNUBattleNet, Warning, TEXT("전투 종료 스냅샷 변환 실패: %s"), *Error);
			OnBattleEnded.Broadcast(Message.Reason);
			ResetBattleState();
			return;
		}

		if (FinalCombatants.Contains(Combatant.CombatantId))
		{
			UE_LOG(LogNUBattleNet, Warning, TEXT("전투 종료 스냅샷에 중복 CombatantId [%d]"), Combatant.CombatantId);
			OnBattleEnded.Broadcast(Message.Reason);
			ResetBattleState();
			return;
		}

		FinalCombatants.Add(Combatant.CombatantId, MoveTemp(Combatant));
	}

	Combatants = MoveTemp(FinalCombatants);
	LastTimeoutMs = 0;
	PendingCombatantIds.Reset();
	LastOrder.Reset();

	UE_LOG(LogNUBattleNet, Log,
		TEXT("서버 전투 종료 BattleId=%lld Reason=%d Combatants=%d EncounterKind=%d SymbolId=%lld"),
		BattleId,
		Message.Reason,
		Combatants.Num(),
		Message.EncounterKind,
		Message.SymbolId);

	// 마지막 스냅샷을 보내기 위해 초기화 전에 broadcast
	OnBattleStateChanged.Broadcast();
	OnBattleEnded.Broadcast(Message.Reason);

	ResetBattleState();
}

void UNUBattleNetSubsystem::HandleCommandAck(int32 ResultCode, const FString& ResponseData)
{
	const ENUBattleCommandSubmitResult Result = NUBattleServer::MapCommandSubmitResult(ResultCode);
	
	if (Result != ENUBattleCommandSubmitResult::Accepted)
	{
		UE_LOG(LogNUBattleNet, Warning,
			TEXT("전투 명령 거절 ResultCode=%d(%s), Result=%d [Combatant=%d Action=%d Skill=%d Target=%d]"),
			ResultCode, NUBattleServer::DescribeCommandResult(ResultCode), static_cast<int32>(Result),
			LastCommandRequest.CombatantId, LastCommandRequest.Action, LastCommandRequest.SkillId,
			LastCommandRequest.TargetId);
		
		// 거절 응답엔 전투원 번호가 없음, 보낸 쪽이 PendingSubmissionCombatantId로 기억해야 함
		OnCommandAck.Broadcast(Result, FNUServerCommandAck());
		return;
	}
	
	FNUServerCommandAck ParsedAck;
	FString Error;
	
	if (!NUBattleServer::ParseCommandAck(ResponseData, ParsedAck, Error))
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("BattleCommand 응답 파싱 실패: %s"), *Error);
		
		// 못 읽었어도 요청 자체는 끝난 것이므로 거절로 알려 대기를 풀어줘야 함
		OnCommandAck.Broadcast(ENUBattleCommandSubmitResult::Rejected, FNUServerCommandAck());
		return;
	}
	
	if (!IsCurrentBattleMessage(ParsedAck.BattleId, TEXT("BattleCommand ACK")))
	{
		// 다른 전투의 늦은 응답, 지금 대기 중인 제출과 무관하므로 알리지 않음
		return;
	}

	// 직접 응답이 SVR_Battle_Order 등 푸시 스트림보다 늦게 도착하면, 이미 다음 턴이 시작된
	// 뒤에 옛 턴의 Pending으로 새 턴 상태를 덮어쓰게 됨 - 이 응답 자체를 통째로 버려야 함
	if (PendingCommandTurn != INDEX_NONE && PendingCommandTurn != Turn)
	{
		UE_LOG(LogNUBattleNet, Warning,
			TEXT("늦게 도착한 BattleCommand 응답 폐기 [보낸 턴=%d, 현재 턴=%d, Combatant=%d]"),
			PendingCommandTurn, Turn, ParsedAck.CombatantId);

		PendingCommandTurn = INDEX_NONE;
		return;
	}

	PendingCommandTurn = INDEX_NONE;

	LastAck = MoveTemp(ParsedAck);
	PendingCombatantIds = LastAck.PendingCombatantIds;

	// 서버에서 명령 제출 시점에 SP를 차감하므로 즉시 반영
	if (FNUServerCombatant* Combatant = Combatants.Find(LastAck.CombatantId))
	{
		Combatant->SP = LastAck.RemainingSP;
	}

	UE_LOG(LogNUBattleNet, Log,
		TEXT("전투 명령 승인 Combatant=%d, SP=%d, Hit=%d, Target=%d, Pending=%d"),
		LastAck.CombatantId,
		LastAck.SpentSP,
		LastAck.HitCount,
		LastAck.TargetId,
		PendingCombatantIds.Num());

	OnBattleStateChanged.Broadcast();
	OnCommandAck.Broadcast(Result, LastAck);
}

void UNUBattleNetSubsystem::HandleAck(int32 ResultCode, const TCHAR* RequestName)
{
	if (bBattleStartPending && FCString::Strcmp(RequestName, TEXT("BattleStart")) == 0)
	{
		bBattleStartAcknowledged = ResultCode == 200;
		if (ResultCode == 200)
		{
			// 조우음은 받아 준 뒤에 --- 요청만 하고 내면 거절된 조우(이미 잡힌 심볼 등)에도 소리가 난다
			UNUSFXSubsystem::Play(this, NU::SFX::Field::Encounter);
		}
		else
		{
			ClearBattleStartPending(true);
			OnBattleStateChanged.Broadcast();
		}
	}
	else if (bBattleStartPending && bBattleStartSyncPending && FCString::Strcmp(RequestName, TEXT("BattleSync")) == 0)
	{
		// 승인된 시작 요청 이후 서버가 전투 없음을 확인한 경우에만 재시작을 허용한다.
		if (ResultCode == 1901)
		{
			ClearBattleStartPending(true);
			OnBattleStateChanged.Broadcast();
		}
		else
		{
			bBattleStartSyncPending = false;
		}
	}

	if (ResultCode == 200)
	{
		UE_LOG(LogNUBattleNet, Log, TEXT("%s 요청 승인"), RequestName);
		return;
	}

	UE_LOG(LogNUBattleNet, Warning, TEXT("%s 요청 거절 [ResultCode=%d]"), RequestName, ResultCode);
}

bool UNUBattleNetSubsystem::IsInBattle() const
{
	return BattleId > 0;
}

int64 UNUBattleNetSubsystem::GetBattleId() const
{
	return BattleId;
}

int32 UNUBattleNetSubsystem::GetTurn() const
{
	return Turn;
}

int32 UNUBattleNetSubsystem::GetLastTimeoutMs() const
{
	return LastTimeoutMs;
}

const TMap<int32, FNUServerCombatant>& UNUBattleNetSubsystem::GetCombatants() const
{
	return Combatants;
}

const TArray<int32>& UNUBattleNetSubsystem::GetPendingCombatantIds() const
{
	return PendingCombatantIds;
}

const TArray<int32>& UNUBattleNetSubsystem::GetLastOrder() const
{
	return LastOrder;
}

const FNUServerCommandAck& UNUBattleNetSubsystem::GetLastAck() const
{
	return LastAck;
}

bool UNUBattleNetSubsystem::HasLastReward() const
{
	return bHasLastReward;
}

const FNUServerBattleReward& UNUBattleNetSubsystem::GetLastReward() const
{
	return LastReward;
}

bool UNUBattleNetSubsystem::IsLastRewardApplied() const
{
	return bLastRewardApplied;
}

void UNUBattleNetSubsystem::MarkLastRewardApplied()
{
	bLastRewardApplied = true;
}

const FNUServerCombatant* UNUBattleNetSubsystem::FindCombatant(int32 CombatantId) const
{
	return Combatants.Find(CombatantId);
}

void UNUBattleNetSubsystem::HandleServerStateChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn || (!IsInBattle() && !bBattleStartPending))
	{
		return;
	}
	
	UE_LOG(LogNUBattleNet, Warning, TEXT("서버 연결 종료로 전투 상태 초기화 [BattleId=%lld]"), BattleId);
	
	ResetBattleState();
}

UEmberServerManager* UNUBattleNetSubsystem::GetServer() const
{
	UGameInstance* GameInstance = GetGameInstance();
	
	return GameInstance ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
}

void UNUBattleNetSubsystem::ResetBattleState()
{
	ClearBattleStartPending(true);
	BattleId = 0;
	Turn = 0;
	LastTimeoutMs = 0;

	Combatants.Reset();
	LastSPActionSeq.Reset();
	PendingCombatantIds.Reset();
	LastOrder.Reset();

	LastAck = FNUServerCommandAck();
	PendingCommandTurn = INDEX_NONE;

	// 전투가 끝나면 미해결 창도 같이 버려야 함, 다음 전투의 창 번호와 섞이면 안 됨
	PendingReactionWindows.Reset();

	OnBattleStateChanged.Broadcast();
}

bool UNUBattleNetSubsystem::IsCurrentBattleMessage(int64 IncomingBattleId, const TCHAR* MessageName) const
{
	if (IncomingBattleId <= 0)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("%s 폐기 : 잘못된 BattleId [%lld]"), MessageName, IncomingBattleId);
		return false;
	}
	
	if (!IsInBattle())
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("%s 폐기: 현재 서버 전투가 없음 [수신=%lld]"), MessageName, IncomingBattleId);
		return false;
	}
	
	if (IncomingBattleId != BattleId)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("%s 폐기: 다른 전투의 늦은 메시지 [현재=%lld, 수신=%lld]"),
			MessageName, BattleId, IncomingBattleId);
		return false;
	}
	
	return true;
}

bool UNUBattleNetSubsystem::IsJoinedPartyBattle(const FGrpcRevGameSVR_Battle_Started& Message) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUCharacterManager* Characters = GameInstance ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;
	const int64 MyIdx = IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;

	// 내가 시작했거나 일반 · 퀘스트 전투면 요청한 쪽 경로가 배경과 심볼을 이미 정함
	if (Message.EncounterKind != 1 || Message.InitiatorCharacterIdx == 0 || Message.InitiatorCharacterIdx == MyIdx)
	{
		return false;
	}

	// 같은 파티 · 같은 공간은 서버가 가림, 클라는 어긋나도 서버가 넣은 전투를 따르고 기록만 남김
	const UNUPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UNUPartySubsystem>() : nullptr;
	const FString* InitiatorName = IsValid(Party) ? Party->GetParty().FindMemberName(Message.InitiatorCharacterIdx) : nullptr;

	if (InitiatorName == nullptr)
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("파티원이 아닌 캐릭터(%lld)가 시작한 심볼 전투에 들어감 SymbolId=%lld"),
			Message.InitiatorCharacterIdx, Message.SymbolId);
	}
	else
	{
		UE_LOG(LogNUBattleNet, Log, TEXT("파티원 %s 이(가) 시작한 심볼 전투에 같이 들어감 SymbolId=%lld"),
			**InitiatorName, Message.SymbolId);
	}

	return true;
}

// SP 절대값은 도착 순서가 아니라 ActionSeq 순서로 반영해야 함
// 반응 조각(10 Reaction)은 클라 보고를 받고 만들어져 그 턴의 다른 행동들보다 늦게 도착하는데
// 싣고 온 sp_after 는 자기 ActionSeq 시점의 값 
// 도착 순서대로 덮으면 "적이 먼저 때리고 내가 나중에 행동" 하는 턴에서 내 행동의 SP 전량 회복이 되돌아감
bool UNUBattleNetSubsystem::TryApplyMirrorSP(int32 CombatantId, int32 SPAfter, int32 ActionSeq)
{
	FNUServerCombatant* Combatant = Combatants.Find(CombatantId);
	if (Combatant == nullptr)
	{
		return false;
	}

	int32& LastSeq = LastSPActionSeq.FindOrAdd(CombatantId, INDEX_NONE);
	if (ActionSeq < LastSeq)
	{
		return false;
	}

	LastSeq = ActionSeq;
	Combatant->SP = FMath::Max(0, SPAfter);

	return true;
}

void UNUBattleNetSubsystem::ApplyEventToSnapshot(const FNUServerBattleEvent& Event, int32 ActionSeq)
{
	switch (Event.Type)
	{
	case ENUBattleEventType::Damage:
	case ENUBattleEventType::Heal:
		if (FNUServerCombatant* Target = Combatants.Find(Event.TargetId))
		{
			Target->Health = FMath::Max(0, Event.HealthAfter);
		}
		break;
		
	case ENUBattleEventType::Death:
		if (FNUServerCombatant* Target = Combatants.Find(Event.TargetId))
		{
			Target->Health = 0;
		}
		break;
		
	case ENUBattleEventType::SPChanged:
		TryApplyMirrorSP(Event.ActorId, Event.SPAfter, ActionSeq);
		break;

	case ENUBattleEventType::Reaction:
		// 체력은 환급량만 더함, 절대값(hp_after)은 행동 시점 기준이라 덮으면 값이 튄다
		if (FNUServerCombatant* Actor = Combatants.Find(Event.ActorId))
		{
			Actor->Health = FMath::Clamp(Actor->Health + Event.Amount, 0, Actor->MaxHealth);
		}

		// SP는 서버 확정값을 받아야 함
		// 안 받으면 패링 비용이 미러에 안 남고, 재생이 끝날 때 RefreshLatestVitals가
		// 표시를 미러로 다시 만들면서 깎였던 SP가 되돌아옴
		TryApplyMirrorSP(Event.ActorId, Event.SPAfter, ActionSeq);
		break;
		
	case ENUBattleEventType::StatusApplied:
	case ENUBattleEventType::StatusStack:
	case ENUBattleEventType::StatusRemoved:
		// 다음 전체 스냅샷이 올 때까지 미러가 어긋나지 않게 여기서 맞춤
		// 화면에 아이콘을 그리는 것은 재생 큐가 연출 시점에 따로 함, 여기는 권위 값만 따라감
		if (FNUServerCombatant* Target = Combatants.Find(Event.TargetId))
		{
			NUBattleServer::ApplyStatusEvent(Event, Target->Statuses);
		}
		break;

	case ENUBattleEventType::SelfCost:
		// 스킬이 시전자 자신의 HP 를 낸 것, 맞은 게 아니라 체력만 따라감
		// 미러에 안 넣으면 재생이 끝날 때 RefreshLatestVitals 가 표시를 미러로 다시 만들면서 깎였던 HP 가 되돌아옴
		if (Event.TargetId != INDEX_NONE && Event.TargetId != Event.ActorId)
		{
			UE_LOG(LogNUBattleNet, Warning, TEXT("SelfCost 의 대상이 시전자가 아님 Actor=%d Target=%d"),
				Event.ActorId, Event.TargetId);
		}
		else if (FNUServerCombatant* Caster = Combatants.Find(Event.ActorId))
		{
			Caster->Health = FMath::Max(0, Event.HealthAfter);
		}
		break;

	case ENUBattleEventType::ResourceChanged:
		if (FNUServerCombatant* Owner = Combatants.Find(NUBattleServer::ResolveResourceEventOwner(Event)))
		{
			NUBattleServer::ApplyResourceEvent(Event, Owner->Resources);
		}
		break;

	case ENUBattleEventType::JobStateChanged:
		if (FNUServerCombatant* Owner = Combatants.Find(NUBattleServer::ResolveResourceEventOwner(Event)))
		{
			NUBattleServer::ApplyJobStateEvent(Event, Owner->JobStates);
		}
		break;
		
	default:
		break;
	}
}

UEmberServerManager* UNUBattleNetSubsystem::GetLoggedInServer(const TCHAR* Operation) const
{
	UEmberServerManager* Server = GetServer();

	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("%s 실패: 서버 미접속"), Operation);
		return nullptr;
	}

	return Server;
}

UEmberServerManager* UNUBattleNetSubsystem::GetActiveBattleServer(const TCHAR* Operation) const
{
	if (!IsInBattle())
	{
		UE_LOG(LogNUBattleNet, Warning, TEXT("%s 실패: 진행 중인 서버 전투가 없음"), Operation);
		return nullptr;
	}

	return GetLoggedInServer(Operation);
}


// -------------------
//   콘솔 디버그 명령
// -------------------
#if !UE_BUILD_SHIPPING
void UNUBattleNetSubsystem::RegisterDebugCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.Start"),
		TEXT("전투 참가를 요청합니다. 사용법: nu.BattleNet.Start <NaiaId>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogNUBattleNet, Warning, TEXT("사용법: nu.BattleNet.Start <NaiaId>"));
				return;
			}

			RequestBattleStart(FCString::Atoi(*Args[0]));
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.Sync"),
		TEXT("현재 전투 상태 동기화를 요청합니다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			RequestSync();
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.Command"),
		TEXT("전투 명령을 제출합니다. 사용법: nu.BattleNet.Command <combatant> <action> <skill> <target> <sp> [item]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 5)
			{
				UE_LOG(LogNUBattleNet, Warning,
					TEXT("사용법: nu.BattleNet.Command <combatant> <action> <skill> <target> <sp> [item]"));
				return;
			}

			FNUServerBattleCommandRequest Request;
			Request.CombatantId = FCString::Atoi(*Args[0]);
			Request.Action = FCString::Atoi(*Args[1]);
			Request.SkillId = FCString::Atoi(*Args[2]);
			Request.TargetId = FCString::Atoi(*Args[3]);
			Request.InvestSP = FCString::Atoi(*Args[4]);
			Request.ItemId = Args.Num() >= 6 ? FCString::Atoi(*Args[5]) : 0;
			SubmitCommand(Request);
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.Leave"),
		TEXT("현재 전투 이탈을 요청합니다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			RequestLeave();
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.Dump"),
		TEXT("클라이언트가 보관 중인 서버 전투 상태를 출력합니다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			DumpDebugState();
		}),
		ECVF_Default));
}

void UNUBattleNetSubsystem::UnregisterDebugCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();
	TArray<IConsoleObject*> StillRegistered;
	Console.ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda(
			[&StillRegistered](const TCHAR*, IConsoleObject* Object)
			{
				StillRegistered.Add(Object);
			}),
		TEXT("nu.BattleNet."));

	for (IConsoleCommand* Command : DebugCommands)
	{
		if (Command && StillRegistered.Contains(Command))
		{
			Console.UnregisterConsoleObject(Command);
		}
	}

	DebugCommands.Reset();
}

void UNUBattleNetSubsystem::DumpDebugState() const
{
	auto JoinIds = [](const TArray<int32>& Ids)
	{
		FString Result;
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(", ");
			}

			Result += FString::FromInt(Ids[Index]);
		}

		return Result;
	};

	UE_LOG(LogNUBattleNet, Log, TEXT("========== BattleNet 상태 =========="));
	UE_LOG(LogNUBattleNet, Log,
		TEXT("BattleId=%lld Turn=%d TimeoutMs=%d Combatants=%d"),
		BattleId, Turn, LastTimeoutMs, Combatants.Num());
	UE_LOG(LogNUBattleNet, Log, TEXT("Pending=[%s]"), *JoinIds(PendingCombatantIds));
	UE_LOG(LogNUBattleNet, Log, TEXT("LastOrder=[%s]"), *JoinIds(LastOrder));

	for (const TPair<int32, FNUServerCombatant>& Pair : Combatants)
	{
		const FNUServerCombatant& Combatant = Pair.Value;

		// 아이콘이 안 뜰 때 값이 안 온 것인지 그리는 쪽 문제인지 갈라야 해서 같이 찍음
		const FString StatusText = Combatant.Statuses.IsEmpty()
			? FString(TEXT("없음"))
			: FString::JoinBy(Combatant.Statuses, TEXT(", "),
				[](const FNUServerBattleStatus& Status)
				{
					return FString::Printf(TEXT("%d(스택 %d, 남은 턴 %d)"),
						Status.StatusId, Status.Stacks, Status.RemainingTurns);
				});

		UE_LOG(LogNUBattleNet, Log,
			TEXT("Combatant=%d Side=%d Kind=%d Owner=%lld Naia=%d Instance=%lld Name='%s' HP=%d/%d SP=%d/%d 상태이상=[%s]"),
			Combatant.CombatantId,
			Combatant.Side,
			Combatant.Kind,
			Combatant.OwnerCharacterIdx,
			Combatant.NaiaId,
			Combatant.NaiaInstanceId,
			*Combatant.Name,
			Combatant.Health,
			Combatant.MaxHealth,
			Combatant.SP,
			Combatant.MaxSP,
			*StatusText);
	}

	if (LastAck.IsValid())
	{
		UE_LOG(LogNUBattleNet, Log,
			TEXT("LastAck: BattleId=%lld Combatant=%d SpentSP=%d HitCount=%d Target=%d RemainingSP=%d"),
			LastAck.BattleId,
			LastAck.CombatantId,
			LastAck.SpentSP,
			LastAck.HitCount,
			LastAck.TargetId,
			LastAck.RemainingSP);
	}

	UE_LOG(LogNUBattleNet, Log, TEXT("====================================="));
}
#endif
