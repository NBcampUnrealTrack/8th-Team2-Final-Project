#include "Field/Presence/NUActorPresenceSpot.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ANUActorPresenceSpot::ANUActorPresenceSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetHidden(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(SceneRoot);
}

FName ANUActorPresenceSpot::GetSpotId() const
{
	return SpotId;
}

#if WITH_EDITOR
EDataValidationResult ANUActorPresenceSpot::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (SpotId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("SpotId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
