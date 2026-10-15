#include "Quest/Component/NUQuestComponent.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterState/NUPlayerState.h"

#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Field/Presence/NUActorPresenceTransitionSubsystem.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Quest/Data/NUQuestTutorialTags.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "Net/EmberServerManager.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Component/NUQuestProviderComponent.h"
#include "Quest/Component/NUQuestSceneComponent.h"
#include "Quest/Integration/NUQuestBattleEventBridgeSubsystem.h"
#include "Quest/Net/NUQuestServerTypes.h"
#include "NUGAS/NUGameplayTags.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace NUQuestObjective
{
	// 선행 목표가 끝났는지. 선행이 없으면 항상 참
	bool IsPrerequisiteMet(const FNUQuestProgressState& QuestProgress, const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective)
	{
		return QuestProgress.IsObjectivePrerequisiteMet(Definition, Objective);
	}

	// 이 이벤트가 목표를 진행시킬 수 있는지.
	// 실제 진행과 머리 위 마커가 같은 규칙을 쓰도록 판정을 여기 하나로 모은다
	bool CanAdvance(const FNUQuestProgressState& QuestProgress, const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective, const FGameplayTag& ObjectiveType, FName TargetId)
	{
		if (Objective.ObjectiveType != ObjectiveType)
		{
			return false;
		}

		if (!Objective.TargetId.IsNone() && Objective.TargetId != TargetId)
		{
			return false;
		}

		const FNUQuestObjectiveProgress* ObjectiveProgress =
			QuestProgress.FindObjectiveProgress(Objective.ObjectiveId);

		if (ObjectiveProgress == nullptr || ObjectiveProgress->IsComplete(Objective.RequiredCount))
		{
			return false;
		}

		return IsPrerequisiteMet(QuestProgress, Definition, Objective);
	}
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

UNUQuestComponent::UNUQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNUQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeLocationTracking();
	InitializeInventoryTracking();
	if (UNUJobComponent* Job = GetOwner()->FindComponentByClass<UNUJobComponent>())
	{
		Job->OnJobChanged.AddUniqueDynamic(this, &ThisClass::EvaluatePersistentObjectives);
	}
	if (UNUNaiaRosterComponent* Roster = GetOwner()->FindComponentByClass<UNUNaiaRosterComponent>())
	{
		Roster->OnRosterChanged.AddUniqueDynamic(this, &ThisClass::EvaluatePersistentObjectives);
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUCharacterManager* CharacterManager = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUCharacterManager>()
		: nullptr;
	bServerBackedSession = CharacterManager != nullptr && CharacterManager->GetSelectedCharacterIdx() != 0;
	FNUCharacterSummary SelectedCharacter;
	if (CharacterManager != nullptr && CharacterManager->GetSelectedCharacter(SelectedCharacter))
	{
		// 서버 퀘스트 수락 조건을 클라이언트에서도 같은 레벨로 표시한다.
		TestPlayerLevel = FMath::Max(SelectedCharacter.Level, 1);
	}

	if (UNUProgressionComponent* ProgressionComponent = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UNUProgressionComponent>()
		: nullptr)
	{
		ProgressionComponent->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
		ProgressionComponent->OnCharacterStatSnapshotChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressionChanged);
		HandleProgressionChanged();
	}

	const APlayerController* LocalPlayerController = World != nullptr
		? World->GetFirstPlayerController()
		: nullptr;
	if (LocalPlayerController == nullptr || LocalPlayerController->PlayerState != GetOwner())
	{
		bAutoAcceptInitialized = true;
		EvaluateAvailableQuests();
		return;
	}

	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleEmberStateChanged);
		if (Server->IsLoggedIn())
		{
			RequestServerQuestList();
		}
	}
	bAutoAcceptInitialized = true;
	EvaluateAvailableQuests();
}

void UNUQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		for (const auto& Entry : BattleCompletionTimers)
		{
			FTimerHandle Handle = Entry.Value;
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	BattleCompletionTimers.Reset();
	if (UNUQuestBattleEventBridgeSubsystem* Bridge = GetWorld() != nullptr && GetWorld()->GetGameInstance() != nullptr
		? GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>()
		: nullptr)
	{
		Bridge->EndSessionsForQuest(this);
	}
	if (UNUJobComponent* Job = GetOwner()->FindComponentByClass<UNUJobComponent>())
	{
		Job->OnJobChanged.RemoveDynamic(this, &ThisClass::EvaluatePersistentObjectives);
	}
	if (UNUNaiaRosterComponent* Roster = GetOwner()->FindComponentByClass<UNUNaiaRosterComponent>())
	{
		Roster->OnRosterChanged.RemoveDynamic(this, &ThisClass::EvaluatePersistentObjectives);
	}
	UnbindLocationTracking();
	UnbindInventoryTracking();
	if (UNUProgressionComponent* ProgressionComponent = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UNUProgressionComponent>()
		: nullptr)
	{
		ProgressionComponent->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
		ProgressionComponent->OnCharacterStatSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	}

	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleEmberStateChanged);
	}

	ResetServerRequests();
	Super::EndPlay(EndPlayReason);
}

// -------------------------------------------------------------------------
// UI Notifications
// -------------------------------------------------------------------------

void UNUQuestComponent::BroadcastQuestOffer(FName QuestId)
{
	if (QuestId.IsNone())
	{
		return;
	}

	OnQuestOfferRequested.Broadcast(QuestId);
}

void UNUQuestComponent::OfferQuestAfterObjective(FName OfferQuestId, FSimpleDelegate CompleteObjective)
{
	if (FindQuestDefinition(OfferQuestId) == nullptr || !CompleteObjective.IsBound())
	{
		return;
	}

	ObjectiveOfferQuestId = OfferQuestId;
	ObjectiveOfferCompletion = MoveTemp(CompleteObjective);
	BroadcastQuestOffer(OfferQuestId);
}

void UNUQuestComponent::CancelObjectiveOffer(FName OfferQuestId)
{
	if (!OfferQuestId.IsNone() && OfferQuestId == ObjectiveOfferQuestId)
	{
		ObjectiveOfferQuestId = NAME_None;
		ObjectiveOfferCompletion.Unbind();
	}
}

void UNUQuestComponent::TryAcceptAfterObjective()
{
	if (AcceptAfterObjectiveQuestId.IsNone() || (bServerBackedSession && AcceptRequestInFlight != 0))
	{
		return;
	}

	const UNUQuestDefinition* Definition = FindQuestDefinition(AcceptAfterObjectiveQuestId);
	if (Definition == nullptr || !AreStartRequirementsMet(Definition))
	{
		return;
	}

	AcceptAfterObjectiveQuestId = NAME_None;
	if (HasQuest(Definition->QuestId) || AddQuest(Definition, ENUQuestState::Available))
	{
		AcceptQuest(Definition);
	}
}

void UNUQuestComponent::BroadcastQuestCompleted(FName QuestId, const FNUQuestServerReward& Reward)
{
	if (QuestId.IsNone())
	{
		return;
	}

	LastCompletedQuestId = QuestId;
	LastCompletionReward = Reward;
	bHasLastCompletionReward = true;
	UNUSFXSubsystem::Play(this, NU::SFX::Quest::Complete);
	OnQuestCompleted.Broadcast(QuestId);
}

void UNUQuestComponent::BroadcastQuestAccepted(FName QuestId)
{
	if (QuestId.IsNone())
	{
		return;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::Quest::Accept);
	OnQuestAccepted.Broadcast(QuestId);

	// 수락 창이 닫힌 다음 틱에 제안한 NPC의 대화를 잇는다.
	if (QuestId == OfferSourceQuestId && OfferSourceActor.IsValid() && GetWorld() != nullptr)
	{
		bOfferConversationPending = true;
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::ContinueOfferConversation, QuestId));
	}
}

void UNUQuestComponent::RememberOfferSource(FName QuestId, AActor* SourceActor)
{
	OfferSourceQuestId = QuestId;
	OfferSourceActor = SourceActor;
}

void UNUQuestComponent::ContinueOfferConversation(FName QuestId)
{
	bOfferConversationPending = false;
	AActor* SourceActor = OfferSourceActor.Get();
	if (QuestId != OfferSourceQuestId || SourceActor == nullptr)
	{
		return;
	}

	OfferSourceActor.Reset();
	OfferSourceQuestId = NAME_None;

	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwner());
	UNUQuestProviderComponent* Provider = SourceActor->FindComponentByClass<UNUQuestProviderComponent>();
	if (PlayerState != nullptr && PlayerState->GetPawn() != nullptr && Provider != nullptr)
	{
		Provider->ContinueAcceptedQuest(PlayerState->GetPawn(), QuestId);
	}
}

// -------------------------------------------------------------------------
// Quest State
// -------------------------------------------------------------------------

bool UNUQuestComponent::AddQuest(const UNUQuestDefinition* Definition, ENUQuestState InitialState)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	if (!IsValid(Definition))
	{
		return false;
	}

	FString ValidationError;
	if (!Definition->ValidateDefinition(ValidationError))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("AddQuest 실패: %s"), *ValidationError);
		return false;
	}

	if (HasQuest(Definition->QuestId))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("AddQuest 실패: 이미 존재하는 퀘스트 [%s]"), *Definition->QuestId.ToString());
		return false;
	}

	// 외부에서 Available을 요청해도 선행 조건이 부족하면 Locked로 등록한다.
	const ENUQuestState ResolvedInitialState =
		InitialState == ENUQuestState::Available && !AreStartRequirementsMet(Definition)
		? ENUQuestState::Locked
		: InitialState;

	FNUQuestProgressState& NewProgress = QuestProgressList.AddDefaulted_GetRef();

	NewProgress.Initialize(*Definition, ResolvedInitialState);
	

	if (NewProgress.QuestState == ENUQuestState::Active && NewProgress.IsRequiredObjectivesComplete(*Definition))
	{
		NewProgress.QuestState = ENUQuestState::ReadyToTurnIn;

		// CompleteQuest는 성공 시 내부에서 알림을 보낸다.
		// 실패하더라도 퀘스트 추가와 ReadyToTurnIn 전이는 이미 일어났으므로 아래에서 알려야 한다.
		if (Definition->bAutoComplete && CompleteQuest(Definition))
		{
			return true;
		}
	}

	NotifyQuestProgressChanged();
	EvaluateAvailableQuests();
	return true;
}

bool UNUQuestComponent::AreStartRequirementsMet(const UNUQuestDefinition* Definition) const
{
	return IsValid(Definition) && FindUnmetRequirement(Definition) == nullptr;
}

const FNUQuestRequirementSpec* UNUQuestComponent::FindUnmetRequirement(const UNUQuestDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return nullptr;
	}

	for (const FNUQuestRequirementSpec& Requirement : Definition->StartRequirements)
	{
		if (!EvaluateRequirement(Requirement))
		{
			return &Requirement;
		}
	}

	return nullptr;
}

bool UNUQuestComponent::AcceptQuest(const UNUQuestDefinition* Definition)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	if (!IsValid(Definition))
	{
		return false;
	}

	if (!AreStartRequirementsMet(Definition))
	{
		UE_LOG(LogNUQuest, Log, TEXT("AcceptQuest 실패: 선행 조건 미충족 QuestId=%s"), *Definition->QuestId.ToString());
		return false;
	}

	FNUQuestProgressState* Progress = FindQuestProgressMutable(Definition->QuestId);
	if (Progress == nullptr)
	{
		return false;
	}

	const bool bCanAcceptNormally = Progress->QuestState == ENUQuestState::Available;
	const bool bCanAcceptRepeatedQuest = 
		Progress->QuestState == ENUQuestState::Completed 
		&& Definition->RepeatPolicy == ENUQuestRepeatPolicy::Repeatable;
	if (!bCanAcceptNormally && !bCanAcceptRepeatedQuest)
	{
		return false;
	}

	if (bServerBackedSession)
	{
		if (!bServerQuestListReady || AcceptRequestInFlight != 0 || !IsServerQuestConnected())
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestAccept 중단: 게임 서버 연결 없음 QuestId=%s"),
				*Definition->QuestId.ToString());
			return false;
		}

		int32 ServerQuestId = 0;
		if (!TryGetServerQuestId(*Definition, ServerQuestId))
		{
			return false;
		}

		AcceptRequestInFlight = ServerQuestId;
		GetServerManager()->SendQuestAccept(ServerQuestId);
		UE_LOG(LogNUQuest, Log, TEXT("QuestAccept 전송 QuestId=%d"), ServerQuestId);
		return true;
	}

	return ApplyAcceptedQuestLocally(*Definition);
}

bool UNUQuestComponent::ApplyAcceptedQuestLocally(const UNUQuestDefinition& Definition)
{
	FNUQuestProgressState* Progress = FindQuestProgressMutable(Definition.QuestId);
	if (Progress == nullptr)
	{
		return false;
	}

	Progress->ResetObjectiveProgress(Definition);
	Progress->QuestState = ENUQuestState::Active;

	// 트래커 자동 고정용. 바로 자동 완료되면 다음 갱신에서 고정이 알아서 풀린다
	BroadcastQuestAccepted(Definition.QuestId);

	// 목표가 없는 퀘스트는 수락 즉시 임무 완료
	if (Progress->IsRequiredObjectivesComplete(Definition))
	{
		Progress->QuestState = ENUQuestState::ReadyToTurnIn;

		// 자동 완료가 실패해도 수락 자체는 성립했으므로 아래에서 알린다.
		if (Definition.bAutoComplete && CompleteQuest(&Definition))
		{
			return true;
		}
	}

	NotifyQuestProgressChanged();
	return true;
}

bool UNUQuestComponent::AddObjectiveProgress(const UNUQuestDefinition* Definition, FName ObjectiveId, int32 Amount, FName SourceInstanceId)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	if (!IsValid(Definition))
	{
		return false;
	}

	if (ObjectiveId.IsNone() || Amount <= 0)
	{
		return false;
	}

	FNUQuestProgressState* QuestProgress = FindQuestProgressMutable(Definition->QuestId);
	if (QuestProgress == nullptr || !QuestProgress->IsInProgress())
	{
		return false;
	}

	const bool bWasActive = QuestProgress->QuestState == ENUQuestState::Active;

	const FNUQuestObjectiveSpec* ObjectiveDefinition =
		Definition->Objectives.FindByPredicate(
			[ObjectiveId] (const FNUQuestObjectiveSpec& Objective)
			{
				return Objective.ObjectiveId == ObjectiveId;
			}
		);

	if (ObjectiveDefinition == nullptr)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("목표 진행 실패: 정의에 없는 ObjectiveId [%s]"),*ObjectiveId.ToString());
		return false;
	}
	if (!NUQuestObjective::IsPrerequisiteMet(*QuestProgress, *Definition, *ObjectiveDefinition))
	{
		return false;
	}

	FNUQuestObjectiveProgress* ObjectiveProgress = QuestProgress->FindObjectiveProgress(ObjectiveId);
	if (ObjectiveProgress == nullptr)
	{
		return false;
	}

	if (!SourceInstanceId.IsNone()
		&& ObjectiveProgress->ProcessedSourceInstanceIds.Contains(SourceInstanceId))
	{
		return false;
	}

	if (bServerBackedSession)
	{
		// 서버가 관측한 사건은 104 푸시만 반영한다. 클라이언트에서 중복 보고하지 않는다.
		const FGameplayTag Type = ObjectiveDefinition->ObjectiveType;

		if (!bServerQuestListReady || !IsServerQuestConnected() || (Type != NU::Quest::Objective::Talk && Type != NU::Quest::Objective::UseObject && Type != NU::Quest::Objective::ReachLocation))
		{
			return false;
		}

		int32 ServerObjectiveId = 0;

		if (!TryGetServerObjectiveId(*Definition, ObjectiveId, ServerObjectiveId))
		{
			return false;
		}

		// 전송 중·대기 중 누적값을 기준으로 합치되 UI는 서버 확정값만 표시한다.
		int32 PendingCount = ObjectiveProgress->CurrentCount;
		int32 ServerQuestId = 0;

		if (!TryGetServerQuestId(*Definition, ServerQuestId))
		{
			return false;
		}

		if (ObjectiveRequestInFlight.QuestId == ServerQuestId && ObjectiveRequestInFlight.ObjectiveId == ServerObjectiveId)
		{
			PendingCount = FMath::Max(PendingCount, ObjectiveRequestInFlight.CurrentCount);
		}

		const uint64 Key = (uint64(uint32(ServerQuestId)) << 32) | uint32(ServerObjectiveId);

		if (const FPendingObjectiveRequest* Pending = QueuedObjectiveRequests.Find(Key))
		{
			PendingCount = FMath::Max(PendingCount, Pending->CurrentCount);
		}

		const int32 Count = int32(FMath::Min(int64(PendingCount) + Amount, int64(ObjectiveDefinition->RequiredCount)));

		if (Count <= PendingCount || !QueueServerObjective(*Definition, ObjectiveId, Count))
		{
			return false;
		}

		if (!SourceInstanceId.IsNone())
		{
			ObjectiveProgress->ProcessedSourceInstanceIds.AddUnique(SourceInstanceId);
		}

		return true;
	}

	const int32 PreviousCount = ObjectiveProgress->CurrentCount;

	ObjectiveProgress->CurrentCount = FMath::Min(ObjectiveProgress->CurrentCount + Amount, ObjectiveDefinition->RequiredCount);
	if (ObjectiveProgress->CurrentCount == PreviousCount)
	{
		return false;
	}

	if (!SourceInstanceId.IsNone())
	{
		ObjectiveProgress->ProcessedSourceInstanceIds.AddUnique(SourceInstanceId);
	}

	if (bWasActive && QuestProgress->IsRequiredObjectivesComplete(*Definition))
	{
		QuestProgress->QuestState = ENUQuestState::ReadyToTurnIn;

		// 보고 NPC와의 대화로 마지막 목표를 채웠으면 그 대화에서 바로 수령한다.
		const bool bTurnInByTalk = Definition->FindTurnInTalkObjective() == ObjectiveDefinition;

		// 바로 완료되는 퀘스트는 완료음이 이어지므로 보고 대기음을 내지 않는다
		if (!Definition->bAutoComplete && !bTurnInByTalk && !bSkippingLocalQuestPrerequisites)
		{
			UNUSFXSubsystem::Play(this, NU::SFX::Quest::Ready);
		}

		// 마지막 목표에 따른 로컬 재배치를 먼저 시작한 뒤 완료를 처리한다.
		NotifyQuestProgressChanged();

		// 자동 완료가 실패해도 목표 진행은 이미 올라갔으므로 아래에서 알린다.
		if ((Definition->bAutoComplete || bTurnInByTalk) && CompleteQuest(Definition, bTurnInByTalk ? Definition->TurnInActorId : NAME_None))
		{
			return true;
		}
		return true;
	}

	if (!bSkippingLocalQuestPrerequisites)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Quest::Progress);
	}
	NotifyQuestProgressChanged();
	return true;
}

bool UNUQuestComponent::CompleteQuest(const UNUQuestDefinition* Definition, FName TurnInNpcId)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	if (!IsValid(Definition))
	{
		return false;
	}

	FNUQuestProgressState* Progress = FindQuestProgressMutable(Definition->QuestId);
	if (Progress == nullptr || Progress->QuestState != ENUQuestState::ReadyToTurnIn)
	{
		return false;
	}
	if (DeferBattleCompletion(*Definition)) { return false; }

	if (bServerBackedSession)
	{
		if (!IsServerQuestConnected())
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestComplete 중단: 게임 서버 연결 없음 QuestId=%s"),
				*Definition->QuestId.ToString());
			return false;
		}

		int32 ServerQuestId = 0;
		if (!TryGetServerQuestId(*Definition, ServerQuestId))
		{
			return false;
		}

		int32 NpcId = 0;
		if (!Definition->bAutoComplete && (TurnInNpcId != Definition->TurnInActorId || !LexTryParseString(NpcId, *TurnInNpcId.ToString()) || NpcId <= 0))
		{
			return false;
		}
		QueueServerCompletion(ServerQuestId, NpcId);
		return true;
	}

	// 오프라인 연출의 재배치가 끝나기 전에는 완료 보상과 후속 의뢰를 열지 않는다.
	// 서버 세션은 위의 기존 서버 완료 요청 경로만 사용한다.
	const UNUActorPresenceTransitionSubsystem* Placement = GetWorld()->GetSubsystem<UNUActorPresenceTransitionSubsystem>();
	if (Definition->bAutoComplete && !bSkippingLocalQuestPrerequisites && Placement != nullptr && Placement->IsTransitionPending())
	{
		const TWeakObjectPtr<const UNUQuestDefinition> PendingDefinition(Definition);
		const int32 DebugSerial = GetDebugCompletionSerial();
		FTimerHandle RetryTimer;
		GetWorld()->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateWeakLambda(this, [this, PendingDefinition, TurnInNpcId, DebugSerial]
		{
			if (PendingDefinition.IsValid() && DebugSerial == GetDebugCompletionSerial()) { CompleteQuest(PendingDefinition.Get(), TurnInNpcId); }
		}), .05f, false);
		return false;
	}
	return ApplyCompletedQuestLocally(*Definition);
}

bool UNUQuestComponent::DeferBattleCompletion(const UNUQuestDefinition& Definition)
{
	// 보스 등 전투 목표가 있는 자동 완료만 미룬다. NPC 보고와 개발용 선행 완료는 기존 순서를 따른다.
	if (!Definition.bAutoComplete || bSkippingLocalQuestPrerequisites
		|| !Definition.Objectives.ContainsByPredicate([](const FNUQuestObjectiveSpec& Objective)
		{
			return Objective.ObjectiveType == NU::Quest::Objective::BattleWon
				|| Objective.ObjectiveType == NU::Quest::Objective::EnemyDefeated;
		}))
	{
		return false;
	}
	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwner());
	const APlayerController* Controller = PlayerState != nullptr ? PlayerState->GetPlayerController() : nullptr;
	UNUQuestSceneComponent* Scene = Controller != nullptr ? Controller->FindComponentByClass<UNUQuestSceneComponent>() : nullptr;
	if (Scene == nullptr || !Scene->ShouldWaitForBattleCompletion(Definition.QuestId)) { return false; }

	FTimerHandle& RetryTimer = BattleCompletionTimers.FindOrAdd(Definition.QuestId);
	if (!GetWorld()->GetTimerManager().IsTimerActive(RetryTimer))
	{
		const FName QuestId = Definition.QuestId;
		const int32 DebugSerial = GetDebugCompletionSerial();
		GetWorld()->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateWeakLambda(this, [this, QuestId, DebugSerial]
		{
			BattleCompletionTimers.Remove(QuestId);
			if (DebugSerial == GetDebugCompletionSerial()) { CompleteQuest(FindQuestDefinition(QuestId)); }
		}), .1f, false);
	}
	return true;
}

bool UNUQuestComponent::ApplyCompletedQuestLocally(const UNUQuestDefinition& Definition)
{
	if (bApplyingLocalRewards) { return false; }
	TGuardValue<bool> RewardGuard(bApplyingLocalRewards, true);
	FNUQuestProgressState* Progress = FindQuestProgressMutable(Definition.QuestId);
	if (Progress == nullptr || Progress->QuestState != ENUQuestState::ReadyToTurnIn)
	{
		return false;
	}

	// 더미 보상이라도 지원하지 않는 타입이 있으면 완료 상태를 먼저 바꾸지 않는다.
	if (!CanApplyRewards(Definition))
	{
		return false;
	}

	FNUQuestServerReward RewardResult;
	for (const FNUQuestRewardSpec& Reward : Definition.Rewards)
	{
		if (Reward.RewardType == NU::Quest::Reward::Experience)
		{
			RewardResult.Experience += Reward.Amount;
		}
		else if (Reward.RewardType == NU::Quest::Reward::Gold) { RewardResult.Gold += Reward.Amount; }
	}

	if (!ApplyRewards(Definition)) { return false; }

	// 보상 알림이 퀘스트 배열을 갱신할 수 있으므로 진행 포인터를 다시 찾는다.
	Progress = FindQuestProgressMutable(Definition.QuestId);
	if (Progress == nullptr) { return false; }
	Progress->QuestState = ENUQuestState::Completed;
	Progress->CompletionCount += 1;
	bApplyingLocalRewards = false;

	if (!bSkippingLocalQuestPrerequisites) { BroadcastQuestCompleted(Definition.QuestId, RewardResult); }

	// QuestCompleted와 보상 플래그를 조건으로 사용하는 퀘스트를 다시 평가한다.
	EvaluateAvailableQuests();
	NotifyQuestProgressChanged();
	return true;
}

bool UNUQuestComponent::HandleQuestEvent(const FNUQuestEvent& QuestEvent)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	if (!QuestEvent.IsValid())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance =
		World != nullptr
		? World->GetGameInstance()
		: nullptr;

	if (GameInstance == nullptr)
	{
		return false;
	}

	const UNUQuestDataSubsystem* QuestDataSubsystem =GameInstance->GetSubsystem<UNUQuestDataSubsystem>();
	if (QuestDataSubsystem == nullptr)
	{
		return false;
	}

	// 일치하는 목표 수집
	struct FMatchedObjective
	{
		const UNUQuestDefinition* Definition = nullptr;
		FName ObjectiveId = NAME_None;
		FName SourceInstanceId = NAME_None;
	};

	TArray<FMatchedObjective> MatchedObjectives;

	for (const FNUQuestProgressState& QuestProgress: QuestProgressList)
	{
		if (!QuestProgress.IsInProgress())
		{
			continue;
		}

		const UNUQuestDefinition* Definition = QuestDataSubsystem->FindDefinition(QuestProgress.QuestId);
		if (Definition == nullptr)
		{
			continue;
		}

		for (const FNUQuestObjectiveSpec& Objective : Definition->Objectives)
		{
			if (!NUQuestObjective::CanAdvance(
				QuestProgress,
				*Definition,
				Objective,
				QuestEvent.ObjectiveType,
				QuestEvent.TargetId))
			{
				continue;
			}

			FMatchedObjective& Match = MatchedObjectives.AddDefaulted_GetRef();

			Match.Definition = Definition;
			Match.ObjectiveId = Objective.ObjectiveId;
			Match.SourceInstanceId = QuestEvent.SourceInstanceId;
		}
	}

	// 진행량 적용
	bool bAnyProgressChanged = false;

	for (const FMatchedObjective& Match : MatchedObjectives)
	{
		if (AddObjectiveProgress(
			Match.Definition,
			Match.ObjectiveId,
			QuestEvent.Amount,
			Match.SourceInstanceId))
		{
			bAnyProgressChanged = true;
		}
	}

	return bAnyProgressChanged;
}

bool UNUQuestComponent::HasActiveObjectiveFor(FGameplayTag InObjectiveType, FName TargetId, FName SourceInstanceId) const
{
	return !GetActiveObjectiveQuestIds(InObjectiveType, TargetId, SourceInstanceId).IsEmpty();
}

TArray<FName> UNUQuestComponent::GetActiveObjectiveQuestIds(FGameplayTag InObjectiveType, FName TargetId, FName SourceInstanceId) const
{
	TArray<FName> QuestIds;
	for (const FNUQuestProgressState& Progress : QuestProgressList)
	{
		if (!FindActiveObjectiveId(Progress.QuestId, InObjectiveType, TargetId, SourceInstanceId).IsNone()) { QuestIds.AddUnique(Progress.QuestId); }
	}
	return QuestIds;
}

FName UNUQuestComponent::FindActiveObjectiveId(FName QuestId, FGameplayTag InObjectiveType, FName TargetId, FName SourceInstanceId) const
{
	const FNUQuestProgressState* Progress = FindQuestProgress(QuestId);
	const UNUQuestDefinition* Definition = FindQuestDefinition(QuestId);
	if (!InObjectiveType.IsValid() || Progress == nullptr || !Progress->IsInProgress() || Definition == nullptr) { return NAME_None; }
	for (const FNUQuestObjectiveSpec& Objective : Definition->Objectives)
	{
		const FNUQuestObjectiveProgress* Count = Progress->FindObjectiveProgress(Objective.ObjectiveId);
		if (!SourceInstanceId.IsNone() && Count != nullptr && Count->ProcessedSourceInstanceIds.Contains(SourceInstanceId)) { continue; }
		if (NUQuestObjective::CanAdvance(*Progress, *Definition, Objective, InObjectiveType, TargetId)) { return Objective.ObjectiveId; }
	}
	return NAME_None;
}

void UNUQuestComponent::AcceptQuestById(FName QuestId)
{
	const UNUQuestDefinition* Definition = FindQuestDefinition(QuestId);
	if (Definition == nullptr)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("AcceptQuestById 실패: 정의 없음 QuestId=%s"), *QuestId.ToString());
		return;
	}

	// 목표를 채워야 열리는 의뢰는 목표부터 채우고, 선행 퀘스트가 끝나는 대로 수락한다(서버는 응답 뒤).
	if (QuestId == ObjectiveOfferQuestId)
	{
		const FSimpleDelegate Completion = MoveTemp(ObjectiveOfferCompletion);
		ObjectiveOfferCompletion.Unbind();
		ObjectiveOfferQuestId = NAME_None;
		AcceptAfterObjectiveQuestId = QuestId;
		Completion.ExecuteIfBound();
		TryAcceptAfterObjective();
		return;
	}

	if (!HasQuest(QuestId) && !AddQuest(Definition, ENUQuestState::Available))
	{
		return;
	}

	if (AcceptQuest(Definition))
	{
		UE_LOG(LogNUQuest, Log, TEXT("Quest: UI/디버그 요청으로 수락 QuestId=%s"), *QuestId.ToString());
	}
}

// -------------------------------------------------------------------------
// Debug
// -------------------------------------------------------------------------

void UNUQuestComponent::DebugSetPlayerLevel(int32 NewLevel)
{
#if !UE_BUILD_SHIPPING
	TestPlayerLevel = FMath::Max(1, NewLevel);
	EvaluateAvailableQuests();
	NotifyQuestProgressChanged();

	UE_LOG(LogNUQuest, Log, TEXT("Quest Test: PlayerLevel=%d"), TestPlayerLevel);
#endif
}

void UNUQuestComponent::DebugAddFlag(FName FlagId)
{
#if !UE_BUILD_SHIPPING
	if (FlagId.IsNone())
	{
		return;
	}

	TestFlags.AddUnique(FlagId);
	EvaluateAvailableQuests();
	NotifyQuestProgressChanged();

	UE_LOG(LogNUQuest, Log, TEXT("Quest Test: Flag 추가 [%s]"), *FlagId.ToString());
#endif
}

void UNUQuestComponent::DebugProgressQuestById(FName QuestId, int32 Amount)
{
#if !UE_BUILD_SHIPPING
	const UNUQuestDefinition* Definition = FindQuestDefinition(QuestId);
	if (Definition == nullptr || Definition->Objectives.IsEmpty() || Amount <= 0)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("Quest Debug Progress 실패 QuestId=%s Amount=%d"),
			*QuestId.ToString(), Amount);
		return;
	}

	AddObjectiveProgress(Definition, Definition->Objectives[0].ObjectiveId, Amount);
#endif
}

void UNUQuestComponent::DebugProgressObjectiveById(FName QuestId, FName ObjectiveId, int32 Amount)
{
#if !UE_BUILD_SHIPPING
	const UNUQuestDefinition* Definition = FindQuestDefinition(QuestId);
	// 기존 진행 경로에서 목표 소속, 선행 목표, 서버 보고 가능 유형을 검사한다.
	if (Definition == nullptr || !AddObjectiveProgress(Definition, ObjectiveId, Amount))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("Quest Debug Progress 실패 QuestId=%s ObjectiveId=%s Amount=%d"),
			*QuestId.ToString(), *ObjectiveId.ToString(), Amount);
	}
#endif
}

bool UNUQuestComponent::DebugSkipLocalQuest(FName QuestId, FString& OutError)
{
	OutError.Reset();
#if !UE_BUILD_SHIPPING
	const UEmberServerManager* Server = GetServerManager();
	const ANUPlayerState* Player = Cast<ANUPlayerState>(GetOwner());
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() != NM_Standalone || Player == nullptr || !Player->HasAuthority()
		|| IsServerBackedSession() || (Server != nullptr && Server->IsLoggedIn()))
	{
		OutError = TEXT("서버에 접속하지 않은 로컬 Standalone에서만 사용할 수 있습니다.");
		return false;
	}
	if (Player->GetBattleEncounter() != nullptr || IsInQuestBattle())
	{
		OutError = TEXT("전투를 종료한 뒤 입력해주세요.");
		return false;
	}
	const FNUQuestProgressState* TargetProgress = FindQuestProgress(QuestId);
	if (TargetProgress != nullptr && TargetProgress->QuestState == ENUQuestState::Completed)
	{
		OutError = TEXT("이미 완료한 퀘스트입니다. 새 로컬 플레이에서 입력해주세요.");
		return false;
	}

	// 번호 범위가 아니라 실제 선행 관계를 따라 준비한다. 순환·누락은 변경 전에 거부한다.
	TArray<const UNUQuestDefinition*> Ordered;
	TSet<FName> Visiting;
	TSet<FName> Visited;
	int32 RequiredLevel = TestPlayerLevel;
	TFunction<bool(FName)> Visit = [&](FName Id)
	{
		if (Visited.Contains(Id)) { return true; }
		const UNUQuestDefinition* Definition = FindQuestDefinition(Id);
		if (Definition == nullptr || Visiting.Contains(Id))
		{
			OutError = FString::Printf(TEXT("퀘스트 정의가 없거나 선행 조건이 순환합니다: %s"), *Id.ToString());
			return false;
		}
		Visiting.Add(Id);
		for (const FNUQuestRequirementSpec& Requirement : Definition->StartRequirements)
		{
			if (Requirement.RequirementType == NU::Quest::Requirement::QuestCompleted)
			{
				if (!EvaluateRequirement(Requirement) && !Visit(Requirement.TargetId)) { return false; }
			}
			else if (Requirement.RequirementType == NU::Quest::Requirement::PlayerLevel)
			{
				RequiredLevel = FMath::Max(RequiredLevel, Requirement.RequiredValue);
			}
		}
		Visiting.Remove(Id);
		Visited.Add(Id);
		Ordered.Add(Definition);
		return true;
	};
	if (!Visit(QuestId)) { return false; }

	// 선행 퀘스트 보상은 적용하되 스킵한 퀘스트의 보상 창을 연속으로 열지 않는다.
	TGuardValue<bool> SkipGuard(bSkippingLocalQuestPrerequisites, true);
	DebugSetPlayerLevel(RequiredLevel);
	for (const UNUQuestDefinition* Definition : Ordered)
	{
		if (Definition->QuestId == QuestId) { continue; }
		if (!DebugCompleteLocalQuest(Definition->QuestId))
		{
			OutError = FString::Printf(TEXT("선행 퀘스트 %s 준비에 실패했습니다. 조건 또는 보상 설정을 확인해주세요."), *Definition->QuestId.ToString());
			return false;
		}
	}
	const UNUQuestDefinition* Target = FindQuestDefinition(QuestId);
	if (!HasQuest(QuestId) && !AddQuest(Target, ENUQuestState::Available))
	{
		OutError = TEXT("시작할 퀘스트를 등록하지 못했습니다.");
		return false;
	}
	TargetProgress = FindQuestProgress(QuestId);
	if (TargetProgress != nullptr && (TargetProgress->QuestState == ENUQuestState::Active || TargetProgress->QuestState == ENUQuestState::ReadyToTurnIn || AcceptQuest(Target))) { return true; }
	OutError = TEXT("선행 퀘스트와 레벨 외의 시작 조건을 만족하지 못했습니다.");
#else
	OutError = TEXT("개발 빌드에서만 사용할 수 있습니다.");
#endif
	return false;
}

bool UNUQuestComponent::DebugCompleteLocalQuest(FName QuestId)
{
#if !UE_BUILD_SHIPPING
	const UEmberServerManager* Server = GetServerManager();
	if (Server != nullptr && Server->IsLoggedIn()) { return false; }
	const ANUPlayerState* Player = Cast<ANUPlayerState>(GetOwner());
	if (Player != nullptr && Player->GetBattleEncounter() != nullptr) { return false; }
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() != NM_Standalone || GetOwner() == nullptr || !GetOwner()->HasAuthority()
		|| IsServerBackedSession() || IsInQuestBattle()) { return false; }
	const UNUQuestDefinition* Definition = FindQuestDefinition(QuestId);
	if (Definition == nullptr) { return false; }
	if (!HasQuest(QuestId) && (!AreStartRequirementsMet(Definition) || !AddQuest(Definition, ENUQuestState::Available))) { return false; }
	const FNUQuestProgressState* Progress = FindQuestProgress(QuestId);
	if (Progress == nullptr) { return false; }
	if (Progress->QuestState == ENUQuestState::Completed) { return true; }
	if (Progress->QuestState == ENUQuestState::Available && !AcceptQuest(Definition)) { return false; }

	// 플레이로 달성한 목표가 아니므로 후속 연출을 몰아서 재생하지 않게 알린다.
	++DebugCompletionSerial;

	// 선행 목표가 뒤쪽에 정의되어 있어도 최대 목표 수만큼 순회하면 진행할 수 있다.
	for (int32 Pass = 0; Pass < Definition->Objectives.Num(); ++Pass)
	{
		bool bChanged = false;
		for (const FNUQuestObjectiveSpec& Objective : Definition->Objectives)
		{
			Progress = FindQuestProgress(QuestId);
			if (Progress == nullptr || Progress->QuestState != ENUQuestState::Active) { break; }
			const FNUQuestObjectiveProgress* Item = Progress->FindObjectiveProgress(Objective.ObjectiveId);
			if (Item != nullptr && !Item->IsComplete(Objective.RequiredCount) && Progress->IsObjectivePrerequisiteMet(*Definition, Objective))
			{
				bChanged |= AddObjectiveProgress(Definition, Objective.ObjectiveId, Objective.RequiredCount - Item->CurrentCount);
			}
		}
		if (!bChanged) { break; }
	}
	Progress = FindQuestProgress(QuestId);
	return Progress != nullptr && (Progress->QuestState == ENUQuestState::Completed
		|| (Progress->QuestState == ENUQuestState::ReadyToTurnIn && CompleteQuest(Definition, Definition->TurnInActorId)));
#else
	return false;
#endif
}

void UNUQuestComponent::DebugCompleteQuestById(FName QuestId)
{
#if !UE_BUILD_SHIPPING
	const UNUQuestDefinition* Definition = FindQuestDefinition(QuestId);
	// 수동 보고 퀘스트도 정의된 NPC ID로 완료를 요청해 서버 검증 경로를 통과한다.
	if (Definition == nullptr || !CompleteQuest(Definition, Definition->TurnInActorId))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("Quest Debug Complete 실패 QuestId=%s"), *QuestId.ToString());
	}
#endif
}

// -------------------------------------------------------------------------
// Server
// -------------------------------------------------------------------------

void UNUQuestComponent::RequestServerQuestList()
{
	UEmberServerManager* Server = GetServerManager();
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUCharacterManager* CharacterManager = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUCharacterManager>()
		: nullptr;

	if (Server == nullptr || !Server->IsLoggedIn()
		|| CharacterManager == nullptr || CharacterManager->GetSelectedCharacterIdx() == 0)
	{
		return;
	}

	// 로그인과 캐릭터 선택이 끝난 뒤, 해당 캐릭터의 저장된 퀘스트 진행도를 요청한다.
	bServerBackedSession = true;
	bServerQuestListReady = false;
	ResetServerRequests();
	Server->SendQuestList();
	UE_LOG(LogNUQuest, Log, TEXT("QuestList 전송"));
}

void UNUQuestComponent::HandleServerListResponse(int32 ResultCode, const FString& ResponseData)
{
	if (ResultCode != 200)
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestList 실패 (%d)"), ResultCode);
		return;
	}

	TArray<FNUQuestServerProgress> ServerProgressList;
	FString Error;
	if (!NUQuestServer::ParseList(ResponseData, ServerProgressList, Error))
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestList JSON 파싱 실패: %s - %s"), *Error, *ResponseData);
		return;
	}

	for (FNUQuestProgressState& Progress : QuestProgressList)
	{
		for (FNUQuestObjectiveProgress& Objective : Progress.ObjectiveProgress)
		{
			Objective.ProcessedSourceInstanceIds.Reset();
		}
	}

	TSet<FName> ServerQuestIds;
	for (const FNUQuestServerProgress& ServerProgress : ServerProgressList)
	{
		const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(ServerProgress.QuestId);
		if (Definition == nullptr)
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestList에 클라이언트 정의가 없는 QuestId=%d"), ServerProgress.QuestId);
			continue;
		}

		// 퀘스트 규칙과 표시 정보는 클라이언트 테이블, 진행 상태는 서버 응답을 사용한다.
		ServerQuestIds.Add(Definition->QuestId);
		if (!ApplyServerProgress(ServerProgress))
		{
			UE_LOG(LogNUQuest, Error, TEXT("QuestList 목표 계약 불일치 QuestId=%d"), ServerProgress.QuestId);
			return;
		}
	}

	for (const auto& Entry : PendingServerPushes)
	{
		if (const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(Entry.Key))
		{
			if (!ApplyServerProgress(Entry.Value))
			{
				return;
			}
			ServerQuestIds.Add(Definition->QuestId);
		}
	}

	PendingServerPushes.Reset();

	// 목록에 없는 행은 서버 기준 미수락이다. NPC가 미리 만든 표시용 행만 초기 상태로 되돌린다.
	for (FNUQuestProgressState& Progress : QuestProgressList)
	{
		if (ServerQuestIds.Contains(Progress.QuestId))
		{
			continue;
		}

		const UNUQuestDefinition* Definition = FindQuestDefinition(Progress.QuestId);
		if (Definition != nullptr)
		{
			Progress.Initialize(*Definition,
				AreStartRequirementsMet(Definition) ? ENUQuestState::Available : ENUQuestState::Locked);
		}
	}

	bServerQuestListReady = true;
	EvaluateAvailableQuests();
	NotifyQuestProgressChanged();

	for (const FNUQuestProgressState& Progress : QuestProgressList)
	{
		const UNUQuestDefinition* Definition = FindQuestDefinition(Progress.QuestId);

		if (Definition != nullptr && Definition->bAutoComplete && Progress.QuestState == ENUQuestState::ReadyToTurnIn)
		{
			CompleteQuest(Definition);
		}
	}

	UE_LOG(LogNUQuest, Log, TEXT("QuestList 적용: %d개"), ServerProgressList.Num());
}

// 104는 전체 진행 스냅샷이다. 목표 수량이나 전투 성공을 클라이언트에서 다시 더하지 않는다.
void UNUQuestComponent::HandleServerQuestUpdated(int32 QuestId, int32 Reason, const FString& ResponseData)
{
	if (!bServerBackedSession || (Reason != 1 && Reason != 2))
	{
		return;
	}

	FNUQuestServerProgress Progress;
	FString Error;

	if (!NUQuestServer::ParseProgress(ResponseData, Progress, Error) || Progress.QuestId != QuestId)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestUpdated 적용 실패 QuestId=%d Error=%s"), QuestId, *Error);
		return;
	}
	if (!bServerQuestListReady)
	{
		const FNUQuestServerProgress* Pending = PendingServerPushes.Find(QuestId);

		if (Pending == nullptr || Progress.Revision > Pending->Revision)
		{
			PendingServerPushes.Add(QuestId, Progress);
		}

		return;
	}

	if (!ApplyServerProgress(Progress))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestUpdated 목표 계약 불일치 QuestId=%d"), QuestId);
		return;
	}

	EvaluateAvailableQuests();
	NotifyQuestProgressChanged();
	const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(QuestId);
	const FNUQuestProgressState* Current = Definition != nullptr ? FindQuestProgress(Definition->QuestId) : nullptr;

	if (Definition != nullptr && Definition->bAutoComplete && Current != nullptr && Current->QuestState == ENUQuestState::ReadyToTurnIn)
	{
		QueueServerCompletion(QuestId);
	}
}

void UNUQuestComponent::HandleServerAcceptResponse(int32 ResultCode, const FString& ResponseData)
{
	const int32 RequestedQuestId = AcceptRequestInFlight;
	if (RequestedQuestId == 0 || !bServerQuestListReady)
	{
		return;
	}
	AcceptRequestInFlight = 0;
	FNUQuestServerProgress ServerProgress;
	FString Error;
	if (ResultCode != 200 || !NUQuestServer::ParseProgress(ResponseData, ServerProgress, Error)
		|| ServerProgress.QuestId != RequestedQuestId || ServerProgress.State != 1
		|| !ApplyServerProgress(ServerProgress))
	{
		if (const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(RequestedQuestId))
		{
			FailedAutoAcceptQuestIds.Add(Definition->QuestId);
		}
		UE_LOG(LogNUQuest, Warning, TEXT("QuestAccept 실패 Code=%d QuestId=%d Error=%s"), ResultCode, RequestedQuestId, *Error);
		EvaluateAvailableQuests();
		return;
	}

	if (const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(ServerProgress.QuestId))
	{
		BroadcastQuestAccepted(Definition->QuestId);
	}
	NotifyQuestProgressChanged();
	EvaluateAvailableQuests();
	if (const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(ServerProgress.QuestId); Definition != nullptr && Definition->bAutoComplete)
	{
		CompleteQuest(Definition);
	}

	UE_LOG(LogNUQuest, Log, TEXT("QuestAccept 성공 QuestId=%d"), ServerProgress.QuestId);
}

void UNUQuestComponent::HandleServerObjectiveResponse(int32 ResultCode, const FString& ResponseData)
{
	const int32 RequestedQuestId = ObjectiveRequestInFlight.QuestId;
	const int32 RequestedObjectiveId = ObjectiveRequestInFlight.ObjectiveId;
	if (RequestedQuestId <= 0 || !bServerQuestListReady)
	{
		return;
	}

	if (ResultCode != 200)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestObjective 거절 (%d) QuestId=%d"), ResultCode, RequestedQuestId);
		ObjectiveRequestInFlight.Reset();
		QueuedCompletionRequests.Remove(RequestedQuestId);
		RequestServerQuestList();
		return;
	}

	FNUQuestServerProgress ServerProgress;
	FString Error;
	if (!NUQuestServer::ParseProgress(ResponseData, ServerProgress, Error)
		|| ServerProgress.QuestId != RequestedQuestId
		|| ServerProgress.State != 1
		|| !ApplyServerProgress(ServerProgress))
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestObjective 응답 적용 실패: %s"), *Error);
		ObjectiveRequestInFlight.Reset();
		QueuedCompletionRequests.Remove(RequestedQuestId);
		RequestServerQuestList();
		return;
	}

	// 확정 수량만 표시하고, 응답 대기 중 쌓인 다음 누적 요청은 별도로 전송한다.
	ObjectiveRequestInFlight.Reset();
	NotifyQuestProgressChanged();
	UE_LOG(LogNUQuest, Log, TEXT("QuestObjective 성공 QuestId=%d"), ServerProgress.QuestId);
	SendNextServerObjective();

	const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(ServerProgress.QuestId);
	const FNUQuestProgressState* Progress = Definition != nullptr
		? FindQuestProgress(Definition->QuestId)
		: nullptr;
	if (Definition != nullptr && Progress != nullptr && Progress->QuestState == ENUQuestState::ReadyToTurnIn)
	{
		// 자동 완료도 마지막 진행도 응답이 적용된 뒤 서버에 완료 요청을 보낸다.
		if (Definition->bAutoComplete)
		{
			QueueServerCompletion(ServerProgress.QuestId);
		}

		// 보고 NPC와의 대화로 마지막 목표를 채웠으면 그 대화에서 바로 수령한다.
		const FNUQuestObjectiveSpec* TurnInTalk = Definition->FindTurnInTalkObjective();
		int32 TurnInTalkId = 0;
		int32 NpcId = 0;
		if (TurnInTalk != nullptr && TryGetServerObjectiveId(*Definition, TurnInTalk->ObjectiveId, TurnInTalkId) && TurnInTalkId == RequestedObjectiveId
			&& LexTryParseString(NpcId, *Definition->TurnInActorId.ToString()) && NpcId > 0)
		{
			QueueServerCompletion(ServerProgress.QuestId, NpcId);
		}
	}
	SendNextServerCompletion();
}

void UNUQuestComponent::HandleServerCalmResultResponse(int32 ResultCode, const FString& ResponseData)
{
	if (!bServerQuestListReady)
	{
		return;
	}

	if (ResultCode != 200)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestCalmResult 거절 (%d)"), ResultCode);
		RequestServerQuestList();
		return;
	}

	FNUQuestServerProgress ServerProgress;
	FString Error;
	if (!NUQuestServer::ParseProgress(ResponseData, ServerProgress, Error) || !ApplyServerProgress(ServerProgress))
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestCalmResult 응답 적용 실패: %s"), *Error);
		RequestServerQuestList();
		return;
	}

	NotifyQuestProgressChanged();
	UE_LOG(LogNUQuest, Log, TEXT("QuestCalmResult 적용 QuestId=%d"), ServerProgress.QuestId);
}

void UNUQuestComponent::HandleServerCompleteResponse(int32 ResultCode, const FString& ResponseData)
{
	const int32 RequestedQuestId = CompletionRequestInFlight;
	if (RequestedQuestId <= 0 || !bServerQuestListReady)
	{
		return;
	}

	CompletionRequestInFlight = 0;

	if (ResultCode != 200)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestComplete 거절 (%d) QuestId=%d"), ResultCode, RequestedQuestId);
		SendNextServerCompletion();
		return;
	}

	FNUQuestServerProgress ServerProgress;
	FNUQuestServerReward Reward;
	FString Error;
	if (!NUQuestServer::ParseComplete(ResponseData, ServerProgress, Reward, Error)
		|| ServerProgress.QuestId != RequestedQuestId
		|| ServerProgress.State != 2
		|| !ApplyServerProgress(ServerProgress))
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestComplete 응답 적용 실패: %s"), *Error);
		SendNextServerCompletion();
		return;
	}

	const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(ServerProgress.QuestId);
	if (Definition != nullptr)
	{
		BroadcastQuestCompleted(Definition->QuestId, Reward);
	}

	if (Reward.Experience > 0 || Reward.bHasGrowth)
	{
		UNUProgressionComponent* ProgressionComponent = GetOwner() != nullptr
			? GetOwner()->FindComponentByClass<UNUProgressionComponent>()
			: nullptr;
		if (ProgressionComponent == nullptr || !ProgressionComponent->RequestProgressionState())
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestComplete 이후 성장 상태 재조회 실패 QuestId=%d"), ServerProgress.QuestId);
		}
	}
	if (Reward.Experience > 0 && !Reward.bHasGrowth)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestComplete 보상에 Growth가 없습니다. QuestId=%d Exp=%lld"), ServerProgress.QuestId, Reward.Experience);
	}
	if (Reward.bHasGrowth)
	{
		UE_LOG(LogNUQuest, Log, TEXT("QuestComplete Growth QuestId=%d Lv=%d->%d Exp=%lld->%lld"),
			ServerProgress.QuestId,
			Reward.Growth.OldLevel,
			Reward.Growth.NewLevel,
			Reward.Growth.OldTotalExperience,
			Reward.Growth.NewTotalExperience);
	}

	// 지급한 아이템은 기존 인벤토리 스냅샷으로 다시 읽는다. 로컬 지급은 하지 않는다.
	if (InventoryComponent.IsValid())
	{
		InventoryComponent->RequestServerInventory();
		InventoryComponent->ShowRewardMissingNotice(ResponseData); // 가방이 가득 차 못 받은 보상(Reward.MissingItems) 안내
	}

	// 보상 지급은 서버에서 끝났으며, 클라이언트는 결과 표시와 후속 퀘스트 갱신만 담당한다.
	UE_LOG(LogNUQuest, Log, TEXT("QuestComplete 성공 QuestId=%d Exp=%lld Gold=%lld"),
		ServerProgress.QuestId, Reward.Experience, Reward.Gold);
	EvaluateAvailableQuests();
	NotifyQuestProgressChanged();
	SendNextServerCompletion();
}

UEmberServerManager* UNUQuestComponent::GetServerManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
}

bool UNUQuestComponent::IsServerQuestConnected() const
{
	const UEmberServerManager* Server = GetServerManager();
	
	return Server != nullptr && Server->IsLoggedIn();
}

bool UNUQuestComponent::TryGetServerQuestId(const UNUQuestDefinition& Definition, int32& OutQuestId) const
{
	if (NUQuestServer::TryConvertQuestId(Definition.QuestNumericId, OutQuestId))
	{
		return true;
	}

	UE_LOG(LogNUQuest, Warning, TEXT("서버 전송 불가 QuestNumericId=%lld QuestId=%s"),
		Definition.QuestNumericId, *Definition.QuestId.ToString());
	
	return false;
}

// 숫자 목표 ID를 그대로 사용한다. 기존 단일 문자열 목표만 옛 quest_id 별칭을 유지한다.
bool UNUQuestComponent::TryGetServerObjectiveId(const UNUQuestDefinition& Definition, FName ObjectiveId, int32& OutObjectiveId) const
{
	// 목표 ID는 서버와 같은 양의 정수다.
	return LexTryParseString(OutObjectiveId, *ObjectiveId.ToString()) && OutObjectiveId > 0;
}

bool UNUQuestComponent::ApplyServerProgress(const FNUQuestServerProgress& ServerProgress)
{
	const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(ServerProgress.QuestId);
	if (Definition == nullptr || Definition->Objectives.IsEmpty())
	{
		return false;
	}

	// 직접 응답과 푸시가 엇갈려도 이미 적용한 확정 상태를 되돌리지 않는다.
	if (const int64* Revision = ServerQuestRevisions.Find(ServerProgress.QuestId); Revision != nullptr && ServerProgress.Revision <= *Revision)
	{
		return true;
	}

	TMap<FName, int32> Counts;
	for (const FNUQuestServerObjective& ServerObjective : ServerProgress.Objectives)
	{
		const FNUQuestObjectiveSpec* Matched = nullptr;
		for (const FNUQuestObjectiveSpec& Objective : Definition->Objectives)
		{
			int32 Id = 0;
			if (TryGetServerObjectiveId(*Definition, Objective.ObjectiveId, Id) && Id == ServerObjective.ObjectiveId)
			{
				Matched = &Objective;
				break;
			}
		}

		// 서버의 옛 quest_id 별칭은 첫 목표를 뜻한다.
		if (Matched == nullptr && ServerObjective.ObjectiveId == ServerProgress.QuestId)
		{
			Matched = &Definition->Objectives[0];
		}

		if (Matched == nullptr || Counts.Contains(Matched->ObjectiveId))
		{
			return false;
		}
		Counts.Add(Matched->ObjectiveId, FMath::Min(ServerObjective.CurrentCount, Matched->RequiredCount));
	}
	// 전체 스냅샷 계약이다. 누락된 목표를 이전 로컬 값으로 채우지 않는다.
	if (Counts.Num() != Definition->Objectives.Num())
	{
		return false;
	}

	FNUQuestProgressState* Progress = FindQuestProgressMutable(Definition->QuestId);
	if (Progress == nullptr)
	{
		Progress = &QuestProgressList.AddDefaulted_GetRef();
		Progress->Initialize(*Definition, ENUQuestState::Locked);
	}

	// 소리는 접속 직후 목록 복원이 아니라 진행 중이던 퀘스트가 실제로 나아간 전환에만 낸다
	const bool bWasActive = bServerQuestListReady && Progress->QuestState == ENUQuestState::Active;
	bool bCountIncreased = false;
	for (FNUQuestObjectiveProgress& Objective : Progress->ObjectiveProgress)
	{
		const int32 NewCount = Counts.FindChecked(Objective.ObjectiveId);
		bCountIncreased |= NewCount > Objective.CurrentCount;
		Objective.CurrentCount = NewCount;
	}

	Progress->CompletionCount = ServerProgress.CompletionCount;
	if (ServerProgress.State == 2)
	{
		Progress->QuestState = ENUQuestState::Completed;
	}
	else if (ServerProgress.State == 1)
	{
		Progress->QuestState = Progress->IsRequiredObjectivesComplete(*Definition)
			? ENUQuestState::ReadyToTurnIn
			: ENUQuestState::Active;
	}
	else
	{
		Progress->QuestState = ServerProgress.bCanAccept && AreStartRequirementsMet(Definition)
			? ENUQuestState::Available
			: ENUQuestState::Locked;
	}

	if (bWasActive && Progress->QuestState == ENUQuestState::ReadyToTurnIn)
	{
		// 바로 완료되는 퀘스트는 완료음이 이어지므로 보고 대기음을 내지 않는다
		if (!Definition->bAutoComplete && Definition->FindTurnInTalkObjective() == nullptr)
		{
			UNUSFXSubsystem::Play(this, NU::SFX::Quest::Ready);
		}
	}
	else if (bWasActive && bCountIncreased && Progress->QuestState == ENUQuestState::Active)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Quest::Progress);
	}

	ServerQuestRevisions.Add(ServerProgress.QuestId, ServerProgress.Revision);
	return true;
}

bool UNUQuestComponent::QueueServerObjective(const UNUQuestDefinition& Definition, FName ObjectiveId, int32 CurrentCount)
{
	int32 QuestId = 0;
	int32 ServerObjectiveId = 0;
	if (!TryGetServerQuestId(Definition, QuestId) || !TryGetServerObjectiveId(Definition, ObjectiveId, ServerObjectiveId))
	{
		return false;
	}

	// 복수 목표가 서로 덮어쓰지 않도록 퀘스트와 목표 ID를 함께 키로 사용한다.
	const uint64 Key = (uint64(uint32(QuestId)) << 32) | uint32(ServerObjectiveId);
	FPendingObjectiveRequest& Queued = QueuedObjectiveRequests.FindOrAdd(Key);
	Queued.QuestId = QuestId;
	Queued.ObjectiveId = ServerObjectiveId;
	Queued.CurrentCount = FMath::Max(Queued.CurrentCount, CurrentCount);
	SendNextServerObjective();
	return true;
}

void UNUQuestComponent::SendNextServerObjective()
{
	// 응답 순서를 보장하기 위해 진행도 요청은 한 번에 하나만 전송한다.
	if (ObjectiveRequestInFlight.IsValid() || QueuedObjectiveRequests.IsEmpty() || !IsServerQuestConnected())
	{
		return;
	}

	auto It = QueuedObjectiveRequests.CreateIterator();
	ObjectiveRequestInFlight = It.Value();
	It.RemoveCurrent();

	GetServerManager()->SendQuestObjective(
		ObjectiveRequestInFlight.QuestId,
		ObjectiveRequestInFlight.ObjectiveId,
		ObjectiveRequestInFlight.CurrentCount);
}

bool UNUQuestComponent::HasPendingServerObjective(int32 QuestId) const
{
	if (ObjectiveRequestInFlight.QuestId == QuestId)
	{
		return true;
	}
	for (const auto& Entry : QueuedObjectiveRequests)
	{
		if (Entry.Value.QuestId == QuestId)
		{
			return true;
		}
	}
	return false;
}

void UNUQuestComponent::QueueServerCompletion(int32 QuestId, int32 NpcId)
{
	if (CompletionRequestInFlight == QuestId)
	{
		return;
	}
	// 104 푸시와 목표 응답도 이 경로를 거친다. 표시뿐 아니라 서버 보상 확정 요청 자체를 기다린다.
	if (const UNUQuestDefinition* Definition = FindQuestDefinitionByServerId(QuestId);
		Definition != nullptr && DeferBattleCompletion(*Definition)) { return; }
	CompletionNpcIds.Add(QuestId, NpcId);
	QueuedCompletionRequests.AddUnique(QuestId);
	SendNextServerCompletion();
}

void UNUQuestComponent::SendNextServerCompletion()
{
	if (CompletionRequestInFlight > 0 || QueuedCompletionRequests.IsEmpty() || !IsServerQuestConnected())
	{
		return;
	}

	for (int32 Index = 0; Index < QueuedCompletionRequests.Num(); ++Index)
	{
		const int32 QuestId = QueuedCompletionRequests[Index];
		// 서버가 최신 진행도를 확인하기 전에 완료 요청이 먼저 도착하지 않도록 기다린다.
		if (HasPendingServerObjective(QuestId))
		{
			continue;
		}

		CompletionRequestInFlight = QuestId;
		QueuedCompletionRequests.RemoveAt(Index);
		GetServerManager()->SendQuestComplete(QuestId, CompletionNpcIds.FindRef(QuestId));
		CompletionNpcIds.Remove(QuestId);
		
		return;
	}
}

void UNUQuestComponent::ResetServerRequests()
{
	AcceptRequestInFlight = 0;
	ServerQuestRevisions.Reset();
	PendingServerPushes.Reset();
	CompletionNpcIds.Reset();
	FailedAutoAcceptQuestIds.Reset();
	ObjectiveRequestInFlight.Reset();
	QueuedObjectiveRequests.Reset();
	CompletionRequestInFlight = 0;
	QueuedCompletionRequests.Reset();
	LastCompletedQuestId = NAME_None;
	LastCompletionReward = FNUQuestServerReward();
	bHasLastCompletionReward = false;
}

void UNUQuestComponent::HandleEmberStateChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn)
	{
		RequestServerQuestList();
	}
	else if (bServerBackedSession)
	{
		bServerQuestListReady = false;
		ResetServerRequests();
		NotifyQuestProgressChanged();
	}
}

// -------------------------------------------------------------------------
// Progression
// -------------------------------------------------------------------------

void UNUQuestComponent::HandleProgressionChanged()
{
	AActor* Owner = GetOwner();
	const UNUProgressionComponent* ProgressionComponent = Owner != nullptr
		? Owner->FindComponentByClass<UNUProgressionComponent>()
		: nullptr;
	if (Owner == nullptr || !Owner->HasAuthority() || ProgressionComponent == nullptr)
	{
		return;
	}

	// 한 세션에서 레벨은 내려가지 않는다. 퀘스트 완료 응답이 임시로 올린 레벨도
	// 성장 서버 응답이 도착하기 전까지 유지한다.
	const int32 NewPlayerLevel = FMath::Max(TestPlayerLevel, ProgressionComponent->GetLevel());
	if (TestPlayerLevel == NewPlayerLevel)
	{
		return;
	}

	TestPlayerLevel = NewPlayerLevel;
	EvaluateAvailableQuests();
	NotifyQuestProgressChanged();

	UE_LOG(LogNUQuest, Log, TEXT("Quest: 성장 레벨 동기화 PlayerLevel=%d"), TestPlayerLevel);
}

// -------------------------------------------------------------------------
// Query
// -------------------------------------------------------------------------

const FNUQuestProgressState* UNUQuestComponent::FindQuestProgress(FName QuestId) const
{
	return QuestProgressList.FindByPredicate(
		[QuestId] (const FNUQuestProgressState& Progress)
		{
			return Progress.QuestId == QuestId;
		}
	);
}

bool UNUQuestComponent::HasQuest(FName QuestId) const
{
	return FindQuestProgress(QuestId) != nullptr;
}

bool UNUQuestComponent::IsQuestProgressReady() const
{
	return HasBegunPlay() && (!bServerBackedSession || bServerQuestListReady);
}

const TArray<FNUQuestProgressState>& UNUQuestComponent::GetQuestProgressList() const
{
	return QuestProgressList;
}

bool UNUQuestComponent::TryGetLastCompletionReward(FName QuestId, FNUQuestServerReward& OutReward) const
{
	if (!bHasLastCompletionReward || QuestId.IsNone() || QuestId != LastCompletedQuestId)
	{
		return false;
	}

	OutReward = LastCompletionReward;
	return true;
}

bool UNUQuestComponent::HasTestFlag(FName FlagId) const
{
	return TestFlags.Contains(FlagId);
}

bool UNUQuestComponent::HasTestUnlockedFeature(FName FeatureId) const
{
	return TestUnlockedFeatures.Contains(FeatureId);
}

FNUQuestProgressState* UNUQuestComponent::FindQuestProgressMutable(FName QuestId)
{
	return QuestProgressList.FindByPredicate(
		[QuestId](const FNUQuestProgressState& Progress)
		{
			return Progress.QuestId == QuestId;
		}
	);
}

const UNUQuestDefinition* UNUQuestComponent::FindQuestDefinition(FName QuestId) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUQuestDataSubsystem* QuestDataSubsystem =
		GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUQuestDataSubsystem>()
		: nullptr;

	return QuestDataSubsystem != nullptr
		? QuestDataSubsystem->FindDefinition(QuestId)
		: nullptr;
}

const UNUQuestDefinition* UNUQuestComponent::FindQuestDefinitionByServerId(int32 QuestId) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUQuestDataSubsystem* QuestDataSubsystem = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUQuestDataSubsystem>()
		: nullptr;

	return QuestDataSubsystem != nullptr
		? QuestDataSubsystem->FindDefinitionByNumericId(QuestId)
		: nullptr;
}

// -------------------------------------------------------------------------
// Requirements
// -------------------------------------------------------------------------

bool UNUQuestComponent::EvaluateRequirement(const FNUQuestRequirementSpec& Requirement) const
{
	if (Requirement.RequirementType == NU::Quest::Requirement::PlayerLevel)
	{
		return TestPlayerLevel >= Requirement.RequiredValue;
	}

	if (Requirement.RequirementType == NU::Quest::Requirement::QuestCompleted)
	{
		const FNUQuestProgressState* Progress = FindQuestProgress(Requirement.TargetId);
		return Progress != nullptr && Progress->CompletionCount > 0;
	}

	if (Requirement.RequirementType == NU::Quest::Requirement::QuestGroupCompletedCount)
	{
		int32 CompletedCount = 0;

		for (const FNUQuestProgressState& Progress : QuestProgressList)
		{
			if (Progress.CompletionCount <= 0)
			{
				continue;
			}

			const UNUQuestDefinition* CompletedDefinition = FindQuestDefinition(Progress.QuestId);
			if (CompletedDefinition != nullptr && CompletedDefinition->QuestGroupId == Requirement.TargetId)
			{
				++CompletedCount;
			}
		}

		return CompletedCount >= Requirement.RequiredValue;
	}

	if (Requirement.RequirementType == NU::Quest::Requirement::FlagSet)
	{
		return TestFlags.Contains(Requirement.TargetId);
	}

	if (Requirement.RequirementType == NU::Quest::Requirement::FlagNotSet)
	{
		return !TestFlags.Contains(Requirement.TargetId);
	}

	UE_LOG(LogNUQuest, Warning, TEXT("Quest: 지원하지 않는 선행 조건 [%s]"), *Requirement.RequirementType.ToString());
	return false;
}

// -------------------------------------------------------------------------
// Rewards
// -------------------------------------------------------------------------

bool UNUQuestComponent::CanApplyRewards(const UNUQuestDefinition& Definition) const
{
	const UNUInventoryComponent* Inventory = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UNUInventoryComponent>() : nullptr;
	TMap<FName, int32> Items;
	int64 Experience = 0;
	for (const FNUQuestRewardSpec& Reward : Definition.Rewards)
	{
		if (Reward.RewardType == NU::Quest::Reward::Experience) { Experience += Reward.Amount; }
		if (Reward.RewardType == NU::Quest::Reward::Gold || Reward.RewardType == NU::Quest::Reward::Item)
		{
			if (Inventory == nullptr || Inventory->IsServerInventoryActive()) { return false; }
			if (Reward.RewardType == NU::Quest::Reward::Item)
			{
				int32 ItemId = 0;
				if (!LexTryParseString(ItemId, *Reward.TargetId.ToString())) { return false; }
				const FName Row = Inventory->FindRowNameByNumericId(ItemId);
				if (Row.IsNone() || Items.FindRef(Row) > MAX_int32 - Reward.Amount) { return false; }
				Items.FindOrAdd(Row) += Reward.Amount;
			}
		}
		// 여기서 false가 나면 퀘스트가 ReadyToTurnIn에 머무른다.
		// 원인을 못 찾는 상황을 막기 위해 반드시 로그를 남긴다.
		FString RewardError;
		if (!Reward.IsApplicable(RewardError))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("Quest: 보상을 적용할 수 없어 완료를 중단합니다 QuestId=%s - %s"),
				*Definition.QuestId.ToString(),
				*RewardError
			);

			return false;
		}
	}

	if (!Items.IsEmpty() && !Inventory->CanReceiveItems(Items))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("퀘스트 보상을 모두 받을 가방 공간이 부족합니다: %s"), *Definition.QuestId.ToString());
		return false;
	}
	if (Experience > 0)
	{
		const UNUProgressionComponent* Progression = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UNUProgressionComponent>() : nullptr;
		if (Progression != nullptr)
		{
			if (!Progression->IsUsingLocalProgressionBackend()) { return false; }
		}
	}
	return true;
}

bool UNUQuestComponent::ApplyRewards(const UNUQuestDefinition& Definition)
{
	UNUInventoryComponent* Inventory = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UNUInventoryComponent>() : nullptr;
	UNUProgressionComponent* Progression = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UNUProgressionComponent>() : nullptr;
	TMap<FName, int32> Items;
	int64 Experience = 0;
	int64 Gold = 0;
	for (const FNUQuestRewardSpec& Reward : Definition.Rewards)
	{
		if (Reward.RewardType == NU::Quest::Reward::Experience) { Experience += Reward.Amount; }
		else if (Reward.RewardType == NU::Quest::Reward::Gold) { Gold += Reward.Amount; }
		else if (Reward.RewardType == NU::Quest::Reward::Item && Inventory != nullptr)
		{
			int32 ItemId = 0;
			LexTryParseString(ItemId, *Reward.TargetId.ToString());
			Items.FindOrAdd(Inventory->FindRowNameByNumericId(ItemId)) += Reward.Amount;
		}
	}
	const auto ApplyExperience = [Progression, Experience]() { return Experience <= 0 || Progression == nullptr || Progression->ApplyLocalExperienceReward(Experience); };
	if (!Items.IsEmpty()) { if (Inventory == nullptr || !Inventory->AddItemsAtomic(Items, ApplyExperience)) { return false; } }
	else if (!ApplyExperience()) { return false; }
	if (Gold > 0 && Inventory != nullptr) { Inventory->AddGold(Gold); }
	for (const FNUQuestRewardSpec& Reward : Definition.Rewards)
	{
		if (Reward.RewardType == NU::Quest::Reward::Experience)
		{
			TestExperience += Reward.Amount;

		}
		else if (Reward.RewardType == NU::Quest::Reward::SetFlag)
		{
			TestFlags.AddUnique(Reward.TargetId);
		}
		else if (Reward.RewardType == NU::Quest::Reward::UnlockFeature)
		{
			TestUnlockedFeatures.AddUnique(Reward.TargetId);
		}
	}

	UE_LOG(LogNUQuest, Log, TEXT("Quest: 보상 적용 QuestId=%s Experience=%d"),
		*Definition.QuestId.ToString(),
		TestExperience
	);
	return true;
}

// -------------------------------------------------------------------------
// Requirements
// -------------------------------------------------------------------------

bool UNUQuestComponent::HasCompletedQuests(const TArray<FName>& QuestIds) const
{
	if (!QuestIds.IsEmpty() && bServerBackedSession && !bServerQuestListReady)
	{
		return false;
	}
	for (FName QuestId : QuestIds)
	{
		const FNUQuestProgressState* Progress = FindQuestProgress(QuestId);
		if (QuestId.IsNone() || Progress == nullptr || (Progress->CompletionCount == 0 && Progress->QuestState != ENUQuestState::Completed))
		{
			return false;
		}
	}
	return true;
}

void UNUQuestComponent::EvaluateAvailableQuests()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !bAutoAcceptInitialized
		|| (bServerBackedSession && !bServerQuestListReady))
	{
		return;
	}
	if (bEvaluatingAvailableQuests)
	{
		bReevaluateAvailableQuests = true;
		return;
	}
	TGuardValue<bool> EvaluationGuard(bEvaluatingAvailableQuests, true);
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUQuestDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
	if (Data != nullptr)
	{
		for (FName QuestId : Data->GetAutoAcceptQuestIds())
		{
			if (!HasQuest(QuestId))
			{
				AddQuest(Data->FindDefinition(QuestId), ENUQuestState::Locked);
			}
		}
	}

	do
	{
		bReevaluateAvailableQuests = false;
		TArray<FName> Candidates;
		for (FNUQuestProgressState& Progress : QuestProgressList)
		{
			if (Progress.QuestState != ENUQuestState::Locked && Progress.QuestState != ENUQuestState::Available)
			{
				continue;
			}
			const UNUQuestDefinition* Definition = FindQuestDefinition(Progress.QuestId);
			if (Definition == nullptr)
			{
				continue;
			}
			Progress.QuestState = AreStartRequirementsMet(Definition) ? ENUQuestState::Available : ENUQuestState::Locked;
			if (Progress.QuestState == ENUQuestState::Locked)
			{
				FailedAutoAcceptQuestIds.Remove(Progress.QuestId);
			}
			else if (Definition->bAutoAccept && !FailedAutoAcceptQuestIds.Contains(Progress.QuestId))
			{
				Candidates.Add(Progress.QuestId);
			}
		}
		// 수락/즉시 완료가 진행 배열과 선행 조건을 바꿀 수 있으므로 순회 이후 실행한다.
		for (FName QuestId : Candidates)
		{
			if (bServerBackedSession && AcceptRequestInFlight != 0)
			{
				break;
			}
			AcceptQuest(FindQuestDefinition(QuestId));
		}
	} while (bReevaluateAvailableQuests);

	// 게시판 의뢰처럼 앞 퀘스트 완료를 기다리던 수락을 이어서 처리한다.
	TryAcceptAfterObjective();
}

// -------------------------------------------------------------------------
// Location
// -------------------------------------------------------------------------

void UNUQuestComponent::InitializeLocationTracking()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}
	LocationComponent = Owner->FindComponentByClass<UNUWorldLocationComponent>();
	if (LocationComponent.IsValid())
	{
		LocationComponent->OnLocationOverlapsChanged.AddUniqueDynamic(this, &ThisClass::EvaluateCurrentLocations);
		RefreshLocationObjectives();
	}
}

void UNUQuestComponent::UnbindLocationTracking()
{
	if (LocationComponent.IsValid())
	{
		LocationComponent->OnLocationOverlapsChanged.RemoveDynamic(this, &ThisClass::EvaluateCurrentLocations);
	}
	LocationComponent = nullptr;
}

void UNUQuestComponent::RefreshLocationObjectives()
{
	if (bIsEvaluatingLocations || !LocationComponent.IsValid())
	{
		return;
	}
	// 수락 전에 진입했거나 같은 프레임에 이동한 경우도 실제 겹침으로 다시 확인한다.
	LocationComponent->RefreshLocation();
	EvaluateCurrentLocations();
}

void UNUQuestComponent::EvaluateCurrentLocations()
{
	const AActor* Owner = GetOwner();
	if (bIsEvaluatingLocations || !LocationComponent.IsValid() || Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}
	TGuardValue<bool> Guard(bIsEvaluatingLocations, true);
	const TArray<FName> Locations = LocationComponent->GetOverlappingLocationIds();
	for (FName LocationId : Locations)
	{
		FNUQuestEvent Event;
		Event.ObjectiveType = NU::Quest::Objective::ReachLocation;
		Event.TargetId = LocationId;
		Event.Amount = 1;
		// 목표별 기존 중복 방지를 사용한다. 다음 퀘스트나 재수락까지 막지 않는다.
		Event.SourceInstanceId = FName(*(TEXT("Location:") + LocationId.ToString()));
		HandleQuestEvent(Event);
	}
}

// -------------------------------------------------------------------------
// Notifications
// -------------------------------------------------------------------------

void UNUQuestComponent::NotifyQuestProgressChanged()
{
	EvaluatePersistentObjectives();
	OnQuestProgressChanged.Broadcast();
	RefreshLocationObjectives();
}

void UNUQuestComponent::EvaluatePersistentObjectives()
{
	if (bEvaluatingPersistentObjectives || GetOwner() == nullptr || !GetOwner()->HasAuthority() || (bServerBackedSession && !bServerQuestListReady))
	{
		return;
	}
	TGuardValue<bool> Guard(bEvaluatingPersistentObjectives, true);
	FNUQuestEvent Event;
	Event.Amount = 1;
	if (const UNUJobComponent* Job = GetOwner()->FindComponentByClass<UNUJobComponent>(); Job != nullptr && Job->HasJob())
	{
		Event.ObjectiveType = NU::Quest::Objective::JobSelected;
		Event.TargetId = FName(*LexToString(Job->GetJobId()));
		Event.SourceInstanceId = TEXT("State:JobSelected");
		HandleQuestEvent(Event);
	}
	if (const UNUNaiaRosterComponent* Roster = GetOwner()->FindComponentByClass<UNUNaiaRosterComponent>(); Roster != nullptr && Roster->HasSelectedStarterNaia())
	{
		Event.ObjectiveType = NU::Quest::Objective::StarterContracted;
		Event.TargetId = FName(*LexToString(Roster->GetSelectedStarterNaiaId()));
		Event.SourceInstanceId = FName(*FString::Printf(TEXT("Starter:%lld"), Roster->GetStarterNaiaInstanceId()));
		HandleQuestEvent(Event);
	}
}

// -------------------------------------------------------------------------
// Quest Battle
// -------------------------------------------------------------------------

bool UNUQuestComponent::IsInQuestBattle() const
{
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUQuestBattleEventBridgeSubsystem* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>() : nullptr;
	return Bridge != nullptr && Bridge->FindSessionForQuest(this) != nullptr;
}
