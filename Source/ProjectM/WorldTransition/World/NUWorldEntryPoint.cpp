#include "WorldTransition/World/NUWorldEntryPoint.h"
#include "WorldTransition/World/NUWorldPortal.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ANUWorldEntryPoint::ANUWorldEntryPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(SceneRoot);
}

FName ANUWorldEntryPoint::GetEntryPointId() const
{
	return EntryPointId;
}

void ANUWorldEntryPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 현재 위치를 유지하며 출입구에 부착한다.
	if (IsValid(PlacementPortal) && PlacementPortal->GetLevel() == GetLevel() && !PlacementPortal->IsAttachedTo(this))
	{
		if (GetAttachParentActor() != PlacementPortal)
		{
			AttachToActor(PlacementPortal, FAttachmentTransformRules::KeepWorldTransform);
		}
		PreviousPlacementPortal = PlacementPortal;
	}
	// 연결 해제 시에도 현재 위치를 유지한다.
	else if (PlacementPortal == nullptr && PreviousPlacementPortal.IsValid())
	{
		if (GetAttachParentActor() == PreviousPlacementPortal.Get())
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		PreviousPlacementPortal.Reset();
	}
}

#if WITH_EDITOR
EDataValidationResult ANUWorldEntryPoint::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (IsValid(PlacementPortal) && (PlacementPortal->GetLevel() != GetLevel() || PlacementPortal->IsAttachedTo(this)))
	{
		Context.AddError(FText::FromString(TEXT("PlacementPortal은 같은 레벨의 순환 부착이 없는 출입구여야 합니다.")));
		Result = EDataValidationResult::Invalid;
	}
	if (EntryPointId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("EntryPointId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
