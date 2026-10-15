#include "Field/Interior/NUInteriorParticipantComponent.h"

#include "Field/FieldPlayerCharacter.h"
#include "Field/Interior/NUWalkableDoor.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Net/EmberServerManager.h"
#include "UI/NUUIManager.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NULocationDefinition.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/ScopedMovementUpdate.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUInteriorTransition, Log, All);

UNUInteriorParticipantComponent::UNUInteriorParticipantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNUInteriorParticipantComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UEmberServerManager* Server = GameInstance->GetSubsystem<UEmberServerManager>())
		{
			Server->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleServerStateChanged);
		}
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		Capsule->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleDoorOverlapBegin);
		Capsule->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleDoorOverlapEnd);
		TArray<AActor*> Overlaps;
		Capsule->GetOverlappingActors(Overlaps, ANUWalkableDoor::StaticClass());
		for (AActor* Actor : Overlaps) { NearbyDoors.AddUnique(CastChecked<ANUWalkableDoor>(Actor)); }
	}
	// 실내와 참가자의 시작 순서에 관계없이 초기 위치와 문 출입을 연결한다.
	for (TObjectIterator<UNUWalkableInteriorComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->HasBegunPlay() && IsValid(It->GetInteriorBounds()))
		{
			It->RegisterParticipant(GetOwner());
		}
	}
	RefreshInterior();
}

void UNUInteriorParticipantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UEmberServerManager* Server = GameInstance->GetSubsystem<UEmberServerManager>())
		{
			Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleServerStateChanged);
		}
	}
	FinishInteriorTransition();
	if (ObservedRoot.IsValid()) { ObservedRoot->TransformUpdated.RemoveAll(this); }
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
		Character->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleControllerChanged);
		Character->GetCapsuleComponent()->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleDoorOverlapBegin);
		Character->GetCapsuleComponent()->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleDoorOverlapEnd);
	}
	if (CurrentInterior.IsValid()) { CurrentInterior->HandleParticipantExited(GetOwner()); }
	Super::EndPlay(EndPlayReason);
}

UNUWalkableInteriorComponent* UNUInteriorParticipantComponent::GetCurrentInterior() const
{
	return CurrentInterior.Get();
}

void UNUInteriorParticipantComponent::AddInterior(UNUWalkableInteriorComponent* Interior)
{
	if (!IsValid(Interior)) { return; }
	const bool bNewInterior = !Interiors.Contains(Interior);
	Interiors.AddUnique(Interior);
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ThisClass::HandleMovementUpdated);
		Character->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleControllerChanged);
	}
	if (!GetOwner()->IsA<ACharacter>() && !ObservedRoot.IsValid() && GetOwner()->GetRootComponent())
	{
		ObservedRoot = GetOwner()->GetRootComponent();
		ObservedRoot->TransformUpdated.AddUObject(this, &ThisClass::HandleTransformChanged);
	}
	// 새로 등록된 실내의 최초 배치 여부만 이번 갱신에서 확인한다.
	TGuardValue<const UNUWalkableInteriorComponent*> PlacementGuard(InitializingInterior, bNewInterior ? Interior : nullptr);
	RefreshInterior();
}

void UNUInteriorParticipantComponent::RemoveInterior(UNUWalkableInteriorComponent* Interior)
{
	Interiors.Remove(Interior);
	if (TransitionInterior.Get() == Interior) { FinishInteriorTransition(); }
	if (CurrentInterior.Get() == Interior) { LeaveInterior(Interior); }
}

void UNUInteriorParticipantComponent::RefreshInterior()
{
	if (!HasBegunPlay() || bEndingPlay || bTransitionPending || bApplyingTransition || !IsValid(GetOwner())) { return; }
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const bool bPlayer = GetOwner()->IsA<AFieldPlayerCharacter>() || (Character && Character->IsPlayerControlled());
	if (bPlayer)
	{
		// 최초 실내 배치만 범위로 복원한다. 이동 중 오버랩은 상태를 직접 변경하지 않는다.
		if (!CurrentInterior.IsValid() && InitializingInterior && InitializingInterior->ContainsParticipant(GetOwner(), false))
		{
			EnterInterior(const_cast<UNUWalkableInteriorComponent*>(InitializingInterior));
		}
		return;
	}
	if (CurrentInterior.IsValid() && CurrentInterior->ContainsParticipant(GetOwner(), true)) { return; }
	for (const auto& Interior : Interiors)
	{
		if (Interior.IsValid() && Interior->ContainsParticipant(GetOwner(), false)) { EnterInterior(Interior.Get()); return; }
	}
	LeaveInterior(CurrentInterior.Get());
}

bool UNUInteriorParticipantComponent::EnterInterior(UNUWalkableInteriorComponent* Interior)
{
	if (!HasBegunPlay() || bEndingPlay || !IsValid(Interior)) { return false; }
	if (CurrentInterior.Get() == Interior) { return true; }
	return ApplyInteriorState(Interior);
}

bool UNUInteriorParticipantComponent::LeaveInterior(UNUWalkableInteriorComponent* Interior)
{
	if (CurrentInterior.Get() != Interior) { return false; }
	if (!bHasExteriorScale) { return true; }
	return ApplyInteriorState(nullptr);
}

bool UNUInteriorParticipantComponent::ApplyRestoredState(UNUWalkableInteriorComponent* Interior, const FVector* Destination)
{
	if (!HasBegunPlay() || bEndingPlay || (Interior != nullptr && !IsValid(Interior)))
	{
		UE_LOG(LogNUInteriorTransition, Warning, TEXT("복원 상태 적용 불가 actor=%s begun_play=%d ending_play=%d interior=%s"),
			*GetNameSafe(GetOwner()), HasBegunPlay(), bEndingPlay, *GetNameSafe(Interior));
		return false;
	}

	return ApplyInteriorState(Interior, Destination);
}

FVector UNUInteriorParticipantComponent::GetDestinationScale(const UNUWalkableInteriorComponent* Interior) const
{
	const FVector BaseScale = bHasExteriorScale ? ExteriorScale : GetOwner()->GetActorScale3D();
	return BaseScale * (Interior ? FMath::Clamp(Interior->InteriorCharacterScale, 0.1f, 1.0f) : 1.0f);
}

bool UNUInteriorParticipantComponent::ApplyInteriorState(UNUWalkableInteriorComponent* Interior, const FVector* Destination)
{
	AActor* Owner = GetOwner();
	FVector Location = Destination ? *Destination : Owner->GetActorLocation();
	const FVector PreviousScale = Owner->GetActorScale3D();
	const FVector DestinationScale = GetDestinationScale(Interior);
	if (PreviousScale.GetMin() <= UE_SMALL_NUMBER || DestinationScale.ContainsNaN() || DestinationScale.GetMin() <= UE_SMALL_NUMBER)
	{
		UE_LOG(LogNUInteriorTransition, Warning, TEXT("실내외 상태 적용 실패: 잘못된 크기 actor=%s previous=%s destination=%s"),
			*GetNameSafe(Owner), *PreviousScale.ToString(), *DestinationScale.ToString());
		return false;
	}
	ACharacter* Character = Cast<ACharacter>(Owner);
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (Capsule)
	{
		const float ShapeScale = (Capsule->GetComponentScale() * DestinationScale / PreviousScale).GetAbsMin();
		// 수평 위치는 그대로 두고 캡슐 바닥 높이를 유지한다.
		if (!Destination) 
		{ 
			Location.Z += Capsule->GetUnscaledCapsuleHalfHeight() * ShapeScale - Capsule->GetScaledCapsuleHalfHeight(); 
		}
		
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetUnscaledCapsuleRadius() * ShapeScale, Capsule->GetUnscaledCapsuleHalfHeight() * ShapeScale);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(InteriorScale), false, Owner);
		if (GetWorld()->OverlapBlockingTestByChannel(Location, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())))
		{
			UE_LOG(LogNUInteriorTransition, Warning, TEXT("실내외 상태 적용 실패: 캡슐 충돌 actor=%s location=%s radius=%.2f half_height=%.2f saved_location=%d"),
				*GetNameSafe(Owner), *Location.ToString(), Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight(), Destination != nullptr);
			
			// 실패했을 때만 추가 조회해 복원을 막은 배치 액터를 남긴다.
			TArray<FOverlapResult> Overlaps;
			GetWorld()->OverlapMultiByChannel(Overlaps, Location, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (Overlap.bBlockingHit)
				{
					UE_LOG(LogNUInteriorTransition, Warning, TEXT("복원 차단 충돌 actor=%s component=%s"),
						*GetNameSafe(Overlap.GetActor()), *GetNameSafe(Overlap.GetComponent()));
				}
			}
			
			return false;
		}
	}
	// 크기와 위치 변경 중 발생하는 이벤트는 상태를 다시 적용하지 못하게 막는다.
	TGuardValue<bool> ApplyingGuard(bApplyingTransition, true);
	FScopedMovementUpdate MovementScope(Owner->GetRootComponent(), EScopedUpdate::DeferredUpdates);
	Owner->SetActorScale3D(DestinationScale);
	// 위치 복원 뒤 Pawn의 바닥·이동 모드도 갱신한다. 도착 충돌은 위에서 검사했다.
	if (!Owner->TeleportTo(Location, Owner->GetActorRotation(), false, true))
	{
		UE_LOG(LogNUInteriorTransition, Warning, TEXT("실내외 상태 적용 실패: TeleportTo actor=%s location=%s"),
			*GetNameSafe(Owner), *Location.ToString());
		Owner->SetActorScale3D(PreviousScale);
		MovementScope.RevertMove();
		return false;
	}
	if (Capsule)
	{
		Capsule->SetCapsuleSize(Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight(), false);
		Character->GetCharacterMovement()->bForceNextFloorCheck = true;
	}
	if (!bHasExteriorScale)
	{
		ExteriorScale = PreviousScale;
		if (Character) { ExteriorWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed; }
	}
	if (Character && bScaleMovement)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = ExteriorWalkSpeed * (Interior ? FMath::Clamp(Interior->InteriorMovementSpeedScale, 0.1f, 1.0f) : 1.0f);
	}
	bHasExteriorScale = IsValid(Interior);
	UNUWalkableInteriorComponent* PreviousInterior = CurrentInterior.Get();
	CurrentInterior = Interior;
	if (IsValid(PreviousInterior)) { PreviousInterior->HandleParticipantExited(Owner); }
	if (IsValid(Interior)) { Interior->HandleParticipantEntered(Owner); }
	// 지연된 이동/겹침이 끝난 뒤 같은 WorldLocation 경로에서 UI와 퀘스트를 갱신한다.
	APlayerState* PlayerState = Character ? Character->GetPlayerState() : nullptr;
	if (UNUWorldLocationComponent* WorldLocation = PlayerState ? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr)
	{
		WorldLocation->ScheduleLocationRefresh();
	}
	return true;
}

const FNULocationDefinition* UNUInteriorParticipantComponent::FindDoorDestination(const ANUWalkableDoor* Door, bool bInside) const
{
	const UNUWalkableInteriorComponent* Interior = Door->GetInterior();
	const int32 LocationId = bInside ? Interior->LocationId
		: (Door->ExteriorLocationId > 0 ? Door->ExteriorLocationId : LastExteriorLocationId);
	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	const UNUWorldDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldDataSubsystem>() : nullptr;
	const FNULocationDefinition* Target = LocationId > 0 && Data != nullptr ? Data->FindLocationDefinition(LocationId) : nullptr;
	return Target != nullptr && Target->bIndoor == bInside ? Target : nullptr;
}

bool UNUInteriorParticipantComponent::CanUseDoor(const ANUWalkableDoor* Door) const
{
	if (bEndingPlay || !IsValid(Door) || Door->GetWorld() != GetWorld()) { return false; }
	const UNUWalkableInteriorComponent* Interior = Door->GetInterior();
	if (!IsValid(Interior) || !IsValid(Interior->GetInteriorBounds()) || !Interiors.Contains(Interior)) { return false; }
	const bool bInside = CurrentInterior.Get() != Interior;
	if (bInside && !Door->bEntryEnabled) { return false; }

	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	const UEmberServerManager* Server = GameInstance != nullptr ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
	if (Server != nullptr)
	{
		// 온라인 출입은 목적지와 해금 조건을 문 개방 전에 검사한다.
		if (Server->IsLoggedIn())
		{
			const FNULocationDefinition* Destination = FindDoorDestination(Door, bInside);
			if (Destination == nullptr) { return false; }
			// 실내에서 시작한 캐릭터는 입장 조건과 무관하게 퇴장할 수 있어야 한다.
			if (!bInside) { return true; }
			const ACharacter* Character = Cast<ACharacter>(GetOwner());
			const APlayerState* PlayerState = Character != nullptr ? Character->GetPlayerState() : nullptr;
			const UNUWorldLocationComponent* WorldLocation = PlayerState != nullptr ? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
			return WorldLocation != nullptr && WorldLocation->IsLocationUnlocked(Destination->LocationId);
		}
		if (!Server->GetUserId().IsEmpty()) { return false; }
	}
	// 로컬 챕터에서도 장소 테이블의 입장 해금 조건을 사용한다. 퇴장은 제한하지 않는다.
	if (bInside)
	{
		const FNULocationDefinition* Destination = FindDoorDestination(Door, true);
		if (Destination != nullptr && Destination->UnlockQuestId > 0)
		{
			const ACharacter* Character = Cast<ACharacter>(GetOwner());
			const APlayerState* PlayerState = Character != nullptr ? Character->GetPlayerState() : nullptr;
			const UNUWorldLocationComponent* WorldLocation = PlayerState != nullptr ? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
			return WorldLocation != nullptr && WorldLocation->IsLocationUnlocked(Destination->LocationId);
		}
	}
	return true;
}

bool UNUInteriorParticipantComponent::RequestDoorTransition(ANUWalkableDoor* Door, const FVector& PreviousLocation)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (bEndingPlay || bApplyingTransition || bTransitionPending || !IsValid(Door) || BlockedDoor.Get() == Door || !IsValid(Controller) || !Controller->IsLocalController() || !IsValid(Controller->PlayerCameraManager)) { return false; }
	UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	UEmberServerManager* Server = GameInstance != nullptr ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
	if (Server != nullptr && !Server->IsLoggedIn() && !Server->GetUserId().IsEmpty())
	{
		// 로그인 이력이 있는 온라인 세션은 연결이 끊겨도 로컬 출입으로 우회하지 않는다.
		BlockedDoor = Door;
		Character->SetActorLocation(PreviousLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Character->GetCharacterMovement()->StopMovementImmediately();
		Door->RejectEntry(Character);
		ShowEntryFailureNotice();
		UE_LOG(LogNUInteriorTransition, Warning, TEXT("서버 연결이 준비되지 않아 문 출입을 차단했습니다. Door=%s"), *GetNameSafe(Door));
		return false;
	}

	if (Server != nullptr && Server->IsLoggedIn())
	{
		APlayerState* PlayerState = Character->GetPlayerState();
		const UNUWorldLocationComponent* WorldLocation = PlayerState != nullptr
			? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
		UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();
		const int32 LocalLocationId = WorldLocation != nullptr ? WorldLocation->GetCurrentLocationId() : 0;

		// 야외 구역 변경이 먼저 확정되어야 문 요청의 from_location_id가 서버 상태와 일치한다.
		if (Sync != nullptr && !Sync->IsLocationMoveReady(LocalLocationId))
		{
			Sync->EvaluateOutdoorLocation();
			return false;
		}
	}

	UNUWalkableInteriorComponent* Interior = Door->GetInterior();
	if (!CanUseDoor(Door)) { return false; }
	// 현재 방과 다른 실내를 가리키는 문은 방·층 이동으로, 같은 실내의 문은 실외 이동으로 처리한다.
	const bool bInside = CurrentInterior.Get() != Interior;
	if (!Door->IsDoorOpen() || !Door->IsTransitionContact(Character, !bInside)) { return false; }
	// 암전 동안 입력을 막고, 확정 시 문과 캐릭터 상태를 다시 검사한다.
	TransitionDoor = Door;
	BlockedDoor = Door;
	TransitionInterior = Interior;
	TransitionController = Controller;
	TransitionOrigin = Character->GetActorLocation();
	TransitionFallbackLocation = PreviousLocation;
	bTransitionInside = bInside;
	bTransitionPending = true;
	bTransitionCommitted = false;
	TransitionFadeInDuration = FMath::Max(0.0f, Door->FadeInDuration);
	Controller->SetIgnoreMoveInput(true);
	bTransitionInputLocked = true;
	Character->GetCharacterMovement()->StopMovementImmediately();
	const float FadeDuration = FMath::Max(0.0f, Door->FadeOutDuration);
	if (FadeDuration > 0.0f)
	{
		Controller->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeDuration, FLinearColor::Black, false, true);
		GetWorld()->GetTimerManager().SetTimer(TransitionCommitTimer, this, &ThisClass::CompleteTransitionFade, FadeDuration, false);
	}
	else
	{
		// 0초 타이머는 실행되지 않으므로 다음 프레임에 확정한다.
		Controller->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
		TransitionCommitTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CompleteTransitionFade);
	}
	return true;
}

bool UNUInteriorParticipantComponent::CommitInteriorTransition()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UNUWalkableInteriorComponent* Interior = TransitionInterior.Get();
	ANUWalkableDoor* Door = TransitionDoor.Get();
	if (!bTransitionPending || bTransitionCommitted || bEndingPlay || !IsValid(Character) || !IsValid(Interior) || !IsValid(Door) || !TransitionController.IsValid()) { return false; }
	// 요청 이후 위치나 소유 컨트롤러가 바뀌었으면 이전 요청을 적용하지 않는다.
	if (!Character->GetActorLocation().Equals(TransitionOrigin, 1.0f) || Character->GetController() != TransitionController.Get() || Door->GetInterior() != Interior || !Door->IsDoorOpen() || !Door->IsTransitionContact(Character, !bTransitionInside)) { return false; }
	if (!CanUseDoor(Door)) { return false; }
	FVector Destination;
	UNUWalkableInteriorComponent* DestinationInterior = bTransitionInside ? Interior : nullptr;
	if (!Door->FindArrivalLocation(Character, bTransitionInside, GetDestinationScale(DestinationInterior), Destination))
	{
		UE_LOG(LogNUInteriorTransition, Warning, TEXT("문 출입 안전 도착점을 찾지 못했습니다. Door=%s"), *GetNameSafe(Door));
		return false;
	}

	UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	UEmberServerManager* Server = GameInstance != nullptr ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
	if (Server != nullptr && !Server->IsLoggedIn() && !Server->GetUserId().IsEmpty())
	{
		// 암전 직전에 연결이 끊긴 경우에도 서버 승인 없이 위치를 바꾸지 않는다.
		UE_LOG(LogNUInteriorTransition, Warning, TEXT("LocationMove 송신 전에 서버 연결이 종료되었습니다. Door=%s"), *GetNameSafe(Door));
		return false;
	}

	if (Server != nullptr && Server->IsLoggedIn())
	{
		APlayerState* PlayerState = Character->GetPlayerState();
		UNUWorldLocationComponent* WorldLocation = PlayerState != nullptr
			? PlayerState->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;

		int32 FromLocationId = WorldLocation != nullptr ? WorldLocation->GetCurrentLocationId() : 0;
		int32 ConfirmedAreaId = 0;
		bool bConfirmedIndoor = false;
		UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();

		if (Sync != nullptr)
		{
			Sync->GetConfirmedSpace(ConfirmedAreaId, FromLocationId, bConfirmedIndoor);
		}

		const UNUWorldDataSubsystem* Data = GameInstance->GetSubsystem<UNUWorldDataSubsystem>();
		const FNULocationDefinition* Source = Data != nullptr ? Data->FindLocationDefinition(FromLocationId) : nullptr;
		const FNULocationDefinition* Target = FindDoorDestination(Door, bTransitionInside);
		if (Target == nullptr) { return false; }
		const int32 ToLocationId = Target->LocationId;

		// 같은 LocationId 안에서의 연출 이동은 서버 공간이 바뀌지 않으므로 로컬로 확정한다.
		if (FromLocationId != ToLocationId)
		{
			PendingFromLocationId = FromLocationId;
			PendingToLocationId = ToLocationId;
			PendingExpectedAreaId = Target->AreaId;
			bPendingFromIndoor = Source != nullptr && Source->bIndoor;
			PendingLocationDestination = Destination;
			PendingLocationMutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
			bAwaitingLocationMove = true;

			const AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(Character);
			const int32 Facing = FieldCharacter != nullptr ? static_cast<int32>(FieldCharacter->Facing) : 0;

			Server->SendLocationMove(FromLocationId, ToLocationId, Door->GateId,
				PendingLocationMutationId, Destination, Facing);

			return true;
		}
	}

	// 위치·크기·실내외 상태는 같은 적용 경로로 확정한다. 실패하면 모두 유지한다.
	bTransitionCommitted = ApplyInteriorState(DestinationInterior, &Destination);
	// 지연된 캡슐 이동이 물리에 반영된 뒤 닫힘을 다시 검사한다.
	if (bTransitionCommitted && Door->bCloseAfterLeaving) { Door->CloseDoor(); }
	Character->GetCharacterMovement()->StopMovementImmediately();
	return bTransitionCommitted;
}

void UNUInteriorParticipantComponent::CompleteTransitionFade()
{
	if (!bTransitionPending) { return; }

	const bool bCommittedOrAwaiting = CommitInteriorTransition();

	if (bAwaitingLocationMove) { return; }
	if (!bCommittedOrAwaiting)
	{
		RollbackRejectedTransition();
		ShowEntryFailureNotice();
	}

	BeginTransitionFadeIn();
}

void UNUInteriorParticipantComponent::BeginTransitionFadeIn()
{
	// 도착 확정 이후에는 화면이 밝아지는 동안에도 바로 걸을 수 있다.
	ReleaseTransitionInput();
	if (TransitionFadeInDuration <= 0.0f) { FinishInteriorTransition(); return; }

	if (TransitionController.IsValid() && IsValid(TransitionController->PlayerCameraManager))
	{
		TransitionController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, TransitionFadeInDuration, FLinearColor::Black, false, false);
	}

	GetWorld()->GetTimerManager().SetTimer(TransitionFinishTimer, this, &ThisClass::FinishInteriorTransition, TransitionFadeInDuration, false);
}

void UNUInteriorParticipantComponent::ReleaseTransitionInput()
{
	if (!bTransitionInputLocked) { return; }
	if (TransitionController.IsValid()) { TransitionController->SetIgnoreMoveInput(false); }
	bTransitionInputLocked = false;
}

void UNUInteriorParticipantComponent::HandleLocationMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response)
{
	if (!bTransitionPending || !bAwaitingLocationMove)
	{
		return;
	}

	UNUWalkableInteriorComponent* DestinationInterior = bTransitionInside ? TransitionInterior.Get() : nullptr;
	const FVector ConfirmedLocation = Response != nullptr
		? FVector(Response->PosX, Response->PosY, Response->PosZ) : FVector::ZeroVector;
	const bool bExpectedIndoor = DestinationInterior != nullptr;
	const bool bValidResponse = ResultCode == 200
		&& Response != nullptr
		&& Response->MutationId == PendingLocationMutationId
		&& Response->FromLocationId == PendingFromLocationId
		&& Response->ToLocationId == PendingToLocationId
		&& Response->_area_id.AreaId == PendingExpectedAreaId
		&& Response->_location_id.LocationId == PendingToLocationId
		&& Response->_is_indoor.IsIndoor == bExpectedIndoor
		&& Response->Facing >= static_cast<int32>(EFieldFacing::Down)
		&& Response->Facing <= static_cast<int32>(EFieldFacing::Right)
		&& !ConfirmedLocation.ContainsNaN()
		&& FVector::DistSquared(ConfirmedLocation, PendingLocationDestination) <= FMath::Square(50.0f);

	if (!bValidResponse)
	{
		UE_LOG(LogNUInteriorTransition, Warning,
			TEXT("LocationMove 거절 또는 응답 검증 실패. code=%d from=%d to=%d mutation=%s message=%s"),
			ResultCode, PendingFromLocationId, PendingToLocationId, *PendingLocationMutationId, *ResultMessage);
		RollbackRejectedTransition();
		ShowEntryFailureNotice();
	}
	else
	{
		bTransitionCommitted = ApplyInteriorState(DestinationInterior, &ConfirmedLocation);

		if (bTransitionCommitted)
		{
			if (AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(GetOwner()))
			{
				FieldCharacter->Facing = static_cast<EFieldFacing>(Response->Facing);
			}

			if (UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>())
			{
				Sync->ApplyConfirmedSpace(Response->_area_id.AreaId,
					Response->_location_id.LocationId, Response->_is_indoor.IsIndoor);
			}
		}

		if (bTransitionCommitted && bTransitionInside && PendingFromLocationId > 0 && !bPendingFromIndoor)
		{
			LastExteriorLocationId = PendingFromLocationId;
		}

		if (bTransitionCommitted && TransitionDoor.IsValid() && TransitionDoor->bCloseAfterLeaving)
		{
			TransitionDoor->CloseDoor();
		}

		if (!bTransitionCommitted)
		{
			UE_LOG(LogNUInteriorTransition, Error,
				TEXT("서버가 확정한 LocationMove 도착 상태를 적용하지 못했습니다. to=%d pos=%s"),
				PendingToLocationId, *ConfirmedLocation.ToCompactString());
			RollbackRejectedTransition();
			ShowEntryFailureNotice();
		}
	}

	if (!bValidResponse && ResultCode == 2402)
	{
		if (UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>())
		{
			if (Sync->ReconcileLocationMoveFailure(ResultCode, ResultMessage, Response))
			{
				Sync->EvaluateOutdoorLocation();
			}
		}
	}

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	ClearPendingLocationMove();

	BeginTransitionFadeIn();
}

void UNUInteriorParticipantComponent::ShowEntryFailureNotice() const
{
	APlayerController* Controller = TransitionController.Get();
	if (!IsValid(Controller))
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		Controller = Character != nullptr ? Cast<APlayerController>(Character->GetController()) : nullptr;
	}

	if (UNUUIManager* UI = UNUUIManager::Get(Controller))
	{
		UI->ShowNotice(NSLOCTEXT("NUInteriorTransition", "EntryFailed", "입장에 실패했습니다. 잠시 후 다시 시도해 주세요."));
	}
}

void UNUInteriorParticipantComponent::ClearPendingLocationMove()
{
	bAwaitingLocationMove = false;
	PendingFromLocationId = 0;
	PendingToLocationId = 0;
	PendingExpectedAreaId = 0;
	PendingLocationMutationId.Reset();
	PendingLocationDestination = FVector::ZeroVector;
	bPendingFromIndoor = false;
}

void UNUInteriorParticipantComponent::RollbackRejectedTransition()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return;
	}

	// 이동 직전 엔진이 검증했던 위치로 되돌린 뒤 문 충돌을 복구한다.
	Character->SetActorLocation(TransitionFallbackLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->GetCharacterMovement()->bForceNextFloorCheck = true;
	if (TransitionDoor.IsValid()) { TransitionDoor->RejectEntry(Character); }
}

void UNUInteriorParticipantComponent::HandleServerStateChanged(EEmberConnState NewState)
{
	if (!bTransitionPending || !bAwaitingLocationMove || NewState == EEmberConnState::LoggedIn)
	{
		return;
	}

	// 응답 대기 중 스트림이 끊기면 암전과 입력 차단을 즉시 풀고 현재 공간을 유지한다.
	UE_LOG(LogNUInteriorTransition, Warning, TEXT("LocationMove 대기 중 서버 연결이 종료되었습니다. state=%d"), static_cast<int32>(NewState));
	RollbackRejectedTransition();
	ShowEntryFailureNotice();
	ClearPendingLocationMove();
	BeginTransitionFadeIn();
}

void UNUInteriorParticipantComponent::FinishInteriorTransition()
{
	GetWorld()->GetTimerManager().ClearTimer(TransitionCommitTimer);
	GetWorld()->GetTimerManager().ClearTimer(TransitionFinishTimer);
	ReleaseTransitionInput();

	if (bTransitionPending && TransitionController.IsValid())
	{
		if (IsValid(TransitionController->PlayerCameraManager)) { TransitionController->PlayerCameraManager->StopCameraFade(); }
	}

	// 성공한 도착점은 반대편 접촉 범위 밖이다. 범위 전체를 벗어나지 않아도 바로 돌아설 수 있다.
	if (bTransitionCommitted) { BlockedDoor.Reset(); }

	ClearPendingLocationMove();

	bTransitionPending = false;
	bTransitionCommitted = false;
	TransitionDoor.Reset();
	TransitionInterior.Reset();
	TransitionController.Reset();
	TransitionFallbackLocation = FVector::ZeroVector;
}

void UNUInteriorParticipantComponent::HandleTransformChanged(USceneComponent* Component, EUpdateTransformFlags UpdateFlags, ETeleportType Teleport)
{
	if (!GetOwner()->IsA<ACharacter>()) { RefreshInterior(); }
}

void UNUInteriorParticipantComponent::HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	if (OldLocation.Equals(GetOwner()->GetActorLocation()) || bApplyingTransition || bTransitionPending || bEndingPlay) { return; }
	RefreshInterior();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->IsLocallyControlled()) { return; }
	if (BlockedDoor.IsValid() && !BlockedDoor->IsTransitionContact(Character, CurrentInterior.IsValid())) { BlockedDoor.Reset(); }
	for (const auto& Door : NearbyDoors)
	{
		if (!Door.IsValid()) { continue; }
		Door->RefreshAutomaticOpen(Character);
		if (Door->IsTransitionApproach(Character, OldLocation, CurrentInterior.IsValid()) && RequestDoorTransition(Door.Get(), OldLocation)) { return; }
	}
}

void UNUInteriorParticipantComponent::HandleDoorOverlapBegin(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ANUWalkableDoor* Door = Cast<ANUWalkableDoor>(OtherActor);
	if (Door && OtherComponent == Door->InteractionVolume) { NearbyDoors.AddUnique(Door); }
}

void UNUInteriorParticipantComponent::HandleDoorOverlapEnd(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	ANUWalkableDoor* Door = Cast<ANUWalkableDoor>(OtherActor);
	if (Door && OtherComponent == Door->InteractionVolume) { NearbyDoors.Remove(Door); }
}

void UNUInteriorParticipantComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	FinishInteriorTransition();
	BlockedDoor.Reset();
	if (CurrentInterior.IsValid())
	{
		CurrentInterior->HandleParticipantExited(Pawn);
		CurrentInterior->HandleParticipantEntered(Pawn);
	}
}
