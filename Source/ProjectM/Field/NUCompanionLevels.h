// Companion levels and the look of an instanced map (NUA).
//
// The world transition streams a destination map in as a level instance (UNUWorldStreamingSubsystem::AcquireSpace).
// A level instance brings only that map's persistent level: the streaming sub-levels the map has when it is opened on
// its own are not loaded. The 2.5D dressing and the ambient / grade of a town live in such sub-levels
// (/Game/NUA/Levels/L_NUA_<Town>_Dressing, _Ambient, _Horizon), and the town's 3D vegetation is hidden in favour of
// them, so a town entered through a portal arrived bare.
//
// This subsystem does two things, for every level instance in a game world:
//  1. Companions. When the instance's level is loaded, the sub-levels listed in that map are streamed in at the same
//     transform, and unloaded with it. nu.World.Companions: 1 = only /Game/NUA/Levels/ (default), 2 = every sub-level, 0 = off.
//  2. Look. While the local player stands in such an instance (one with companions and a sun of its own, or whose
//     sun actor has the tag NUA_OwnLook), the sun, sky light, height fog, sky atmosphere, clouds and unbound
//     post-process volumes of the rest of the world (the persistent level and its sub-levels) are switched off, so
//     the map looks as it does when opened on its own; they come back when the player leaves or the instance
//     unloads. Other level instances (battle stages, interiors) are left alone.
//     nu.World.InstanceLook: 1 = on (default), 0 = off.
//     nu.World.InstanceRadius: how near the instance's origin counts as standing in it (cm, default 80000).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NUCompanionLevels.generated.h"

class APostProcessVolume;
class ULevel;
class ULevelStreaming;
class ULevelStreamingDynamic;
class USceneComponent;

UCLASS()
class UNUCompanionLevels : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual void Deinitialize() override;

private:
	struct FSpace
	{
		TWeakObjectPtr<ULevelStreaming> Base;
		TArray<TWeakObjectPtr<ULevelStreamingDynamic>> Companions;
		bool bOwnsLight = false;
	};

	void Discover(UWorld* World);
	void Forget();
	const FSpace* FindOccupied(UWorld* World) const;
	bool BelongsToSpace(const ULevel* Level) const;
	void SetWorldLook(UWorld* World, bool bOn);

	TArray<FSpace> Spaces;
	TArray<TWeakObjectPtr<USceneComponent>> HiddenComponents;
	TArray<TWeakObjectPtr<APostProcessVolume>> DisabledVolumes;
	TWeakObjectPtr<ULevelStreaming> OccupiedBase;
	bool bWorldLookOff = false;
};
