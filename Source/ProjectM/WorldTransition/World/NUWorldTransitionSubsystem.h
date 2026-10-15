#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WorldTransition/Data/NUWorldPortalDefinition.h"
#include "WorldTransition/Data/NUWorldTransitionTypes.h"
#include "NUWorldTransitionSubsystem.generated.h"

class ACharacter;
class APawn;
class ANUPlayerController;
class ANUWorldEntryPoint;
class ANUWorldPortal;
class ULevel;
class ULevelStreaming;
class ULevelSequence;
class UNUWorldStreamingSubsystem;
class UFieldSyncSubsystem;
class UNUWalkableInteriorComponent;
struct FGrpcRevGameLocationMove;
struct FGrpcRevGamePortalMove;
struct FGrpcRevGamePortalArrive;

// 서버가 확정한 위치 복원과 부활 이동을 같은 전환 경로로 처리한다.
struct FNUWorldRestoreTransitionRequest
{
	FNUWorldDestination Destination;
	FVector Arrival = FVector::ZeroVector;
	int32 Facing = 0;
	bool bUseSavedArrival = false;
	int32 LocationId = 0;
	int32 FallbackLocationId = 0;
	// 0보다 크면 준비된 레벨의 해당 부활 거점 액터에서 도착 좌표를 읽는다.
	int32 RecoveryPointId = 0;
	bool bIndoor = false;
	FNUWorldDestination ReturnDestination;
	int32 ReturnLocationId = 0;
	FSimpleDelegate OnSucceeded;
	FSimpleDelegate OnFailed;
};

// 공유 실내도 실제 입장한 문과 층 순서로 되돌아간다.
struct FNUWorldReturnRoute
{
	FNUWorldDestination Destination;
	int32 LocationId = 0;
	bool bIndoor = false;
};

// 목적지 로딩부터 텔레포트와 페이드까지 전환을 관리
// 중복 이동을 차단하고 실패 시 위치와 입력을 복구한다.
UCLASS()
class PROJECTM_API UNUWorldTransitionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool CanRequestTransition(const AActor* Interactor) const;
	// 이동 요청 시작, 완료 여부는 전환 상태로 확인
	bool RequestTransition(AActor* Interactor, const FNUWorldDestination& Destination);
	// 숫자 PortalId는 테이블과 배치 매핑을 거쳐 요청한다.
	bool RequestPortalTransition(AActor* Interactor, const ANUWorldPortal* Portal);
	bool RequestLocationTransition(AActor* Interactor, const ANUWorldPortal* Portal);
	bool CanRequestLocationTransition(const AActor* Interactor, const ANUWorldPortal* Portal) const;
	bool HandleLocationMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response);
	// UEmberServerManager가 전달한 포탈 승인/도착 확정 응답을 현재 전환에 반영한다.
	void HandlePortalMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGamePortalMove* Response);
	void HandlePortalArriveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGamePortalArrive* Response);
	// 서버가 가리킨 공간을 준비하고 복원 좌표 또는 부활 거점으로 이동한다.
	bool RequestRestoreTransition(AActor* Interactor, const FNUWorldRestoreTransitionRequest& Request);
	int32 GetLastRestoreLocationId() const { return LastRestoreLocationId; }
	// 이동 전에는 위치 복구, 이동 후에는 연출 종료
	void CancelTransition();

	bool IsTransitionInProgress() const;
	ENUWorldTransitionState GetTransitionState() const;
	ENUWorldTransitionFailure GetLastFailure() const;

	// 현재 공간 조회하여 지역을 판단할 때 사용
	ULevelStreaming* GetActiveStreamingLevel() const;
	ULevel* GetActiveLevel(const APawn* Pawn) const;
	FString GetActiveMapPackageName(const APawn* Pawn) const;
	int32 GetActiveAreaId(const APawn* Pawn) const;

private:
	bool StartTransition(AActor* Interactor, const FNUWorldDestination& Destination, bool bInternalRestore);
	bool IsBattleActive(const ACharacter* Character) const;

	// 목적지 준비 상태 확인
	void ContinueTransition();

	// 페이드 진행과 종료 처리
	void HandleFadeOutFinished();
	void HandleFadeInterrupted();
	void BeginFadeIn();
	void FinishTransition();

	// 도착 지점 검색 및 검사
	ANUWorldEntryPoint* FindEntryPoint(ULevel* Level, FName EntryPointId) const;
	bool FindSafeArrival(ACharacter* Character, const ANUWorldEntryPoint* EntryPoint, FVector& OutLocation) const;
	void CompleteTransition(ANUWorldEntryPoint* EntryPoint);
	bool PrepareLocationArrival();
	bool ResolveLocationPortal(const ANUWorldPortal* Portal, FNUWorldDestination& OutDestination, int32& OutLocationId, bool& bOutIndoor) const;
	void CheckLocationConnection();
	void CommitConfirmedTransition(const FVector& Arrival);

	// 실패 처리 / 캐릭터 상태 복구
	void FailTransition(ENUWorldTransitionFailure Failure, const TCHAR* Detail);
	void RestoreCharacterState(bool bRestoreOriginalTransform);

	// 자동 재진입 방지 / 도착 장소 갱신
	void SuppressArrivalPortals(ACharacter* Character);
	void RefreshWorldLocation();
	void ResetPortalRequest();
	void RejectPortalRequest(ENUWorldTransitionFailure Failure, const FString& Detail);
	void ReportPortalPreparationFailure(ENUWorldTransitionFailure Failure);
	void CompleteRestoreRequest(bool bSuccess);
	int32 GetTransitionFacing() const;

	UPROPERTY(Transient)
	TObjectPtr<UNUWorldStreamingSubsystem> Streaming;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequence> FadeInSequence;

	// 현재 공간과 준비 중인 목적지를 별도로 유지
	FNUWorldSpaceHandle ActiveSpace;
	FNUWorldSpaceHandle PendingSpace;
	FNUWorldDestination ActiveDestination;
	FNUWorldDestination PendingDestination;
	FNUWorldPortalRoute PendingPortalRoute;

	TWeakObjectPtr<ACharacter> ActiveCharacter;
	TWeakObjectPtr<ACharacter> TransitionCharacter;
	TWeakObjectPtr<ANUPlayerController> TransitionController;
	TWeakObjectPtr<ACharacter> LocationRefreshCharacter;
	TWeakObjectPtr<UFieldSyncSubsystem> SuspendedFieldSync;
	TWeakObjectPtr<UNUWalkableInteriorComponent> PendingInterior;
	float LocationFadeOutSeconds = 0.15f;
	float LocationFadeInSeconds = 0.15f;
	bool bLocationRequestActive = false;
	bool bLocationMoveSent = false;
	bool bPendingIndoor = false;
	int32 PendingSourceLocationId = 0;
	int32 PendingTargetLocationId = 0;
	int32 PendingGateId = 0;
	FString PendingLocationMutationId;
	FTimerHandle LocationConnectionTimer;
	double LocationRequestSentAt = 0.0;
	TArray<FNUWorldReturnRoute> ActiveReturnRoutes;
	TArray<FNUWorldReturnRoute> PendingReturnRoutes;

	// 복구에 사용할 이동 전 상태
	FTransform OriginalCharacterTransform = FTransform::Identity;
	FVector PendingArrival = FVector::ZeroVector;
	TEnumAsByte<EMovementMode> OriginalMovementMode = MOVE_Walking;
	uint8 OriginalCustomMovementMode = 0;

	FTimerHandle PreparationTimer;
	FTimerHandle FadeTimer;
	FTimerHandle LocationRefreshTimer;

	float FadeInSeconds = 0.0f;
	bool bOwnsFade = false;
	bool bDestinationCommitted = false;
	bool bPortalRequestActive = false;
	bool bPortalArrivalSent = false;
	bool bRestoreRequestActive = false;
	bool bRestoreUsesSavedArrival = false;
	FString PendingPortalMutationId;
	FString PendingServerTransitionId;
	FVector PendingRestoreArrival = FVector::ZeroVector;
	int32 PendingRestoreFacing = 0;
	int32 RestoreFallbackLocationId = 0;
	int32 PendingRecoveryPointId = 0;
	int32 LastRestoreLocationId = 0;
	FSimpleDelegate RestoreSucceededDelegate;
	FSimpleDelegate RestoreFailedDelegate;

	ENUWorldTransitionState State = ENUWorldTransitionState::Idle;
	ENUWorldTransitionFailure LastFailure = ENUWorldTransitionFailure::None;
	bool bInputBlockRequested = false;
	static const FName InputBlockReason;
};
