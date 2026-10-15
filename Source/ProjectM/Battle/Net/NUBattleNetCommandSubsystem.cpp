// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleNetCommandSubsystem.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "NUBattleNetSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleNetCommand, Log, All);

namespace
{
	// 콘솔에서 행동 이름을 받기 위한 변환, action 번호는 NUBattleServer::MapRootActionToServerAction이 맡음
	bool ParseRootActionArg(const FString& Arg, ENUBattleRootAction& OutAction, bool& bOutCommuneItem)
	{
		if (Arg.Equals(TEXT("attack"), ESearchCase::IgnoreCase))
		{
			OutAction = ENUBattleRootAction::Attack;
			bOutCommuneItem = false;
			return true;
		}

		if (Arg.Equals(TEXT("skill"), ESearchCase::IgnoreCase))
		{
			OutAction = ENUBattleRootAction::Skill;
			bOutCommuneItem = false;
			return true;
		}

		if (Arg.Equals(TEXT("flee"), ESearchCase::IgnoreCase))
		{
			OutAction = ENUBattleRootAction::Flee;
			bOutCommuneItem = false;
			return true;
		}

		if (Arg.Equals(TEXT("capture"), ESearchCase::IgnoreCase))
		{
			OutAction = ENUBattleRootAction::Item;
			bOutCommuneItem = true;
			return true;
		}

		if (Arg.Equals(TEXT("naiaswap"), ESearchCase::IgnoreCase))
		{
			OutAction = ENUBattleRootAction::NaiaSwap;
			bOutCommuneItem = false;
			return true;
		}

		return false;
	}

	FString JoinIds(const TArray<int32>& Ids)
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
	}
}


void UNUBattleNetCommandSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// UNUBattleNetSubsystem 부터 초기화 보장
	Collection.InitializeDependency<UNUBattleNetSubsystem>();

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleBattleStateChanged);
		BattleNet->OnCommandAck.AddUObject(this, &ThisClass::HandleCommandAckReceived);
	}

	// 디버그 명령 추가
#if !UE_BUILD_SHIPPING
	RegisterDebugCommands();
#endif
}

void UNUBattleNetCommandSubsystem::Deinitialize()
{
	// 디버그 명령 제거
#if !UE_BUILD_SHIPPING
	UnregisterDebugCommands();
#endif

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnBattleStateChanged.RemoveAll(this);
		BattleNet->OnCommandAck.RemoveAll(this);
	}

	Super::Deinitialize();
}

int32 UNUBattleNetCommandSubsystem::GetCurrentCombatantId() const
{
	// 응답 대기중엔 아무도 현재 전투원이 아님, 같은 전투원에게 두 번 명령하는 걸 막음
	if (PendingSubmissionCombatantId != INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return ControllableCombatantIds.IsEmpty() ? INDEX_NONE : ControllableCombatantIds[0];
}

const TArray<int32>& UNUBattleNetCommandSubsystem::GetControllableCombatantIds() const
{
	return ControllableCombatantIds;
}

ENUBattleNetCommandPhase UNUBattleNetCommandSubsystem::GetPhase() const
{
	return Phase;
}

bool UNUBattleNetCommandSubsystem::SubmitCurrentCommand(ENUBattleRootAction RootAction, int32 SkillId,
	int32 TargetId, int32 InvestSP, int32 ItemId, bool bCommuneItem)
{
	UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return false;
	}

	const int32 CombatantId = GetCurrentCombatantId();
	if (CombatantId == INDEX_NONE)
	{
		UE_LOG(LogNUBattleNetCommand, Warning, TEXT("지금 명령을 낼 전투원이 없음"));
		return false;
	}

	int32 ServerAction = 0;
	if (!NUBattleServer::MapRootActionToServerAction(RootAction, bCommuneItem, ServerAction))
	{
		UE_LOG(LogNUBattleNetCommand, Warning,
			TEXT("서버로 보낼 수 없는 행동 RootAction=%d bCommuneItem=%s"),
			static_cast<int32>(RootAction), bCommuneItem ? TEXT("true") : TEXT("false"));
		return false;
	}

	FNUServerBattleCommandRequest Request;
	Request.CombatantId = CombatantId;
	Request.Action = ServerAction;
	Request.SkillId = SkillId;
	Request.TargetId = TargetId;
	Request.InvestSP = InvestSP;
	Request.ItemId = ItemId;

	// 응답이 오기 전까진 현재 전투원을 다시 안 내주도록 먼저 잠금
	PendingSubmissionCombatantId = CombatantId;
	Phase = ENUBattleNetCommandPhase::Submitting;

	BattleNet->SubmitCommand(Request);

	return true;
}

UNUBattleNetSubsystem* UNUBattleNetCommandSubsystem::GetBattleNet() const
{
	UGameInstance* GameInstance = GetGameInstance();

	return GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
}

void UNUBattleNetCommandSubsystem::HandleBattleStateChanged()
{
	RebuildControllableList();
}

void UNUBattleNetCommandSubsystem::HandleCommandAckReceived(ENUBattleCommandSubmitResult Result,
	const FNUServerCommandAck& Ack)
{
	if (PendingSubmissionCombatantId == INDEX_NONE)
	{
		// 이 서브시스템을 거치지 않고 나간 명령(nu.BattleNet.Command 등)의 응답, 관여하지 않음
		return;
	}

	const int32 SubmittedCombatantId = PendingSubmissionCombatantId;
	PendingSubmissionCombatantId = INDEX_NONE;

	if (Result != ENUBattleCommandSubmitResult::Accepted)
	{
		// 거절 응답엔 전투원 번호가 없으므로 보내기 전에 기억해둔 값을 씀
		RebuildControllableList();
		OnCommandRejected.Broadcast(SubmittedCombatantId, Result);
		return;
	}

	if (Ack.CombatantId != INDEX_NONE && Ack.CombatantId != SubmittedCombatantId)
	{
		UE_LOG(LogNUBattleNetCommand, Warning, TEXT("보낸 전투원(%d)과 승인된 전투원(%d)이 다름"), 
			SubmittedCombatantId, Ack.CombatantId);
	}

	RebuildControllableList();
}

void UNUBattleNetCommandSubsystem::RebuildControllableList()
{
	UNUBattleNetSubsystem* BattleNet = GetBattleNet();

	if (!IsValid(BattleNet) || !BattleNet->IsInBattle())
	{
		ResetSelection();
		return;
	}

	const int32 CurrentTurn = BattleNet->GetTurn();

	// 새 턴이면 이전 제출 대기를 무조건 비움, 턴 경계는 서버가 반드시 보내주므로
	// 직접 응답이 완전히 유실되어도 한 턴만 막힘
	if (CurrentTurn != SelectionTurn)
	{
		SelectionTurn = CurrentTurn;
		PendingSubmissionCombatantId = INDEX_NONE;
	}

	TArray<int32> MyCombatants;
	for (const TPair<int32, FNUServerCombatant>& Pair : BattleNet->GetCombatants())
	{
		if (IsMyCombatant(Pair.Value))
		{
			MyCombatants.Add(Pair.Key);
		}
	}

	// 캐릭터(Kind 0) -> 나이아(Kind 1) 순, CombatantId 번호 순서에 기대지 않음
	MyCombatants.Sort([BattleNet](int32 A, int32 B)
	{
		const FNUServerCombatant* CombatantA = BattleNet->FindCombatant(A);
		const FNUServerCombatant* CombatantB = BattleNet->FindCombatant(B);
		const int32 KindA = CombatantA ? CombatantA->Kind : 0;
		const int32 KindB = CombatantB ? CombatantB->Kind : 0;

		if (KindA != KindB)
		{
			return KindA < KindB;
		}

		return A < B;
	});

	const TArray<int32>& Pending = BattleNet->GetPendingCombatantIds();

	// 서버가 제출 상태·행동 순서 푸시를 직접 응답보다 먼저 보내므로 제출할 때마다 여기로 옴
	// 직접 응답이 정말 유실돼도 같은 자리로 오고 처리도 같아서 둘을 구분하지 않음
	// 뒤이어 도착하는 직접 응답이 미러의 RemainingSP 를 맞추므로 여기서 끝내도 값은 어긋나지 않음
	if (PendingSubmissionCombatantId != INDEX_NONE && !Pending.Contains(PendingSubmissionCombatantId))
	{
		UE_LOG(LogNUBattleNetCommand, Log,
			TEXT("전투원 %d 푸시가 직접 응답보다 먼저 도착, 승인으로 간주하고 진행"),
			PendingSubmissionCombatantId);
		PendingSubmissionCombatantId = INDEX_NONE;
	}

	MyCombatants.RemoveAll([&Pending](int32 Id)
	{
		return !Pending.Contains(Id);
	});

	ControllableCombatantIds = MoveTemp(MyCombatants);

	if (PendingSubmissionCombatantId != INDEX_NONE)
	{
		Phase = ENUBattleNetCommandPhase::Submitting;
	}
	else if (ControllableCombatantIds.IsEmpty())
	{
		Phase = ENUBattleNetCommandPhase::WaitingForOthers;
	}
	else
	{
		Phase = ENUBattleNetCommandPhase::Selecting;
	}

	NotifyCurrentCombatantIfChanged();
}

void UNUBattleNetCommandSubsystem::ResetSelection()
{
	ControllableCombatantIds.Reset();
	PendingSubmissionCombatantId = INDEX_NONE;
	SelectionTurn = INDEX_NONE;
	Phase = ENUBattleNetCommandPhase::Inactive;
	bWarnedMissingOwner = false;

	NotifyCurrentCombatantIfChanged();
}

void UNUBattleNetCommandSubsystem::NotifyCurrentCombatantIfChanged()
{
	const int32 Current = GetCurrentCombatantId();

	if (Current == LastNotifiedCombatantId)
	{
		return;
	}

	LastNotifiedCombatantId = Current;
	OnCommandTurnChanged.Broadcast(Current);
}

int64 UNUBattleNetCommandSubsystem::GetMyCharacterIdx() const
{
	UGameInstance* GameInstance = GetGameInstance();
	const UNUCharacterManager* Characters = GameInstance ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;

	return IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;
}

bool UNUBattleNetCommandSubsystem::IsMyCombatant(const FNUServerCombatant& Combatant) const
{
	if (Combatant.Side != 0 || !Combatant.IsAlive())
	{
		return false;
	}

	if (Combatant.OwnerCharacterIdx == 0)
	{
#if !UE_BUILD_SHIPPING
		if (bAssumeOwnerless)
		{
			return true;
		}
#endif
		if (!bWarnedMissingOwner)
		{
			bWarnedMissingOwner = true;
			UE_LOG(LogNUBattleNetCommand, Warning,
				TEXT("아군 전투원 %d의 OwnerCharacterIdx가 0, 서버가 채우지 않음"), Combatant.CombatantId);
		}

		return false;
	}

	const int64 MyIdx = GetMyCharacterIdx();
	return MyIdx != 0 && Combatant.OwnerCharacterIdx == MyIdx;
}


// -------------------
//   콘솔 디버그 명령
// -------------------
#if !UE_BUILD_SHIPPING
void UNUBattleNetCommandSubsystem::RegisterDebugCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.My"),
		TEXT("내 전투원 명령 순서 상태를 출력합니다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			DumpMyState();
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.Next"),
		TEXT("현재 전투원에게 명령을 제출합니다. 사용법: nu.BattleNet.Next <attack|skill|flee|capture|naiaswap> <skill> <target> <sp> [item]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 4)
			{
				UE_LOG(LogNUBattleNetCommand, Warning,
					TEXT("사용법: nu.BattleNet.Next <attack|skill|flee|capture|naiaswap> <skill> <target> <sp> [item]"));
				return;
			}

			ENUBattleRootAction RootAction;
			bool bCommuneItem = false;
			if (!ParseRootActionArg(Args[0], RootAction, bCommuneItem))
			{
				UE_LOG(LogNUBattleNetCommand, Warning, TEXT("알 수 없는 행동 [%s]"), *Args[0]);
				return;
			}

			const int32 SkillId = FCString::Atoi(*Args[1]);
			const int32 TargetId = FCString::Atoi(*Args[2]);
			const int32 InvestSP = FCString::Atoi(*Args[3]);
			const int32 ItemId = Args.Num() >= 5 ? FCString::Atoi(*Args[4]) : 0;

			SubmitCurrentCommand(RootAction, SkillId, TargetId, InvestSP, ItemId, bCommuneItem);
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.Auto"),
		TEXT("현재 전투원에게 기본 공격(대상은 서버가 고름)을 제출합니다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			SubmitCurrentCommand(ENUBattleRootAction::Attack, 0, -1, 0, 0, false);
		}),
		ECVF_Default));

	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.BattleNet.AssumeOwnerless"),
		TEXT("서버가 OwnerCharacterIdx를 안 채웠을 때 Side만으로 내 전투원 취급합니다(서버 버그 우회). 사용법: nu.BattleNet.AssumeOwnerless <0|1>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogNUBattleNetCommand, Warning, TEXT("사용법: nu.BattleNet.AssumeOwnerless <0|1>"));
				return;
			}

			bAssumeOwnerless = FCString::Atoi(*Args[0]) != 0;
			UE_LOG(LogNUBattleNetCommand, Log, TEXT("AssumeOwnerless = %s"),
				bAssumeOwnerless ? TEXT("true") : TEXT("false"));

			RebuildControllableList();
		}),
		ECVF_Default));
}

void UNUBattleNetCommandSubsystem::UnregisterDebugCommands()
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

void UNUBattleNetCommandSubsystem::DumpMyState() const
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();

	UE_LOG(LogNUBattleNetCommand, Log, TEXT("========== BattleNetCommand 상태 =========="));
	UE_LOG(LogNUBattleNetCommand, Log, TEXT("MyCharacterIdx=%lld"), GetMyCharacterIdx());
	UE_LOG(LogNUBattleNetCommand, Log, TEXT("Phase=%s Controllable=[%s] Current=%d"),
		*UEnum::GetValueAsString(Phase),
		*JoinIds(ControllableCombatantIds),
		GetCurrentCombatantId());
	UE_LOG(LogNUBattleNetCommand, Log, TEXT("PendingSubmission=%d SelectionTurn=%d"),
		PendingSubmissionCombatantId, SelectionTurn);

	if (IsValid(BattleNet))
	{
		UE_LOG(LogNUBattleNetCommand, Log, TEXT("ServerPending=[%s]"), *JoinIds(BattleNet->GetPendingCombatantIds()));
	}

	UE_LOG(LogNUBattleNetCommand, Log, TEXT("============================================"));
}
#endif
