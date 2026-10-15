#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Recovery/Data/NURecoveryTypes.h"
#include "NURecoveryComponent.generated.h"

class ANUPlayerState;
class UNUAbilitySystemComponent;
class UGameplayEffect;
class UNUDialogueDefinition;
struct FOnAttributeChangeData;
enum class EEmberConnState : uint8;

// 플레이어별 등록 거점과 사망 상태
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNURecoveryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNURecoveryComponent();

	// 실행 권한과 로컬 이용 조건
	bool CanExecuteAuthorityRecovery() const;
	bool IsUsingExternalServer() const;
	bool CanRegisterPoint(int32 LocationId) const;
	bool IsPaidRecovery() const;

	// 외부 서버를 사용하지 않는 실행의 HP 회복과 휴식 시작
	bool CanRecoverHealth() const;
	bool RecoverHealth(float HealthRatio, TSubclassOf<UGameplayEffect> EffectClass, AActor* Source);
	bool StartRest(AActor* Source);

	// 외부 서버 요청 및 확정 결과 반영
	// 비용과 회복량은 여기서 계산하지 않는다.
	bool RequestServerRecovery(int32 NpcId, bool bRest, UNUDialogueDefinition* FailureDialogue);
	void ApplyServerRecoveryState();
	void ProcessServerRespawn();

	// 로컬 휴식 종료용 API
	UFUNCTION(BlueprintCallable, Category = "NU|Recovery")
	void StopRest();

	UFUNCTION(BlueprintPure, Category = "NU|Recovery")
	bool IsResting() const;

	// 거점 등록과 서버 확정 등록 ID 조회
	UFUNCTION(BlueprintCallable, Category = "NU|Recovery")
	bool RegisterPoint(int32 LocationId);

	UFUNCTION(BlueprintPure, Category = "NU|Recovery")
	int32 GetRegisteredLocationId() const;

	// 로컬 사망 사건을 한 번만 처리하고, 거점 이동 실패 시 재시도한다.
	bool QueueDeath(const FGuid& DeathId, bool bBattleHandoff = false);
	bool HasPendingDeath() const;

	UFUNCTION(BlueprintCallable, Category = "NU|Recovery")
	bool RetryPendingRespawn();

	bool TryGetDeathGoldLoss(int64& OutLoss) const;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 서버 결과 반영 대기와 실패 안내. HP와 골드는 각각 한 번만 반영한다.
	void HandleRecoveryRequestFailed(int32 Code, const FText& Message);

	// Pawn과 GAS 초기화 또는 전투 정리가 끝나지 않았으면 타이머로 이어서 처리한다.
	FTimerHandle ServerRecoveryTimer;
	uint64 AppliedServerState = 0;
	uint64 AppliedGoldState = 0;
	TWeakObjectPtr<UNUDialogueDefinition> ServerFailureDialogue;

	// 로컬 플레이어·나이아 대상 검증과 경과 시간 회복.
	bool GatherHealthTargets(TArray<UNUAbilitySystemComponent*>& OutTargets, bool& bNeedsRecovery) const;
	bool ApplyRestElapsed();
	void FinishRest(bool bApplyRemainder);

	UFUNCTION()
	void HandleRestRosterChanged();

	// 회복 적용 중 재진입을 막고, 휴식 시작 당시의 대상 목록을 보관한다.
	bool bApplyingHealth = false;

	UPROPERTY(Replicated)
	bool bResting = false;

	TWeakObjectPtr<AActor> RestSource;
	TWeakObjectPtr<APawn> RestPawn;
	double LastRestTime = 0.0;
	TArray<int64> RestRosterIds;
	TArray<int64> GetRestRosterIds() const;

	// 소유자 초기화와 로컬 사망 처리
	ANUPlayerState* GetPlayerState() const;
	void InitializeBindings();
	void ProcessPendingDeath();
	void HandleServerRespawnTransitionSucceeded();
	void HandleServerRespawnTransitionFailed();
	void CompleteServerRespawn();
	bool ResolveDeathCost();
	bool ValidateResurrectionTargets() const;

	// 거점 액터 탐색과 레벨 로딩. 서버 부활과 로컬 부활이 함께 사용하는 이동 준비 과정이다.
	bool PrepareRecoveryDestination(const FNURecoveryPointDefinition& Point, FTransform& OutTransform);
	bool FindLoadedRecoveryDestination(const FNURecoveryPointDefinition& Point, FTransform& OutTransform) const;
	void FailPendingDeath(const FText& Message);
	bool bPendingCostResolved = false;
	double DestinationWaitStarted = -1.0;
	double ServerRespawnFieldEntryStarted = -1.0;
	int32 ServerRespawnAreaId = 0;
	int32 ServerRespawnLocationId = 0;
	bool bServerRespawnTransitionActive = false;
	bool bServerRespawnFieldEntryPending = false;

	// 로컬 사망 감지와 HP 복원
	// 서버 세션의 사망 판정은 부활 푸시를 따른다.
	void CheckFieldDeath();
	bool RestoreVitals();
	bool RestoreASC(UNUAbilitySystemComponent* ASC);
	void Notify(const FText& Message) const;
	void HandleHealthChanged(const FOnAttributeChangeData& Change);

	// 로컬 자동 등록과 외부 서버 전환 이벤트
	UFUNCTION()
	void EvaluateAutomaticRegistration();

	UFUNCTION()
	void HandleServerStateChanged(EEmberConnState NewState);

	void HandleServerPointRegistered(int32 LocationId);

	UFUNCTION(Server, Reliable)
	void ServerRegisterPoint(int32 LocationId);

	UFUNCTION(Server, Reliable)
	void ServerRetryPendingRespawn();

	// 소유 클라이언트에 전달하는 로컬 회복 상태
	UPROPERTY(Replicated)
	int32 RegisteredLocationId = 0;

	UPROPERTY(Replicated)
	FGuid PendingDeathId;

	UPROPERTY(Replicated)
	bool bServerBackedSession = false;

	// 로컬 사망 중복 처리 방지와 확정한 손실액
	FGuid LastCompletedDeathId;
	int64 PendingGoldLoss = 0;
	TSet<FName> AppliedAutoRegistrationQuests;

	// 로컬 초기화/사망 처리 예약과 HP 변경 구독
	FTimerHandle RecoveryTimer;
	FDelegateHandle HealthChangedHandle;
	bool bProcessingDeath = false;
};
