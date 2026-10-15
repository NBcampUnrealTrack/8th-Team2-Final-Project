#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "CharacterProgression/Data/NUProgressionNodeTypes.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"
#include "CharacterProgression/Net/NUCharacterStatServerTypes.h"
#include "NUProgressionComponent.generated.h"

class UGameplayEffect;
class UNUAbilitySystemComponent;
class UEmberServerManager;
class UNUJobComponent;
class UNUProgressionNetSubsystem;
struct FNUJobState;
struct FNUProgressionNodeRewardRow;
struct FNUProgressionServerSnapshot;

// 성장 상태가 바뀜
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUProgressionChanged);
// 외부 서버 스탯 요청 상태 또는 스냅샷이 변경될 때 발생
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUCharacterStatSnapshotChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUProgressionRequestStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNUProgressionRequestFailed, int32, ResultCode, const FString&, Message);

// 서버가 확정한 레벨, 스탯, 성장 노드 상태를 보유하고 GAS와 UI에 전달한다.
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTM_API UNUProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUProgressionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

// -------------------------------------------------------------------------
// 상태 변경
// -------------------------------------------------------------------------
public:
	// 서버가 확정된 상태를 반영하는 경로
	// 로그인 상태 복원, 레벨업 결과 반영, 성장 노드 선택 등등
	bool ApplyState(const FNUProgressionState& NewState);

	// 현재 상태를 ASC에 다시 투영한다. 
	// 상시 GE를 재적용하지 않기 대문에 여러 번 불러도 결과가 같음
	bool RebuildAttributes();

	// 신규 캐릭터가 처음 만들어졌을 때 HP와 SP를 최대로 채움
	// ApplyState와 분리해 사망/소진 상태를 성장 갱신이 복구하지 않게 함
	bool InitializeCurrentVitalsOnce();

	// 서버가 노드 획득 조건을 검사하고 포인트·획득 상태·효과를 한 번에 반영한다.
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Progression|Node")
	void ServerAcquireProgressionNode(int32 NodeId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Progression|Node")
	void ServerAcquireAdvancementNode(int32 NodeId, int32 AdvancementId);

	bool ApplyCachedServerSnapshot();
	bool ApplyServerSnapshot(const FNUProgressionServerSnapshot& Snapshot);
	bool RequestProgressionState();
	bool RequestNodeAcquire(int32 NodeId, int32 ChoiceAdvancementId);
	bool HasServerProgressionSnapshot() const;
	bool IsServerProgressionRequestPending() const;
	bool IsUsingLocalProgressionBackend() const { return bLocalProgressionBackendReady; }
	bool ApplyLocalExperienceReward(int64 Amount);

	UFUNCTION(BlueprintPure, Category = "NU|Progression|Node")
	int32 GetAvailablePoints() const;

	UFUNCTION(BlueprintPure, Category = "NU|Progression|Node")
	bool IsNodeAcquired(int32 NodeId) const { return State.AcquiredNodeIds.Contains(NodeId); }

	UFUNCTION(BlueprintPure, Category = "NU|Progression|Node")
	int32 GetCurrentNodeId() const { return State.CurrentNodeId; }

	UFUNCTION(BlueprintPure, Category = "NU|Progression|Node")
	ENUProgressionNodeState GetNodeState(int32 NodeId) const;

	int32 ResolveAvailableParentNodeId(int32 NodeId) const;
	int32 GetProgressionGraphId() const;

	// 최종 캐릭터 스탯 요청
	void RequestCharacterStats();

	// CharacterStat 조회와 배분 응답을 동일한 스냅샷으로 파싱해 적용
	void HandleServerCharacterStatResponse(int32 ResultCode, const FString& ResponseData);

	UFUNCTION(BlueprintPure, Category = "NU|Character|Stats")
	bool HasCharacterStatSnapshot() const { return bHasCharacterStatSnapshot; }

	UFUNCTION(BlueprintPure, Category = "NU|Character|Stats")
	bool IsCharacterStatRequestPending() const { return bCharacterStatRequestPending; }

	UFUNCTION(BlueprintPure, Category = "NU|Character|Stats")
	FNUCharacterStatSnapshot GetCharacterStatSnapshot() const { return CharacterStatSnapshot; }

// -------------------------------------------------------------------------
// PIE 디버그
// -------------------------------------------------------------------------
public:
	// 입력값으로 Primary Stats를 설정하고 GAS 환산 결과를 확인
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Progression|Debug", meta = (DevelopmentOnly))
	void ServerDebugSetPrimaryStats(int32 InStrength, int32 InDexterity, int32 InIntelligence, int32 InLuck, int32 InVitality);

	// 임시 경험치 증가량을 입력해 현재 경험치/레벨업을 확인
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Progression|Debug", meta = (DevelopmentOnly))
	void ServerDebugIncreaseExperience(int64 Amount);

	// 최초 직업 선택 직후 LV20 진입 줄기 완료 상태를 만들어 2차 전직 선택 UI를 검증한다.
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Progression|Debug", meta = (DevelopmentOnly))
	void ServerDebugPrepareSecondAdvancement();

// -------------------------------------------------------------------------
// 레벨 및 경험치 조회
// -------------------------------------------------------------------------
public:
	const FNUProgressionState& GetState() const { return State; }

	// 외부 서버 스냅샷이 있으면 그 레벨을 우선해 반환한다.
	UFUNCTION(BlueprintPure, Category = "NU|Progression")
	int32 GetLevel() const;

	UFUNCTION(BlueprintPure, Category = "NU|Progression")
	int64 GetTotalExperience() const { return State.TotalExperience; }

	UFUNCTION(BlueprintPure, Category = "NU|Progression")
	int64 GetExperienceIntoLevel() const { return State.ExperienceIntoLevel; }

	UFUNCTION(BlueprintPure, Category = "NU|Progression")
	int64 GetRequiredExperienceToNext() const;

	UFUNCTION(BlueprintPure, Category = "NU|Progression")
	float GetExperienceProgressRatio() const;

// -------------------------------------------------------------------------
// 스탯 조회
// -------------------------------------------------------------------------
public:
	UFUNCTION(BlueprintPure, Category = "NU|Progression")
	const FNUPrimaryStats& GetPrimaryStats() const { return State.PrimaryStats; }

// -------------------------------------------------------------------------
// 이벤트 및 복제 데이터
// -------------------------------------------------------------------------
public:
	UPROPERTY(BlueprintAssignable, Category = "NU|Progression")
	FOnNUProgressionChanged OnProgressionChanged;

	UPROPERTY(BlueprintAssignable, Category = "NU|Character|Stats")
	FOnNUCharacterStatSnapshotChanged OnCharacterStatSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "NU|Progression|Server")
	FOnNUProgressionRequestStateChanged OnProgressionRequestStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "NU|Progression|Server")
	FOnNUProgressionRequestFailed OnProgressionRequestFailed;

private:
	friend class UNUJobComponent;

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_LocalProgressionBackendReady();

	UFUNCTION(Client, Reliable)
	void ClientDebugUseProgressionState(const FNUProgressionState& DebugState);

	// 컴포넌트 소유자가 INUCombatantInterface를 구현하고 있다면 ASC를 가져옴
	UNUAbilitySystemComponent* GetOwnerASC() const;
	UNUJobComponent* GetJobComponent() const;
	UNUProgressionNetSubsystem* GetProgressionNetSubsystem() const;
	void HandleProgressionSnapshotChanged();
	void HandleProgressionRequestStateChanged();
	void HandleProgressionRequestFailed(int32 ResultCode, const FText& Message);
	void RefreshCharacterStatsAfterProgressionChange();
	// 170~172 상태가 성장 도메인의 레벨, 경험치, 스탯 기준이다.
	FNUProgressionState BuildEffectiveProgressionState() const;
	FNUProgressionNodeRuleContext BuildNodeRuleContext() const;

	bool ValidateState(const FNUProgressionState& CandidateState, const FNUProgressionNodeRuleContext& RuleContext, FString& OutError) const;
	bool CanAcquireNode(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionNodeRuleContext& RuleContext, int32 ProgressionGraphId, FString& OutError) const;
	bool TryInitializeLocalJobProgression(int32 JobId, FString& OutError);
	bool TryAcquireLocalNode(const FNUProgressionNodeDefinitionRow& Node, const FNUJobState& RequestedJobState, FString& OutError);
	bool ApplyLocalNodeRewards(const FNUProgressionNodeDefinitionRow& Node, const TArray<const FNUProgressionNodeRewardRow*>& Rewards, FNUProgressionState& InOutProgressionState, FNUJobState& InOutJobState, FString& OutError) const;
	bool ApplyPrimaryStatReward(FNUProgressionState& InOutState, int32 StatId, float Value, FString& OutError) const;

	// 현재 서버에게 받은 stats을 그대로 반환
	FNUPrimaryStats ResolvePrimaryStats(const FNUProgressionState& CandidateState) const;
	
	// 성장 상태를 GAS로 옮기는 함수
	/** 
	* 1. ASC 획득
	* 2. 두 상시 GE 적용 보장
	* 3. Primary Stats 적용
	*/
	bool ProjectStateToASC(const FNUProgressionState& CandidateState);

	// Base Primary Stats 값을 서버 상태로 교체
	void ApplyPrimaryStats(UNUAbilitySystemComponent* ASC, const FNUPrimaryStats& PrimaryStats) const;

	// GE가 적용되어 있는지 확인
	bool EnsureStatEffectsApplied(UNUAbilitySystemComponent* ASC);
	// GE 적용
	bool EnsureEffectApplied(UNUAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass,
		FActiveGameplayEffectHandle& InOutHandle, const TCHAR* EffectLabel);

	void InitializeCurrentVitals(UNUAbilitySystemComponent* ASC) const;
	UEmberServerManager* GetServerManager() const;

private:
	UPROPERTY(ReplicatedUsing = OnRep_State,
		VisibleAnywhere, BlueprintReadOnly, Category = "NU|Progression", meta = (AllowPrivateAccess = "true"))
	FNUProgressionState State;

	UPROPERTY(ReplicatedUsing = OnRep_LocalProgressionBackendReady)
	bool bLocalProgressionBackendReady = false;

// -------------------------------------------------------------------------
// GE와 GE를 식별하는 Handle
// -------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "NU|Progression", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> BaseSecondaryStatsEffect;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Progression", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> SecondaryStatScalingEffect;

	FActiveGameplayEffectHandle BaseSecondaryStatsHandle;
	FActiveGameplayEffectHandle SecondaryStatScalingHandle;

	UPROPERTY(Transient)
	FNUCharacterStatSnapshot CharacterStatSnapshot;

	bool bCurrentVitalsInitialized = false;
	bool bHasAppliedServerProgressionSnapshot = false;
	bool bHasCharacterStatSnapshot = false;
	bool bCharacterStatRequestPending = false;
	bool bRefreshCharacterStatsAfterPendingRequest = false;
};
