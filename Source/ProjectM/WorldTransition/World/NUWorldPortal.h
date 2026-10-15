#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldTransition/Data/NUWorldTransitionTypes.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "NUWorldPortal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ACharacter;

// 목적지를 지정하는 출입구 액터
// 상호작용 또는 Overlap으로 목적지 이동을 요청
UCLASS(Blueprintable)
class PROJECTM_API ANUWorldPortal : public AActor, public INUInteractableInterface
{
	GENERATED_BODY()

public:
	ANUWorldPortal();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;

	FName GetPortalId() const;
	const FNUWorldDestination& GetDestination() const;
	UBoxComponent* GetInteractionVolume() const;
	ENUWorldPortalActivation GetActivationMode() const;
	bool UsesPortalTable() const;
	ENUWorldPortalSide GetPlacementSide() const { return PlacementSide; }
	bool UsesLocationMove() const { return bLocationTransition; }
	int32 GetDestinationLocationId() const { return DestinationLocationId; }
	bool IsDestinationIndoor() const { return bDestinationIndoor; }
	int32 GetGateId() const { return GateId; }
	bool ReturnsToEntrance() const { return bReturnToEntrance; }
	FName GetReturnEntryPointId() const { return ReturnEntryPointId; }
	float GetLocationFadeOutDuration() const { return LocationFadeOutDuration; }
	float GetLocationFadeInDuration() const { return LocationFadeInDuration; }

	// 범위를 벗어날 때까지 자동 재진입을 차단
	void SuppressUntilExit(ACharacter* Character);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 건물 출입은 목적 레벨 준비 후 LocationMove로 확정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location")
	bool bLocationTransition = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location", meta = (ClampMin = "0", Units = "s", EditCondition = "bLocationTransition", EditConditionHides))
	float LocationFadeOutDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location", meta = (ClampMin = "0", Units = "s", EditCondition = "bLocationTransition", EditConditionHides))
	float LocationFadeInDuration = 0.15f;

	// 공통 실내의 출구는 입장 시 보관한 출발 공간과 문을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location", meta = (EditCondition = "bLocationTransition"))
	bool bReturnToEntrance = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location", meta = (EditCondition = "bLocationTransition && !bReturnToEntrance"))
	FName ReturnEntryPointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location", meta = (ClampMin = "0", EditCondition = "bLocationTransition"))
	int32 DestinationLocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location", meta = (EditCondition = "bLocationTransition"))
	bool bDestinationIndoor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition|Location", meta = (ClampMin = "0", EditCondition = "bLocationTransition"))
	int32 GateId = 0;

	// Interaction은 상호작용, Overlap은 범위 진입 시 이동.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	ENUWorldPortalActivation ActivationMode = ENUWorldPortalActivation::Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	TObjectPtr<UBoxComponent> InteractionVolume;

	// 실외 문을 직접 통과하지 못하게 막는다. 필요한 출입구에서만 충돌을 켠다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	TObjectPtr<UBoxComponent> EntranceBlocker;

	// 출입구 위치를 표시하는 임시 메시.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	FName PortalId = NAME_None;

	// Source는 정방향, Destination은 테이블의 반대 방향으로 이동한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition", meta = (DisplayName = "배치된 쪽", EditCondition = "!bLocationTransition", EditConditionHides))
	ENUWorldPortalSide PlacementSide = ENUWorldPortalSide::Source;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	FNUWorldDestination Destination;

private:
	UFUNCTION()
	void HandlePortalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePortalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	// 다음 프레임에 출입 조건을 다시 확인한다.
	void TryAutomaticTransition();

	TWeakObjectPtr<ACharacter> PendingCharacter;
	TSet<TWeakObjectPtr<ACharacter>> SuppressedCharacters;
	FTimerHandle AutomaticTransitionTimer;
};
