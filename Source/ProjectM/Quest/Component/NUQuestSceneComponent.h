#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestSceneComponent.generated.h"

class ANUCameraShotActor;
class ALevelSequenceActor;
class ULevelSequencePlayer;
class ANUNPCBase;
class ANUPlayerState;
class ANUStorySequenceCue;
class APawn;
class APlayerController;
class UNUQuestComponent;
class UNUQuestProviderComponent;
class UNUDialogueUIController;
struct FNUQuestSceneRow;

// 연출 표(QuestSceneTable)의 연출을 조건과 접근에 맞춰 재생하고, 연출 완료에 따른 진행 판정은 QuestComponent에 위임한다.
UCLASS()
class PROJECTM_API UNUQuestSceneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUQuestSceneComponent();
	bool IsScenePlaying() const { return !PlayingSceneId.IsNone() || !DialogueCameraSceneId.IsNone(); }
	// 화면에 보이는 재배치를 가려 줄 카메라 연출(월드 시퀀스 없는 카메라 컷)이 대기 중인지.
	bool HasPendingCameraScene() const;
	// 월드 시퀀스가 끝나며 이전 값을 복원하므로 재생 중에는 재배치하지 않는다.
	bool IsWorldSequencePlaying() const { return WorldSequencePlayer != nullptr; }

	// 플레이어의 완료 알림을 대화·전투와 같은 퀘스트의 후속 연출이 끝난 뒤로 미룬다. 관리하지 않는 세션이면 false.
	// 완료와 보상은 이미 적용된 상태이며 알림 표시 순서만 바꾼다.
	bool DeferQuestNotice(FName QuestId, FSimpleDelegate ShowNotice);

	// 전투 자동 완료는 필드 복귀와 마지막 목표의 후속 연출 뒤에 요청한다.
	bool ShouldWaitForBattleCompletion(FName QuestId);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindDialoguePresentation(APlayerController* Controller);
	void HandleDialogueLinePresented(FName SourceRowName);
	void EndDialogueCamera();
	TWeakObjectPtr<UNUDialogueUIController> BoundDialogueController;
	FName DialogueCameraSceneId;
	FTimerHandle DialogueCameraTimer;
	// 조건이 참이 된 연출과 범위에 들어선 연출을 모으고, 다른 UI와 겹치지 않을 때 순서대로 재생한다.
	void CollectConditionScenes(const UNUQuestComponent& CurrentQuestComponent);
	void CollectApproachScenes(const APlayerController& PlayerController, const UNUQuestComponent& CurrentQuestComponent);
	void TryPlayNextScene(APlayerController* PlayerController, const ANUPlayerState* PlayerState);
	void HandleSceneFinished(bool bCompleted);

	// 말을 거는 연출의 대화가 닫히면 직접 말을 건 대화를 마쳤을 때와 같이 퀘스트를 진행한다.
	void HandleTalkFinished(bool bCompleted);

	// 스킵한 연출이 함께 건너뛰도록 지정한 연출을 이번 접속에서 본 것으로 처리한다.
	void SkipLinkedScenes(FName SkippedSceneId);

	bool IsSceneConditionMet(const FNUQuestSceneRow& Scene, const UNUQuestComponent& CurrentQuestComponent) const;
	bool IsInsideApproach(const FNUQuestSceneRow& Scene, const APawn& Pawn, const TSet<FName>& InsideTriggerIds);
	ANUNPCBase* FindSceneNpc(int32 NpcId);

	// 연출에 카메라 컷이 있으면 그동안 시점을 옮기고, 연출이 끝나면 원래 시점으로 돌린다.
	ANUCameraShotActor* BeginCameraShot(APlayerController* PlayerController, FName ShotId);
	void EndCameraShot(bool bImmediate);
	void HandleCameraOnlyFinished();
	bool PlayWorldSequence(APlayerController* PlayerController, FName SceneId, const FNUQuestSceneRow& Scene);
	void StopWorldSequence();
	void CancelActiveScene();
	void FinishWorldSequence();
	UFUNCTION()
	void HandleWorldSequenceFinished();
	UFUNCTION()
	void HandleWorldSequenceStopped();

	// 시네마틱 연출은 대화창 대신 Level Sequence 슬라이드쇼로 재생한다.
	bool PlayCinematic(APlayerController* PlayerController, const FNUQuestSceneRow& Scene);

	UFUNCTION()
	void HandleCinematicFinished(bool bCompleted);

	// 개발용 완료로 채운 목표는 접속 시점처럼 과거 진행으로 보고 대기열을 다시 구성한다.
	void SyncDebugCompletion(const UNUQuestComponent& CurrentQuestComponent);
	void ResetSceneQueue();

	// 미룬 완료 알림은 대화와 전투가 없고 같은 퀘스트의 후속 연출이 모두 끝나면 띄운다.
	bool IsBattleActive(const ANUPlayerState* PlayerState) const;
	bool HasFollowUpSceneForQuest(FName QuestId) const;
	void ReleaseReadyNotices(APlayerController* PlayerController, const ANUPlayerState* PlayerState);

	struct FDeferredNotice
	{
		FName QuestId;
		FSimpleDelegate ShowNotice;
	};

	TWeakObjectPtr<UNUQuestComponent> QuestComponent;

	// 한 번 재생하기로 한 연출. 접속 전에 이미 참이던 조건의 연출은 재생하지 않고 여기에만 넣는다.
	TSet<FName> ObservedSceneIds;
	bool bHasInitialSnapshot = false;
	int32 ObservedDebugCompletionSerial = 0;

	TArray<FName> PendingSceneIds;
	FName PlayingSceneId;
	FName FailedCinematicSceneId;
	FName FailedWorldSequenceSceneId;

	// 완료 순서대로 띄운다.
	TArray<FDeferredNotice> DeferredNotices;

	// 연출 ID -> 범위에 들어선 시각. 범위를 벗어나면 지운다.
	TMap<FName, double> ApproachEnterTimes;

	// 접근 연출이 기다리거나 말을 거는 연출이 부르는 NPC. 매번 월드를 뒤지지 않도록 찾은 것을 둔다.
	TMap<int32, TWeakObjectPtr<ANUNPCBase>> SceneNpcs;

	// 말을 거는 연출이 연 대화의 상대와 그때 정한 퀘스트 선택지.
	TWeakObjectPtr<UNUQuestProviderComponent> TalkProvider;
	FNUQuestInteractionOption TalkOption;

	// 연출이 끝나면 돌아갈 원래 시점.
	TWeakObjectPtr<ANUCameraShotActor> ActiveCameraShot;
	TWeakObjectPtr<AActor> CameraReturnTarget;
	FTimerHandle CameraOnlyTimer;
	double CameraReturnEndTime = 0.0;
	FTimerHandle WorldSequenceFinishTimer;
	bool bScenePresentationFinished = false;
	bool bWorldSequenceFinished = true;
	bool bWorldSequenceFinishedNaturally = false;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> WorldSequencePlayer;
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> WorldSequenceActor;

	UPROPERTY(Transient)
	TObjectPtr<ANUStorySequenceCue> CinematicCue;
};
