#include "Quest/Component/NUQuestSceneComponent.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterTypes.h"

#include "Field/Presence/NUActorPresenceTransitionSubsystem.h"

#include "Field/Presence/NUActorPresenceComponent.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "Dialogue/Data/NUDialogueDataSubsystem.h"
#include "Dialogue/UI/NUDialogueUIController.h"
#include "Quest/World/NUApproachTrigger.h"
#include "Field/NPC/NUNPCBase.h"
#include "Cinematics/NUCameraShotActor.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Component/NUQuestProviderComponent.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestSceneRow.h"
#include "UI/NUUIManager.h"
#include "Cinematics/NUStorySequenceCue.h"
#include "Cinematics/NUStorySlideshowData.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"

namespace NUQuestScene
{
	const FName CinematicReason(TEXT("QuestCinematic"));
	const FName CameraShotReason(TEXT("QuestCameraShot"));

	// 정적 대사 표는 그대로 두고 이번 플레이어에게 보여줄 복사본만 채운다.
	void ResolvePlayerName(APlayerController* Controller, TArray<FNUDialogueLine>& Lines)
	{
		const UGameInstance* Instance = Controller != nullptr ? Controller->GetGameInstance() : nullptr;
		const UNUCharacterManager* Characters = Instance != nullptr ? Instance->GetSubsystem<UNUCharacterManager>() : nullptr;
		FNUCharacterSummary Character;
		FText PlayerName = NSLOCTEXT("NUQuest", "StoryPlayerFallback", "여행자");
		if (Characters != nullptr && Characters->GetSelectedCharacter(Character) && !Character.Name.IsEmpty())
		{
			PlayerName = FText::FromString(Character.Name);
		}
		else if (Controller != nullptr && Controller->PlayerState != nullptr && !Controller->PlayerState->GetPlayerName().IsEmpty())
		{
			PlayerName = FText::FromString(Controller->PlayerState->GetPlayerName());
		}
		for (FNUDialogueLine& Line : Lines)
		{
			FNUDialogueLine Resolved;
			FString Error;
			if (!Line.Resolve(Resolved, Error) || !Resolved.DialogueText.ToString().Contains(TEXT("{PlayerName}"))) { continue; }
			FFormatNamedArguments Args;
			Args.Add(TEXT("PlayerName"), PlayerName);
			Resolved.DialogueText = FText::Format(Resolved.DialogueText, Args);
			Resolved.DialogueRow = FDataTableRowHandle();
			Line = MoveTemp(Resolved);
		}
	}
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

UNUQuestSceneComponent::UNUQuestSceneComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UNUQuestSceneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	ANUPlayerState* PlayerState = PlayerController != nullptr ? PlayerController->GetPlayerState<ANUPlayerState>() : nullptr;
	if (PlayerController == nullptr || !PlayerController->IsLocalController() || GetNetMode() != NM_Standalone
		|| PlayerState == nullptr || PlayerState->GetQuestComponent() == nullptr)
	{
		return;
	}

	// 온라인에서는 서버 진행 정보를 받은 뒤 로컬 안내 연출을 재생한다.
	UNUQuestComponent* CurrentQuestComponent = PlayerState->GetQuestComponent();
	if (!CurrentQuestComponent->IsQuestProgressReady())
	{
		return;
	}

	// PlayerState가 교체되면 새 진행 정보에 맞춰 관찰 상태와 대기열을 초기화한다.
	if (QuestComponent.Get() != CurrentQuestComponent)
	{
		QuestComponent = CurrentQuestComponent;
		ObservedSceneIds.Reset();
		ObservedDebugCompletionSerial = CurrentQuestComponent->GetDebugCompletionSerial();
		ResetSceneQueue();
	}

	if (CurrentQuestComponent->GetQuestProgressList().IsEmpty())
	{
		return;
	}

	SyncDebugCompletion(*CurrentQuestComponent);
	BindDialoguePresentation(PlayerController);
	// 대사를 빨리 마치거나 취소해도 빈 카메라 컷만 남지 않게 함께 정리한다.
	if (!DialogueCameraSceneId.IsNone() && BoundDialogueController.IsValid() && !BoundDialogueController->IsDialogueOpen())
	{
		EndDialogueCamera();
	}
	CollectConditionScenes(*CurrentQuestComponent);
	CollectApproachScenes(*PlayerController, *CurrentQuestComponent);
	TryPlayNextScene(PlayerController, PlayerState);
	ReleaseReadyNotices(PlayerController, PlayerState);
}

void UNUQuestSceneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundDialogueController.IsValid()) { BoundDialogueController->OnLinePresented.RemoveAll(this); }
	EndDialogueCamera();
	StopWorldSequence();
	// 카메라 컷은 원래 시점으로 즉시 돌리고, 대사 없는 컷의 완료 처리는 하지 않는다.
	GetWorld()->GetTimerManager().ClearTimer(CameraOnlyTimer);
	EndCameraShot(true);
	if (ANUPlayerController* NUPlayerController = Cast<ANUPlayerController>(GetOwner()))
	{
		NUPlayerController->RequestGameInputBlock(NUQuestScene::CameraShotReason, false, this);
	}

	// 종료 중에는 시네마틱 중단을 목표 완료로 처리하지 않는다.
	if (IsValid(CinematicCue))
	{
		PlayingSceneId = NAME_None;
		CinematicCue->StopStory();
	}

	if (!PlayingSceneId.IsNone())
	{
		if (UNUUIManager* UIManager = UNUUIManager::Get(Cast<APlayerController>(GetOwner())))
		{
			// 종료 콜백이 목표 완료로 처리되지 않도록 재생 상태부터 비운다.
			PlayingSceneId = NAME_None;

			if (UNUDialogueUIController* DialogueController = UIManager->GetController<UNUDialogueUIController>())
			{
				DialogueController->CloseDialogue(false);
			}
		}
	}

	// 보상은 이미 적용됐으므로 종료 중에는 미룬 알림을 띄우지 않고 버린다.
	DeferredNotices.Reset();

	Super::EndPlay(EndPlayReason);
}

bool UNUQuestSceneComponent::DeferQuestNotice(FName QuestId, FSimpleDelegate ShowNotice)
{
	UNUQuestComponent* CurrentQuestComponent = QuestComponent.Get();
	if (CurrentQuestComponent == nullptr || QuestId.IsNone() || !ShowNotice.IsBound())
	{
		return false;
	}

	// 완료 알림은 목표 완료와 같은 프레임에 오므로, 다음 틱을 기다리지 않고 방금 끝난 목표의 후속 연출부터 대기열에 올린다.
	SyncDebugCompletion(*CurrentQuestComponent);
	CollectConditionScenes(*CurrentQuestComponent);

	// 완료 직후 이어서 열리는 대화(계약 확정 등)가 있을 수 있으므로 표시는 다음 틱 이후에 판단한다.
	FDeferredNotice& Notice = DeferredNotices.AddDefaulted_GetRef();
	Notice.QuestId = QuestId;
	Notice.ShowNotice = MoveTemp(ShowNotice);
	return true;
}

bool UNUQuestSceneComponent::ShouldWaitForBattleCompletion(FName QuestId)
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const ANUPlayerState* PlayerState = Controller != nullptr ? Controller->GetPlayerState<ANUPlayerState>() : nullptr;
	if (Controller == nullptr || !Controller->IsLocalController() || GetNetMode() != NM_Standalone
		|| PlayerState == nullptr || PlayerState->GetQuestComponent() == nullptr)
	{
		return false;
	}

	// 첫 진행 스냅샷은 다음 틱에 연결한다. 연결 전 완료 요청이 연출보다 먼저 나가지 않게 한다.
	if (QuestComponent.Get() != PlayerState->GetQuestComponent()) { return true; }
	SyncDebugCompletion(*QuestComponent);
	CollectConditionScenes(*QuestComponent);
	return IsBattleActive(PlayerState) || IsScenePlaying() || HasFollowUpSceneForQuest(QuestId)
		|| GetWorld()->GetTimeSeconds() < CameraReturnEndTime;
}

// -------------------------------------------------------------------------
// Scene Queue
// -------------------------------------------------------------------------

void UNUQuestSceneComponent::BindDialoguePresentation(APlayerController* Controller)
{
	UNUUIManager* UI = UNUUIManager::Get(Controller);
	UNUDialogueUIController* Dialogue = UI != nullptr ? UI->GetController<UNUDialogueUIController>() : nullptr;
	if (BoundDialogueController.Get() == Dialogue) { return; }
	if (BoundDialogueController.IsValid()) { BoundDialogueController->OnLinePresented.RemoveAll(this); }
	BoundDialogueController = Dialogue;
	if (Dialogue != nullptr) { Dialogue->OnLinePresented.AddUObject(this, &ThisClass::HandleDialogueLinePresented); }
}

void UNUQuestSceneComponent::HandleDialogueLinePresented(FName SourceRowName)
{
	if (SourceRowName.IsNone() || !QuestComponent.IsValid() || !PlayingSceneId.IsNone()) { return; }
	const UNUQuestDataSubsystem* Data = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (Data == nullptr || Controller == nullptr) { return; }
	for (const FName SceneId : Data->GetSceneIds())
	{
		const FNUQuestSceneRow* Scene = Data->FindScene(SceneId);
		if (Scene == nullptr || Scene->Trigger != ENUQuestSceneTrigger::OnDialogueLine
			|| Scene->TriggerDialogueRow != SourceRowName || ObservedSceneIds.Contains(SceneId)
			|| !IsSceneConditionMet(*Scene, *QuestComponent)) { continue; }
		const ANUCameraShotActor* Shot = BeginCameraShot(Controller, Scene->CameraShotId);
		if (Shot == nullptr) { return; }
		ObservedSceneIds.Add(SceneId);
		DialogueCameraSceneId = SceneId;
		if (ANUPlayerController* LocalController = Cast<ANUPlayerController>(Controller))
		{
			LocalController->RequestGameInputBlock(NUQuestScene::CameraShotReason, true, this);
		}
		GetWorld()->GetTimerManager().SetTimer(DialogueCameraTimer, this, &ThisClass::EndDialogueCamera,
			FMath::Max(.1f, Shot->GetBlendInSeconds() + Shot->GetHoldSeconds()), false);
		UE_LOG(LogNUQuest, Log, TEXT("대사 행 %s에 맞춰 카메라 컷: %s"), *SourceRowName.ToString(), *SceneId.ToString());
		return;
	}
}

void UNUQuestSceneComponent::EndDialogueCamera()
{
	GetWorld()->GetTimerManager().ClearTimer(DialogueCameraTimer);
	if (!DialogueCameraSceneId.IsNone())
	{
		EndCameraShot(false);
		DialogueCameraSceneId = NAME_None;
		if (ANUPlayerController* Controller = Cast<ANUPlayerController>(GetOwner()))
		{
			Controller->RequestGameInputBlock(NUQuestScene::CameraShotReason, false, this);
		}
	}
}

void UNUQuestSceneComponent::CollectConditionScenes(const UNUQuestComponent& CurrentQuestComponent)
{
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	if (QuestData == nullptr)
	{
		return;
	}

	for (const FName SceneId : QuestData->GetSceneIds())
	{
		const FNUQuestSceneRow* Scene = QuestData->FindScene(SceneId);
		if (Scene == nullptr || Scene->Trigger != ENUQuestSceneTrigger::OnCondition || ObservedSceneIds.Contains(SceneId)
			|| !IsSceneConditionMet(*Scene, CurrentQuestComponent))
		{
			continue;
		}

		// 접속 때 이미 참이어도 재생하는 연출은 조건이 참이면 언제든 재생한다.
		// 나머지는 접속한 뒤 조건이 참이 됐을 때만 재생하고, 접속 전에 이미 지난 연출은 다시 보여주지 않는다.
		ObservedSceneIds.Add(SceneId);
		if (Scene->bPlayIfAlreadyMet || bHasInitialSnapshot)
		{
			PendingSceneIds.Add(SceneId);
		}
	}

	bHasInitialSnapshot = true;
}

void UNUQuestSceneComponent::CollectApproachScenes(const APlayerController& PlayerController, const UNUQuestComponent& CurrentQuestComponent)
{
	const APawn* Pawn = PlayerController.GetPawn();
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	if (Pawn == nullptr || QuestData == nullptr)
	{
		ApproachEnterTimes.Reset();
		return;
	}

	TArray<AActor*> OverlappingActors;
	Pawn->GetOverlappingActors(OverlappingActors, ANUApproachTrigger::StaticClass());
	TSet<FName> InsideTriggerIds;
	for (const AActor* Actor : OverlappingActors)
	{
		if (const ANUApproachTrigger* Trigger = Cast<ANUApproachTrigger>(Actor))
		{
			InsideTriggerIds.Add(Trigger->GetApproachActorId());
		}
	}

	// 범위를 벗어난 연출은 머문 시간을 처음부터 다시 잰다.
	const double Now = GetWorld()->GetTimeSeconds();
	TMap<FName, double> InsideEnterTimes;
	for (const FName SceneId : QuestData->GetSceneIds())
	{
		const FNUQuestSceneRow* Scene = QuestData->FindScene(SceneId);
		if (Scene == nullptr || Scene->Trigger != ENUQuestSceneTrigger::OnApproach || ObservedSceneIds.Contains(SceneId)
			|| !IsSceneConditionMet(*Scene, CurrentQuestComponent) || !IsInsideApproach(*Scene, *Pawn, InsideTriggerIds))
		{
			continue;
		}

		const double* PreviousEnterTime = ApproachEnterTimes.Find(SceneId);
		const double EnterTime = PreviousEnterTime != nullptr ? *PreviousEnterTime : Now;
		if (Now - EnterTime < Scene->DwellSeconds)
		{
			InsideEnterTimes.Add(SceneId, EnterTime);
			continue;
		}

		// 같은 연출은 한 세션에 한 번만 재생한다.
		ObservedSceneIds.Add(SceneId);
		PendingSceneIds.Add(SceneId);
	}
	ApproachEnterTimes = MoveTemp(InsideEnterTimes);
}

void UNUQuestSceneComponent::TryPlayNextScene(APlayerController* PlayerController, const ANUPlayerState* PlayerState)
{
	const UNUActorPresenceTransitionSubsystem* Placement = GetWorld()->GetSubsystem<UNUActorPresenceTransitionSubsystem>();
	// 기다리는 재배치는 연출 카메라가 다른 곳을 비출 때 옮겨지므로 암전 중일 때만 연출을 미룬다.
	if (!DialogueCameraSceneId.IsNone() || GetWorld()->GetTimeSeconds() < CameraReturnEndTime
		|| (Placement != nullptr && Placement->IsFading())) { return; }
	// 전투 전환과 기존 대화/창이 끝날 때까지 대기열을 유지한다.
	if (!PlayingSceneId.IsNone() || PendingSceneIds.IsEmpty() || IsBattleActive(PlayerState))
	{
		return;
	}

	UNUUIManager* UIManager = UNUUIManager::Get(PlayerController);
	if (UIManager == nullptr || UIManager->IsAnyWindowOpen(NUUILayer::Window))
	{
		return;
	}

	UNUDialogueUIController* DialogueController = UIManager->GetController<UNUDialogueUIController>();
	const UNUDialogueDataSubsystem* DialogueData = GetWorld()->GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>();
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	if (DialogueController == nullptr || DialogueData == nullptr || QuestData == nullptr)
	{
		return;
	}

	const FName SceneId = PendingSceneIds[0];
	const FNUQuestSceneRow* Scene = QuestData->FindScene(SceneId);
	if (Scene == nullptr)
	{
		// 재임포트 등으로 사라진 연출이 뒤의 연출과 완료 알림을 막지 않게 한다.
		UE_LOG(LogNUQuest, Error, TEXT("연출을 찾을 수 없어 건너뜁니다: %s"), *SceneId.ToString());
		PendingSceneIds.RemoveAt(0);
		return;
	}

	// 월드 시퀀스는 끝나며 인물을 이전 자리로 복원하므로 기다리는 재배치를 먼저 마친 뒤 재생한다.
	if (!Scene->WorldSequence.IsNull() && Placement != nullptr && Placement->IsTransitionPending())
	{
		return;
	}

	// 전투나 다른 창을 기다리는 사이에 끝난 목표의 도입 연출은 뒤늦게 재생하지 않는다.
	if (const UNUQuestComponent* CurrentQuest = QuestComponent.Get();
		CurrentQuest != nullptr && !IsSceneConditionMet(*Scene, *CurrentQuest))
	{
		PendingSceneIds.RemoveAt(0);
		return;
	}

	if (!Scene->Cinematic.IsNull())
	{
		PlayingSceneId = SceneId;
		if (!PlayCinematic(PlayerController, *Scene))
		{
			// 로드 실패를 시청/스킵으로 간주하지 않는다. 대기열에 남겨 재시도한다.
			PlayingSceneId = NAME_None;
			if (FailedCinematicSceneId != SceneId)
			{
				FailedCinematicSceneId = SceneId;
				UE_LOG(LogNUQuest, Error, TEXT("연출 시네마틱을 재생할 수 없습니다: %s %s"), *SceneId.ToString(), *Scene->Cinematic.ToString());
			}
		}
		else
		{
			FailedCinematicSceneId = NAME_None;
			PendingSceneIds.RemoveAt(0);
			UE_LOG(LogNUQuest, Log, TEXT("연출 시네마틱 재생: %s"), *SceneId.ToString());

			// 레벨 위에 자막만 띄우는 시네마틱은 재생하는 동안 카메라 컷을 보여주고, 끝나면 돌아온다.
			if (!Scene->CameraShotId.IsNone())
			{
				BeginCameraShot(PlayerController, Scene->CameraShotId);
			}
		}
		return;
	}

	// 말을 거는 연출은 그 NPC의 지금 퀘스트 대사를 열고, 끝까지 읽으면 직접 말을 건 것과 같이 진행한다.
	if (Scene->TalkNpcId > 0)
	{
		ANUNPCBase* Npc = FindSceneNpc(Scene->TalkNpcId);
		const UNUActorPresenceComponent* Presence = Npc != nullptr ? Npc->FindComponentByClass<UNUActorPresenceComponent>() : nullptr;
		if (Presence != nullptr && Presence->IsPlacementTransitionPending()) { return; }
		UNUQuestProviderComponent* Provider = Npc != nullptr ? Npc->FindComponentByClass<UNUQuestProviderComponent>() : nullptr;
		// 연출 조건의 퀘스트 선택지를 고른다. 같은 NPC의 다른 퀘스트 대사가 대신 열리지 않게 한다.
		FNUQuestInteractionOption Option;
		if (Provider != nullptr && PlayerController->GetPawn() != nullptr)
		{
			for (const FNUQuestInteractionOption& Candidate : Provider->CollectQuestInteractionOptions(PlayerController->GetPawn()))
			{
				if (Candidate.QuestId == FName(*LexToString(Scene->Condition.QuestId)))
				{
					Option = Candidate;
					break;
				}
			}
		}

		const FName TalkDialogueId = Option.Phase != ENUQuestInteractionPhase::Idle ? DialogueData->FindQuestDialogue(Scene->TalkNpcId, Option) : NAME_None;
		TArray<FNUDialogueLine> TalkLines;
		if (TalkDialogueId.IsNone() || !DialogueData->GetLines(TalkDialogueId, TalkLines))
		{
			// 할 말이 없으면(이미 직접 말을 걸었으면 등) 건너뛴다. 플레이어는 직접 말을 걸어 진행할 수 있다.
			UE_LOG(LogNUQuest, Warning, TEXT("말을 걸 NPC의 퀘스트 대사가 없어 연출을 건너뜁니다: %s Npc=%d"), *SceneId.ToString(), Scene->TalkNpcId);
			PendingSceneIds.RemoveAt(0);
			return;
		}

		NUQuestScene::ResolvePlayerName(PlayerController, TalkLines);
		PlayingSceneId = SceneId;
		if (!DialogueController->OpenDialogueLines(TalkLines, FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleTalkFinished)))
		{
			PlayingSceneId = NAME_None;
			return;
		}

		PendingSceneIds.RemoveAt(0);
		TalkProvider = Provider;
		TalkOption = Option;
		if (!Scene->CameraShotId.IsNone())
		{
			BeginCameraShot(PlayerController, Scene->CameraShotId);
		}
		return;
	}

	// 대사 없이 카메라만 보여주는 연출은 정한 시간만큼 머문 뒤 닫는다.
	if (Scene->DialogueId.IsNone())
	{
		if (!PlayWorldSequence(PlayerController, SceneId, *Scene))
		{
			return;
		}
		PendingSceneIds.RemoveAt(0);
		PlayingSceneId = SceneId;
		const ANUCameraShotActor* Shot = BeginCameraShot(PlayerController, Scene->CameraShotId);
		if (Shot == nullptr && WorldSequencePlayer == nullptr)
		{
			// 보여줄 컷이 없으면(로그는 남김) 뒤의 연출과 알림이 막히지 않도록 끝난 것으로 처리한다.
			HandleSceneFinished(true);
			return;
		}

		if (ANUPlayerController* NUPlayerController = Cast<ANUPlayerController>(PlayerController))
		{
			NUPlayerController->RequestGameInputBlock(NUQuestScene::CameraShotReason, true, this);
		}
		const float Hold = Shot != nullptr ? Shot->GetBlendInSeconds() + Shot->GetHoldSeconds() : 0.1f;
		GetWorld()->GetTimerManager().SetTimer(CameraOnlyTimer, this, &ThisClass::HandleCameraOnlyFinished, FMath::Max(Hold, 0.1f), false);
		return;
	}

	TArray<FNUDialogueLine> DialogueLines;
	if (!DialogueData->GetLines(Scene->DialogueId, DialogueLines))
	{
		UE_LOG(LogNUQuest, Error, TEXT("연출 대사를 읽을 수 없어 건너뜁니다: %s %s"), *SceneId.ToString(), *Scene->DialogueId.ToString());
		PendingSceneIds.RemoveAt(0);
		return;
	}

	// 창을 열었을 때만 대기열에서 꺼내고, 실패한 경우 다음 틱에 다시 시도한다.
	if (!PlayWorldSequence(PlayerController, SceneId, *Scene))
	{
		return;
	}
	NUQuestScene::ResolvePlayerName(PlayerController, DialogueLines);
	PlayingSceneId = SceneId;
	if (DialogueController->OpenDialogueLines(DialogueLines, FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleSceneFinished)))
	{
		PendingSceneIds.RemoveAt(0);

		// 대사를 읽는 동안 카메라 컷을 보여주고, 대화창이 닫히면 돌아온다.
		if (!Scene->CameraShotId.IsNone())
		{
			BeginCameraShot(PlayerController, Scene->CameraShotId);
		}
	}
	else
	{
		StopWorldSequence();
		PlayingSceneId = NAME_None;
	}
}

void UNUQuestSceneComponent::HandleSceneFinished(bool bCompleted)
{
	if (PlayingSceneId.IsNone())
	{
		return;
	}

	bScenePresentationFinished = true;
	if (!bWorldSequenceFinished)
	{
		return;
	}
	StopWorldSequence();
	if (ANUPlayerController* Controller = Cast<ANUPlayerController>(GetOwner()))
	{
		Controller->RequestGameInputBlock(NUQuestScene::CameraShotReason, false, this);
	}
	EndCameraShot(false);
	PlayingSceneId = NAME_None;
}

void UNUQuestSceneComponent::HandleTalkFinished(bool bCompleted)
{
	UNUQuestProviderComponent* Provider = TalkProvider.Get();
	const FNUQuestInteractionOption Option = TalkOption;
	TalkProvider.Reset();
	TalkOption = FNUQuestInteractionOption();

	// 연출이 취소되며 닫힌 대화는 진행하지 않는다.
	if (PlayingSceneId.IsNone())
	{
		return;
	}

	// 직접 말을 건 대화와 같은 경로로 처리하며, 온라인 목표 진행은 QuestComponent의 서버 요청을 거친다.
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (bCompleted && Provider != nullptr && Pawn != nullptr && Provider->IsInteractionOptionValid(Pawn, Option))
	{
		Provider->HandleDialogueFinished(Pawn, Option);
	}
	HandleSceneFinished(bCompleted);
}

bool UNUQuestSceneComponent::PlayWorldSequence(APlayerController* PlayerController, FName SceneId, const FNUQuestSceneRow& Scene)
{
	StopWorldSequence();
	bScenePresentationFinished = false;
	if (Scene.WorldSequence.IsNull())
	{
		return true;
	}
	ULevelSequence* Sequence = Scene.WorldSequence.LoadSynchronous();
	if (Sequence == nullptr)
	{
		if (FailedWorldSequenceSceneId != SceneId)
		{
			FailedWorldSequenceSceneId = SceneId;
			UE_LOG(LogNUQuest, Error, TEXT("월드 연출을 로드할 수 없습니다: %s %s"), *SceneId.ToString(), *Scene.WorldSequence.ToString());
		}
		return false;
	}
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableCameraCuts = !Scene.CameraShotId.IsNone();
	ALevelSequenceActor* Actor = nullptr;
	WorldSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, Settings, Actor);
	WorldSequenceActor = Actor;
	if (WorldSequencePlayer == nullptr || WorldSequencePlayer->GetDuration().AsSeconds() <= 0.0)
	{
		StopWorldSequence();
		if (FailedWorldSequenceSceneId != SceneId)
		{
			FailedWorldSequenceSceneId = SceneId;
			UE_LOG(LogNUQuest, Error, TEXT("월드 연출을 재생할 수 없거나 길이가 0입니다: %s %s"), *SceneId.ToString(), *Scene.WorldSequence.ToString());
		}
		return false;
	}
	FailedWorldSequenceSceneId = NAME_None;
	bWorldSequenceFinished = false;
	WorldSequencePlayer->OnFinished.AddDynamic(this, &ThisClass::HandleWorldSequenceFinished);
	WorldSequencePlayer->OnStop.AddDynamic(this, &ThisClass::HandleWorldSequenceStopped);
	WorldSequencePlayer->OnPause.AddDynamic(this, &ThisClass::HandleWorldSequenceStopped);
	if (ANUPlayerController* Controller = Cast<ANUPlayerController>(PlayerController))
	{
		Controller->RequestGameInputBlock(NUQuestScene::CameraShotReason, true, this);
	}
	WorldSequencePlayer->Play();
	return true;
}

void UNUQuestSceneComponent::HandleWorldSequenceFinished()
{
	bWorldSequenceFinishedNaturally = true;
	HandleWorldSequenceStopped();
}

void UNUQuestSceneComponent::HandleWorldSequenceStopped()
{
	// 시퀀서 평가 중에는 재생 액터를 파괴하지 않는다.
	if (!WorldSequenceFinishTimer.IsValid())
	{
		WorldSequenceFinishTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FinishWorldSequence);
	}
}

void UNUQuestSceneComponent::FinishWorldSequence()
{
	WorldSequenceFinishTimer.Invalidate();
	if (!bWorldSequenceFinishedNaturally)
	{
		const FName InterruptedSceneId = PlayingSceneId;
		CancelActiveScene();
		ObservedSceneIds.Remove(InterruptedSceneId);
		UE_LOG(LogNUQuest, Log, TEXT("월드 연출이 중단되어 현재 퀘스트 배치를 복원합니다: %s"), *InterruptedSceneId.ToString());
		return;
	}
	bWorldSequenceFinished = true;
	if (bScenePresentationFinished)
	{
		HandleSceneFinished(true);
	}
}

void UNUQuestSceneComponent::StopWorldSequence()
{
	GetWorld()->GetTimerManager().ClearTimer(WorldSequenceFinishTimer);
	if (WorldSequencePlayer != nullptr)
	{
		if (ANUPlayerController* Controller = Cast<ANUPlayerController>(GetOwner()))
		{
			Controller->RequestGameInputBlock(NUQuestScene::CameraShotReason, false, this);
		}
	}
	if (WorldSequencePlayer != nullptr)
	{
		WorldSequencePlayer->OnFinished.RemoveAll(this);
		WorldSequencePlayer->OnStop.RemoveAll(this);
		WorldSequencePlayer->OnPause.RemoveAll(this);
		WorldSequencePlayer->Stop();
	}
	if (IsValid(WorldSequenceActor))
	{
		WorldSequenceActor->Destroy();
	}
	WorldSequencePlayer = nullptr;
	WorldSequenceActor = nullptr;
	bWorldSequenceFinished = true;
	bWorldSequenceFinishedNaturally = false;
	bScenePresentationFinished = false;
}

void UNUQuestSceneComponent::CancelActiveScene()
{
	EndDialogueCamera();
	const bool bHadActiveScene = !PlayingSceneId.IsNone();
	const bool bHadWorldSequence = WorldSequencePlayer != nullptr;
	// 창 종료 콜백이 이전 연출을 완료 처리하지 않게 재생 ID부터 지운다.
	PlayingSceneId = NAME_None;
	GetWorld()->GetTimerManager().ClearTimer(CameraOnlyTimer);
	StopWorldSequence();
	EndCameraShot(true);
	if (IsValid(CinematicCue))
	{
		CinematicCue->OnFinished.RemoveDynamic(this, &ThisClass::HandleCinematicFinished);
		CinematicCue->StopStory();
	}
	if (IsValid(CinematicCue))
	{
		CinematicCue->Destroy();
	}
	CinematicCue = nullptr;
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (bHadActiveScene)
	{
		if (UNUUIManager* UIManager = UNUUIManager::Get(Controller))
		{
			if (UNUDialogueUIController* DialogueController = UIManager->GetController<UNUDialogueUIController>())
			{
				DialogueController->CloseDialogue(false);
			}
		}
	}
	if (ANUPlayerController* NUController = Cast<ANUPlayerController>(Controller))
	{
		NUController->RequestGameInputBlock(NUQuestScene::CameraShotReason, false, this);
		NUController->RequestGameInputBlock(NUQuestScene::CinematicReason, false, this);
		NUController->RequestCursor(NUQuestScene::CinematicReason, false, this);
	}
	if (bHadWorldSequence)
	{
		// Stop의 이전 값 복원이 최신 퀘스트 배치를 덮었을 수 있으므로 마지막에 다시 확정한다.
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (UNUActorPresenceComponent* Presence = It->FindComponentByClass<UNUActorPresenceComponent>())
			{
				Presence->RefreshPresenceImmediately();
			}
		}
	}
}

void UNUQuestSceneComponent::SkipLinkedScenes(FName SkippedSceneId)
{
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	const FNUQuestSceneRow* Scene = QuestData != nullptr ? QuestData->FindScene(SkippedSceneId) : nullptr;
	if (Scene == nullptr)
	{
		return;
	}

	for (const FName LinkedSceneId : Scene->SkipAlsoSceneIds)
	{
		// 같은 연출은 한 세션에 한 번만 재생하므로 본 것으로 표시하면 다시 나오지 않는다.
		ObservedSceneIds.Add(LinkedSceneId);
		PendingSceneIds.Remove(LinkedSceneId);
		ApproachEnterTimes.Remove(LinkedSceneId);
		UE_LOG(LogNUQuest, Log, TEXT("연출 스킵으로 함께 건너뜀: %s -> %s"), *SkippedSceneId.ToString(), *LinkedSceneId.ToString());
	}
}

bool UNUQuestSceneComponent::IsSceneConditionMet(const FNUQuestSceneRow& Scene, const UNUQuestComponent& CurrentQuestComponent) const
{
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	// NPC에게 말을 걸기 전에는 진행 행 자체가 없을 수 있다. 첫 접근 컷은 등록 여부가 아닌 실제 수락 조건으로 판단한다.
	const bool bIncludeInProgress = Scene.Condition.State == ENUQuestConditionState::AvailableOrInProgress;
	if ((Scene.Condition.State == ENUQuestConditionState::Available || bIncludeInProgress) && Scene.Condition.ObjectiveId == 0)
	{
		const FName QuestId(*LexToString(Scene.Condition.QuestId));
		const FNUQuestProgressState* Progress = CurrentQuestComponent.FindQuestProgress(QuestId);
		if (bIncludeInProgress && Progress != nullptr && Progress->IsInProgress()) { return true; }
		const UNUQuestDefinition* Definition = QuestData != nullptr ? QuestData->FindDefinition(QuestId) : nullptr;
		return (Progress == nullptr || Progress->QuestState == ENUQuestState::Locked || Progress->QuestState == ENUQuestState::Available)
			&& Definition != nullptr && CurrentQuestComponent.AreStartRequirementsMet(Definition);
	}
	return Scene.Condition.IsMet(
		[&CurrentQuestComponent](FName QuestId) { return CurrentQuestComponent.FindQuestProgress(QuestId); },
		[QuestData](FName QuestId) { return QuestData != nullptr ? QuestData->FindDefinition(QuestId) : nullptr; });
}

bool UNUQuestSceneComponent::IsInsideApproach(const FNUQuestSceneRow& Scene, const APawn& Pawn, const TSet<FName>& InsideTriggerIds)
{
	if (Scene.ApproachNpcId <= 0)
	{
		return InsideTriggerIds.Contains(Scene.ApproachActorId);
	}

	// 숨겨진 NPC(월드 상태로 안 보이는 NPC)에게는 다가간 것으로 보지 않는다.
	const ANUNPCBase* Npc = FindSceneNpc(Scene.ApproachNpcId);
	return Npc != nullptr && !Npc->IsHidden() && FVector::Dist2D(Npc->GetActorLocation(), Pawn.GetActorLocation()) <= Scene.ApproachRadius;
}

ANUNPCBase* UNUQuestSceneComponent::FindSceneNpc(int32 NpcId)
{
	if (ANUNPCBase* CachedNpc = SceneNpcs.FindRef(NpcId).Get())
	{
		return CachedNpc;
	}

	for (TActorIterator<ANUNPCBase> It(GetWorld()); It; ++It)
	{
		if (It->GetNpcId() == NpcId)
		{
			SceneNpcs.Add(NpcId, *It);
			return *It;
		}
	}
	return nullptr;
}

// -------------------------------------------------------------------------
// Cinematic
// -------------------------------------------------------------------------

bool UNUQuestSceneComponent::PlayCinematic(APlayerController* PlayerController, const FNUQuestSceneRow& Scene)
{
	UNUStorySlideshowData* Data = Scene.Cinematic.LoadSynchronous();
	ULevelSequence* Sequence = Data != nullptr ? Data->Sequence.LoadSynchronous() : nullptr;
	if (Data == nullptr || Sequence == nullptr)
	{
		return false;
	}

	// 시퀀스의 자막 상태 트랙을 받을 액터는 재생할 때만 만든다.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	CinematicCue = GetWorld()->SpawnActor<ANUStorySequenceCue>(SpawnParameters);
	if (CinematicCue != nullptr) { CinematicCue->ViewportZOrder = NUUILayer::Transition; }
	if (CinematicCue == nullptr || !CinematicCue->PlayStory(PlayerController, Data, Sequence))
	{
		if (IsValid(CinematicCue))
		{
			CinematicCue->Destroy();
		}

		CinematicCue = nullptr;
		return false;
	}

	CinematicCue->OnFinished.AddUniqueDynamic(this, &ThisClass::HandleCinematicFinished);

	// 재생하는 동안 필드 입력을 막고, 스킵 버튼을 누를 수 있게 커서를 연다.
	if (ANUPlayerController* NUPlayerController = Cast<ANUPlayerController>(PlayerController))
	{
		NUPlayerController->RequestGameInputBlock(NUQuestScene::CinematicReason, true, this);
		NUPlayerController->RequestCursor(NUQuestScene::CinematicReason, true, this);
	}

	return true;
}

void UNUQuestSceneComponent::HandleCinematicFinished(bool bCompleted)
{
	const bool bWasSkipped = IsValid(CinematicCue) && CinematicCue->bWasSkipped;
	if (IsValid(CinematicCue))
	{
		CinematicCue->OnFinished.RemoveDynamic(this, &ThisClass::HandleCinematicFinished);
	}

	if (IsValid(CinematicCue))
	{
		CinematicCue->Destroy();
	}
	CinematicCue = nullptr;

	if (ANUPlayerController* NUPlayerController = Cast<ANUPlayerController>(GetOwner()))
	{
		NUPlayerController->RequestGameInputBlock(NUQuestScene::CinematicReason, false, this);
		NUPlayerController->RequestCursor(NUQuestScene::CinematicReason, false, this);
	}

	if (bCompleted || bWasSkipped)
	{
		// 사용자가 스킵했으면 표에 지정된 이어지는 연출(컷 2 등)도 함께 넘긴다.
		if (bWasSkipped)
		{
			SkipLinkedScenes(PlayingSceneId);
		}
		HandleSceneFinished(true);
	}
	else
	{
		// 외부 중단은 완료가 아니다. 다음 기회에 다시 재생한다.
		EndCameraShot(true);
		ObservedSceneIds.Remove(PlayingSceneId);
		PlayingSceneId = NAME_None;
	}
}

// -------------------------------------------------------------------------
// Camera Shot
// -------------------------------------------------------------------------

ANUCameraShotActor* UNUQuestSceneComponent::BeginCameraShot(APlayerController* PlayerController, FName ShotId)
{
	ANUCameraShotActor* Shot = ANUCameraShotActor::FindShot(GetWorld(), ShotId);
	if (PlayerController == nullptr || Shot == nullptr)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("카메라 컷을 찾을 수 없어 시점을 옮기지 않습니다: %s"), *ShotId.ToString());
		return nullptr;
	}

	// 대사 줄 사이의 컷은 이어서 블렌드하고 최초 복귀 시점만 보관한다.
	// 앞 연출의 컷에서 돌아가는 블렌드 중이면 시점은 아직 그 컷이므로 캐릭터로 돌아가게 한다.
	if (!ActiveCameraShot.IsValid())
	{
		AActor* CurrentTarget = PlayerController->GetViewTarget();
		CameraReturnTarget = Cast<ANUCameraShotActor>(CurrentTarget) != nullptr ? PlayerController->GetPawn() : CurrentTarget;
	}
	Shot->PrepareShot(*PlayerController);
	CameraReturnEndTime = 0.0;
	PlayerController->SetViewTargetWithBlend(Shot, Shot->GetBlendInSeconds(), VTBlend_EaseInOut, 2.f);
	ActiveCameraShot = Shot;
	return Shot;
}

void UNUQuestSceneComponent::EndCameraShot(bool bImmediate)
{
	if (bImmediate) { CameraReturnEndTime = 0.0; }
	ANUCameraShotActor* Shot = ActiveCameraShot.Get();
	AActor* ReturnTarget = CameraReturnTarget.Get();
	ActiveCameraShot.Reset();
	CameraReturnTarget.Reset();

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (Shot == nullptr || PlayerController == nullptr)
	{
		return;
	}

	// 연출 중 캐릭터가 바뀌었으면 지금 캐릭터로 돌아간다.
	if (ReturnTarget == nullptr)
	{
		ReturnTarget = PlayerController->GetPawn();
	}

	// 전환 중(블렌드 중)이어도 컷이 아직 시점을 잡고 있을 때만 되돌린다. 다른 기능이 시점을 가져갔으면 건드리지 않는다.
	const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
	const bool bShotOwnsView = PlayerController->GetViewTarget() == Shot || (CameraManager != nullptr && CameraManager->PendingViewTarget.Target == Shot);
	if (ReturnTarget != nullptr && bShotOwnsView)
	{
		const float BlendOutSeconds = bImmediate ? 0.f : Shot->GetBlendOutSeconds();
		CameraReturnEndTime = GetWorld()->GetTimeSeconds() + BlendOutSeconds;
		PlayerController->SetViewTargetWithBlend(ReturnTarget, BlendOutSeconds, VTBlend_EaseInOut, 2.f);
	}
}

void UNUQuestSceneComponent::HandleCameraOnlyFinished()
{
	HandleSceneFinished(true);
}

// -------------------------------------------------------------------------
// Queue Reset
// -------------------------------------------------------------------------

void UNUQuestSceneComponent::SyncDebugCompletion(const UNUQuestComponent& CurrentQuestComponent)
{
	const int32 Serial = CurrentQuestComponent.GetDebugCompletionSerial();
	if (ObservedDebugCompletionSerial == Serial)
	{
		return;
	}

	ObservedDebugCompletionSerial = Serial;
	ResetSceneQueue();
}

void UNUQuestSceneComponent::ResetSceneQueue()
{
	CancelActiveScene();
	// 대기 중이던 연출은 다시 판정한다. 접속 때 이미 참이어도 재생하는 연출은 다시 대기하고, 이미 지난 연출은 생략된다.
	for (const FName SceneId : PendingSceneIds)
	{
		ObservedSceneIds.Remove(SceneId);
	}

	PendingSceneIds.Reset();
	ApproachEnterTimes.Reset();
	bHasInitialSnapshot = false;
}

// -------------------------------------------------------------------------
// Deferred Notice
// -------------------------------------------------------------------------

bool UNUQuestSceneComponent::IsBattleActive(const ANUPlayerState* PlayerState) const
{
	if (PlayerState != nullptr && PlayerState->GetBattleEncounter() != nullptr)
	{
		return true;
	}

	const UNUBattleNetSubsystem* BattleNet = GetWorld()->GetGameInstance()->GetSubsystem<UNUBattleNetSubsystem>();
	const UNUBattleNetStageSubsystem* Stage = GetWorld()->GetSubsystem<UNUBattleNetStageSubsystem>();
	// 서버 종료 통지 뒤에도 마지막 행동 재생·암전·카메라 복원이 남아 있을 수 있다.
	return (BattleNet != nullptr && (BattleNet->IsInBattle() || BattleNet->IsBattleStartPending()))
		|| (Stage != nullptr && Stage->IsTransitionActive());
}

bool UNUQuestSceneComponent::HasPendingCameraScene() const
{
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	return QuestData != nullptr && PendingSceneIds.ContainsByPredicate([this, QuestData](FName SceneId)
	{
		const FNUQuestSceneRow* Scene = QuestData->FindScene(SceneId);
		// 도착을 기다리는 대화 컷은 재배치용 암전을 막지 않는다.
		if (Scene != nullptr && Scene->TalkNpcId > 0)
		{
			for (TActorIterator<ANUNPCBase> It(GetWorld()); It; ++It)
			{
				if (It->GetNpcId() != Scene->TalkNpcId) { continue; }
				const UNUActorPresenceComponent* Presence = It->FindComponentByClass<UNUActorPresenceComponent>();
				if (Presence != nullptr && Presence->IsPlacementTransitionPending()) { return false; }
			}
		}
		return Scene != nullptr && !Scene->CameraShotId.IsNone() && Scene->WorldSequence.IsNull();
	});
}

bool UNUQuestSceneComponent::HasFollowUpSceneForQuest(FName QuestId) const
{
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	if (QuestData == nullptr)
	{
		return false;
	}

	// 같은 퀘스트의 조건 연출이 남아 있으면 그 뒤에 알림을 띄운다. 접속 때 다시 나오는 도입 연출은 결과 연출이 아니라서 기다리지 않는다.
	auto IsFollowUpScene = [QuestData, QuestId](FName SceneId)
	{
		const FNUQuestSceneRow* Scene = QuestData->FindScene(SceneId);
		return Scene != nullptr && Scene->Trigger == ENUQuestSceneTrigger::OnCondition && !Scene->bPlayIfAlreadyMet && FName(*LexToString(Scene->Condition.QuestId)) == QuestId;
	};

	return IsFollowUpScene(PlayingSceneId) || PendingSceneIds.ContainsByPredicate(IsFollowUpScene);
}

void UNUQuestSceneComponent::ReleaseReadyNotices(APlayerController* PlayerController, const ANUPlayerState* PlayerState)
{
	if (DeferredNotices.IsEmpty() || IsBattleActive(PlayerState) || GetWorld()->GetTimeSeconds() < CameraReturnEndTime)
	{
		return;
	}

	// 보상 창이 대화창 위를 덮지 않게 NPC 대화와 연출이 모두 닫힌 뒤에 띄운다.
	// 수락 창이 떠 있거나 수락 직후 같은 NPC의 대화가 이어지기 직전이어도 기다린다.
	UNUUIManager* UIManager = UNUUIManager::Get(PlayerController);
	const UNUDialogueUIController* DialogueController = UIManager != nullptr ? UIManager->GetController<UNUDialogueUIController>() : nullptr;
	if (DialogueController == nullptr || DialogueController->IsDialogueOpen() || !PlayingSceneId.IsNone()
		|| UIManager->IsAnyWindowOpen(NUUILayer::Window) || (QuestComponent.IsValid() && QuestComponent->IsOfferConversationPending()))
	{
		return;
	}

	TArray<FSimpleDelegate> ReadyNotices;
	for (int32 Index = 0; Index < DeferredNotices.Num();)
	{
		if (HasFollowUpSceneForQuest(DeferredNotices[Index].QuestId))
		{
			++Index;
			continue;
		}

		ReadyNotices.Add(MoveTemp(DeferredNotices[Index].ShowNotice));
		DeferredNotices.RemoveAt(Index);
	}

	for (FSimpleDelegate& Notice : ReadyNotices)
	{
		Notice.ExecuteIfBound();
	}
}
