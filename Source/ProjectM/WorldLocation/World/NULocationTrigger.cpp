#include "WorldLocation/World/NULocationTrigger.h"

#include "Components/BoxComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ANULocationTrigger::ANULocationTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	TriggerBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBounds"));
	SetRootComponent(TriggerBounds);
	TriggerBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBounds->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBounds->SetGenerateOverlapEvents(true);
	bGenerateOverlapEventsDuringLevelStreaming = true;
}

FName ANULocationTrigger::GetLocationId() const
{
	return LocationId;
}

bool ANULocationTrigger::ShouldShowEntryNotice() const
{
	return bShowEntryNotice;
}

int32 ANULocationTrigger::GetLocationPriority() const
{
	return LocationPriority;
}

UBoxComponent* ANULocationTrigger::GetTriggerBounds() const
{
	return TriggerBounds;
}

#if WITH_EDITOR
EDataValidationResult ANULocationTrigger::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (LocationId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("LocationId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
