#include "Interaction/Component/NUInteractorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "Interaction/NUInteractionSpatialRules.h"

UNUInteractorComponent::UNUInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(false);
}

void UNUInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(DetectionComponent))
	{
		DetectionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionRangeBeginOverlap);
		DetectionComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionRangeEndOverlap);
	}

	DetectionComponent = nullptr;
	CurrentInteractable = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UNUInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateCurrentInteractable();
}

void UNUInteractorComponent::InitializeDetectionComponent(UPrimitiveComponent* InDetectionComponent)
{
	if (DetectionComponent == InDetectionComponent)
	{
		UpdateCurrentInteractable();
		return;
	}

	if (IsValid(DetectionComponent))
	{
		DetectionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionRangeBeginOverlap);
		DetectionComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionRangeEndOverlap);
	}

	DetectionComponent = InDetectionComponent;
	if (IsValid(DetectionComponent))
	{
		DetectionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleInteractionRangeBeginOverlap);
		DetectionComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleInteractionRangeEndOverlap);
	}

	UpdateCurrentInteractable();
}

void UNUInteractorComponent::HandleInteractionRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UpdateCurrentInteractable();
}

void UNUInteractorComponent::HandleInteractionRangeEndOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	UpdateCurrentInteractable();
}

AActor* UNUInteractorComponent::UpdateCurrentInteractable()
{
	AActor* PreviousInteractable = GetCurrentInteractable();
	bool bHasInteractionCandidates = false;
	AActor* NewInteractable = FindNearestInteractionTarget(bHasInteractionCandidates);

	// CanInteract 상태가 바뀔 수 있는 후보가 겹쳐 있을 때만 저주기 Tick으로 갱신한다.
	SetComponentTickEnabled(bHasInteractionCandidates);

	// 선택 대상이 바뀐 경우에만 상태 저장 및 변경 사실 알림
	if (PreviousInteractable != NewInteractable)
	{
		CurrentInteractable = NewInteractable;
		OnCurrentInteractableChanged.Broadcast(PreviousInteractable, NewInteractable);
	}

	return NewInteractable;
}

bool UNUInteractorComponent::RevalidateInteractionTarget(AActor* Target)
{
	return IsValid(Target) && UpdateCurrentInteractable() == Target;
}

AActor* UNUInteractorComponent::GetCurrentInteractable() const
{
	return IsValid(CurrentInteractable) ? CurrentInteractable.Get() : nullptr;
}

bool UNUInteractorComponent::HasCurrentInteractable() const
{
	return IsValid(CurrentInteractable);
}

AActor* UNUInteractorComponent::FindNearestInteractionTarget(bool& bOutHasInteractionCandidates) const
{
	bOutHasInteractionCandidates = false;

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(DetectionComponent))
	{
		return nullptr;
	}

	TArray<AActor*> OverlappingActors;
	DetectionComponent->GetOverlappingActors(OverlappingActors);

	AActor* BestInteractable = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (AActor* Candidate : OverlappingActors)
	{
		if (!IsValid(Candidate) || Candidate == Owner)
		{
			continue;
		}

		// 일반 액터 제외
		if (!Candidate->GetClass()->ImplementsInterface(UNUInteractableInterface::StaticClass()))
		{
			continue;
		}
		bOutHasInteractionCandidates = true;

		// 상호작용 할 수 없는 대상 제외(잠김, 처치됨, 조건 미충족 등)
		if (!NUInteractionSpatialRules::CanInteract(Owner, Candidate) || !INUInteractableInterface::Execute_CanInteract(Candidate, Owner))
		{
			continue;
		}

		// 플레이어와 가장 가까운 액터 선택
		const float DistanceSquared = FVector::DistSquared(
			Owner->GetActorLocation(),
			Candidate->GetActorLocation()
		);

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestInteractable = Candidate;
		}
	}

	return BestInteractable;
}
