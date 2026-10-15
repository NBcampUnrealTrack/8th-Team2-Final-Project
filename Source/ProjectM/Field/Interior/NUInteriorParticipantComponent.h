#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NUInteriorParticipantComponent.generated.h"

class ACharacter;
class ANUWalkableDoor;
class APawn;
class AController;
class APlayerController;
class UNUWalkableInteriorComponent;
enum class EEmberConnState : uint8;
struct FGrpcRevGameLocationMove;
struct FNULocationDefinition;

// 짧은 암전 중 문 가까운 안전 위치와 크기, 이동속도, 실내외 상태를 함께 전환한다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUInteriorParticipantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUInteriorParticipantComponent();

	UFUNCTION(BlueprintPure, Category = "NU|Interior")
	UNUWalkableInteriorComponent* GetCurrentInterior() const;
	UFUNCTION(BlueprintCallable, Category = "NU|Interior")
	void RefreshInterior();
	UFUNCTION(BlueprintCallable, Category = "NU|Interior|Transition")
	bool CommitInteriorTransition();
	UFUNCTION(BlueprintCallable, Category = "NU|Interior|Transition")
	void FinishInteriorTransition();
	UFUNCTION(BlueprintPure, Category = "NU|Interior|Transition")
	bool IsInteriorTransitionPending() const { return bTransitionPending || bApplyingTransition; }
	// 문을 열기 전에 연결된 실내와 현재 방향의 목적지를 검사한다.
	bool CanUseDoor(const ANUWalkableDoor* Door) const;
	// UEmberServerManager가 전달한 서버 확정값으로 대기 중인 문 출입을 완료한다.
	void HandleLocationMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response);

	void AddInterior(UNUWalkableInteriorComponent* Interior);
	void RemoveInterior(UNUWalkableInteriorComponent* Interior);
	bool EnterInterior(UNUWalkableInteriorComponent* Interior);
	bool LeaveInterior(UNUWalkableInteriorComponent* Interior);
	// LocationRestore가 저장 좌표와 실내외 상태를 충돌 검사 후 한 번에 적용한다.
	bool ApplyRestoredState(UNUWalkableInteriorComponent* Interior, const FVector* Destination);
	FVector GetTransitionScale(const UNUWalkableInteriorComponent* Interior) const { return GetDestinationScale(Interior); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior")
	bool bScaleMovement = true;
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Door transitions preserve movement settings."))
	bool bScaleStepHeight = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const FNULocationDefinition* FindDoorDestination(const ANUWalkableDoor* Door, bool bInside) const;
	bool RequestDoorTransition(ANUWalkableDoor* Door, const FVector& PreviousLocation);
	FVector GetDestinationScale(const UNUWalkableInteriorComponent* Interior) const;
	// 도착 크기의 충돌 검사에 통과한 경우에만 위치와 실내 상태를 적용한다.
	bool ApplyInteriorState(UNUWalkableInteriorComponent* Interior, const FVector* Destination = nullptr);
	void CompleteTransitionFade();
	void BeginTransitionFadeIn();
	void ReleaseTransitionInput();
	void ShowEntryFailureNotice() const;
	void ClearPendingLocationMove();
	void RollbackRejectedTransition();
	UFUNCTION()
	void HandleServerStateChanged(EEmberConnState NewState);
	UFUNCTION()
	void HandleDoorOverlapBegin(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleDoorOverlapEnd(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	void HandleTransformChanged(USceneComponent* Component, EUpdateTransformFlags UpdateFlags, ETeleportType Teleport);
	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
	TArray<TWeakObjectPtr<UNUWalkableInteriorComponent>> Interiors;
	TArray<TWeakObjectPtr<ANUWalkableDoor>> NearbyDoors;
	TWeakObjectPtr<UNUWalkableInteriorComponent> CurrentInterior;
	TWeakObjectPtr<USceneComponent> ObservedRoot;
	TWeakObjectPtr<ANUWalkableDoor> TransitionDoor;
	// 실패한 문 접촉이 이동 갱신마다 같은 전환 요청을 반복하지 않도록 막는다.
	TWeakObjectPtr<ANUWalkableDoor> BlockedDoor;
	TWeakObjectPtr<UNUWalkableInteriorComponent> TransitionInterior;
	TWeakObjectPtr<APlayerController> TransitionController;
	FVector TransitionOrigin = FVector::ZeroVector;
	FVector TransitionFallbackLocation = FVector::ZeroVector;
	FTimerHandle TransitionCommitTimer;
	FTimerHandle TransitionFinishTimer;
	float TransitionFadeInDuration = 0.0f;
	float ExteriorWalkSpeed = 0.0f;
	int32 LastExteriorLocationId = 0;

	int32 PendingFromLocationId = 0;
	int32 PendingToLocationId = 0;
	int32 PendingExpectedAreaId = 0;
	FString PendingLocationMutationId;
	FVector PendingLocationDestination = FVector::ZeroVector;

	bool bTransitionPending = false;
	bool bTransitionInputLocked = false;
	bool bTransitionCommitted = false;
	bool bTransitionInside = false;
	bool bAwaitingLocationMove = false;
	bool bPendingFromIndoor = false;
	FVector ExteriorScale = FVector::OneVector;
	bool bHasExteriorScale = false;
	const UNUWalkableInteriorComponent* InitializingInterior = nullptr;
	bool bApplyingTransition = false;
	bool bEndingPlay = false;
};
