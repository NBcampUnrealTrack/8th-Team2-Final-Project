#include "Field/Interior/NUWalkableDoor.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"
#include "Interaction/Component/NUInteractorComponent.h"
#include "Interaction/NUInteractionSpatialRules.h"
#include "Net/EmberServerManager.h"
#include "UI/NUUIManager.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUWalkableDoor, Log, All);

ANUWalkableDoor::ANUWalkableDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	bGenerateOverlapEventsDuringLevelStreaming = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot")));
	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(RootComponent);
	InteractionVolume->SetBoxExtent(FVector(90.0f, 100.0f, 140.0f));
	InteractionVolume->SetCollisionProfileName(TEXT("NU_Interactable"));
	InteractionVolume->SetGenerateOverlapEvents(true);
	DoorBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBlocker"));
	DoorBlocker->SetupAttachment(RootComponent);
	DoorBlocker->SetBoxExtent(FVector(50.0f, 15.0f, 110.0f));
	DoorBlocker->SetCollisionProfileName(TEXT("BlockAll"));
	DoorBlocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	DoorBlocker->SetGenerateOverlapEvents(false);
	DoorPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorPanel"));
	DoorPanel->SetupAttachment(RootComponent);
	DoorPanel->SetCollisionProfileName(TEXT("NoCollision"));
	Blackout = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blackout"));
	Blackout->SetupAttachment(RootComponent);
	Blackout->SetCollisionProfileName(TEXT("NoCollision"));
	Blackout->SetCastShadow(false);
}

void ANUWalkableDoor::BeginPlay()
{
	Super::BeginPlay();
	// BP 태그로 출입 범위와 발 기준 도착점을 찾아 문 인스턴스에 연결한다.
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		if (Component->ComponentHasTag(EntryTriggerTag)) { EntryTransitionTrigger = Cast<UBoxComponent>(Component); }
		if (Component->ComponentHasTag(ExitTriggerTag)) { ExitTransitionTrigger = Cast<UBoxComponent>(Component); }
		if (Component->ComponentHasTag(InteriorArrivalTag)) { InteriorArrivalPoint = Component; }
		if (Component->ComponentHasTag(ExteriorArrivalTag)) { ExteriorArrivalPoint = Component; }
	}
	Interior = IsValid(InteriorOwner) ? InteriorOwner->FindComponentByClass<UNUWalkableInteriorComponent>() : nullptr;
	if (Interior.IsValid())
	{
		Interior->OnPresentationChanged.AddDynamic(this, &ThisClass::HandlePresentationChanged);
	}
	HandlePresentationChanged(Interior.IsValid() && Interior->IsLocalPlayerInside());
	OnDoorOpenChanged(bDoorOpen);
	InteractionVolume->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionExit);
	InteractionVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionEntry);
	TArray<AActor*> Characters;
	InteractionVolume->GetOverlappingActors(Characters, ACharacter::StaticClass());
	for (AActor* Character : Characters)
	{
		if (bOpenOnOverlap) { OpenForCharacter(Cast<ACharacter>(Character), true); }
	}
}

void ANUWalkableDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InteractionVolume->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionExit);
	InteractionVolume->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionEntry);
	if (Interior.IsValid())
	{
		Interior->OnPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged);
	}
	Super::EndPlay(EndPlayReason);
}

bool ANUWalkableDoor::CanInteract_Implementation(AActor* Interactor) const
{
	const ACharacter* Character = Cast<ACharacter>(Interactor);
	const UNUInteriorParticipantComponent* Participant = IsValid(Character) ? Character->FindComponentByClass<UNUInteriorParticipantComponent>() : nullptr;
	return !bOpenOnOverlap && !bDoorOpen && IsValid(Character) && RejectedCharacter.Get() != Character
		&& Character->IsLocallyControlled() && InteractionVolume->IsOverlappingComponent(Character->GetCapsuleComponent())
		&& Participant != nullptr && Participant->CanUseDoor(this)
		&& NUInteractionSpatialRules::CanInteract(Interactor, const_cast<ANUWalkableDoor*>(this));
}

bool ANUWalkableDoor::Interact_Implementation(AActor* Interactor)
{
	return CanInteract_Implementation(Interactor) && OpenForCharacter(Cast<ACharacter>(Interactor));
}

void ANUWalkableDoor::HandleInteractionEntry(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (bOpenOnOverlap && IsValid(Character) && OtherComponent == Character->GetCapsuleComponent())
	{
		OpenForCharacter(Character);
	}
}

bool ANUWalkableDoor::OpenForCharacter(ACharacter* Character, bool bSilent)
{
	if (!IsValid(Character) || RejectedCharacter.Get() == Character || !Character->IsLocallyControlled() || !InteractionVolume->IsOverlappingComponent(Character->GetCapsuleComponent()) || !NUInteractionSpatialRules::CanInteract(Character, this))
	{
		return false;
	}

	UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	UEmberServerManager* Server = GameInstance != nullptr ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
	if (Server != nullptr && !Server->IsLoggedIn() && !Server->GetUserId().IsEmpty())
	{
		// 온라인 세션이 끊긴 상태에서는 문 연출과 통로 충돌 해제를 시작하지 않는다.
		RejectedCharacter = Character;
		if (UNUUIManager* UI = UNUUIManager::Get(Cast<APlayerController>(Character->GetController())))
		{
			UI->ShowNotice(NSLOCTEXT("NUInteriorTransition", "EntryFailed", "입장에 실패했습니다. 잠시 후 다시 시도해 주세요."));
		}
		UE_LOG(LogNUWalkableDoor, Warning, TEXT("서버 연결이 준비되지 않아 문 열기를 차단했습니다. Door=%s"), *GetName());
		RefreshInteractors();
		return false;
	}

	const UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>();
	if (Participant == nullptr || !Participant->CanUseDoor(this)) { return false; }

	if (!bDoorOpen)
	{
		bDoorOpen = true;
		DoorBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// BeginPlay 의 시작 위치 겹침으로 열릴 때는 소리를 내지 않는다
		if (!bSilent)
		{
			UNUSFXSubsystem::Play(this, NU::SFX::Field::DoorOpen);
		}
		OnDoorOpenChanged(true);
		RefreshInteractors();
	}
	return true;
}

UBoxComponent* ANUWalkableDoor::GetInteriorBounds() const
{
	return Interior.IsValid() ? Interior->GetInteriorBounds() : nullptr;
}

void ANUWalkableDoor::RefreshAutomaticOpen(ACharacter* Character)
{
	if (bOpenOnOverlap && !bDoorOpen) { OpenForCharacter(Character); }
}

float ANUWalkableDoor::GetInteriorContactDepth() const
{
	const UBoxComponent* Bounds = GetInteriorBounds();
	if (!Bounds) { return 0.0f; }
	if (ExitTransitionTrigger.IsValid())
	{
		const FVector Axis = DoorBlocker->GetRightVector();
		const double Direction = FVector::DotProduct(Bounds->GetComponentLocation() - DoorBlocker->GetComponentLocation(), Axis) >= 0.0 ? 1.0 : -1.0;
		return FVector::DotProduct(ExitTransitionTrigger->GetComponentLocation() - DoorBlocker->GetComponentLocation(), Axis) * Direction + ExitTransitionTrigger->GetScaledBoxExtent().Y;
	}
	const FTransform Transform = DoorBlocker->GetComponentTransform();
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	const FBox Room = FBox(-Extent, Extent).TransformBy(Bounds->GetComponentTransform().ToMatrixWithScale() * Transform.ToInverseMatrixWithScale());
	const bool bPositive = Transform.InverseTransformPosition(Bounds->GetComponentLocation()).Y >= 0.0;
	const float Edge = (bPositive ? Room.Min.Y : -Room.Max.Y) * Transform.GetScale3D().GetAbs().Y;
	return FMath::Max(Edge, DoorBlocker->GetScaledBoxExtent().Y) + FMath::Max(0.0f, ContactPadding);
}

bool ANUWalkableDoor::IsTransitionContact(const ACharacter* Character, bool bInside) const
{
	const UBoxComponent* Bounds = GetInteriorBounds();
	if (!IsValid(Character) || !Bounds || !InteractionVolume->IsOverlappingComponent(Character->GetCapsuleComponent())) { return false; }
	const UBoxComponent* Trigger = bInside ? ExitTransitionTrigger.Get() : EntryTransitionTrigger.Get();
	if (Trigger)
	{
		const FTransform TriggerTransform = Trigger->GetComponentTransform();
		const FVector TriggerScale = TriggerTransform.GetScale3D().GetAbs();
		if (TriggerScale.GetMin() <= UE_SMALL_NUMBER) { return false; }
		const FVector Position = TriggerTransform.InverseTransformPosition(Character->GetActorLocation()) * TriggerScale;
		const FVector TriggerExtent = Trigger->GetScaledBoxExtent();
		const float CapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (!bInside)
		{
			const double Direction = TriggerTransform.InverseTransformPosition(Bounds->GetComponentLocation()).Y >= 0.0 ? 1.0 : -1.0;
			// 진입은 캡슐 앞면이 아니라 중심이 트리거의 실내쪽 면을 넘어야 한다. 얇은 면을 한 프레임에 지나도 인정한다.
			return FMath::Abs(Position.X) <= TriggerExtent.X && Position.Y * Direction >= TriggerExtent.Y && FMath::Abs(Position.Z) <= TriggerExtent.Z;
		}
		// 캡슐 옆면이 스친 것만으로 전환하지 않는다. 중심이 BP의 출입구 폭 안에 있어야 한다.
		return FMath::Abs(Position.X) <= TriggerExtent.X && FMath::Abs(Position.Y) <= TriggerExtent.Y + CapsuleRadius && FMath::Abs(Position.Z) <= TriggerExtent.Z;
	}
	// 아직 전환 Box를 배치하지 않은 기존 문은 기존 문턱 판정을 사용한다.
	const FTransform Transform = DoorBlocker->GetComponentTransform();
	const FVector Scale = Transform.GetScale3D().GetAbs();
	if (Scale.GetMin() <= UE_SMALL_NUMBER) { return false; }
	const FVector Local = Transform.InverseTransformPosition(Character->GetActorLocation());
	const FVector Extent = DoorBlocker->GetUnscaledBoxExtent();
	const double Direction = Transform.InverseTransformPosition(Bounds->GetComponentLocation()).Y >= 0.0 ? 1.0 : -1.0;
	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float Width = FMath::Max(0.0f, Extent.X * Scale.X - (bInside ? Radius : 0.0f));
	if (FMath::Abs(Local.X) * Scale.X > Width || FMath::Abs(Local.Z) > Extent.Z) { return false; }
	const double Depth = Local.Y * Scale.Y * Direction;
	// 실외의 원래 크기는 문틀보다 클 수 있다. 캡슐 앞면이 문턱에 닿으면 축소 진입한다.
	const float ExteriorDepth = DoorBlocker->GetScaledBoxExtent().Y + (bInside ? 0.0f : Radius) + FMath::Max(0.0f, ContactPadding);
	return Depth >= -ExteriorDepth && Depth <= (bInside ? GetInteriorContactDepth() : DoorBlocker->GetScaledBoxExtent().Y);
}

bool ANUWalkableDoor::IsTransitionApproach(const ACharacter* Character, const FVector& PreviousLocation, bool bInside) const
{
	if (!IsTransitionContact(Character, bInside)) { return false; }
	const FVector Axis = DoorBlocker->GetRightVector().GetSafeNormal2D();
	const FVector Delta = Character->GetActorLocation() - PreviousLocation;
	const double Direction = FVector::DotProduct(GetInteriorBounds()->GetComponentLocation() - DoorBlocker->GetComponentLocation(), Axis) >= 0.0 ? 1.0 : -1.0;
	const double Forward = FVector::DotProduct(Delta, Axis) * Direction * (bInside ? -1.0 : 1.0);
	// 중심이 출입구 폭 안에 있고 문을 향해 이동해야 한다. 벽을 따라 비스듬히 들어오는 이동도 허용한다.
	return Forward > UE_KINDA_SMALL_NUMBER;
}

bool ANUWalkableDoor::FindArrivalLocation(const ACharacter* Character, bool bInside, const FVector& DestinationScale, FVector& OutLocation) const
{
	const double SearchStart = FPlatformTime::Seconds();
	int32 CandidateCount = 0;
	ON_SCOPE_EXIT
	{
		UE_LOG(LogNUWalkableDoor, Verbose, TEXT("Arrival search: %.3f ms, %d candidates, inside=%d"), (FPlatformTime::Seconds() - SearchStart) * 1000.0, CandidateCount, bInside);
	};
	const UBoxComponent* Bounds = GetInteriorBounds();
	if (!IsValid(Character) || !Bounds) { return false; }
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector CurrentScale = Character->GetActorScale3D();
	if (CurrentScale.GetMin() <= UE_SMALL_NUMBER || DestinationScale.ContainsNaN() || DestinationScale.GetMin() <= UE_SMALL_NUMBER) { return false; }
	// 퇴장 시 복원될 크기까지 반영하여 도착 공간을 검사한다.
	const float CapsuleScale = (Capsule->GetComponentScale() * DestinationScale / CurrentScale).GetAbsMin();
	const float Radius = Capsule->GetUnscaledCapsuleRadius() * CapsuleScale;
	const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight() * CapsuleScale;
	const USceneComponent* ArrivalPoint = bInside ? InteriorArrivalPoint.Get() : ExteriorArrivalPoint.Get();
	if (ArrivalPoint)
	{
		// BP에 배치한 발 위치를 사용한다. XY를 탐색하거나 다른 위치로 밀어내지 않는다.
		++CandidateCount;
		const FVector Feet = ArrivalPoint->GetComponentLocation();
		const FVector Center = Feet + FVector(0.0f, 0.0f, HalfHeight);
		const FVector FloorRange(0.0f, 0.0f, Movement->MaxStepHeight);
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorArrivalPoint), false, Character);
		const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
		FHitResult Floor;
		if (!GetWorld()->SweepSingleByChannel(Floor, Center + FloorRange, Center - FloorRange, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses) || Floor.bStartPenetrating || !Movement->IsWalkable(Floor)) { return false; }
		FVector Destination(Feet.X, Feet.Y, Floor.Location.Z + UCharacterMovementComponent::MIN_FLOOR_DIST);
		const FVector BoundsExtent = Bounds->GetUnscaledBoxExtent();
		if (bInside != FBox(-BoundsExtent, BoundsExtent).IsInsideOrOn(Bounds->GetComponentTransform().InverseTransformPosition(Destination))) { return false; }
		if (GetWorld()->OverlapBlockingTestByChannel(Destination, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses)) { return false; }
		OutLocation = Destination;
		return true;
	}
	// 도착점이 없는 기존 문은 문 앞의 제한된 후보 위치를 검사한다.
	const FVector Origin = DoorBlocker->GetComponentLocation();
	const FVector DoorForward = DoorBlocker->GetRightVector().GetSafeNormal2D();
	const FVector Inward = FVector::DotProduct(Bounds->GetComponentLocation() - Origin, DoorForward) >= 0.0f ? DoorForward : -DoorForward;
	if (Inward.IsNearlyZero()) { return false; }
	const FVector Right(-Inward.Y, Inward.X, 0.0f);
	const FTransform BoundsTransform = Bounds->GetComponentTransform();
	const FVector BoundsScale = BoundsTransform.GetScale3D().GetAbs();
	if (BoundsScale.GetMin() <= UE_SMALL_NUMBER) { return false; }
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	const FBox Room(-Extent, Extent);
	const FVector Padding(Radius / BoundsScale.X, Radius / BoundsScale.Y, 0.0f);
	const float Clearance = FMath::Max(1.0f, ArrivalClearance);
	// 반대 방향의 접촉 판정 바로 밖에서 시작한다. 도착 직후 가만히 있어도 다시 전환되지 않는다.
	const float ExitContactDepth = GetInteriorContactDepth() + (ExitTransitionTrigger.IsValid() ? Radius : 0.0f);
	const float EntryContactDepth = EntryTransitionTrigger.IsValid() ? -FVector::DotProduct(EntryTransitionTrigger->GetComponentLocation() - Origin, Inward) + EntryTransitionTrigger->GetScaledBoxExtent().Y + Radius : DoorBlocker->GetScaledBoxExtent().Y + Radius + FMath::Max(0.0f, ContactPadding);
	const float FirstDistance = bInside ? FMath::Max(ExitContactDepth, DoorBlocker->GetScaledBoxExtent().Y + Radius) + Clearance : FMath::Max(EntryContactDepth, DoorBlocker->GetScaledBoxExtent().Y + Radius) + Clearance;
	const float MaxLateral = FMath::Max(0.0f, DoorBlocker->GetScaledBoxExtent().X - Radius - Clearance);
	const float PreferredLateral = FMath::Clamp(static_cast<float>(FVector::DotProduct(Character->GetActorLocation() - Origin, Right)), -MaxLateral, MaxLateral);
	const float Lanes[] = { PreferredLateral, -MaxLateral, MaxLateral };
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorArrival), false, Character);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const FCollisionShape ArrivalShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	for (int32 Depth = 0; Depth < FMath::Clamp(ArrivalSearchSteps, 1, 16); ++Depth)
	{
		const float Distance = FirstDistance + Depth * FMath::Max(1.0f, ArrivalSearchStep);
		for (int32 Lane = 0; Lane < UE_ARRAY_COUNT(Lanes); ++Lane)
		{
			if ((Lane > 0 && FMath::IsNearlyEqual(Lanes[Lane], Lanes[0])) || (Lane > 1 && FMath::IsNearlyEqual(Lanes[Lane], Lanes[1]))) { continue; }
			++CandidateCount;
			FVector Point = Origin + Inward * (bInside ? Distance : -Distance) + Right * Lanes[Lane];
			const FVector Local = BoundsTransform.InverseTransformPosition(Point);
			if (bInside && (FMath::Abs(Local.X) + Padding.X >= Extent.X || FMath::Abs(Local.Y) + Padding.Y >= Extent.Y)) { continue; }
			const float TraceTop = Bounds->GetComponentLocation().Z + HalfHeight;
			FHitResult Floor;
			// 선의 접점에 반높이를 더하면 경사면에서 확대된 캡슐이 지면을 관통할 수 있다.
			// 도착 크기의 캡슐을 직접 내려 바닥에 닿는 중심 높이를 찾는다.
			if (!GetWorld()->SweepSingleByChannel(Floor, FVector(Point.X, Point.Y, TraceTop), FVector(Point.X, Point.Y, TraceTop - Bounds->GetScaledBoxExtent().Z * 2.0f - HalfHeight - Movement->MaxStepHeight), Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), ArrivalShape, Query, Responses) || Floor.bStartPenetrating || !Movement->IsWalkable(Floor)) { continue; }
			Point.Z = Floor.Location.Z + UCharacterMovementComponent::MIN_FLOOR_DIST;
			if (bInside != Room.IsInsideOrOn(BoundsTransform.InverseTransformPosition(Point))) { continue; }
			if (GetWorld()->OverlapBlockingTestByChannel(Point, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), ArrivalShape, Query, Responses)) { continue; }
			OutLocation = Point;
			return true;
		}
	}
	return false;
}

bool ANUWalkableDoor::IsDoorOpen() const
{
	return bDoorOpen;
}

bool ANUWalkableDoor::CloseDoor()
{
	if (!bDoorOpen || (bOpenOnOverlap && Interior.IsValid() && Interior->IsLocalPlayerInside()))
	{
		return false;
	}
	TArray<AActor*> Characters;
	InteractionVolume->GetOverlappingActors(Characters, ACharacter::StaticClass());
	for (AActor* Actor : Characters)
	{
		const ACharacter* Character = CastChecked<ACharacter>(Actor);
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		if (bOpenOnOverlap && Character->IsLocallyControlled() && InteractionVolume->IsOverlappingComponent(Capsule)) { return false; }
		// 회전된 문의 월드 AABB는 실제 통로보다 넓다. 가까운 도착점에서 닫힘을 잘못 막지 않는다.
		if (Capsule->OverlapComponent(DoorBlocker->GetComponentLocation(), DoorBlocker->GetComponentQuat(), FCollisionShape::MakeBox(DoorBlocker->GetScaledBoxExtent()))) { return false; }
	}
	// 캐릭터가 문 충돌체와 겹치지 않을 때만 통로 충돌을 복구한다.
	bDoorOpen = false;
	DoorBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UNUSFXSubsystem::Play(this, NU::SFX::Field::DoorClose);
	OnDoorOpenChanged(false);
	return true;
}

bool ANUWalkableDoor::RejectEntry(ACharacter* Character)
{
	if (!IsValid(Character))
	{
		return false;
	}

	RejectedCharacter = Character;
	if (!bDoorOpen)
	{
		return true;
	}

	// 실패 지점이 문 충돌체와 겹치면 강제로 닫지 않는다. 호출부가 직전 안전 위치로 복구한 뒤 호출한다.
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (Capsule != nullptr && Capsule->OverlapComponent(DoorBlocker->GetComponentLocation(), DoorBlocker->GetComponentQuat(), FCollisionShape::MakeBox(DoorBlocker->GetScaledBoxExtent())))
	{
		return false;
	}

	bDoorOpen = false;
	DoorBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UNUSFXSubsystem::Play(this, NU::SFX::Field::DoorClose);
	OnDoorOpenChanged(false);
	RefreshInteractors();
	return true;
}

void ANUWalkableDoor::HandleInteractionExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	const ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (RejectedCharacter.Get() == Character && Character != nullptr && OtherComponent == Character->GetCapsuleComponent())
	{
		RejectedCharacter.Reset();
	}

	if (bCloseAfterLeaving && IsValid(Character))
	{
		CloseDoor();
	}
}

void ANUWalkableDoor::HandlePresentationChanged(bool bInside)
{
	OnInteriorViewChanged(bInside);
	if (bCloseAfterLeaving && (!bInside || !bOpenOnOverlap))
	{
		CloseDoor();
	}
}

void ANUWalkableDoor::RefreshInteractors()
{
	TArray<AActor*> Characters;
	InteractionVolume->GetOverlappingActors(Characters, ACharacter::StaticClass());
	for (AActor* Character : Characters)
	{
		if (UNUInteractorComponent* Interactor = Character->FindComponentByClass<UNUInteractorComponent>())
		{
			Interactor->UpdateCurrentInteractable();
		}
	}
}
