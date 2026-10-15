// Fill out your copyright notice in the Description page of Project Settings.

#include "NUBattleNetActionQueueSubsystem.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Capture/NUCaptureMath.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUActiveSkillContext.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Battle/Data/NUSkillVFXProfile.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Formation/NUBattleFormationMath.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Battle/Presentation/NUBattleActionPresentation.h"
#include "Battle/UI/NUBattleResultTypes.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Inventory/NUInventoryComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleNetActionQueue, Log, All);


namespace
{
	// 서버 BattleCombatant.kind - 0 캐릭터 · 1 나이아 · 2 적
	constexpr int32 NetCombatantKindCharacter = 0;

	// 나이아 회피율은 아직 없으므로 기본값 40퍼
	constexpr float NetNaiaDodgeChance = 0.4f;

	// 서버가 회피 창을 아직 한 번도 안 보내서 회피 경로를 시험할 방법이 없음
	// 서버가 보내기 시작하면 지울 것
	TAutoConsoleVariable<bool> CVarForceDodgeWindow(
		TEXT("nu.Battle.Debug.ForceDodgeWindow"),
		false,
		TEXT("서버가 보낸 패링 창을 회피 창으로 바꿔 연다. 넷 회피 경로 시험용."));
}

void UNUBattleNetActionQueueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UNUBattleNetStageSubsystem>();

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	BattleNet = IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	StageSubsystem = IsValid(World) ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;

	if (IsValid(BattleNet))
	{
		BattleNet->OnCombatantsReplaced.AddUObject(this, &ThisClass::HandleCombatantsReplaced);
		BattleNet->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleBattleStateChanged);
		BattleNet->OnBattleActionReceived.AddUObject(this, &ThisClass::HandleActionReceived);
		BattleNet->OnBattleTurnEndReceived.AddUObject(this, &ThisClass::HandleTurnEndReceived);
		BattleNet->OnBattleEnded.AddUObject(this, &ThisClass::HandleBattleEnded);
	}

	if (IsValid(StageSubsystem))
	{
		StageSubsystem->SetVitalsAutoRefreshSuppressed(true);
	}

	HandleBattleStateChanged();
}

void UNUBattleNetActionQueueSubsystem::Deinitialize()
{
	if (IsValid(BattleNet))
	{
		BattleNet->OnCombatantsReplaced.RemoveAll(this);
		BattleNet->OnBattleStateChanged.RemoveAll(this);
		BattleNet->OnBattleActionReceived.RemoveAll(this);
		BattleNet->OnBattleTurnEndReceived.RemoveAll(this);
		BattleNet->OnBattleEnded.RemoveAll(this);
	}

	ResetQueue();

	if (IsValid(StageSubsystem))
	{
		StageSubsystem->SetVitalsAutoRefreshSuppressed(false);
	}

	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->DestroyNetBattleHUD();
	}

	BattleNet = nullptr;
	StageSubsystem = nullptr;

	Super::Deinitialize();
}

void UNUBattleNetActionQueueSubsystem::HandleBattleStateChanged()
{
	if (!IsValid(BattleNet) || !BattleNet->IsInBattle())
	{
		return;
	}
	
	if (IsValid(StageSubsystem))
	{
		StageSubsystem->SetVitalsAutoRefreshSuppressed(true);
	}
	
	if (CombatantSnapshots.IsEmpty())
	{
		InitializeCombatantSnapshots();
	}

	// 턴이 바뀌면 상태의 대기 → 켜짐 · 남은 턴이 이벤트 없이 바뀜, 재생 중이면 그 턴 이벤트가 다 나간 뒤에 그림
	if (!IsPlaybackBusy() && BattleNet->GetTurn() != JobViewTurn)
	{
		JobViewTurn = BattleNet->GetTurn();
		PushAllJobResourceViews();
	}
	
	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->EnsureNetBattleHUD();
	}

	// 여기서 RefreshLatestVitals를 불러 미러 값으로 덮으면 안 됨
	// 단계에서 오는 TurnBegin/Submit/Order 같은 상태 변화가 재생 큐엔 아직 아무 행동도 없을 때
	// 끼어들 수 있는데, 이때 SubmitPendingCommand가 제출 즉시 밀어둔 예측 SP가 아직 미러에
	// 반영 안 된 경우(응답보다 Order 푸시가 먼저 오는 경우) 그 예측을 안 깎인 값으로
	// 덮어써버림 - 실행 순서상 마지막 제출자(나이아)가 정확히 이 경합 창에 걸림
	// 정말 다 재생돼서 비는 시점의 정리는 TryPlayNext의 RefreshLatestVitals 호출로 충분함
}

void UNUBattleNetActionQueueSubsystem::HandleCombatantsReplaced()
{
	if (!IsValid(BattleNet) || !BattleNet->IsInBattle())
	{
		return;
	}

	// 교체된 전투원은 명령 수집 단계에서만 생기므로 재생 중인 행동이 없음
	// 표시값도 서버 스냅샷이 최신이라 그대로 다시 깔면 됨
	InitializeCombatantSnapshots();

	// 전투 시작(SVR_Battle_Started)이 이 통지를 상태 변화보다 먼저 쏘므로 여기가 전투의 첫 지점임
	// 상태 변화 쪽에 두면 스냅샷이 이미 채워져 있어 아무 일도 일어나지 않음
	EnsureCharacterStatSnapshot();
}

void UNUBattleNetActionQueueSubsystem::EnsureCharacterStatSnapshot() const
{
	UNUProgressionComponent* Progression = GetLocalProgression();
	if (!IsValid(Progression) || Progression->HasCharacterStatSnapshot())
	{
		return;
	}

	Progression->RequestCharacterStats();
}

void UNUBattleNetActionQueueSubsystem::HandleActionReceived(int32 ActorId, int32 ActionSeq,
	const TArray<FNUServerBattleEvent>& Events)
{
	FNUNetQueuedAction Action;
	Action.ActorId = ActorId;
	Action.ActionSeq = ActionSeq;
	Action.Events = Events;

	// 같은 action_seq 가 또 오면 새 행동이 아니라 이어지는 조각
	// 전진·모션을 다시 재생하면 같은 공격이 두 번 나가므로 이벤트만 반영해야 함
	// 이어지는 조각은 항상 Reaction 으로 시작함
	Action.bMotionless = !Events.IsEmpty()
		&& Events[0].Type == ENUBattleEventType::Reaction;

	// 조각은 자기 행동(같은 ActionSeq) 바로 뒤에 끼워야 함
	// 반응 보고를 받고 만들어져서 뒤따라 오는데, 그 사이에 다음 행동들이 이미 큐에 들어와 있다
	// 큐 맨 뒤에 붙이면 늦게 재생되고, 조각이 싣고 온 hp_after·sp_after 는 그 행동 시점의
	// 값이라 그 뒤에 지나간 변화(내 턴의 SP 전량 회복 등)를 되돌려 버림
	int32 InsertIndex = 0;
	while (PendingActions.IsValidIndex(InsertIndex)
		&& PendingActions[InsertIndex].ActionSeq <= ActionSeq)
	{
		++InsertIndex;
	}
	PendingActions.Insert(MoveTemp(Action), InsertIndex);

	if (PendingActions[InsertIndex].bMotionless)
	{
		UE_LOG(LogNUBattleNetActionQueue, Log,
			TEXT("이어지는 조각 Actor=%d Seq=%d Events=%d 재생 순서=%d"),
			ActorId, ActionSeq, Events.Num(), InsertIndex);
	}

	TryPlayNext();
}

void UNUBattleNetActionQueueSubsystem::HandleTurnEndReceived(int32 Turn, const TArray<FNUServerBattleEvent>& Events)
{
	TMap<int32, int32> GroupIndexByActor;
	TArray<FNUNetQueuedAction> GroupedActions;

	for (const FNUServerBattleEvent& Event : Events)
	{
		int32* ExistingIndex = GroupIndexByActor.Find(Event.ActorId);
		if (!ExistingIndex)
		{
			const int32 NewIndex = GroupedActions.AddDefaulted();
			FNUNetQueuedAction& NewAction = GroupedActions[NewIndex];
			NewAction.ActorId = Event.ActorId;
			NewAction.bMotionless = true;
			GroupIndexByActor.Add(Event.ActorId, NewIndex);
			ExistingIndex = GroupIndexByActor.Find(Event.ActorId);
		}

		GroupedActions[*ExistingIndex].Events.Add(Event);
	}

	for (FNUNetQueuedAction& Action : GroupedActions)
	{
		PendingActions.Add(MoveTemp(Action));
	}

	UE_LOG(LogNUBattleNetActionQueue, Verbose,
		TEXT("턴 종료 이벤트 큐 등록 Turn=%d Groups=%d Events=%d"),
		Turn, GroupedActions.Num(), Events.Num());

	// 이 턴 것까지 재생을 마치면 이 turn 번호로 BattleActionAck를 보내야 함
	// 도트가 하나도 없는 턴이어도 보고는 해야 하므로 항상 기록해둠
	PendingAckTurn = Turn;

	TryPlayNext();
}

void UNUBattleNetActionQueueSubsystem::HandleBattleEnded(int32 Reason)
{
	// 서버는 처치 행동과 전투 종료를 거의 같이 보낸다
	// 여기서 바로 정리하면 마지막 타격 연출이 재생되기도 전에 행동 큐와 팝업 레이어가 사라짐
	if (IsPlaybackBusy())
	{
		bEndPending = true;
		PendingEndReason = Reason;

		UE_LOG(LogNUBattleNetActionQueue, Log,
			TEXT("전투 종료 통지를 재생 뒤로 미룸 Reason=%d, 남은 행동=%d"),
			Reason, PendingActions.Num());
		return;
	}

	FinishBattlePlayback(Reason);
}

void UNUBattleNetActionQueueSubsystem::FinishBattlePlayback(int32 Reason)
{
	const int32 DroppedActions = PendingActions.Num();

	// 서버가 지급한 보상을 소지금·성장 표시에 반영함, 결과창 숫자와는 별개로 필요함
	ApplyServerBattleRewards();

	// 큐를 비우기 전에 띄워야 함, 포획 기록과 전투원 사본이 아직 여기 있음
	ShowBattleResult(Reason);

	// 패배는 결과창이 없어 여기서 소리를 냄 (승리음은 결과창이 냄)
	if (Reason == static_cast<int32>(ENUBattleResult::Defeat))
	{
		UNUSFXSubsystem::Play(GetWorld(), NU::SFX::Battle::Defeat);
	}

	ResetQueue();

	if (IsValid(StageSubsystem))
	{
		StageSubsystem->SetVitalsAutoRefreshSuppressed(false);
	}

	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->DestroyNetBattleHUD();
	}

	UE_LOG(LogNUBattleNetActionQueue, Log,
		TEXT("넷 전투 행동 큐 정리 Reason=%d, 버린 행동=%d"),
		Reason, DroppedActions);

	ContinueTeardown();
}

void UNUBattleNetActionQueueSubsystem::ApplyServerBattleRewards()
{
	if (!IsValid(BattleNet) || !BattleNet->HasLastReward() || BattleNet->IsLastRewardApplied())
	{
		return;
	}

	const ANUPlayerController* PC = GetLocalPlayerController();
	ANUPlayerState* PlayerState = IsValid(PC) ? Cast<ANUPlayerState>(PC->PlayerState) : nullptr;
	if (!IsValid(PlayerState))
	{
		return;
	}

	const FNUServerBattleReward& Reward = BattleNet->GetLastReward();
	BattleNet->MarkLastRewardApplied();

	// ★ 전투 뒤에 소지금 잔액을 알려주는 푸시가 없음
	// 골드가 서버 값으로 맞춰지는 곳은 로그인(CharacterList)과 상점 응답뿐이라,
	// 여기서 안 더하면 다음 상점에 들를 때까지 화면의 소지금이 전투 전 값으로 남음
	if (Reward.Gold > 0)
	{
		if (UNUInventoryComponent* Inventory = PlayerState->GetInventoryComponent())
		{
			Inventory->AddGold(Reward.Gold);
		}
	}

	// 경험치·레벨은 서버가 확정했으므로 그 상태를 다시 받아 맞춤
	// 클라가 곡선을 다시 계산하면 서버와 어긋남, 퀘스트 완료도 같은 방식으로 재조회한다
	// 레벨이 오르면 파생 능력치도 바뀌는데 재조회가 적용될 때 캐릭터 스탯까지 같이 갱신됨
	// (UNUProgressionComponent::ApplyState 끝의 RefreshCharacterStatsAfterProgressionChange)
	if (Reward.Experience > 0 || Reward.bHasGrowth)
	{
		UNUProgressionComponent* Progression = PlayerState->GetProgressionComponent();
		if (!IsValid(Progression) || !Progression->RequestProgressionState())
		{
			UE_LOG(LogNUBattleNetActionQueue, Warning,
				TEXT("전투 보상 뒤 성장 상태 재조회 실패, 레벨·경험치 표시가 낡은 채로 남음"));
		}
	}

	UE_LOG(LogNUBattleNetActionQueue, Log,
		TEXT("전투 보상 반영 경험치=%lld 골드=%lld"), Reward.Experience, Reward.Gold);
}

void UNUBattleNetActionQueueSubsystem::ShowBattleResult(int32 Reason)
{
	// SVR_Battle_End.reason(1 승리·2 패배·3 도주)이 ENUBattleResult 값과 그대로 대응함
	const ENUBattleResult Result = Reason >= 1 && Reason <= 3
		? static_cast<ENUBattleResult>(Reason)
		: ENUBattleResult::None;

	// 보통은 승리로 끝났을 때만 띄움
	if (Result != ENUBattleResult::Victory && CapturedNaiaIds.IsEmpty())
	{
		return;
	}

	ANUPlayerController* PC = GetLocalPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	FNUBattleResultView ResultView;
	ResultView.Result = Result;

	// 보상은 승리일 때만 온다, 포획으로 끝나면 없어서 0 으로 뜸
	const FNUServerBattleReward* Reward = IsValid(BattleNet) && BattleNet->HasLastReward()
		? &BattleNet->GetLastReward()
		: nullptr;

	TArray<int32> RewardItemIds;
	TArray<int32> RewardItemCounts;
	if (Reward)
	{
		ResultView.Experience = Reward->Experience;
		ResultView.Gold = Reward->Gold;

		for (const FNUServerBattleRewardItem& Item : Reward->Items)
		{
			RewardItemIds.Add(Item.ItemId);
			RewardItemCounts.Add(Item.Count);
		}
	}

	NUBattleResult::BuildMembers(
		this,
		Reward && Reward->bHasGrowth ? &Reward->Growth : nullptr,
		ResultView.Members
		);

	NUBattleResult::BuildItemEntries(this, RewardItemIds, RewardItemCounts, ResultView.Items);
	NUBattleResult::BuildNaiaEntries(this, CapturedNaiaIds, ResultView.CapturedNaia);

	UE_LOG(LogNUBattleNetActionQueue, Log,
		TEXT("결과창 Reason=%d 경험치=%lld 골드=%lld 명단=%d 아이템=%d 포획=%d"),
		Reason,
		ResultView.Experience,
		ResultView.Gold,
		ResultView.Members.Num(),
		ResultView.Items.Num(),
		ResultView.CapturedNaia.Num());

	bResultScreenOpen = PC->ShowBattleResult(
		ResultView,
		FSimpleDelegate::CreateUObject(this, &ThisClass::NotifyResultDismissed));
}

void UNUBattleNetActionQueueSubsystem::NotifyResultDismissed()
{
	if (!bResultScreenOpen)
	{
		return;
	}

	bResultScreenOpen = false;

	// 창의 수명은 PlayerController 가 들고 있음
	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->CloseBattleResult();
	}

	ContinueTeardown();
}

void UNUBattleNetActionQueueSubsystem::ContinueTeardown()
{
	// 결과창을 확인할 때까지 무대를 남겨둔다, 여기서 넘기면 창 뒤에서 세트가 사라짐
	if (bResultScreenOpen)
	{
		return;
	}

	// 전투 종료를 이 액션을 기다리느라 미뤄뒀다면 지금 이어서 함
	if (IsValid(StageSubsystem))
	{
		StageSubsystem->NotifyActionPlaybackDrained();
	}
}

bool UNUBattleNetActionQueueSubsystem::IsPlaybackBusy() const
{
	return bIsPlaying || !PendingActions.IsEmpty();
}

bool UNUBattleNetActionQueueSubsystem::IsSideWiped() const
{
	bool bAnyAllyAlive = false;
	bool bAnyEnemyAlive = false;

	for (const TPair<int32, FNUNetCombatantSnapshot>& Pair : CombatantSnapshots)
	{
		if (Pair.Value.Health <= 0)
		{
			continue;
		}

		if (Pair.Value.Side == 0)
		{
			bAnyAllyAlive = true;
		}
		else
		{
			bAnyEnemyAlive = true;
		}
	}

	return !bAnyAllyAlive || !bAnyEnemyAlive;
}

void UNUBattleNetActionQueueSubsystem::TryPlayNext()
{
	if (bIsPlaying)
	{
		return;
	}

	// 종료 통지를 받은 뒤 한쪽이 전멸했다면 그 행동에서 전투가 끝난 것이므로 남은 행동은 버림
	// 같은 턴에 뒤이어 제출된 행동까지 서버가 보냈을 때 시체를 때리는 연출이 나오는 것을 막음
	if (bEndPending && IsSideWiped())
	{
		FinishBattlePlayback(PendingEndReason);
		return;
	}

	if (PendingActions.IsEmpty())
	{
		if (bEndPending)
		{
			FinishBattlePlayback(PendingEndReason);
			return;
		}

		RefreshLatestVitals();

		UE_LOG(LogNUBattleNetActionQueue, Log, TEXT("행동 재생 큐 비워짐"));

		// 턴 종료(도트·만료)까지 재생을 마친 경우에만 보고함 - 행동 하나가 끝나 큐가
		// 잠깐 비는 것(다음 행동자의 SVR_Battle_Action이 아직 도착 전)과는 구분해야 함
		// 아무 때나 보내면 서버가 기다리는 turn과 달라 거절되거나, 안 끝난 턴을 끝났다고 보고하게 됨
		if (PendingAckTurn != INDEX_NONE)
		{
			const int32 AckTurn = PendingAckTurn;
			PendingAckTurn = INDEX_NONE;

			if (IsValid(BattleNet))
			{
				BattleNet->NotifyActionPlaybackFinished(AckTurn);
			}
		}

		OnPlaybackDrained.Broadcast();
		return;
	}

	FNUNetQueuedAction Action = MoveTemp(PendingActions[0]);
	PendingActions.RemoveAt(0, EAllowShrinking::No);

	// 큐에 들어온 뒤에 행동자가 쓰러졌으면 그 행동은 재생하지 않음
	// 아바타를 숨겨도 연출은 위치만 있으면 그대로 터져서 시체 자리에서 공격 이펙트가 나옴
	// 로컬 전투도 ExecuteNextAction 에서 같은 검사를 함(IsAlive)
	// 턴 종료 묶음(도트·만료)은 모션도 연출도 없고 체력만 반영하므로 그대로 재생함
	const FNUNetCombatantSnapshot* ActorSnapshot = CombatantSnapshots.Find(Action.ActorId);
	if (!Action.bMotionless && ActorSnapshot && ActorSnapshot->Health <= 0)
	{
		UE_LOG(LogNUBattleNetActionQueue, Log,
			TEXT("행동자 %d 가 이미 쓰러져 행동 연출을 건너뜀"), Action.ActorId);

		TryPlayNext();
		return;
	}

	bIsPlaying = true;
	PlayAction(MoveTemp(Action));
}

void UNUBattleNetActionQueueSubsystem::PlayAction(FNUNetQueuedAction&& Action)
{
	CurrentAction = MoveTemp(Action);
	BuildHitSlots(CurrentAction.Events, ActiveHitSlots);
	NextHitSlotIndex = 0;

	const int32 HitCount = FMath::Max(1, ActiveHitSlots.Num());
	FNUBattleActionMotionSpec Spec = BuildMotionSpec(CurrentAction, HitCount);
	ActiveHitInterval = Spec.HitInterval;

	ShowActionBannerFor(CurrentAction, HitCount);

	if (IsValid(StageSubsystem))
	{
		if (ANUBattleAvatar* Avatar = StageSubsystem->FindAvatar(CurrentAction.ActorId))
		{
			// 행동 시간과 타격·시전 지점은 공격 모션이 있으면 그 길이와 모션의 Hit·Cast 표시
			// 아래 타격 / 반응 창 / 종료 예약이 이 값을 사용함
			// 로컬 전투(UNUBattleGameplayAbility::ActivateAbility)와 같은 규칙
			Avatar->ResolveMotionTiming(Spec);

			// 이어지는 조각 · 턴 종료 묶음은 이벤트만 반영함 (모션 · 연출이 없음)
			// 모션을 다시 틀면 아직 돌아오는 중인 아바타가 그 자리에서 행동 시간만큼 멈췄다가 제자리로 순간이동함
			if (!CurrentAction.bMotionless)
			{
				Avatar->MulticastPlayAction(Spec);
			}
		}
	}

	float FirstHitDelay = 0.f;
	float TotalDuration = 0.f;
	NUBattlePresentation::ComputeTimeline(Spec, FirstHitDelay, TotalDuration);

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		ResetQueue();
		return;
	}

	if (FirstHitDelay <= KINDA_SMALL_NUMBER)
	{
		PlayNextHitSlot();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			HitTimerHandle,
			this,
			&ThisClass::PlayNextHitSlot,
			FirstHitDelay,
			false
			);
	}

	World->GetTimerManager().SetTimer(
		FinishTimerHandle,
		this,
		&ThisClass::FinishCurrentAction,
		FMath::Max(TotalDuration, KINDA_SMALL_NUMBER),
		false
		);

	// 반응 창은 타격 시각을 기준으로 열려야 하므로 타격 예약과 같은 값을 넘김
	ScheduleReactionWindows(FirstHitDelay, Spec.HitInterval);
}

void UNUBattleNetActionQueueSubsystem::PlayNextHitSlot()
{
	if (!ActiveHitSlots.IsValidIndex(NextHitSlotIndex))
	{
		return;
	}

	for (const FNUServerBattleEvent& Event : ActiveHitSlots[NextHitSlotIndex])
	{
		ApplyEvent(Event);
	}

	++NextHitSlotIndex;
	if (!ActiveHitSlots.IsValidIndex(NextHitSlotIndex))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		HitTimerHandle,
		this,
		&ThisClass::PlayNextHitSlot,
		FMath::Max(ActiveHitInterval, KINDA_SMALL_NUMBER),
		false
		);
}

void UNUBattleNetActionQueueSubsystem::FinishCurrentAction()
{
	// 보고해야 서버가 그 타격에서 풀리는 창(must_report)이 남아 있으면 행동을 끝내지 않음
	// 포획 흔들림과 똑같이 bIsPlaying 이 유지되어 다음 행동·ack·세트 철거가 같이 미뤄짐
	if (HasUnreportedMustReport())
	{
		if (UWorld* World = GetWorld())
		{
			// 창이 닫히는 시각을 큐가 따로 계산하지 않고 짧게 되물어본다
			World->GetTimerManager().SetTimer(
				FinishTimerHandle,
				this,
				&ThisClass::FinishCurrentAction,
				0.1f,
				false
				);

			return;
		}
	}

	// 포획 흔들림이 남아 있으면 행동을 끝내지 않고 기다린다
	// 다음 행동도, 턴 종료 보고(BattleActionAck)도 그만큼 미뤄진다 —
	// 보고는 별도 패킷이고 제한 시간이 그때부터 도는 규약이라 늦어도 손해가 없다
	// bIsPlaying 이 유지되므로 무대 철거·명령 메뉴도 같은 시각까지 자동으로 미뤄진다
	const float ShakeRemaining = IsValid(StageSubsystem)
		? StageSubsystem->GetCaptureShakeRemainingSeconds()
		: 0.f;

	if (ShakeRemaining > KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				FinishTimerHandle,
				this,
				&ThisClass::FinishCurrentAction,
				ShakeRemaining,
				false
				);

			return;
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}

	// 타격이 끝내 안 온 방어자의 반응 결과는 여기서라도 띄움
	FlushReactionPopups();
	ClearReactionWindows();

	CurrentAction = FNUNetQueuedAction();
	ActiveHitSlots.Reset();
	NextHitSlotIndex = 0;
	ActiveHitInterval = 0.f;
	bIsPlaying = false;

	TryPlayNext();
}

void UNUBattleNetActionQueueSubsystem::ScheduleReactionWindows(float FirstHitDelay, float HitInterval)
{
	ClearReactionWindows();

	// 이어지는 조각은 앞 조각에서 이미 창을 열었으므로 다시 열면 안됨
	if (CurrentAction.bMotionless || CurrentAction.ActionSeq <= 0 || !IsValid(BattleNet))
	{
		return;
	}

	TArray<FNUServerBattleReactionWindow> Windows;
	BattleNet->CollectReactionWindows(CurrentAction.ActionSeq, Windows);
	if (Windows.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	ReactionActionStartTime = World->GetTimeSeconds();

	// 같은 타격의 창은 한 묶음, HitIndex 의 시작 번호를 모르므로 값이 아니라 순서로 셈
	int32 LastHitIndex = MIN_int32;
	for (const FNUServerBattleReactionWindow& Window : Windows)
	{
		if (Window.HitIndex != LastHitIndex)
		{
			LastHitIndex = Window.HitIndex;

			// 서버가 아직 회피 창(parryable=false)을 한 번도 보내지 않아서 켜고 시험하는 스위치
			// 이 묶음의 값이 아래 모든 분기를 가르므로 여기 한 곳에서만 뒤집으면 됨
			const bool bParryable = Window.bParryable
				&& !CVarForceDodgeWindow.GetValueOnGameThread();

			FNUNetReactionGroup& NewGroup = ReactionGroups.AddDefaulted_GetRef();
			NewGroup.bParryable = bParryable;

			// 도전 구간이 타격 시각에 끝나도록 앞구간 길이만큼 당겨서 열음
			// 이 값이 음수면 모션이 도전 구간보다 짧다는 뜻이라 창이 잘림
			// 회피 창은 성공 구간이 없어 앞구간이 0 으로 읽히므로 창 길이 전체를 당겨야 함,
			// 그래야 로컬처럼 타격 순간에 창이 닫힘
			const FNUParryTimingSpec OpenTiming = Window.ToTimingSpec();
			const float OpenLead = bParryable
				? OpenTiming.GetLeadTime()
				: OpenTiming.ChallengeDuration;

			const float HitTime = FirstHitDelay + (ReactionGroups.Num() - 1) * HitInterval;
			NewGroup.OpenDelay = FMath::Max(0.f, HitTime - OpenLead);
		}

		FNUNetReactionGroup& Group = ReactionGroups.Last();
		Group.Windows.Add(Window);
		Group.bMustReport = Group.bMustReport || Window.bMustReport;
	}

	UE_LOG(LogNUBattleNetActionQueue, Log,
		TEXT("반응 창 예약 Seq=%d 묶음=%d 창=%d"),
		CurrentAction.ActionSeq, ReactionGroups.Num(), Windows.Num());

	ScheduleNextReactionOpen();
}

void UNUBattleNetActionQueueSubsystem::ScheduleNextReactionOpen()
{
	if (!ReactionGroups.IsValidIndex(NextReactionGroupIndex))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - ReactionActionStartTime;
	const float Remaining = ReactionGroups[NextReactionGroupIndex].OpenDelay - Elapsed;

	if (Remaining <= KINDA_SMALL_NUMBER)
	{
		// 앞 창이 길어 이미 지난 경우, 창은 하나씩만 열 수 있어서 늦게라도 연다
		OpenNextReactionWindow();
		return;
	}

	World->GetTimerManager().SetTimer(
		ReactionOpenTimerHandle,
		this,
		&ThisClass::OpenNextReactionWindow,
		Remaining,
		false
		);
}

void UNUBattleNetActionQueueSubsystem::OpenNextReactionWindow()
{
	if (!ReactionGroups.IsValidIndex(NextReactionGroupIndex))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	OpenReactionGroupIndex = NextReactionGroupIndex;
	ReactionWindowOpenTime = World->GetTimeSeconds();

	// 이 창이 열려 있는 동안 들어온 피해만 세야 함, 앞 타격 값이 남으면 엉뚱한 양을 환급함
	CurrentWindowDamage.Reset();

	const FNUNetReactionGroup& Group = ReactionGroups[OpenReactionGroupIndex];
	const FNUServerBattleReactionWindow& First = Group.Windows[0];
	const FNUParryTimingSpec Timing = First.ToTimingSpec();

	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		if (Group.bParryable)
		{
			// 창이 열린 순간이 도전 구간의 시작이 되도록 타격까지의 거리를 앞구간 길이로 줌
			// 그러면 입력 경과(PressOffset)가 서버의 성공 구간과 같은 기준이 됨
			PC->ClientOpenParryWindow(
				CurrentAction.ActionSeq,
				Timing.GetLeadTime(),
				Timing,
				First.SpCost
				);
		}
		else
		{
			// 회피는 확률 판정이라 성공 구간 사용 X, 창이 열려 있는 동안 누르기만 하면 됨
			// 서버가 회피 창에도 성공 구간을 채워 보내더라도 그 값은 보지 않음
			PC->ClientOpenDodgeWindow(
				CurrentAction.ActionSeq,
				First.SpCost, 
				Timing.ChallengeDuration
				);

			// 서버가 회피 창을 보낸 적이 없어 인게임 확인 전까지 남겨둠
			UE_LOG(LogNUBattleNetActionQueue, Log,
				TEXT("회피 창 열림 Id=%d 전투원=%d 길이=%.2f초 SP=%d"),
				First.ReactionId, First.DefenderId, Timing.ChallengeDuration, First.SpCost);
		}
	}

	// 안 눌러도 창은 닫혀야 함, must_report 면 그때 무입력을 보고해야 서버에서 대기가 풀림
	World->GetTimerManager().SetTimer(
		ReactionCloseTimerHandle,
		this,
		&ThisClass::CloseReactionWindow,
		FMath::Max(Timing.ChallengeDuration, KINDA_SMALL_NUMBER),
		false
		);
}

void UNUBattleNetActionQueueSubsystem::CloseReactionWindow()
{
	if (!ReactionGroups.IsValidIndex(OpenReactionGroupIndex))
	{
		return;
	}

	// 무대응, 기획대로 SP 는 안 나가고 피해를 그대로 받음
	ReportReactionGroup(false, false, -1.f);
	AdvanceReactionGroup();
}

bool UNUBattleNetActionQueueSubsystem::IsReactionWindowOpen() const
{
	return ReactionGroups.IsValidIndex(OpenReactionGroupIndex);
}

void UNUBattleNetActionQueueSubsystem::HandleReactionPress(float PressOffset)
{
	if (!ReactionGroups.IsValidIndex(OpenReactionGroupIndex))
	{
		return;
	}

	const FNUParryTimingSpec Timing = ReactionGroups[OpenReactionGroupIndex].Windows[0].ToTimingSpec();

	// 창이 열린 순간부터 잰 값이라 기준점도 앞구간 길이
	const bool bTimingSuccess = Timing.Judge(PressOffset, Timing.GetLeadTime());

	ReportReactionGroup(true, bTimingSuccess, PressOffset);
	AdvanceReactionGroup();
}

void UNUBattleNetActionQueueSubsystem::HandleDodgePress()
{
	if (!ReactionGroups.IsValidIndex(OpenReactionGroupIndex))
	{
		return;
	}

	// 패링 창이 열려 있다면 판정은 UNUParryComponent 가 처리함
	if (ReactionGroups[OpenReactionGroupIndex].bParryable)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float PressOffset = IsValid(World)
		? World->GetTimeSeconds() - ReactionWindowOpenTime
		: 0.f;

	// 회피는 성공 구간이 없어 두 번째 인자를 보지 않음, 확률은 전투원마다 따로 판정
	ReportReactionGroup(true, false, PressOffset);
	AdvanceReactionGroup();
}

void UNUBattleNetActionQueueSubsystem::ReportReactionGroup(bool bPressed, bool bTimingSuccess,
	float PressOffset)
{
	if (!ReactionGroups.IsValidIndex(OpenReactionGroupIndex) || !IsValid(BattleNet))
	{
		return;
	}

	const FNUNetReactionGroup& Group = ReactionGroups[OpenReactionGroupIndex];
	const int32 PressTimeMs = bPressed ? FMath::RoundToInt(PressOffset * 1000.f) : -1;

	// 패링은 타이밍이 곧 결과라 묶음 하나에 한 번 판정하지만,
	// 회피는 확률이라 전투원마다 따로 판정함
	const bool bDodgeGroup = !Group.bParryable;

	// 판정 결과는 묶음 하나에 한 번이지만 SP 는 전투원별로 봄
	// 전체 공격에서 누군가는 SP를 내고 다른 누군가는 못 내는 경우가 있을 수 있음
	for (const FNUServerBattleReactionWindow& Window : Group.Windows)
	{
		const FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(Window.DefenderId);
		const bool bCanPay = Snapshot && Snapshot->SP >= Window.SpCost;

		// 눌렀고 SP 를 낼 수 있을 때만 판정으로 감
		// SP 는 시도에 내는 값이라 판정에 떨어져도 서버가 돌려주지 않음
		bool bSuccess = false;
		if (bPressed && bCanPay)
		{
			if (bDodgeGroup)
			{
				const float DodgeChance = ResolveDodgeChance(Window.DefenderId);
				bSuccess = FMath::FRand() < DodgeChance;

				UE_LOG(LogNUBattleNetActionQueue, Log,
					TEXT("회피 굴림 전투원=%d 확률=%.0f%% 결과=%s"),
					Window.DefenderId,
					DodgeChance * 100.f,
					bSuccess ? TEXT("성공") : TEXT("실패"));
			}
			else
			{
				bSuccess = bTimingSuccess;
			}
		}

		if (bPressed)
		{
			// SP 부족은 판정 실패와 다른 팝업
			ENUCombatPopupType PopupType = ENUCombatPopupType::NotEnoughSP;
			if (bCanPay && bDodgeGroup)
			{
				PopupType = bSuccess ? ENUCombatPopupType::Dodge : ENUCombatPopupType::DodgeFail;
			}
			else if (bCanPay)
			{
				PopupType = bSuccess ? ENUCombatPopupType::Parry : ENUCombatPopupType::ParryFail;
			}

			// 결과 팝업과 패링 · 회피 성공 연출은 로컬처럼 그 방어자에게 타격이 들어오는 순간에 띄움 (ApplyEvent 의 피해)
			// 누르는 순간 띄우면 공격이 닿기 전에 히트스톱 · 버스트가 지나가 버림, 타격이 이미 들어왔으면 지금
			if (CurrentWindowDamage.Contains(Window.DefenderId))
			{
				ShowPopup(Window.DefenderId, FNUCombatPopupData(PopupType));
			}
			else
			{
				PendingReactionPopups.Add(Window.DefenderId, FNUCombatPopupData(PopupType));
			}
		}

		if (bPressed && bCanPay)
		{
			// 시도하는 순간 SP 가 나감 (성공·실패 무관)
			// 서버 확정값(sp_after)은 이 행동의 연출이 끝난 뒤에 와서 그때까지 안 깎인 채로 보임
			ApplySP(Window.DefenderId, Snapshot->SP - Window.SpCost);
		}

		if (bSuccess)
		{
			// 서버 환급(10 Reaction)은 이 행동의 연출이 다 끝난 뒤에 온옴
			// 그때까지 기다리면 체력이 깎였다가 되차오르므로 판정 즉시 표시를 맞춰 둠
			// 서버 확정값이 오면 그 값으로 덮으므로 어긋나도 그 시점에 바로잡힘
			bPendingReactionFullAvoid = bDodgeGroup;

			if (const int32* AppliedDamage = CurrentWindowDamage.Find(Window.DefenderId))
			{
				// 타격이 먼저 들어왔음, 지금 되돌린다
				CreditReactionRefund(Window.DefenderId, *AppliedDamage);
			}
			else
			{
				// 타격이 아직임, 들어올 때 깎아서 넣는다
				PendingReactionDefenders.Add(Window.DefenderId);
			}
		}

		// 안 눌렀거나 SP 가 모자란 시도는 서버가 차감도 환급도 하지 않으므로 보낼 것이 없음
		// 다만 기다리는 창이면 그래도 보내야 서버가 그 타격에서 대기가 풀림
		if ((!bPressed || !bCanPay) && !Window.bMustReport)
		{
			// 안 보내는 창도 목록에서는 지워야 함, 다음 턴의 같은 ActionSeq 로 다시 뽑히면 안 됨
			BattleNet->DiscardReaction(Window.ReactionId);
			continue;
		}

		BattleNet->ReportReaction(Window.ReactionId, bSuccess, PressTimeMs);
	}

	// 위젯 연출용, 패링 창을 연 사람에게만 알림
	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		if (bPressed && !bDodgeGroup)
		{
			PC->ClientParryResult(bTimingSuccess);
		}
	}
}

int32 UNUBattleNetActionQueueSubsystem::ResolveReactionRefund(int32 Damage) const
{
	if (Damage <= 0)
	{
		return 0;
	}

	// 회피는 공격 자체를 피한 판정이라 전액, 패링은 절반이고 나머지는 그대로 맞음
	return bPendingReactionFullAvoid 
		? Damage 
		: FMath::FloorToInt(static_cast<float>(Damage) / 2.f + 0.5f);
}

void UNUBattleNetActionQueueSubsystem::CreditReactionRefund(int32 CombatantId, int32 Damage)
{
	const FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(CombatantId);
	if (!Snapshot)
	{
		return;
	}

	const int32 Refund = ResolveReactionRefund(Damage);
	if (Refund <= 0)
	{
		return;
	}

	// 이 턴에 서버가 더 보낼 절대값들은 이 환급을 모르므로 얹을 몫으로 적어둔다
	PredictedRefundOffset.FindOrAdd(CombatantId) += Refund;

	// 예측이 어긋나도 바가 넘치지 않게 최대 체력에서 자름, 서버 확정값이 오면 그 값으로 맞춰짐
	ApplyHealth(CombatantId, FMath::Min(Snapshot->MaxHealth, Snapshot->Health + Refund));
}

void UNUBattleNetActionQueueSubsystem::AdvanceReactionGroup()
{
	OpenReactionGroupIndex = INDEX_NONE;
	++NextReactionGroupIndex;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReactionCloseTimerHandle);
	}

	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->CloseParryWindow();
		PC->CloseDodgeWindow();
	}

	ScheduleNextReactionOpen();
}

void UNUBattleNetActionQueueSubsystem::FlushReactionPopups()
{
	for (const TPair<int32, FNUCombatPopupData>& Pending : PendingReactionPopups)
	{
		ShowPopup(Pending.Key, Pending.Value);
	}

	PendingReactionPopups.Reset();
}

bool UNUBattleNetActionQueueSubsystem::HasUnreportedMustReport() const
{
	for (int32 Index = NextReactionGroupIndex; ReactionGroups.IsValidIndex(Index); ++Index)
	{
		if (ReactionGroups[Index].bMustReport)
		{
			return true;
		}
	}

	return false;
}

void UNUBattleNetActionQueueSubsystem::ClearReactionWindows()
{
	ReactionGroups.Reset();
	NextReactionGroupIndex = 0;
	OpenReactionGroupIndex = INDEX_NONE;
	ReactionActionStartTime = 0.f;
	ReactionWindowOpenTime = 0.f;
	// 창 단위 상태만 지운다
	// PredictedRefundOffset 은 여기서 지우면 안 됨 - 이 함수는 행동을 시작할 때마다 불리는데,
	// 그 환급은 같은 턴의 뒤이은 행동(회복 등)이 보내는 절대값에도 얹어야 하는 값이다
	PendingReactionDefenders.Reset();
	CurrentWindowDamage.Reset();
	PendingReactionPopups.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReactionOpenTimerHandle);
		World->GetTimerManager().ClearTimer(ReactionCloseTimerHandle);
	}

	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->CloseParryWindow();
		PC->CloseDodgeWindow();
	}
}

void UNUBattleNetActionQueueSubsystem::ResetQueue()
{
	ClearReactionWindows();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}

	PendingActions.Reset();
	CurrentAction = FNUNetQueuedAction();
	CapturedNaiaIds.Reset();
	PredictedRefundOffset.Reset();
	ActiveHitSlots.Reset();
	CombatantSnapshots.Reset();
	NextHitSlotIndex = 0;
	ActiveHitInterval = 0.f;
	bIsPlaying = false;
	bEndPending = false;
	PendingEndReason = 0;
	PendingAckTurn = INDEX_NONE;
	JobViewTurn = INDEX_NONE;
}

void UNUBattleNetActionQueueSubsystem::BuildHitSlots(const TArray<FNUServerBattleEvent>& Events,
	TArray<TArray<FNUServerBattleEvent>>& OutHitSlots) const
{
	OutHitSlots.Reset();

	// SelfCost(스킬의 자기 HP 소모)는 타격이 아니라 세지 않음 - 마지막 Damage 뒤 조각에 실려 마지막 타격 시점에 반영됨
	int32 DamageCount = 0;
	TSet<int32> DamageTargets;
	for (const FNUServerBattleEvent& Event : Events)
	{
		if (Event.Type == ENUBattleEventType::Damage)
		{
			++DamageCount;
			DamageTargets.Add(Event.TargetId);
		}
	}

	if (DamageCount == 0 || DamageTargets.Num() > 1)
	{
		OutHitSlots.Add(Events);
		return;
	}

	OutHitSlots.SetNum(DamageCount);
	int32 CurrentSlot = 0;
	bool bSawDamage = false;
	for (const FNUServerBattleEvent& Event : Events)
	{
		if (Event.Type == ENUBattleEventType::Damage)
		{
			if (bSawDamage)
			{
				++CurrentSlot;
			}
			bSawDamage = true;
		}

		OutHitSlots[CurrentSlot].Add(Event);
	}
}

FNUBattleActionMotionSpec UNUBattleNetActionQueueSubsystem::BuildMotionSpec(const FNUNetQueuedAction& Action, 
	int32 HitCount) const
{
	FNUBattleActionMotionSpec Spec;
	if (Action.bMotionless)
	{
		Spec.Motion = ENUBattleActionMotion::InPlace;
		Spec.TelegraphTime = 0.f;
		Spec.ApproachTime = 0.f;
		Spec.ActionTime = 0.4f;
		Spec.ReturnTime = 0.f;
		Spec.HitTimeNormalized = 0.f;
		Spec.HitCount = HitCount;
		Spec.HitInterval = 0.3f;
		Spec.ActionLocation = ResolveActionLocation(Action.ActorId, INDEX_NONE, Spec.Motion);
		return Spec;
	}

	const FNUNetCombatantSnapshot* ActorSnapshot = CombatantSnapshots.Find(Action.ActorId);
	const bool bIsAlly = ActorSnapshot && ActorSnapshot->Side == 0;
	NUBattlePresentation::ApplyNetPreset(Spec, bIsAlly, HitCount);

	// 포획·도주·아이템은 공격이 아니라 기본 공격 연출을 물려받으면 안 됨
	// 로컬도 BPGA_UseItem·BPGA_Flee 가 연출을 비워 두고 Motion 기본값(InPlace)으로 두고 있어서,
	// 여기서 안 가르면 포션을 마시는데 기본 공격의 검기가 나가고 앞으로 걸어 나감
	const FNUServerBattleEvent* KindEvent = FindActionKindEvent(Action.Events);
	if (KindEvent)
	{
		Spec.Motion = ENUBattleActionMotion::InPlace;
		Spec.ActionLocation = ResolveActionLocation(Action.ActorId, INDEX_NONE, Spec.Motion);

		return Spec;
	}

	const int32 SkillId = ResolveSkillId(Action.Events);
	const int32 TargetId = ResolveTargetId(Action.Events);
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	// 기본 공격 연출 → 피해 없는 스킬은 제자리 → DT_SkillVFX 행 덮기, VFX 뷰어와 같은 함수
	const bool bHasProfile = NUBattlePresentation::ApplySkillPresentation(Spec, Data, SkillId);

	// 서버가 BattleEvent.skill_id 를 안 채우면 여기서 SkillId 가 0이 되고
	// 배너·모션·VFX 가 한꺼번에 기본 공격으로 떨어짐, 화면만 보고는 못 가려서 남김
	UE_LOG(LogNUBattleNetActionQueue, Log,
		TEXT("연출 결정 Actor=%d SkillId=%d Target=%d"), Action.ActorId, SkillId, TargetId);

	// 행이 없으면 어빌리티 CDO 기본값(근접 슬래시)이 그대로 남음
	// 투사체가 안 나오는 대표 경로라 조용히 넘기지 않음
	if (IsValid(Data) && SkillId > 0 && !bHasProfile)
	{
		UE_LOG(LogNUBattleNetActionQueue, Warning,
			TEXT("스킬 %d 의 연출 프로필이 DT_SkillVFX 에 없음, 기본 연출로 재생함"), SkillId);
	}

	Spec.ActionLocation = ResolveActionLocation(Action.ActorId, TargetId, Spec.Motion);
	if (IsValid(StageSubsystem))
	{
		if (const ANUBattleAvatar* TargetAvatar = StageSubsystem->FindAvatar(TargetId))
		{
			// 대상의 진형 자리 - 지금 위치는 앞 행동에서 아직 돌아오는 중일 수 있음
			Spec.ProjectileTargetLocation = NUBattlePresentation::GetFormationLocation(TargetAvatar);
		}
	}

	return Spec;
}

FVector UNUBattleNetActionQueueSubsystem::ResolveActionLocation(int32 ActorId, int32 TargetId, 
	ENUBattleActionMotion Motion) const
{
	if (!IsValid(StageSubsystem))
	{
		return FVector::ZeroVector;
	}

	ANUBattleAvatar* SourceAvatar = StageSubsystem->FindAvatar(ActorId);
	if (!IsValid(SourceAvatar))
	{
		return FVector::ZeroVector;
	}

	// 제자리 행동은 지금 서 있는 자리, 전진은 진형 자리 기준 (돌아오는 중인 전투원을 읽지 않게)
	if (Motion == ENUBattleActionMotion::InPlace)
	{
		return SourceAvatar->GetActorLocation();
	}

	const FVector SourceWorld = NUBattlePresentation::GetFormationLocation(SourceAvatar);

	FVector AllySum = FVector::ZeroVector;
	FVector EnemySum = FVector::ZeroVector;
	int32 AllyCount = 0;
	int32 EnemyCount = 0;

	for (const TPair<int32, FNUNetCombatantSnapshot>& Pair : CombatantSnapshots)
	{
		ANUBattleAvatar* Avatar = StageSubsystem->FindAvatar(Pair.Key);
		if (!IsValid(Avatar))
		{
			continue;
		}

		if (Pair.Value.Side == 0)
		{
			AllySum += NUBattlePresentation::GetFormationLocation(Avatar);
			++AllyCount;
		}
		else
		{
			EnemySum += NUBattlePresentation::GetFormationLocation(Avatar);
			++EnemyCount;
		}
	}

	if (AllyCount == 0 || EnemyCount == 0)
	{
		return SourceWorld;
	}

	const FNUNetCombatantSnapshot* SourceSnapshot = CombatantSnapshots.Find(ActorId);
	if (!SourceSnapshot)
	{
		return SourceWorld;
	}

	ANUBattleAvatar* TargetAvatar = StageSubsystem->FindAvatar(TargetId);
	const bool bHasTarget = IsValid(TargetAvatar);
	UWorld* World = GetWorld();
	const UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;
	if (!IsValid(EncounterSubsystem))
	{
		return SourceWorld;
	}

	// 전진 방향 · 대상 없는 접근의 폴백은 VFX 뷰어와 같은 함수가 처리함
	return NUBattlePresentation::ResolveNetActionLocation(
		Motion,
		SourceWorld,
		SourceSnapshot->Side == 0,
		AllySum / AllyCount,
		EnemySum / EnemyCount,
		bHasTarget ? NUBattlePresentation::GetFormationLocation(TargetAvatar) : FVector::ZeroVector,
		bHasTarget,
		EncounterSubsystem->GetArenaLocation()
		);
}

int32 UNUBattleNetActionQueueSubsystem::ResolveSkillId(const TArray<FNUServerBattleEvent>& Events) const
{
	for (const FNUServerBattleEvent& Event : Events)
	{
		if (Event.SkillId > 0)
		{
			return Event.SkillId;
		}
	}

	return 0;
}

const FNUServerBattleEvent* UNUBattleNetActionQueueSubsystem::FindActionKindEvent(
	const TArray<FNUServerBattleEvent>& Events) const
{
	// 행동의 종류를 드러내는 이벤트는 행동당 하나뿐이라 먼저 나온 것이 곧 그 행동의 종류
	for (const FNUServerBattleEvent& Event : Events)
	{
		if (Event.Type == ENUBattleEventType::Capture
			|| Event.Type == ENUBattleEventType::Flee
			|| Event.Type == ENUBattleEventType::ItemUsed)
		{
			return &Event;
		}
	}

	return nullptr;
}

void UNUBattleNetActionQueueSubsystem::ShowActionBannerFor(const FNUNetQueuedAction& Action,
	int32 HitCount) const
{
	// 턴 종료 묶음(도트·만료)과 이어지는 조각은 새 행동이 아니라 배너를 띄우지 않음
	if (Action.bMotionless)
	{
		return;
	}

	ANUPlayerController* PC = GetLocalPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	FNUBattleActionBanner Banner;
	Banner.HitCount = HitCount;

	const FNUServerBattleEvent* KindEvent = FindActionKindEvent(Action.Events);
	const ENUBattleEventType Kind = KindEvent ? KindEvent->Type : ENUBattleEventType::None;

	switch (Kind)
	{
	case ENUBattleEventType::Flee:
		// 도주는 실패해도 행동은 한 것이라 결과와 무관하게 띄워줌
		Banner.DisplayName = NUBattleActionName::GetFlee();
		break;

	case ENUBattleEventType::Capture:
	case ENUBattleEventType::ItemUsed:
		{
			// 둘 다 아이템 이름을 띄움, 포획을 아이템 명령으로 내서 교감석 이름이 뜸
			const FNUItemDefRow* Definition = FindItemDefinition(KindEvent->ItemId);
			if (!Definition)
			{
				// 클라 표에 그 숫자가 없음, 배너만 빠지고 나머지 연출은 그대로 돔
				UE_LOG(LogNUBattleNetActionQueue, Warning,
					TEXT("아이템 %d 의 정의가 없어 배너를 건너뜀 (표의 NumericId 확인)"),
					KindEvent->ItemId);

				return;
			}

			Banner.DisplayName = Definition->DisplayName;
		}
		break;

	default:
		{
			// 공격, skill_id 가 0이면 기본 공격, 아니면 스킬
			const int32 SkillId = ResolveSkillId(Action.Events);
			const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
			const UNUBattleDataSubsystem* Data = IsValid(GameInstance)
				? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
				: nullptr;
			// 나이아 · 직업 스킬 모두 이름은 스킬 정보에서 옴
			FNUBattleSkillInfo SkillInfo;
			const bool bKnownSkill = IsValid(Data) && SkillId > 0 && Data->TryGetBattleSkillInfo(SkillId, SkillInfo);

			Banner.DisplayName = bKnownSkill
				? SkillInfo.DisplayName
				: NUBattleActionName::GetBasicAttack();
		}
		break;
	}

	if (Banner.DisplayName.IsEmpty())
	{
		return;
	}

	PC->ShowActionBanner(Banner);
}

int32 UNUBattleNetActionQueueSubsystem::ResolveTargetId(const TArray<FNUServerBattleEvent>& Events) const
{
	for (const FNUServerBattleEvent& Event : Events)
	{
		if (Event.TargetId != INDEX_NONE)
		{
			return Event.TargetId;
		}
	}

	return INDEX_NONE;
}

void UNUBattleNetActionQueueSubsystem::PlayBattleCue(FGameplayTag CueTag, int32 TargetId,
	int32 SourceId, int32 Magnitude) const
{
	if (!IsValid(StageSubsystem))
	{
		return;
	}

	ANUBattleAvatar* TargetAvatar = StageSubsystem->FindAvatar(TargetId);
	if (!IsValid(TargetAvatar))
	{
		return;
	}

	// Instigator 를 채우는 규칙(못 찾으면 대상 자신)은 VFX 뷰어와 같은 함수에 있음
	ANUBattleAvatar* SourceAvatar = StageSubsystem->FindAvatar(SourceId);
	NUBattlePresentation::ExecuteBattleCue(TargetAvatar, SourceAvatar, CueTag, static_cast<float>(Magnitude));
}

void UNUBattleNetActionQueueSubsystem::ApplyEvent(const FNUServerBattleEvent& Event)
{
	switch (Event.Type)
	{
	case ENUBattleEventType::Damage:
		{
			ENUElementAffinity Affinity = ENUElementAffinity::None;
			if (Event.ElementMultiplier > 1.5)
			{
				Affinity = ENUElementAffinity::Weak;
			}
			else if (Event.ElementMultiplier < 0.8)
			{
				Affinity = ENUElementAffinity::Resist;
			}

			// 창이 열려 있는 동안의 피해량, 판정이 이 뒤에 들어오면 여기서 환급량을 구함
			CurrentWindowDamage.Add(Event.TargetId, Event.Amount);

			// 서버는 반응과 무관하게 전액 피해를 보냄
			// 클라가 이미 성공으로 판정했으면 그 감산을 여기서 미리 반영함
			const int32 Refund = PendingReactionDefenders.Remove(Event.TargetId) > 0
				? ResolveReactionRefund(Event.Amount)
				: 0;
			const int32 ShownDamage = Event.Amount - Refund;

			// 이 턴에 서버가 보낼 체력값들은 이 환급량을 모르므로 얹을 몫으로 적어둬야함
			if (Refund > 0)
			{
				PredictedRefundOffset.FindOrAdd(Event.TargetId) += Refund;
			}

			// 판정 때 미뤄 둔 반응 결과를 타격 순간에 띄움 (패링 성공이면 히트스톱 · 버스트), 피해 숫자보다 먼저
			// 로컬도 타격 처리 직전에 판정 팝업을 띄워 같은 순서
			FNUCombatPopupData ReactionPopup;
			if (PendingReactionPopups.RemoveAndCopyValue(Event.TargetId, ReactionPopup))
			{
				ShowPopup(Event.TargetId, ReactionPopup);
			}

			// 회피로 전액 무효면 0이 뜸, 그때는 위의 반응 결과 팝업만
			if (ShownDamage > 0)
			{
				ShowPopup(
					Event.TargetId,
					FNUCombatPopupData(
						Event.bCritical ? ENUCombatPopupType::Critical : ENUCombatPopupType::Damage,
						ShownDamage,
						Affinity
						)
					);
			}

			// 기존에 사용하던 Hit GameplayCue 재생
			PlayBattleCue(GameplayCue::Battle::Hit, Event.TargetId, Event.ActorId, ShownDamage);

			// 오프셋에 이 환급이 이미 들어 있으므로 서버 값만 넘김
			ApplyServerHealth(Event.TargetId, Event.HealthAfter);
		}
		break;

	case ENUBattleEventType::Heal:
		ShowPopup(Event.TargetId, FNUCombatPopupData(ENUCombatPopupType::Heal, Event.Amount));
		// 로컬은 UNUGE_SkillHeal·UNUGE_ItemHeal 이 이 큐를 달고 있음
		PlayBattleCue(GameplayCue::Battle::Heal, Event.TargetId, Event.ActorId, Event.Amount);
		ApplyServerHealth(Event.TargetId, Event.HealthAfter);
		break;

	case ENUBattleEventType::Death:
		// 전멸 판정이 표시용 체력을 보므로, 피해 이벤트 없이 사망만 오는 경우에도 0으로 맞춰둠
		if (FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(Event.TargetId))
		{
			Snapshot->Health = 0;
		}

		if (IsValid(StageSubsystem))
		{
			if (ANUBattleAvatar* TargetAvatar = StageSubsystem->FindAvatar(Event.TargetId))
			{
				TargetAvatar->SetDeadVisual(true);
			}
		}
		break;

	case ENUBattleEventType::SPChanged:
		UE_LOG(LogNUBattleNetActionQueue, Log,
			TEXT("SPChanged 수신 Actor=%d Target=%d SPAfter=%d"),
			Event.ActorId,
			Event.TargetId,
			Event.SPAfter
			);
		ApplySP(Event.ActorId, Event.SPAfter);
		break;

	case ENUBattleEventType::Reaction:
		// 표시는 판정하는 순간 이미 깎았으므로 여기서 다시 반영하지 않음
		// 반영하면 서버 기준선에 클라가 미리 깎아둔 투입 SP 가 없어서 중간값으로 튐
		UE_LOG(LogNUBattleNetActionQueue, Log,
			TEXT("반응 확정 전투원=%d 성공=%s 환급=%d HP=%d SP=%d"),
			Event.ActorId,
			Event.bSuccess ? TEXT("true") : TEXT("false"),
			Event.Amount,
			Event.HealthAfter,
			Event.SPAfter
			);
		break;

	case ENUBattleEventType::ActionFailed:
		ShowPopup(Event.ActorId, FNUCombatPopupData(ENUCombatPopupType::ActionFail));
		break;

	case ENUBattleEventType::Capture:
		{
			// 교감석은 판정을 실제로 굴렸을 때만 없어짐, 
			// 못 잡는 나이아·자리 없음으로 실패하면 흔들림이 비어 오고 그때는 사용판정 X
			if (Event.CaptureShakes.Num() > 0)
			{
				NotifyItemConsumed(Event.ItemId);
			}

			// 포획은 승패와 무관하게 확정되므로 결과창에 그대로 올림
			if (Event.bSuccess)
			{
				const FNUNetCombatantSnapshot* Captured = CombatantSnapshots.Find(Event.TargetId);
				if (Captured != nullptr && Captured->NaiaId > 0)
				{
					CapturedNaiaIds.Add(Captured->NaiaId);
				}
			}

			// 흔들림이 비어 있으면 판정을 안 한 것이라 흔들 것이 없음
			// 굴린 경우의 결과 글자는 아바타가 흔들림 끝에 띄우므로 여기서 띄우지 않음
			ANUBattleAvatar* TargetAvatar = IsValid(StageSubsystem)
				? StageSubsystem->FindAvatar(Event.TargetId)
				: nullptr;

			// 앞에서부터 통과한 수만 세기, 0이면 흔들림 없이 곧바로 열림
			if (Event.CaptureShakes.Num() > 0 && IsValid(TargetAvatar))
			{
				TargetAvatar->PlayCaptureShakes(NUCapture::CountPassedShakes(Event.CaptureShakes), Event.bSuccess);
			}
			else
			{
				ShowPopup(
					Event.TargetId,
					FNUCombatPopupData(
						Event.bSuccess ? ENUCombatPopupType::CaptureSuccess : ENUCombatPopupType::CaptureFail
						)
					);
			}
		}
		break;

	case ENUBattleEventType::Hallucinated:
		UE_LOG(LogNUBattleNetActionQueue, Log, TEXT("환각 행동 이벤트 Actor=%d"), Event.ActorId);
		break;

	case ENUBattleEventType::Flee:
		UE_LOG(LogNUBattleNetActionQueue, Log,
			TEXT("도주 이벤트 Actor=%d Success=%s"),
			Event.ActorId,
			Event.bSuccess ? TEXT("true") : TEXT("false")
			);

		// 도주 시도 자체에 소리를 냄, 성패는 전투가 끝나는 쪽이 보여 줌
		UNUSFXSubsystem::Play(GetWorld(), NU::SFX::Battle::Flee);
		break;

	case ENUBattleEventType::StatusApplied:
	case ENUBattleEventType::StatusRemoved:
	case ENUBattleEventType::StatusStack:
		// 아이콘은 타격이 재생되는 이 시점에 붙고 떨어져야 함
		// 서버 미러를 그대로 그리면 아직 안 맞은 전투원에게 화상이 미리 뜸
		if (FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(Event.TargetId))
		{
			NUBattleServer::ApplyStatusEvent(Event, Snapshot->Statuses);
			PushStatusViews(Event.TargetId);

			// 부여와 중첩만, 해제는 조용함
			if (Event.Type != ENUBattleEventType::StatusRemoved)
			{
				UNUSFXSubsystem::Play(GetWorld(), NU::SFX::Battle::Status);
			}
		}
		break;

	case ENUBattleEventType::ItemUsed:
		// 아이템 소모, 효과는 뒤따라 오는 Heal·StatusRemoved·SPChanged 로 판단
		UE_LOG(LogNUBattleNetActionQueue, Log,
			TEXT("아이템 사용 Actor=%d Target=%d ItemId=%d Success=%s"),
			Event.ActorId,
			Event.TargetId,
			Event.ItemId,
			Event.bSuccess ? TEXT("true") : TEXT("false")
			);

		if (Event.bSuccess)
		{
			NotifyItemConsumed(Event.ItemId);
			UNUSFXSubsystem::Play(GetWorld(), NU::SFX::Item::Use);
		}
		else
		{
			// 턴은 썼는데 아무 일도 안 일어난 것이라 화면에 이유를 남김
			ShowPopup(Event.ActorId, FNUCombatPopupData(ENUCombatPopupType::ActionFail));
		}
		break;

	case ENUBattleEventType::SelfCost:
		// 스킬이 시전자 자신의 HP 를 낸 것, 맞은 게 아니라서 팝업 · 피격 큐 · 흔들림 · 명중 연출 없이 체력 바만 내림
		// 로컬 PaySelfHPCost 도 체력만 고침
		ApplyServerHealth(Event.ActorId, Event.HealthAfter);
		break;

	case ENUBattleEventType::ResourceChanged:
		// 자원 수치는 이 이벤트가 재생되는 시점에 올라야 함, 서버 미러를 바로 그리면 아직 안 맞은 타격의 템포가 미리 오름
		{
			const int32 OwnerId = NUBattleServer::ResolveResourceEventOwner(Event);
			if (FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(OwnerId))
			{
				NUBattleServer::ApplyResourceEvent(Event, Snapshot->Resources);
				PushJobResourceViews(OwnerId);
			}
		}
		break;

	case ENUBattleEventType::JobStateChanged:
		{
			const int32 OwnerId = NUBattleServer::ResolveResourceEventOwner(Event);
			if (FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(OwnerId))
			{
				NUBattleServer::ApplyJobStateEvent(Event, Snapshot->JobStates);
				PushJobResourceViews(OwnerId);
			}
		}
		break;

	case ENUBattleEventType::NaiaSwapped:
	case ENUBattleEventType::AttackStart:
	case ENUBattleEventType::None:
	case ENUBattleEventType::Count:
		// 모션 시작 신호(AttackStart)는 PlayAction 이 행동 단위로 읽으므로 여기서 할 일이 없음
		break;
	}
}

void UNUBattleNetActionQueueSubsystem::ApplyHealth(int32 CombatantId, int32 Health)
{
	FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(CombatantId);
	if (!Snapshot || !IsValid(StageSubsystem))
	{
		return;
	}

	const bool bWasDown = Snapshot->Health <= 0;
	Snapshot->Health = FMath::Max(0, Health);

	if (ANUBattleAvatar* Avatar = StageSubsystem->FindAvatar(CombatantId))
	{
		// 서버가 부활을 따로 알려주지 않음, 죽은 대상의 체력이 다시 올라오는 것이 부활 
		// 체력을 올리는 경로가 전부 이 함수를 지나므로 판단을 한 곳에 둠
		if (bWasDown && Snapshot->Health > 0)
		{
			UE_LOG(LogNUBattleNetActionQueue, Log,
				TEXT("전투원 %d 이 체력 %d 로 다시 일어남 - 사망 표시 해제"), CombatantId, Snapshot->Health);

			Avatar->SetDeadVisual(false);
		}

		Avatar->SetVitals(Snapshot->Health, Snapshot->MaxHealth, Snapshot->SP, Snapshot->MaxSP);
	}
}

void UNUBattleNetActionQueueSubsystem::ApplyServerHealth(int32 CombatantId, int32 Health)
{
	const FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(CombatantId);
	if (!Snapshot)
	{
		return;
	}

	// 죽은 전투원에게는 예측 환급을 얹지 않음, 얹으면 죽었는데 체력이 차있는것처럼 보임
	if (Health <= 0)
	{
		PredictedRefundOffset.Remove(CombatantId);
		ApplyHealth(CombatantId, 0);

		return;
	}

	const int32* Offset = PredictedRefundOffset.Find(CombatantId);
	const int32 Adjusted = Offset != nullptr ? Health + *Offset : Health;

	ApplyHealth(CombatantId, FMath::Min(Snapshot->MaxHealth, Adjusted));
}

void UNUBattleNetActionQueueSubsystem::ApplySP(int32 CombatantId, int32 SP)
{
	FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(CombatantId);
	if (!Snapshot || !IsValid(StageSubsystem))
	{
		return;
	}

	Snapshot->SP = FMath::Max(0, SP);

	if (ANUBattleAvatar* Avatar = StageSubsystem->FindAvatar(CombatantId))
	{
		Avatar->SetVitals(Snapshot->Health, Snapshot->MaxHealth, Snapshot->SP, Snapshot->MaxSP);
	}
}

void UNUBattleNetActionQueueSubsystem::ShowPopup(int32 CombatantId, const FNUCombatPopupData& PopupData) const
{
	if (!IsValid(StageSubsystem))
	{
		return;
	}

	ANUBattleAvatar* Avatar = StageSubsystem->FindAvatar(CombatantId);
	ANUPlayerController* PC = GetLocalPlayerController();
	if (!IsValid(Avatar) || !IsValid(PC))
	{
		return;
	}

	PC->ShowCombatPopup(Avatar, PopupData);
}

void UNUBattleNetActionQueueSubsystem::PushStatusViews(int32 CombatantId) const
{
	const FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(CombatantId);
	ANUBattleAvatar* Avatar = IsValid(StageSubsystem) ? StageSubsystem->FindAvatar(CombatantId) : nullptr;

	if (!Snapshot || !IsValid(Avatar))
	{
		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* BattleData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	TArray<FNUStatusEffectView> Views;
	NUBattleServer::BuildStatusViews(BattleData, Snapshot->Statuses, Views);

	Avatar->SetStatusViews(Views);
}

void UNUBattleNetActionQueueSubsystem::PushJobResourceViews(int32 CombatantId) const
{
	const FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(CombatantId);
	ANUBattleAvatar* Avatar = IsValid(StageSubsystem) ? StageSubsystem->FindAvatar(CombatantId) : nullptr;

	if (!Snapshot || !IsValid(Avatar) || !IsValid(BattleNet))
	{
		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* BattleData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	TArray<FNUJobResourceView> Gauges;
	TArray<FNUJobResourceView> Badges;
	NUBattleServer::BuildJobResourceViews(BattleData, Snapshot->Resources, Snapshot->JobStates,
		BattleNet->GetTurn(), Gauges, Badges);

	Avatar->SetJobResourceViews(Gauges, Badges);
}

void UNUBattleNetActionQueueSubsystem::PushAllJobResourceViews() const
{
	for (const TPair<int32, FNUNetCombatantSnapshot>& Pair : CombatantSnapshots)
	{
		PushJobResourceViews(Pair.Key);
	}
}

void UNUBattleNetActionQueueSubsystem::InitializeCombatantSnapshots()
{
	CombatantSnapshots.Reset();

	// 미러 값은 환급이 반영된 것
	PredictedRefundOffset.Reset();
	if (!IsValid(BattleNet))
	{
		return;
	}

	for (const TPair<int32, FNUServerCombatant>& Pair : BattleNet->GetCombatants())
	{
		FNUNetCombatantSnapshot Snapshot;
		Snapshot.Side = Pair.Value.Side;
		Snapshot.Kind = Pair.Value.Kind;
		Snapshot.NaiaId = Pair.Value.NaiaId;
		Snapshot.Health = Pair.Value.Health;
		Snapshot.SP = Pair.Value.SP;
		Snapshot.MaxHealth = Pair.Value.MaxHealth;
		Snapshot.MaxSP = Pair.Value.MaxSP;
		Snapshot.Statuses = Pair.Value.Statuses;
		Snapshot.Resources = Pair.Value.Resources;
		Snapshot.JobStates = Pair.Value.JobStates;
		CombatantSnapshots.Add(Pair.Key, Snapshot);
	}

	// 사본을 새로 깐 직후는 서버 값과 표시가 같은 시점이라 아이콘도 여기서 한 번 맞춤
	// 전투 시작·나이아 교체·재생이 다 끝난 뒤 세 경우가 전부 이 함수를 지남
	for (const TPair<int32, FNUNetCombatantSnapshot>& Pair : CombatantSnapshots)
	{
		PushStatusViews(Pair.Key);
		PushJobResourceViews(Pair.Key);
	}

	JobViewTurn = BattleNet->GetTurn();
}

void UNUBattleNetActionQueueSubsystem::RefreshLatestVitals()
{
	if (!IsValid(BattleNet) || !BattleNet->IsInBattle())
	{
		return;
	}

	InitializeCombatantSnapshots();
	if (IsValid(StageSubsystem))
	{
		StageSubsystem->RefreshVitals();
	}
}

const FNUItemDefRow* UNUBattleNetActionQueueSubsystem::FindItemDefinition(int32 ItemId) const
{
	const UNUInventoryComponent* Inventory = GetLocalInventory();

	return IsValid(Inventory) ? Inventory->FindItemDefByNumericId(ItemId) : nullptr;
}

void UNUBattleNetActionQueueSubsystem::NotifyItemConsumed(int32 ItemId) const
{
	UNUInventoryComponent* Inventory = GetLocalInventory();
	if (!IsValid(Inventory) || ItemId <= 0)
	{
		return;
	}

	// 전투중 사용할 아이템의 수량을 감소시킴
	// 서버에서 원본을 가지고 있어서 표시만 변경하는 것
	const FName RowName = Inventory->FindRowNameByNumericId(ItemId);
	const FNUItemInstance* Instance = RowName.IsNone()
		? nullptr
		: Inventory->FindFirstByRowName(RowName);
	if (!Instance)
	{
		// 서버가 인정한 사용인데 클라에 없음, 목록이 어긋난 것이라 로그 남김
		UE_LOG(LogNUBattleNetActionQueue, Warning,
			TEXT("소모할 아이템 %d 가 인벤에 없음"), ItemId);

		return;
	}

	// RemoveItem 이 배열을 건드리므로 참조가 아니라 값으로 들고 부름
	const FGuid InstanceId = Instance->InstanceId;
	Inventory->RemoveItem(InstanceId, 1);

	UE_LOG(LogNUBattleNetActionQueue, Log,
		TEXT("아이템 %d [%s] 하나 소모 - 남은 개수 %d"),
		ItemId,
		*RowName.ToString(),
		Inventory->GetCountByItemId(RowName)
		);
}

UNUInventoryComponent* UNUBattleNetActionQueueSubsystem::GetLocalInventory() const
{
	const ANUPlayerController* PlayerController = GetLocalPlayerController();
	const ANUPlayerState* PlayerState = IsValid(PlayerController)
		? Cast<ANUPlayerState>(PlayerController->PlayerState)
		: nullptr;

	return IsValid(PlayerState) ? PlayerState->GetInventoryComponent() : nullptr;
}

UNUProgressionComponent* UNUBattleNetActionQueueSubsystem::GetLocalProgression() const
{
	const ANUPlayerController* PlayerController = GetLocalPlayerController();
	const ANUPlayerState* PlayerState = IsValid(PlayerController)
		? Cast<ANUPlayerState>(PlayerController->PlayerState)
		: nullptr;

	return IsValid(PlayerState) ? PlayerState->GetProgressionComponent() : nullptr;
}

float UNUBattleNetActionQueueSubsystem::ResolveDodgeChance(int32 CombatantId) const
{
	const FNUNetCombatantSnapshot* Snapshot = CombatantSnapshots.Find(CombatantId);

	// 나이아 회피율은 아직 Stat이 없으므로 기본값 사용
	if (!Snapshot || Snapshot->Kind != NetCombatantKindCharacter)
	{
		return NetNaiaDodgeChance;
	}

	// 반응 창은 내가 조종하는 전투원에게만 오므로 여기 오는 캐릭터는 내 캐릭터
	// 파티 전투가 붙어 남의 캐릭터 창까지 오게 되면 소유자 검사를 먼저 해야 함
	const UNUProgressionComponent* Progression = GetLocalProgression();
	if (!IsValid(Progression) || !Progression->HasCharacterStatSnapshot())
	{
		// 전투 시작 때 보낸 캐릭터 스탯 요청이 아직 안 돌아왔음
		// 기본값으로 판정하고 로그만 남김
		UE_LOG(LogNUBattleNetActionQueue, Warning,
			TEXT("회피 확률: 캐릭터 스탯이 아직 없어 기본값을 씀 전투원=%d"), CombatantId);

		return NetNaiaDodgeChance;
	}

	// 서버가 장비까지 반영해 확정한 값이라 클라가 보정하지 않음
	return FMath::Clamp(Progression->GetCharacterStatSnapshot().DodgeRate, 0.f, 1.f);
}

ANUPlayerController* UNUBattleNetActionQueueSubsystem::GetLocalPlayerController() const
{
	UWorld* World = GetWorld();

	return IsValid(World) ? Cast<ANUPlayerController>(World->GetFirstPlayerController()) : nullptr;
}
