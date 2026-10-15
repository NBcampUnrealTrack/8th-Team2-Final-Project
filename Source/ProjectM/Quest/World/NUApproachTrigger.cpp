#include "Quest/World/NUApproachTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ANUApproachTrigger::ANUApproachTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	// 장소 트리거와 같이 폰과만 겹침을 만든다.
	TriggerBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBounds"));
	SetRootComponent(TriggerBounds);
	TriggerBounds->InitBoxExtent(FVector(150.f, 150.f, 100.f));
	TriggerBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBounds->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBounds->SetGenerateOverlapEvents(true);
	bGenerateOverlapEventsDuringLevelStreaming = true;
}

FName ANUApproachTrigger::GetApproachActorId() const
{
	return ApproachActorId;
}

bool ANUApproachTrigger::IsLocalPlayerInside() const
{
	const APlayerController* PlayerController = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	return Pawn != nullptr && TriggerBounds->IsOverlappingActor(Pawn);
}

ANUApproachTrigger* ANUApproachTrigger::Find(UWorld* World, FName InApproachActorId)
{
	if (World == nullptr || InApproachActorId.IsNone())
	{
		return nullptr;
	}

	for (TActorIterator<ANUApproachTrigger> It(World); It; ++It)
	{
		if (It->ApproachActorId == InApproachActorId)
		{
			return *It;
		}
	}
	return nullptr;
}

void ANUApproachTrigger::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn != nullptr && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled())
	{
		OnLocalPlayerEntered.Broadcast();
	}
}

#if WITH_EDITOR
EDataValidationResult ANUApproachTrigger::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (ApproachActorId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("ApproachActorId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
