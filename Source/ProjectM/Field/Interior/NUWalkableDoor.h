#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "NUWalkableDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UNUWalkableInteriorComponent;
class ACharacter;

// 로컬 플레이어가 접근하면 통로를 열고 외부에서는 검은 면으로 내부를 가린다.
UCLASS(Blueprintable)
class PROJECTM_API ANUWalkableDoor : public AActor, public INUInteractableInterface
{
	GENERATED_BODY()

public:
	ANUWalkableDoor();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;
	UFUNCTION(BlueprintPure, Category = "NU|Door")
	bool IsDoorOpen() const;
	UFUNCTION(BlueprintCallable, Category = "NU|Door")
	bool CloseDoor();
	// 서버가 출입을 거절했을 때 문을 닫고, 캐릭터가 접근 범위를 벗어날 때까지 재개방을 막는다.
	bool RejectEntry(ACharacter* Character);
	bool IsTransitionContact(const ACharacter* Character, bool bInside) const;
	bool IsTransitionApproach(const ACharacter* Character, const FVector& PreviousLocation, bool bInside) const;
	// 바닥, 실내외 방향, 도착 캡슐의 충돌을 모두 만족하는 위치를 반환한다.
	bool FindArrivalLocation(const ACharacter* Character, bool bInside, const FVector& DestinationScale, FVector& OutLocation) const;
	void RefreshAutomaticOpen(ACharacter* Character);
	UNUWalkableInteriorComponent* GetInterior() const { return Interior.Get(); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Door")
	TObjectPtr<UBoxComponent> InteractionVolume;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Door")
	TObjectPtr<UBoxComponent> DoorBlocker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Door")
	TObjectPtr<UStaticMeshComponent> DoorPanel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Door")
	TObjectPtr<UStaticMeshComponent> Blackout;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NU|Door")
	TObjectPtr<AActor> InteriorOwner;
	// 입장을 막아도 이미 실내에 있는 캐릭터의 퇴장은 허용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door", meta = (DisplayName = "Entry Enabled"))
	bool bEntryEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door")
	bool bCloseAfterLeaving = true;

	// 서버 LocationMove 로그에서 문을 구분하는 ID, 연결표가 없으면 0을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Location", meta = (ClampMin = "0"))
	int32 GateId = 0;

	// 실내에서 시작한 세션처럼 이전 실외 장소를 모를 때 사용할 문 바깥 LocationId다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Location", meta = (ClampMin = "0"))
	int32 ExteriorLocationId = 0;

	// 끄면 실외와 실내 모두 F 상호작용으로 문을 연다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door", meta = (DisplayName = "Open On Overlap"))
	bool bOpenOnOverlap = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Transition", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutDuration = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Transition", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInDuration = 0.15f;
	// 문턱 앞뒤의 작은 여유다. 접근 볼륨과 실제 전환 범위를 분리한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Transition", meta = (ClampMin = "0.0", Units = "cm"))
	float ContactPadding = 2.0f;
	// 기존 문 BP의 Box Component에 지정하는 태그다. 위치와 크기는 BP/레벨 인스턴스에서 편집한다.
	UPROPERTY(EditDefaultsOnly, Category = "NU|Door|Transition")
	FName EntryTriggerTag = TEXT("NU.Door.Entry");
	UPROPERTY(EditDefaultsOnly, Category = "NU|Door|Transition")
	FName ExitTriggerTag = TEXT("NU.Door.Exit");
	UPROPERTY(EditDefaultsOnly, Category = "NU|Door|Arrival")
	FName InteriorArrivalTag = TEXT("NU.Door.Arrival.Interior");
	UPROPERTY(EditDefaultsOnly, Category = "NU|Door|Arrival")
	FName ExteriorArrivalTag = TEXT("NU.Door.Arrival.Exterior");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Arrival", meta = (ClampMin = "1.0", Units = "cm"))
	float ArrivalClearance = 2.0f;
	// BP 도착점이 없는 문에서 사용하는 후보 탐색 간격이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Arrival", meta = (ClampMin = "1.0", Units = "cm"))
	float ArrivalSearchStep = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Door|Arrival", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ArrivalSearchSteps = 6;
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Door")
	void OnDoorOpenChanged(bool bOpen);
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Door")
	void OnInteriorViewChanged(bool bInside);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<UBoxComponent> EntryTransitionTrigger;
	TWeakObjectPtr<UBoxComponent> ExitTransitionTrigger;
	TWeakObjectPtr<USceneComponent> InteriorArrivalPoint;
	TWeakObjectPtr<USceneComponent> ExteriorArrivalPoint;
	TWeakObjectPtr<ACharacter> RejectedCharacter;
	UFUNCTION()
	void HandleInteractionEntry(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	bool OpenForCharacter(ACharacter* Character, bool bSilent = false);
	UBoxComponent* GetInteriorBounds() const;
	float GetInteriorContactDepth() const;
	UFUNCTION()
	void HandleInteractionExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	UFUNCTION()
	void HandlePresentationChanged(bool bInside);
	void RefreshInteractors();

	TWeakObjectPtr<UNUWalkableInteriorComponent> Interior;
	bool bDoorOpen = false;
};
