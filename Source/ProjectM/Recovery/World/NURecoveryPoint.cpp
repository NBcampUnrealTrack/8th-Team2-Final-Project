#include "Recovery/World/NURecoveryPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"

ANURecoveryPoint::ANURecoveryPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RecoveryRoot")));
	RespawnLocation = CreateDefaultSubobject<UCapsuleComponent>(TEXT("SpawnCapsule"));
	UCapsuleComponent* Capsule = RespawnLocation;
	Capsule->SetupAttachment(GetRootComponent());
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->SetHiddenInGame(true);
	UArrowComponent* Direction = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingDirection"));
	Direction->SetupAttachment(Capsule);
	Direction->SetHiddenInGame(true);
}

FTransform ANURecoveryPoint::GetRecoveryTransform() const
{
	return RespawnLocation->GetComponentTransform();
}
