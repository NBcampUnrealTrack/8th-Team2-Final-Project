#include "Field/NUCompanionLevels.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUCompanion, Log, All);

static TAutoConsoleVariable<int32> CVarNUCompanions(
	TEXT("nu.World.Companions"), 1,
	TEXT("Sub-levels of a map that is streamed in as a level instance: 1 = load those under /Game/NUA/Levels/ with it, 2 = load all of them, 0 = off"));
static TAutoConsoleVariable<int32> CVarNUInstanceLook(
	TEXT("nu.World.InstanceLook"), 1,
	TEXT("1 = while the player stands in a level instance that has its own sun, the sun / sky / fog / unbound post volumes of the rest of the world are off"));
static TAutoConsoleVariable<float> CVarNUInstanceRadius(
	TEXT("nu.World.InstanceRadius"), 80000.0f,
	TEXT("Distance (cm, on the ground plane) from a level instance's origin within which the player counts as standing in it"));

namespace
{
	const TCHAR* const NUACompanionRoot = TEXT("/Game/NUA/Levels/");
	// a sun actor with this tag marks a map that must keep its own look when streamed in although it has no companion levels
	const FName NUAOwnLookTag(TEXT("NUA_OwnLook"));
}

bool UNUCompanionLevels::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UNUCompanionLevels::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNUCompanionLevels, STATGROUP_Tickables);
}

void UNUCompanionLevels::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		SetWorldLook(World, true);
	}
	Spaces.Reset();
	Super::Deinitialize();
}

void UNUCompanionLevels::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	Forget();
	Discover(World);

	const FSpace* Occupied = CVarNUInstanceLook.GetValueOnGameThread() != 0 ? FindOccupied(World) : nullptr;
	const bool bWantOff = Occupied != nullptr;
	const ULevelStreaming* NewBase = Occupied != nullptr ? Occupied->Base.Get() : nullptr;
	if (bWantOff != bWorldLookOff || (bWantOff && NewBase != OccupiedBase.Get()))
	{
		// moving straight from one instance into another: give the world its look back first, then take it for the new one
		SetWorldLook(World, true);
		OccupiedBase = const_cast<ULevelStreaming*>(NewBase);
		if (bWantOff)
		{
			SetWorldLook(World, false);
		}
	}
}

// Level instances that left: their companions go with them.
void UNUCompanionLevels::Forget()
{
	for (int32 Index = Spaces.Num() - 1; Index >= 0; --Index)
	{
		const ULevelStreaming* Base = Spaces[Index].Base.Get();
		if (IsValid(Base) && !Base->GetIsRequestingUnloadAndRemoval() && Base->ShouldBeLoaded())
		{
			continue;
		}
		for (const TWeakObjectPtr<ULevelStreamingDynamic>& Companion : Spaces[Index].Companions)
		{
			if (ULevelStreamingDynamic* Level = Companion.Get())
			{
				Level->SetIsRequestingUnloadAndRemoval(true);
			}
		}
		UE_LOG(LogNUCompanion, Log, TEXT("instance gone, %d companion levels released"), Spaces[Index].Companions.Num());
		Spaces.RemoveAt(Index);
	}
}

// New level instances: once the map itself is loaded its sub-level list can be read.
void UNUCompanionLevels::Discover(UWorld* World)
{
	const int32 Mode = CVarNUCompanions.GetValueOnGameThread();
	// LoadLevelInstance adds to the world's list: walk a copy
	const TArray<ULevelStreaming*> Streaming = World->GetStreamingLevels();
	for (ULevelStreaming* Base : Streaming)
	{
		if (!IsValid(Base) || !Base->IsA<ULevelStreamingDynamic>() || Base->GetIsRequestingUnloadAndRemoval())
		{
			continue;
		}
		const bool bKnown = Spaces.ContainsByPredicate([Base](const FSpace& Space)
		{
			return Space.Base.Get() == Base || Space.Companions.Contains(Base);
		});
		ULevel* Loaded = Base->GetLoadedLevel();
		if (bKnown || !IsValid(Loaded))
		{
			continue;
		}

		FSpace Space;
		Space.Base = Base;
		bool bHasSun = false;
		bool bTagged = false;
		for (const AActor* Actor : Loaded->Actors)
		{
			if (IsValid(Actor) && Actor->FindComponentByClass<UDirectionalLightComponent>() != nullptr)
			{
				bHasSun = true;
				bTagged |= Actor->ActorHasTag(NUAOwnLookTag);
			}
		}

		const UWorld* Source = Loaded->GetTypedOuter<UWorld>();
		int32 Listed = 0;
		if (Mode > 0 && Source != nullptr && Source != World)
		{
			for (const ULevelStreaming* Sub : Source->GetStreamingLevels())
			{
				if (!IsValid(Sub))
				{
					continue;
				}
				++Listed;
				const FString Package = UWorld::RemovePIEPrefix(Sub->GetWorldAssetPackageName());
				if (Package.IsEmpty() || (Mode == 1 && !Package.StartsWith(NUACompanionRoot)))
				{
					continue;
				}
				const FSoftObjectPath Path(Package + TEXT(".") + FPackageName::GetShortName(Package));
				bool bAccepted = false;
				ULevelStreamingDynamic* Companion = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
					World, TSoftObjectPtr<UWorld>(Path), Sub->LevelTransform * Base->LevelTransform, bAccepted);
				if (bAccepted && IsValid(Companion))
				{
					Space.Companions.Add(Companion);
				}
				else
				{
					UE_LOG(LogNUCompanion, Warning, TEXT("companion level not accepted: %s"), *Package);
				}
			}
		}
		// The look is taken over only for maps this pipeline dressed (they have companions, or their sun says so):
		// battle stages and interiors are level instances too and keep the lighting they were built with.
		Space.bOwnsLight = bHasSun && (Space.Companions.Num() > 0 || bTagged);
		UE_LOG(LogNUCompanion, Log, TEXT("instance %s: %d sub-levels listed, %d companions requested, sun %d, takes the look %d"),
			*UWorld::RemovePIEPrefix(Base->GetWorldAssetPackageName()), Listed, Space.Companions.Num(), bHasSun ? 1 : 0, Space.bOwnsLight ? 1 : 0);
		Spaces.Add(MoveTemp(Space));
	}
}

const UNUCompanionLevels::FSpace* UNUCompanionLevels::FindOccupied(UWorld* World) const
{
	const APlayerController* Controller = World->GetFirstPlayerController();
	const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return nullptr;
	}
	const FVector Where = Pawn->GetActorLocation();
	const float Radius = CVarNUInstanceRadius.GetValueOnGameThread();
	const FSpace* Best = nullptr;
	double BestDistance = 0.0;
	for (const FSpace& Space : Spaces)
	{
		const ULevelStreaming* Base = Space.Base.Get();
		if (!Space.bOwnsLight || !IsValid(Base) || !Base->IsLevelVisible())
		{
			continue;
		}
		const double Distance = FVector::Dist2D(Where, Base->LevelTransform.GetLocation());
		if (Distance <= Radius && (Best == nullptr || Distance < BestDistance))
		{
			Best = &Space;
			BestDistance = Distance;
		}
	}
	return Best;
}

bool UNUCompanionLevels::BelongsToSpace(const ULevel* Level) const
{
	for (const FSpace& Space : Spaces)
	{
		if (const ULevelStreaming* Base = Space.Base.Get())
		{
			if (Base->GetLoadedLevel() == Level)
			{
				return true;
			}
		}
		for (const TWeakObjectPtr<ULevelStreamingDynamic>& Companion : Space.Companions)
		{
			if (Companion.IsValid() && Companion->GetLoadedLevel() == Level)
			{
				return true;
			}
		}
	}
	return false;
}

// bOn = false: the rest of the world (every level that is not an instance or a companion) loses its sun, sky, fog and
// unbound post volumes, remembered here. bOn = true: exactly those come back.
void UNUCompanionLevels::SetWorldLook(UWorld* World, bool bOn)
{
	if (bOn)
	{
		if (!bWorldLookOff)
		{
			return;
		}
		for (const TWeakObjectPtr<USceneComponent>& Component : HiddenComponents)
		{
			if (USceneComponent* Scene = Component.Get())
			{
				Scene->SetVisibility(true);
			}
		}
		for (const TWeakObjectPtr<APostProcessVolume>& Volume : DisabledVolumes)
		{
			if (APostProcessVolume* Post = Volume.Get())
			{
				Post->bEnabled = true;
			}
		}
		UE_LOG(LogNUCompanion, Log, TEXT("world look back on (%d components, %d post volumes)"), HiddenComponents.Num(), DisabledVolumes.Num());
		HiddenComponents.Reset();
		DisabledVolumes.Reset();
		bWorldLookOff = false;
		return;
	}

	for (const ULevel* Level : World->GetLevels())
	{
		if (!IsValid(Level) || BelongsToSpace(Level))
		{
			continue;
		}
		for (AActor* Actor : Level->Actors)
		{
			if (!IsValid(Actor))
			{
				continue;
			}
			if (APostProcessVolume* Post = Cast<APostProcessVolume>(Actor))
			{
				if (Post->bEnabled && Post->bUnbound)
				{
					Post->bEnabled = false;
					DisabledVolumes.Add(Post);
				}
				continue;
			}
			TInlineComponentArray<USceneComponent*> Components(Actor);
			for (USceneComponent* Component : Components)
			{
				const bool bLook = Component->IsA<UDirectionalLightComponent>() || Component->IsA<USkyLightComponent>()
					|| Component->IsA<UExponentialHeightFogComponent>() || Component->IsA<USkyAtmosphereComponent>()
					|| Component->IsA<UVolumetricCloudComponent>();
				if (bLook && Component->IsVisible())
				{
					Component->SetVisibility(false);
					HiddenComponents.Add(Component);
				}
			}
		}
	}
	bWorldLookOff = true;
	UE_LOG(LogNUCompanion, Log, TEXT("world look off while in an instance with its own sun (%d components, %d post volumes)"), HiddenComponents.Num(), DisabledVolumes.Num());
}
