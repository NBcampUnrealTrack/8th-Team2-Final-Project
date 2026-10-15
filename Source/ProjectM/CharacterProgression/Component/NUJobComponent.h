#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterProgression/Data/NUJobTypes.h"
#include "NUJobComponent.generated.h"

// 직업, 전직 상태 변경 시 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUJobChanged);

// 플레이어의 직업, 전직 상태 및 최초 직업 선택 권한을 관리
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUJobComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUJobComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 직업 상태 적용 */
	bool ApplyState(const FNUJobState& NewState);
	bool CanApplyState(const FNUJobState& CandidateState, FString& OutError) const;

	UFUNCTION(BlueprintPure, Category = "NU|Job")
	const FNUJobState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "NU|Job")
	int32 GetJobId() const { return State.JobId; }

	UFUNCTION(BlueprintPure, Category = "NU|Job")
	int32 GetAdvancementId() const { return State.AdvancementId; }

	UFUNCTION(BlueprintPure, Category = "NU|Job")
	bool HasJob() const { return State.JobId > 0; }

	// Q9 데이터 연결 전에는 설정된 NPC의 전직 선택지를 실행한 경우 최초 직업 선택 가능
	bool IsInitialJobSelectionNpc(FName NpcId) const;
	bool CanAuthorizeInitialJobSelection(FName NpcId) const;
	bool TryAuthorizeInitialJobSelection(FName NpcId);
	bool IsInitialJobSelectionAuthorized() const { return bInitialJobSelectionAuthorized; }
	bool RequestInitialJobSelection(int32 JobId);

	// 대화 권한과 정적 데이터를 검증한 뒤 직업, 시작 노드, 기본 스킬 3종을 함께 확정
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Job")
	void ServerSelectInitialJob(int32 JobId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Job|Debug", meta = (DevelopmentOnly))
	void ServerDebugSetJobState(int32 InJobId, int32 InAdvancementId);

	// 직업 또는 전직 상태가 변경될 때 발생
	UPROPERTY(BlueprintAssignable, Category = "NU|Job")
	FOnNUJobChanged OnJobChanged;

private:
	friend class UNUProgressionComponent;

	UFUNCTION()
	void OnRep_State();
	bool ApplyStateInternal(const FNUJobState& NewState, bool bNotify);
	void NotifyStateChanged();

	// 현재 직업 상태
	UPROPERTY(ReplicatedUsing=OnRep_State, 
		VisibleAnywhere, BlueprintReadOnly, Category = "NU|Job", meta = (AllowPrivateAccess = "true"))
	FNUJobState State;

	// 대화를 마친 현재 세션에서만 유효하며 직업 확정 후 소비
	bool bInitialJobSelectionAuthorized = false;
};
