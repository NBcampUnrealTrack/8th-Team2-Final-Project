#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Quest/Data/NUQuestEvent.h"
#include "Quest/Net/NUQuestServerTypes.h"
#include "Quest/Data/NUQuestProgressTypes.h"
#include "NUQuestComponent.generated.h"

class UNUInventoryComponent;
class UNUQuestDefinition;
class UEmberServerManager;
class UNUWorldLocationComponent;
enum class EEmberConnState : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUQuestProgressChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUQuestOfferRequested, FName, QuestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUQuestAccepted, FName, QuestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUQuestCompleted, FName, QuestId);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTM_API UNUQuestComponent : public UActorComponent
{
	GENERATED_BODY()

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
public:	
	UNUQuestComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

// -------------------------------------------------------------------------
// Quest State
// -------------------------------------------------------------------------
public:
	// 플레이어에게 퀘스트 진행 정보 추가
	bool AddQuest(const UNUQuestDefinition* Definition, ENUQuestState InitialState = ENUQuestState::Available);

	// 현재 플레이어 상태가 퀘스트 시작 조건을 만족하는지 확인
	bool AreStartRequirementsMet(const UNUQuestDefinition* Definition) const;

	// 선행 조건 중 처음으로 충족하지 못한 항목. 전부 충족하거나 조회 불가면 nullptr
	const FNUQuestRequirementSpec* FindUnmetRequirement(const UNUQuestDefinition* Definition) const;

	// Available 상태의 퀘스트를 수락
	bool AcceptQuest(const UNUQuestDefinition* Definition);

	// 목표 진행도 증가
	bool AddObjectiveProgress(const UNUQuestDefinition* Definition, FName ObjectiveId, int32 Amount = 1, FName SourceInstanceId = NAME_None);

	// ReadyToTurnIn 상태의 퀘스트 완료 처리
	bool CompleteQuest(const UNUQuestDefinition* Definition, FName TurnInNpcId = NAME_None);

	// 상호작용·전투·인벤토리에서 발행한 이벤트로 진행 중인 목표 갱신
	UFUNCTION(BlueprintCallable, Category = "NU|Quest")
	bool HandleQuestEvent(const FNUQuestEvent& QuestEvent);

	// UI에서 QuestId만 전달해 수락한다. 진행 정보가 없으면 먼저 등록한다.
	UFUNCTION(BlueprintCallable, Category = "NU|Quest")
	void AcceptQuestById(FName QuestId);

// -------------------------------------------------------------------------
// UI Notifications
// -------------------------------------------------------------------------
public:
	// 제안할 퀘스트를 UI에 표시
	void BroadcastQuestOffer(FName QuestId);

	// 제안을 연 NPC를 기억해, 수락 직후 그 NPC와의 첫 대화 목표를 다시 말을 걸지 않고 이어서 진행한다.
	void RememberOfferSource(FName QuestId, AActor* SourceActor);

	// 게시판처럼 의뢰를 받는 행동이 앞 퀘스트의 마지막 목표일 때 쓴다. 의뢰서를 먼저 띄우고,
	// 수락하면 CompleteObjective로 목표를 채운 뒤 선행 퀘스트가 끝나는 대로 수락한다. 거절하면 아무것도 바꾸지 않는다.
	void OfferQuestAfterObjective(FName OfferQuestId, FSimpleDelegate CompleteObjective);
	void CancelObjectiveOffer(FName OfferQuestId);

	// 수락 창이 닫히고 그 NPC와의 대화가 다음 틱에 이어지기를 기다리는 중이면 true
	bool IsOfferConversationPending() const { return bOfferConversationPending; }

// -------------------------------------------------------------------------
// Debug
// -------------------------------------------------------------------------
public:
	// 정식 성장/플래그 시스템 전까지 선행 조건을 직접 확인하기 위한 개발용 값
	void DebugSetPlayerLevel(int32 NewLevel);
	void DebugAddFlag(FName FlagId);
	void DebugProgressQuestById(FName QuestId, int32 Amount = 1);
	void DebugProgressObjectiveById(FName QuestId, FName ObjectiveId, int32 Amount = 1);
	void DebugCompleteQuestById(FName QuestId);

	// 개발용 로컬 진행에서 선행 퀘스트를 완료하고 지정 퀘스트부터 플레이한다.
	bool DebugSkipLocalQuest(FName QuestId, FString& OutError);

	// 개발용 완료로 목표를 채울 때마다 증가한다. 연출은 이 값이 바뀌면 지난 목표의 후속 연출을 생략한다.
	int32 GetDebugCompletionSerial() const
	{
		return DebugCompletionSerial;
	}

// -------------------------------------------------------------------------
// Server
// -------------------------------------------------------------------------
public:
	// EmberServerManager가 gRPC 응답을 받으면 아래 핸들러로 전달한다.
	void RequestServerQuestList();
	void HandleServerQuestUpdated(int32 QuestId, int32 Reason, const FString& ResponseData);

	bool IsServerBackedSession() const
	{
		return bServerBackedSession;
	}

	void HandleServerListResponse(int32 ResultCode, const FString& ResponseData);
	void HandleServerAcceptResponse(int32 ResultCode, const FString& ResponseData);
	void HandleServerObjectiveResponse(int32 ResultCode, const FString& ResponseData);
	void HandleServerCompleteResponse(int32 ResultCode, const FString& ResponseData);

	// 진정 결과(106) 응답의 진행을 적용한다. 목표 진행은 먼저 오는 104가 이미 반영하므로 판(Revision)이 같으면 그대로 둔다.
	void HandleServerCalmResultResponse(int32 ResultCode, const FString& ResponseData);

// -------------------------------------------------------------------------
// Query
// -------------------------------------------------------------------------
public:
	// 퀘스트 진행 정보를 찾기
	const FNUQuestProgressState* FindQuestProgress(FName QuestId) const;

	// 해당 퀘스트 진행 정보가 존재하는지 확인
	bool HasQuest(FName QuestId) const;

	// 진행 정보를 믿을 수 있는지. 로컬 세션은 BeginPlay 뒤부터, 서버 세션은 퀘스트 목록을 받은 뒤부터 참이고 연결이 끊기면 다시 거짓이 된다.
	bool IsQuestProgressReady() const;

	// 빈 목록은 제한 없음. 목록의 모든 퀘스트를 한 번 이상 완료해야 한다.
	UFUNCTION(BlueprintPure, Category = "NU|Quest")
	bool HasCompletedQuests(const TArray<FName>& QuestIds) const;

	const TArray<FNUQuestProgressState>& GetQuestProgressList() const;
	bool TryGetLastCompletionReward(FName QuestId, FNUQuestServerReward& OutReward) const;

	// 진행 중인 퀘스트에 이 대상을 가리키는 미완료 목표가 있는지.
	// 머리 위 마커가 실제 진행과 같은 규칙으로 판정하도록 HandleQuestEvent와 판정을 공유한다
	UFUNCTION(BlueprintPure, Category = "NU|Quest")
	bool HasActiveObjectiveFor(FGameplayTag InObjectiveType, FName TargetId, FName SourceInstanceId = NAME_None) const;

	// 현재 진행 가능한 목표가 이 대상을 가리키는 퀘스트 ID 목록.
	// Target 상호작용도 퀘스트별 대사를 선택할 수 있도록 QuestId를 보존한다.
	UFUNCTION(BlueprintPure, Category = "NU|Quest")
	TArray<FName> GetActiveObjectiveQuestIds(FGameplayTag InObjectiveType, FName TargetId, FName SourceInstanceId = NAME_None) const;
	FName FindActiveObjectiveId(FName QuestId, FGameplayTag InObjectiveType, FName TargetId, FName SourceInstanceId = NAME_None) const;

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Test")
	int32 GetTestPlayerLevel() const { return TestPlayerLevel; }

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Test")
	int32 GetTestExperience() const { return TestExperience; }

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Test")
	bool HasTestFlag(FName FlagId) const;

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Test")
	bool HasTestUnlockedFeature(FName FeatureId) const;

	const TArray<FName>& GetTestFlags() const { return TestFlags; }
	const TArray<FName>& GetTestUnlockedFeatures() const { return TestUnlockedFeatures; }

private:
	struct FPendingObjectiveRequest
	{
		int32 QuestId = 0;
		int32 ObjectiveId = 0;
		int32 CurrentCount = 0;

		bool IsValid() const { return QuestId > 0 && ObjectiveId > 0 && CurrentCount >= 0; }
		void Reset() { *this = FPendingObjectiveRequest(); }
	};

	FNUQuestProgressState* FindQuestProgressMutable(FName QuestId);
	const UNUQuestDefinition* FindQuestDefinition(FName QuestId) const;
	const UNUQuestDefinition* FindQuestDefinitionByServerId(int32 QuestId) const;
// -------------------------------------------------------------------------
// Requirements
// -------------------------------------------------------------------------
private:
	bool EvaluateRequirement(const FNUQuestRequirementSpec& Requirement) const;
	void EvaluateAvailableQuests();
	void TryAcceptAfterObjective();
// -------------------------------------------------------------------------
// Rewards
// -------------------------------------------------------------------------
private:
	bool CanApplyRewards(const UNUQuestDefinition& Definition) const;
	bool ApplyRewards(const UNUQuestDefinition& Definition);
	bool DeferBattleCompletion(const UNUQuestDefinition& Definition);
	TMap<FName, FTimerHandle> BattleCompletionTimers;

// -------------------------------------------------------------------------
// Server Internals
// -------------------------------------------------------------------------
private:
	UEmberServerManager* GetServerManager() const;
	bool IsServerQuestConnected() const;
	bool TryGetServerQuestId(const UNUQuestDefinition& Definition, int32& OutQuestId) const;
	bool ApplyAcceptedQuestLocally(const UNUQuestDefinition& Definition);
	bool ApplyCompletedQuestLocally(const UNUQuestDefinition& Definition);
	bool ApplyServerProgress(const FNUQuestServerProgress& ServerProgress);
	bool QueueServerObjective(const UNUQuestDefinition& Definition, FName ObjectiveId, int32 CurrentCount);
	bool TryGetServerObjectiveId(const UNUQuestDefinition& Definition, FName ObjectiveId, int32& OutObjectiveId) const;
	void SendNextServerObjective();
	bool HasPendingServerObjective(int32 QuestId) const;
	void QueueServerCompletion(int32 QuestId, int32 NpcId = 0);
	void SendNextServerCompletion();
	void ResetServerRequests();

	UFUNCTION()
	void HandleEmberStateChanged(EEmberConnState NewState);

// -------------------------------------------------------------------------
// Progression
// -------------------------------------------------------------------------
private:
	UFUNCTION()
	void HandleProgressionChanged();

	UFUNCTION()
	void EvaluatePersistentObjectives();

	bool bEvaluatingPersistentObjectives = false;

// -------------------------------------------------------------------------
// Quest Battle
// -------------------------------------------------------------------------
private:
	// 연습·진정 전투 상태는 전투 연결부의 세션(UNUQuestBattleSession)이 소유한다. 여기서는 진행 중인지만 묻는다.
	bool IsInQuestBattle() const;

// -------------------------------------------------------------------------
// Inventory
// -------------------------------------------------------------------------
private:
	void InitializeInventoryTracking();
	void UnbindInventoryTracking();
	void HandleItemAcquired(FName ItemId, int32 Count);
	void HandlePurchaseSucceeded(int32 NpcId);
	void HandleStorageAccessed(int32 NpcId);
	TWeakObjectPtr<UNUInventoryComponent> InventoryComponent;

// -------------------------------------------------------------------------
// Location
// -------------------------------------------------------------------------
private:
	void InitializeLocationTracking();
	void UnbindLocationTracking();
	void RefreshLocationObjectives();

	UFUNCTION()
	void EvaluateCurrentLocations();

	TWeakObjectPtr<UNUWorldLocationComponent> LocationComponent;
	bool bIsEvaluatingLocations = false;

// -------------------------------------------------------------------------
// Notifications
// -------------------------------------------------------------------------
private:
	// 퀘스트 진행도 변경
	void NotifyQuestProgressChanged();

	// 수락된 퀘스트를 트래커에 자동으로 고정하기 위한 알림.
	// bAutoAccept로 수락된 경우에도 와야 하므로 UI 입력 경로가 아니라 여기서 보낸다.
	void BroadcastQuestAccepted(FName QuestId);

	// 보상 창 표시용. 보상이 적용된 뒤에 보낸다
	void BroadcastQuestCompleted(FName QuestId, const FNUQuestServerReward& Reward);

// -------------------------------------------------------------------------
// Events
// -------------------------------------------------------------------------
public:
	UPROPERTY(BlueprintAssignable, Category = "NU|Quest")
	FOnNUQuestProgressChanged OnQuestProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "NU|Quest")
	FOnNUQuestOfferRequested OnQuestOfferRequested;

	UPROPERTY(BlueprintAssignable, Category = "NU|Quest")
	FOnNUQuestAccepted OnQuestAccepted;

	UPROPERTY(BlueprintAssignable, Category = "NU|Quest")
	FOnNUQuestCompleted OnQuestCompleted;

private:
	// 퀘스트 진행 정보
	bool DebugCompleteLocalQuest(FName QuestId);
	bool bSkippingLocalQuestPrerequisites = false;
	int32 DebugCompletionSerial = 0;

	void ContinueOfferConversation(FName QuestId);

	TWeakObjectPtr<AActor> OfferSourceActor;
	FName OfferSourceQuestId;
	bool bOfferConversationPending = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest", meta = (AllowPrivateAccess = "true"))
	TArray<FNUQuestProgressState> QuestProgressList;

	// 정식 성장 시스템 연결 전 사용하는 더미 플레이어 레벨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Test", meta = (AllowPrivateAccess = "true"))
	int32 TestPlayerLevel = 1;

	// 정식 경험치 시스템 연결 전 퀘스트 보상 누적값
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Test", meta = (AllowPrivateAccess = "true"))
	int32 TestExperience = 0;

	// 선행 조건과 SetFlag 보상을 실제 플레이에서 확인하기 위한 임시 저장소
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Test", meta = (AllowPrivateAccess = "true"))
	TArray<FName> TestFlags;

	// UnlockFeature 보상을 실제 플레이에서 확인하기 위한 임시 저장소
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Test", meta = (AllowPrivateAccess = "true"))
	TArray<FName> TestUnlockedFeatures;

	bool bServerBackedSession = false;
	bool bApplyingLocalRewards = false;
	bool bServerQuestListReady = false;
	bool bAutoAcceptInitialized = false;
	bool bEvaluatingAvailableQuests = false;
	bool bReevaluateAvailableQuests = false;
	int32 AcceptRequestInFlight = 0;
	TSet<FName> FailedAutoAcceptQuestIds;
	// OfferQuestAfterObjective로 띄운 의뢰와 수락 때 채울 목표
	FName ObjectiveOfferQuestId = NAME_None;
	FSimpleDelegate ObjectiveOfferCompletion;
	// 목표를 채운 뒤 선행 퀘스트 완료를 기다렸다 수락할 퀘스트
	FName AcceptAfterObjectiveQuestId = NAME_None;
	FPendingObjectiveRequest ObjectiveRequestInFlight;
	TMap<uint64, FPendingObjectiveRequest> QueuedObjectiveRequests;
	TMap<int32, int64> ServerQuestRevisions;
	TMap<int32, FNUQuestServerProgress> PendingServerPushes;
	TMap<int32, int32> CompletionNpcIds;
	int32 CompletionRequestInFlight = 0;
	TArray<int32> QueuedCompletionRequests;
	FName LastCompletedQuestId;
	FNUQuestServerReward LastCompletionReward;
	bool bHasLastCompletionReward = false;
};
