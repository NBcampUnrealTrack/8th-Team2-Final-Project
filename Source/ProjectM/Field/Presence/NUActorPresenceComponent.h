#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "NUActorPresenceComponent.generated.h"

class ANUActorPresenceSpot;
class ANUApproachTrigger;
class UNUNaiaRosterComponent;
class UNUQuestComponent;
class APlayerController;
struct FNUActorPresenceRow;

// 로컬 진행에 따른 액터 표시·배치와 선택적인 퀘스트 역할 변경. 일반 상호작용은 액터가 소유한다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUActorPresenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUActorPresenceComponent();

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUActorPresenceChanged, const FNUActorPresenceRow*);
	FOnNUActorPresenceChanged OnPresenceChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	FName PresenceId = NAME_None;

	UFUNCTION(BlueprintPure, Category = "NU|ActorPresence")
	bool IsPresent() const { return bPresent; }

	// 외부 연출을 중단한 뒤 현재 진행에 맞는 자리를 즉시 복원한다.
	void RefreshPresenceImmediately();

	// 대화가 끝나고 가려진 화면에서 재배치되기를 기다리는지.
	UFUNCTION(BlueprintPure, Category = "NU|ActorPresence")
	bool IsPlacementTransitionPending() const { return bPlacementPending; }
	void ApplyPendingPlacement();
	bool IsPlacementVisibleTo(const APlayerController& Controller) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindQuest();
	UFUNCTION()
	void RefreshPresence();
	void ApplyStep(FName RowName, const FNUActorPresenceRow* Step);
	void CancelPendingPlacement();
	// 시작 계약 여부를 믿을 수 있는지. 서버 세션이면 로스터 목록을 받은 뒤부터다.
	bool IsStarterContractKnown() const;
	ANUActorPresenceSpot* FindSpot(FName SpotId) const;

	// 접근 트리거(ApproachActorId)를 쓰는 행은 조건이 참인 동안 플레이어가 그 범위에 들어선 뒤부터 참이다.
	void BindApproachTriggers();
	void HandleApproachEntered();
	bool IsApproachMet(FName RowName, const FNUActorPresenceRow& Row);
	TArray<TPair<TWeakObjectPtr<ANUApproachTrigger>, FDelegateHandle>> ApproachBindings;
	// 이번 접속에서 조건이 참일 때 범위에 들어선 행. 한 번 들어서면 범위를 벗어나도 유지한다.
	TSet<FName> ApproachedRows;

	TWeakObjectPtr<UNUQuestComponent> BoundQuest;
	TWeakObjectPtr<UNUNaiaRosterComponent> BoundRoster;
	FTimerHandle BindTimer;
	FName CurrentRowName = NAME_None;
	FTransform InitialTransform;
	FName InitialQuestActorId;
	FGameplayTag InitialObjectiveType;
	bool bInitialCollisionEnabled = true;
	bool bPresent = true;
	bool bApplied = false;
	bool bProgressKnown = false;
	bool bQuestRoleOverridden = false;
	// 시작 계약 조건(ContractedStarterNaiaId)을 쓰는 행이 있어 로스터도 봐야 하는지.
	bool bNeedsStarterContract = false;
	bool bHasResolvedStep = false;
	int32 AppliedDebugSerial = 0;
	uint64 LastDebugCompletionFrame = MAX_uint64;
	FName PendingRowName;
	bool bPlacementPending = false;
	bool bApplyingPlacement = false;
};
