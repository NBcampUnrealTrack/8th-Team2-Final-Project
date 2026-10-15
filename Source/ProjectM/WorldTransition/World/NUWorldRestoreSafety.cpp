#include "WorldTransition/World/NUWorldRestoreSafety.h"

#include "WorldTransition/World/NUWorldEntryPoint.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"
#include "Field/Interior/NUWalkableDoor.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	bool GetShape(ACharacter* Character, UNUWalkableInteriorComponent* Interior, FCollisionShape& OutShape)
	{
		if (!IsValid(Character) || !Character->GetCapsuleComponent() || !Character->GetCharacterMovement()) { return false; }
		const FVector Previous = Character->GetActorScale3D();
		const UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
		const FVector Scale = Participant ? Participant->GetTransitionScale(Interior) : Previous;
		
		if (Previous.ContainsNaN() || Previous.GetMin() <= UE_SMALL_NUMBER || Scale.ContainsNaN() || Scale.GetMin() <= UE_SMALL_NUMBER) { return false; }
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const float ShapeScale = (Capsule->GetComponentScale() * Scale / Previous).GetAbsMin();
		OutShape = FCollisionShape::MakeCapsule(Capsule->GetUnscaledCapsuleRadius() * ShapeScale, Capsule->GetUnscaledCapsuleHalfHeight() * ShapeScale);
		
		return true;
	}
}

bool NUWorldRestoreSafety::IsSafe(ACharacter* Character, UNUWalkableInteriorComponent* Interior, const FVector& Location)
{
	FCollisionShape Shape;
	if (Location.ContainsNaN() || !GetShape(Character, Interior, Shape)) { return false; }
	
	UWorld* World = Character->GetWorld();
	if (!World) { return false; }
	
	// 바닥이 있어도 다른 장소의 좌표에 실내 표시 상태를 적용해서는 안 된다.
	if (Interior)
	{
		const UBoxComponent* Bounds = Interior->GetInteriorBounds();
		if (Interior->GetWorld() != World || !IsValid(Bounds)) { return false; }
		const FVector Extent = Bounds->GetUnscaledBoxExtent();
		if (!FBox(-Extent, Extent).IsInsideOrOn(Bounds->GetComponentTransform().InverseTransformPosition(Location))) { return false; }
	}
	
	const AWorldSettings* Settings = World->GetWorldSettings();
	if (Settings && Settings->bEnableWorldBoundsChecks && (Location.Z <= Settings->KillZ
		|| Location.GetAbsMax() + Shape.GetCapsuleHalfHeight() >= HALF_WORLD_MAX)) { return false; }
	
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RestoreSafety), false, Character);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	if (World->OverlapBlockingTestByChannel(Location, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses)) { return false; }
	
	FHitResult Floor;
	const double Distance = Character->GetCharacterMovement()->MaxStepHeight + UCharacterMovementComponent::MAX_FLOOR_DIST;
	return World->SweepSingleByChannel(Floor, Location, Location - FVector(0, 0, Distance), Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses)
		&& !Floor.bStartPenetrating && Character->GetCharacterMovement()->IsWalkable(Floor);
}

bool NUWorldRestoreSafety::FindInteriorArrival(ACharacter* Character, UNUWalkableInteriorComponent* Interior, FVector& OutLocation)
{
	if (!IsValid(Character) || !IsValid(Interior) || Character->GetWorld() != Interior->GetWorld()) { return false; }
	const UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
	const FVector Scale = Participant ? Participant->GetTransitionScale(Interior) : Character->GetActorScale3D();
	
	// 실제 문 진입과 같은 도착점을 사용한다. 다른 방이나 실외 진입점으로 대체하지 않는다.
	for (TActorIterator<ANUWalkableDoor> It(Character->GetWorld()); It; ++It)
	{
		FVector Candidate;
		if (It->GetInterior() == Interior && It->FindArrivalLocation(Character, true, Scale, Candidate)
			&& IsSafe(Character, Interior, Candidate))
		{
			OutLocation = Candidate;
			return true;
		}
	}
	return false;
}

bool NUWorldRestoreSafety::FindEntryArrival(ACharacter* Character, UNUWalkableInteriorComponent* Interior, const ANUWorldEntryPoint* Entry, FVector& OutLocation)
{
	FCollisionShape Shape;
	if (!IsValid(Entry) || !GetShape(Character, Interior, Shape)) { return false; }
	const FVector Center = Entry->GetActorLocation() + (Entry->UsesFeetLocation() ? FVector(0, 0, Shape.GetCapsuleHalfHeight()) : FVector::ZeroVector);
	return FindFloorArrival(Character, Interior, Center, OutLocation);
}

bool NUWorldRestoreSafety::FindFloorArrival(ACharacter* Character, UNUWalkableInteriorComponent* Interior, const FVector& Center, FVector& OutLocation)
{
	FCollisionShape Shape;
	if (Center.ContainsNaN() || !GetShape(Character, Interior, Shape)) { return false; }
	
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const double Step = Character->GetCharacterMovement()->MaxStepHeight;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RestoreEntry), false, Character);
	FHitResult Floor;
	if (!Character->GetWorld()->SweepSingleByChannel(Floor, Center + FVector(0, 0, Step), Center - FVector(0, 0, Step), Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))
		|| Floor.bStartPenetrating || !Character->GetCharacterMovement()->IsWalkable(Floor)) { return false; }
	OutLocation = Floor.Location + FVector(0, 0, UCharacterMovementComponent::MIN_FLOOR_DIST);
	
	return IsSafe(Character, Interior, OutLocation);
}
