#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NUInteractorComponent.generated.h"

class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNUCurrentInteractableChanged,
	AActor*, PreviousInteractable,
	AActor*, CurrentInteractable
);


// 플레이어의 충돌 컴포넌트와 겹친 상호작용 대상 중 가장 가까운 대상을 선택
// 대상의 옵션 수집과 실제 실행은 이후 Router와 Provider에서 담당
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUInteractorComponent();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleInteractionRangeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleInteractionRangeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex
	);

public:
	// 플레이어 Capsule처럼 대상 InteractionVolume과 겹칠 충돌 컴포넌트를 연결
	void InitializeDetectionComponent(UPrimitiveComponent* InDetectionComponent);

	// 범위 안에서 상호작용할 대상을 다시 업데이트하여 선택
	UFUNCTION(BlueprintCallable, Category = "NU|Interaction")
	AActor* UpdateCurrentInteractable();

	// 실행 직전에 현재 Overlap 대상과 대상 조건을 같은 규칙으로 다시 확인
	UFUNCTION(BlueprintCallable, Category = "NU|Interaction")
	bool RevalidateInteractionTarget(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "NU|Interaction")
	AActor* GetCurrentInteractable() const;

	UFUNCTION(BlueprintPure, Category = "NU|Interaction")
	bool HasCurrentInteractable() const;

	// 프롬프트 UI가 대상 변경만 관찰할 수 있도록 알림
	UPROPERTY(BlueprintAssignable, Category = "NU|Interaction")
	FNUCurrentInteractableChanged OnCurrentInteractableChanged;

private:
	// 가장 가까이 있는 타겟 찾기
	AActor* FindNearestInteractionTarget(bool& bOutHasInteractionCandidates) const;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> DetectionComponent;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentInteractable;
};
