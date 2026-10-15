// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleProjectile.h"

#include "Audio/NUSFXSubsystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ANUBattleProjectile::ANUBattleProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	TrailComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailComponent"));
	RootComponent = TrailComponent;
}

void ANUBattleProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);

	SetActorLocation(FMath::Lerp(Origin, Destination, Alpha));

	if (Alpha < 1.f)
	{
		return;
	}

	// 도착 시 Impact 띄우고 제거
	UNUSFXSubsystem::Play(this, ImpactSFX);

	if (UNiagaraSystem* ImpactSystem = ImpactVFX.LoadSynchronous())
	{
		UNiagaraComponent* ImpactComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, 
			ImpactSystem, 
			Destination, 
			FRotator::ZeroRotator,
			FVector(1.f), 
			true,
			true
			);

		if (IsValid(ImpactComponent))
		{
			ImpactComponent->SetColorParameter(TEXT("EffectColor"), EffectColor);
		}
	}

	Destroy();
}

void ANUBattleProjectile::Launch(UNiagaraSystem* TrailSystem, TSoftObjectPtr<UNiagaraSystem> InImpactVFX,
	const FVector& StartLocation, const FVector& EndLocation, float FlightTime, const FLinearColor& Color,
	const FGameplayTag& InImpactSFX)
{
	Origin = StartLocation;
	Destination = EndLocation;
	Origin.Z += 50.f;
	Destination.Z += 50.f;
	
	Duration = FMath::Max(FlightTime, KINDA_SMALL_NUMBER);
	Elapsed = 0.f;
	EffectColor = Color;
	ImpactVFX = InImpactVFX;
	ImpactSFX = InImpactSFX;

	SetActorLocation(Origin);

	if (!IsValid(TrailComponent))
	{
		return;
	}

	TrailComponent->SetAsset(TrailSystem);
	TrailComponent->SetColorParameter(TEXT("EffectColor"), EffectColor);
	TrailComponent->Activate(true);
}
