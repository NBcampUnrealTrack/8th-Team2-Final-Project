// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/NUBGMZone.h"

#include "Audio/NUBGMSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ANUBGMZone::ANUBGMZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
	SetRootComponent(ZoneBounds);

	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneBounds->SetCollisionObjectType(ECC_WorldDynamic);
	ZoneBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneBounds->SetGenerateOverlapEvents(true);
	ZoneBounds->SetBoxExtent(FVector(500.f, 500.f, 500.f));
}



void ANUBGMZone::BeginPlay()
{
	Super::BeginPlay();

	if (const UWorld* World = GetWorld())
	{
		if (UNUBGMSubsystem* BGM = IsValid(World->GetGameInstance()) ? World->GetGameInstance()->GetSubsystem<UNUBGMSubsystem>() : nullptr)
		{
			BGM->RegisterZone(this);
		}
	}

	// WholeLevel/BattleStage는 등록만으로 층이 정해지므로 박스 오버랩이 필요 없다
	if (Scope != ENUBGMZoneScope::Area)
	{
		return;
	}

	ZoneBounds->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleZoneBeginOverlap);
	ZoneBounds->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleZoneEndOverlap);
}

void ANUBGMZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		if (UNUBGMSubsystem* BGM = IsValid(World->GetGameInstance()) ? World->GetGameInstance()->GetSubsystem<UNUBGMSubsystem>() : nullptr)
		{
			BGM->UnregisterZone(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FName ANUBGMZone::GetBGMId() const
{
	return BGMId;
}

ENUBGMZoneScope ANUBGMZone::GetScope() const
{
	return Scope;
}

int32 ANUBGMZone::GetPriority() const
{
	return Priority;
}

const APawn* ANUBGMZone::ResolveOverlappingLocalPawn(const AActor* OtherActor, const UPrimitiveComponent* OtherComponent)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsValid(Pawn) || !Pawn->IsLocallyControlled())
	{
		return nullptr;
	}

	if (OtherComponent != Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
	{
		return nullptr;
	}

	return Pawn;
}

void ANUBGMZone::HandleZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ResolveOverlappingLocalPawn(OtherActor, OtherComponent) == nullptr)
	{
		return;
	}

	NotifyOverlapChanged();
}

void ANUBGMZone::HandleZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (ResolveOverlappingLocalPawn(OtherActor, OtherComponent) == nullptr)
	{
		return;
	}

	NotifyOverlapChanged();
}

void ANUBGMZone::NotifyOverlapChanged() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(World->GetGameInstance()))
	{
		return;
	}

	if (UNUBGMSubsystem* BGM = World->GetGameInstance()->GetSubsystem<UNUBGMSubsystem>())
	{
		BGM->RefreshAreaLayer();
	}
}

bool ANUBGMZone::IsLocalPawnInside() const
{
	TArray<AActor*> OverlappingActors;
	ZoneBounds->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (const AActor* OverlappingActor : OverlappingActors)
	{
		const APawn* Pawn = Cast<APawn>(OverlappingActor);
		const UPrimitiveComponent* PawnRoot = IsValid(Pawn) ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
		if (IsValid(PawnRoot) && Pawn->IsLocallyControlled() && ZoneBounds->IsOverlappingComponent(PawnRoot))
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR
EDataValidationResult ANUBGMZone::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (BGMId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("NUBGMZone: BGMId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
