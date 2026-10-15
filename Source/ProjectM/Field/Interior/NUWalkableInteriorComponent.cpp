#include "Field/Interior/NUWalkableInteriorComponent.h"

#include "Field/Net/FieldRemoteCharacter.h"
#include "Field/Net/FieldRemoteSpirit.h"
#include "Field/NUFieldSpirit.h"
#include "Field/NPC/NUNPCBase.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUInteriorPresentationSettings.h"
#include "Quest/Component/NUQuestProviderComponent.h"

#include "Camera/CameraComponent.h"
#include "Blueprint/UserWidget.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
bool IsInteriorPresentationActor(const AActor* Actor)
{
	return Actor->IsA<ACharacter>() || Actor->IsA<ANUNPCBase>()
		|| Actor->IsA<AFieldRemoteCharacter>() || Actor->IsA<AFieldRemoteSpirit>()
		|| Actor->IsA<ANUFieldSpirit>() || Actor->FindComponentByClass<UNUQuestProviderComponent>();
}

FBox2D ProjectInteriorBounds(const FBox& Bounds, const FTransform& View)
{
	FBox2D Projection(ForceInit);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Point(Corner & 1 ? Bounds.Max.X : Bounds.Min.X, Corner & 2 ? Bounds.Max.Y : Bounds.Min.Y, Corner & 4 ? Bounds.Max.Z : Bounds.Min.Z);
		const FVector Local = View.InverseTransformPosition(Point);
		if (Local.X > 1.0) { Projection += FVector2D(Local.Y / Local.X, Local.Z / Local.X); }
	}
	return Projection;
}
}

UNUWalkableInteriorComponent::UNUWalkableInteriorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNUWalkableInteriorComponent::BeginPlay()
{
	Super::BeginPlay();
	TInlineComponentArray<USceneComponent*> Components(GetOwner());
	for (USceneComponent* Component : Components)
	{
		if (Component->GetFName() == ForegroundBoundsName)
		{
			ForegroundBounds = Cast<UBoxComponent>(Component);
		}
		if (Component->GetFName() == InteriorBoundsName)
		{
			InteriorBounds = Cast<UBoxComponent>(Component);
		}
	}
	if (!IsValid(InteriorBounds))
	{
		UE_LOG(LogTemp, Error, TEXT("Walkable interior requires interior bounds: %s"), *GetNameSafe(GetOwner()));
		return;
	}
	InteriorBounds->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleEntry);
	InteriorBounds->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleExit);
	// 먼저 시작한 참가자만 연결한다. 나중에 시작한 참가자는 자신의 BeginPlay에서 등록한다.
	for (TObjectIterator<UNUInteriorParticipantComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->HasBegunPlay())
		{
			RegisterParticipant(It->GetOwner());
		}
	}
}

void UNUWalkableInteriorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (IsValid(InteriorBounds))
	{
		InteriorBounds->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleEntry);
	}
	if (IsValid(InteriorBounds))
	{
		InteriorBounds->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleExit);
	}
	for (const auto& Participant : Participants)
	{
		if (Participant.IsValid())
		{
			Participant->RemoveInterior(this);
		}
	}
	if (CameraArm.IsValid())
	{
		CameraArm->TargetArmLength = OriginalCameraDistance;
	}
	ClearCharacter();
	SetPresentation(false);
	Super::EndPlay(EndPlayReason);
}

bool UNUWalkableInteriorComponent::IsLocalPlayerInside() const
{
	return bLocalPlayerInside;
}

// 문턱의 진입, 이탈 기준을 구분해 실내외 상태가 반복 전환되는 것을 막는다.
bool UNUWalkableInteriorComponent::ContainsParticipant(AActor* Actor, bool bRetention) const
{
	if (bEndingPlay || !IsValid(Actor) || !IsValid(InteriorBounds))
	{
		return false;
	}
	const FVector Position = InteriorBounds->GetComponentTransform().InverseTransformPosition(Actor->GetActorLocation());
	const FVector Extent = InteriorBounds->GetUnscaledBoxExtent();
	FVector Minimum = -Extent;
	// 문턱의 앞뒤에만 여유를 두고 벽 가장자리에서는 실내 상태를 유지한다.
	Minimum.Y += bRetention ? -DoorwayBoundaryMargin : DoorwayBoundaryMargin;
	return FBox(Minimum, Extent).IsInsideOrOn(Position);
}

// 캐릭터를 실내 판정에 등록하고, 현재 실내 화면에서 표시할지 확인한다.
void UNUWalkableInteriorComponent::RegisterParticipant(AActor* Actor)
{
	if (bEndingPlay || !IsValid(Actor) || Actor->IsActorBeingDestroyed())
	{
		return;
	}
	if (IsInteriorPresentationActor(Actor))
	{
		ObservePresentationActor(Actor);
	}
	RefreshForegroundActor(Actor);
	UNUInteriorParticipantComponent* Participant = Actor->FindComponentByClass<UNUInteriorParticipantComponent>();
	if (!Participant || !Participant->HasBegunPlay())
	{
		return;
	}
	Participants.AddUnique(Participant);
	Participant->AddInterior(this);
}

void UNUWalkableInteriorComponent::HandleEntry(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!IsValid(Character) || OtherComponent != Character->GetCapsuleComponent())
	{
		return;
	}
	RegisterParticipant(Character);
	if (UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>())
	{
		Participant->RefreshInterior();
	}
}

void UNUWalkableInteriorComponent::HandleExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (IsValid(Character) && OtherComponent == Character->GetCapsuleComponent())
	{
		if (UNUInteriorParticipantComponent* Participant = Character->FindComponentByClass<UNUInteriorParticipantComponent>())
		{
			Participant->RefreshInterior();
		}
	}
}

// 실내 진입 시 소환을 해제하고 로컬 플레이어의 카메라와 건물 표시를 전환한다.
void UNUWalkableInteriorComponent::HandleParticipantEntered(AActor* Actor)
{
	RefreshForegroundActor(Actor);
	ACharacter* Character = Cast<ACharacter>(Actor);
	if (!bEndingPlay && IsValid(Character))
	{
		if (ANUPlayerState* PlayerState = Character->GetPlayerState<ANUPlayerState>())
		{
			// 권한 측의 기존 해제 경로가 아바타 제거와 필드 동기화까지 처리한다.
			PlayerState->SetNaiaSummoned(false);
		}
	}
	const APlayerController* Controller = IsValid(Character) ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (bEndingPlay || bLocalPlayerInside || !IsValid(Controller) || !Controller->IsLocalController())
	{
		return;
	}
	LocalCharacter = Character;
	Character->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleCharacterDestroyed);
	CameraArm = Character->FindComponentByClass<USpringArmComponent>();
	if (CameraArm.IsValid())
	{
		OriginalCameraDistance = CameraArm->TargetArmLength;
		CameraArm->TargetArmLength *= FMath::Max(0.1f, InteriorCameraDistanceScale);
	}
	SetPresentation(true);
}

// 실내 이탈 시 저장한 카메라 거리와 외부 화면을 복구한다.
void UNUWalkableInteriorComponent::HandleParticipantExited(AActor* Actor)
{
	RefreshForegroundActor(Actor);
	if (LocalCharacter.Get() != Actor || !bLocalPlayerInside)
	{
		return;
	}
	if (CameraArm.IsValid())
	{
		CameraArm->TargetArmLength = OriginalCameraDistance;
	}
	ClearCharacter();
	SetPresentation(false);
}

void UNUWalkableInteriorComponent::HandleCharacterDestroyed(AActor* DestroyedActor)
{
	HandleParticipantExited(DestroyedActor);
}

void UNUWalkableInteriorComponent::SetPresentation(bool bInside)
{
	bLocalPlayerInside = bInside;
	OnPresentationChanged.Broadcast(bInside);
}

void UNUWalkableInteriorComponent::ClearCharacter()
{
	RestoreExteriorView();
	if (LocalCharacter.IsValid())
	{
		LocalCharacter->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleCharacterDestroyed);
	}
	LocalCharacter.Reset();
	CameraArm.Reset();
}

// 방 범위를 재질에 전달하고 주변 어둡게 처리와 가림 효과를 적용한다.
void UNUWalkableInteriorComponent::ApplyInteriorView(bool bInside)
{
	if (!bInside)
	{
		RestoreExteriorView();
		return;
	}
	ACharacter* Character = LocalCharacter.Get();
	if (!bLocalPlayerInside || !IsValid(Character) || PresentationCamera.IsValid())
	{
		return;
	}
	UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
	APlayerController* Controller = Cast<APlayerController>(Character->GetController());
	if (!IsValid(Camera) || !IsValid(Controller))
	{
		return;
	}
	PresentationController = Controller;
	PresentationCamera = Camera;
	OriginalPostProcessWeight = Camera->PostProcessBlendWeight;
	OriginalFocusSettings = Camera->PostProcessSettings;
	UpdateInteriorFocus();
	if (SceneryPresentationParameters)
	{
		UMaterialParameterCollectionInstance* Parameters = GetWorld()->GetParameterCollectionInstance(SceneryPresentationParameters);
		const FTransform Transform = InteriorBounds->GetComponentTransform();
		const FVector Extent = (InteriorBounds->GetUnscaledBoxExtent() + PresentationBoundsPadding) * Transform.GetScale3D().GetAbs();
		Parameters->SetVectorParameterValue(TEXT("RoomCenter"), FLinearColor(Transform.GetLocation()));
		Parameters->SetVectorParameterValue(TEXT("RoomExtent"), FLinearColor(Extent));
		Parameters->SetVectorParameterValue(TEXT("RoomAxisX"), FLinearColor(Transform.GetUnitAxis(EAxis::X)));
		Parameters->SetVectorParameterValue(TEXT("RoomAxisY"), FLinearColor(Transform.GetUnitAxis(EAxis::Y)));
		Parameters->SetVectorParameterValue(TEXT("RoomAxisZ"), FLinearColor(Transform.GetUnitAxis(EAxis::Z)));
		Parameters->SetScalarParameterValue(TEXT("InteriorActive"), 1.0f);
		bOwnsSceneryPresentation = true;
	}
	if (IsValid(ExteriorDimmingMaterial))
	{
		DimmingInstance = UMaterialInstanceDynamic::Create(ExteriorDimmingMaterial, this);
		const FTransform BoundsTransform = InteriorBounds->GetComponentTransform();
		const FVector Extent = (InteriorBounds->GetUnscaledBoxExtent() + PresentationBoundsPadding) * BoundsTransform.GetScale3D().GetAbs();
		DimmingInstance->SetVectorParameterValue(TEXT("RoomCenter"), FLinearColor(BoundsTransform.GetLocation()));
		DimmingInstance->SetVectorParameterValue(TEXT("RoomExtent"), FLinearColor(Extent));
		DimmingInstance->SetVectorParameterValue(TEXT("RoomAxisX"), FLinearColor(BoundsTransform.GetUnitAxis(EAxis::X)));
		DimmingInstance->SetVectorParameterValue(TEXT("RoomAxisY"), FLinearColor(BoundsTransform.GetUnitAxis(EAxis::Y)));
		DimmingInstance->SetVectorParameterValue(TEXT("RoomAxisZ"), FLinearColor(BoundsTransform.GetUnitAxis(EAxis::Z)));
		UpdateInteriorSurroundings();
		DimmingInstance->SetScalarParameterValue(TEXT("EdgeSoftness"), FMath::Max(0.1f, PresentationEdgeSoftness));
		Camera->PostProcessSettings.AddBlendable(DimmingInstance, 1.0f);
		Camera->PostProcessBlendWeight = 1.0f;
	}
	for (const FName Tag : ForegroundComponentTags)
	{
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsWithTag(this, Tag, Actors);
		for (AActor* Actor : Actors)
		{
			TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
			for (UPrimitiveComponent* Component : Components)
			{
				if (Component->ComponentHasTag(Tag) && !Controller->HiddenPrimitiveComponents.Contains(Component))
				{
					Controller->HiddenPrimitiveComponents.Add(Component);
					HiddenForegroundComponents.Add(Component);
				}
			}
		}
	}
	// 새 캐릭터의 표시 감시는 실내 화면을 사용하는 동안에만 필요하다.
	ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::RegisterParticipant));
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		ObservePresentationActor(*It);
		RefreshForegroundActor(*It);
	}
	RefreshForegroundOcclusion();

	// 캐릭터 전체를 반복 검사하지 않는다. 이동한 대상만 다음 프레임에 갱신한다.
	// 생성 알림이 없는 동적 메쉬와 이름표는 숨겨진 대상만 낮은 빈도로 보완한다.
	GetWorld()->GetTimerManager().SetTimer(HiddenComponentsTimer, this, &ThisClass::RefreshHiddenComponents, 1.0f, true);

	GetWorld()->GetTimerManager().SetTimer(PresentationSettingsTimer, this, &ThisClass::RefreshPresentationSettings, 0.1f, true);
}

// 실내 화면이 활성화된 동안 캐릭터의 위치 변경과 종료 이벤트를 구독한다.
void UNUWalkableInteriorComponent::ObservePresentationActor(AActor* Actor)
{
	if (!PresentationController.IsValid() || !IsValid(Actor) || Actor == GetOwner() || Actor == LocalCharacter.Get())
	{
		return;
	}

	USceneComponent* Root = Actor->GetRootComponent();
	if (IsValid(Root) && !ObservedPresentationRoots.Contains(Root))
	{
		ObservedPresentationRoots.Add(Root);
		Root->TransformUpdated.AddUObject(this, &ThisClass::HandlePresentationTransformChanged);
		Actor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandlePresentationActorEndPlay);

		// 생성 직후에는 이름표 등의 초기화가 남을 수 있어 다음 프레임에 한 번 더 확인한다.
		ChangedPresentationActors.Add(Actor);
		PendingPresentationComponents.Add(Actor);

		if (!GetWorld()->GetTimerManager().TimerExists(ChangedCharactersTimer))
		{
			ChangedCharactersTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshChangedCharacters);
		}
	}
}

// 이동한 캐릭터를 모아 다음 프레임에 한 번씩 표시 상태를 갱신한다.
void UNUWalkableInteriorComponent::HandlePresentationTransformChanged(USceneComponent* Component, EUpdateTransformFlags UpdateFlags, ETeleportType Teleport)
{
	if (bEndingPlay || !PresentationController.IsValid() || !IsValid(Component))
	{
		return;
	}

	ChangedPresentationActors.Add(Component->GetOwner());

	if (!GetWorld()->GetTimerManager().TimerExists(ChangedCharactersTimer))
	{
		ChangedCharactersTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshChangedCharacters);
	}
}

// 종료된 캐릭터의 이벤트 연결과 표시 상태 기록을 정리한다.
void UNUWalkableInteriorComponent::HandlePresentationActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		Root->TransformUpdated.RemoveAll(this);
		ObservedPresentationRoots.Remove(Root);
	}

	Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandlePresentationActorEndPlay);

	ChangedPresentationActors.Remove(Actor);
	PendingPresentationComponents.Remove(Actor);
	PresentationVisibility.Remove(Actor);
	HiddenCharacters.Remove(Actor);
}

// 변경된 캐릭터만 검사하고 새로 등록된 대상은 컴포넌트도 확인한다.
void UNUWalkableInteriorComponent::RefreshChangedCharacters()
{
	GetWorld()->GetTimerManager().ClearTimer(ChangedCharactersTimer);
	const TSet<TWeakObjectPtr<AActor>> Changed = MoveTemp(ChangedPresentationActors);
	ChangedPresentationActors.Reset();

	for (const auto& Actor : Changed)
	{
		RefreshForegroundActor(Actor.Get(), PendingPresentationComponents.Remove(Actor) > 0);
	}
}

// 캐릭터 전체 순회 없이 실내 초점과 주변 연출 설정만 갱신한다.
void UNUWalkableInteriorComponent::RefreshPresentationSettings()
{
	UpdateInteriorFocus();
	UpdateInteriorSurroundings();
	RefreshForegroundOcclusion();
}

// 후보는 입장·생성 시 모으고 카메라 이동에 따른 가림만 주기적으로 갱신한다.
void UNUWalkableInteriorComponent::RefreshForegroundOcclusion()
{
	if (!PresentationCamera.IsValid() || !IsValid(InteriorBounds) || !LocalCharacter.IsValid()) { return; }
	ForegroundViewTransform = PresentationCamera->GetComponentTransform();
	ForegroundRoomProjection = ProjectInteriorBounds(InteriorBounds->Bounds.GetBox(), ForegroundViewTransform);
	ForegroundRoomDepth = ForegroundViewTransform.InverseTransformPosition(InteriorBounds->GetComponentLocation()).X;
	ForegroundFloorHeight = LocalCharacter->GetActorLocation().Z - LocalCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	for (auto It = ForegroundMeshes.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) { It.RemoveCurrent(); continue; }
		SetForegroundOccluded(It->Get(), IsForegroundOccluder(It->Get()));
	}
}

bool UNUWalkableInteriorComponent::IsForegroundOccluder(const UStaticMeshComponent* Component) const
{
	if (!IsValid(Component) || !Component->IsVisible() || Component->bHiddenInGame || !ForegroundRoomProjection.bIsValid || ForegroundRoomDepth <= 1.0) { return false; }
	const FBox Box = Component->Bounds.GetBox();
	const FVector Local = InteriorBounds->GetComponentTransform().InverseTransformPosition(Box.GetCenter());
	const FVector Extent = InteriorBounds->GetUnscaledBoxExtent();
	// 같은 건물의 위층 소품은 현재 방의 천장 위에 있을 때만 가린다.
	if (FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y) { return Box.Min.Z > InteriorBounds->Bounds.GetBox().Max.Z; }
	if (Box.Max.Z <= ForegroundFloorHeight + ForegroundFloorClearance || Box.GetSize().Z <= ForegroundFloorClearance) { return false; }
	// 방 뒤쪽 물체와 화면에서 방에 겹치지 않는 물체는 그대로 둔다.
	if (ForegroundViewTransform.InverseTransformPosition(Box.GetCenter()).X >= ForegroundRoomDepth) { return false; }
	const FBox2D Projection = ProjectInteriorBounds(Box, ForegroundViewTransform);
	return Projection.bIsValid && Projection.Intersect(ForegroundRoomProjection);
}

void UNUWalkableInteriorComponent::SetForegroundOccluded(UPrimitiveComponent* Component, bool bHidden)
{
	APlayerController* Controller = PresentationController.Get();
	if (!IsValid(Controller)) { return; }
	if (bHidden && !Controller->HiddenPrimitiveComponents.Contains(Component))
	{
		Controller->HiddenPrimitiveComponents.Add(Component);
		HiddenForegroundComponents.Add(Component);
		HiddenOccluders.Add(Component);
	}
	else if (!bHidden && HiddenOccluders.Remove(Component) > 0)
	{
		Controller->HiddenPrimitiveComponents.Remove(Component);
		HiddenForegroundComponents.Remove(Component);
	}
}

// 숨겨진 캐릭터에 뒤늦게 붙은 메쉬·이름표를 보완 검사하고 무효 참조를 정리한다.
void UNUWalkableInteriorComponent::RefreshHiddenComponents()
{
	const TSet<TWeakObjectPtr<AActor>> Hidden = HiddenCharacters;
	for (const auto& Actor : Hidden)
	{
		RefreshForegroundActor(Actor.Get(), true);
	}

	for (auto It = HiddenCharacterWidgets.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = HiddenCharacters.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	HiddenForegroundComponents.RemoveAll([](const auto& Component) { return !Component.IsValid(); });
}

// 공통 주변 연출 설정을 읽고 값이 바뀌었을 때만 재질에 전달한다.
void UNUWalkableInteriorComponent::UpdateInteriorSurroundings()
{
	if (!IsValid(DimmingInstance) || !IsValid(InteriorBounds))
	{
		return;
	}

	if (!PresentationSettings.IsValid())
	{
		for (TActorIterator<ANUInteriorPresentationSettings> It(GetWorld()); It; ++It)
		{
			if (It->GetLevel() == GetWorld()->PersistentLevel && !It->IsActorBeingDestroyed())
			{
				PresentationSettings = *It;
				break;
			}
		}
	}

	const ANUInteriorPresentationSettings* Settings = PresentationSettings.Get();
	const FVector Extent = (InteriorBounds->GetUnscaledBoxExtent() + PresentationBoundsPadding)
		* InteriorBounds->GetComponentScale().GetAbs();
	const float Brightness = Settings ? Settings->ExteriorBrightness : ExteriorBrightness;
	const float Distance = Settings ? Settings->ExteriorVisibleDistance : FMath::Max(Extent.X, Extent.Y) * 1.25f;
	const float Softness = Settings ? Settings->ExteriorFadeSoftness : 0.92f;
	const FVector Parameters(FMath::Clamp(Brightness, 0.0f, 1.0f), FMath::Max(0.0f, Distance), FMath::Clamp(Softness, 0.0f, 1.0f));

	if (!bHasSurroundingsParameters || Parameters != LastSurroundingsParameters)
	{
		DimmingInstance->SetScalarParameterValue(TEXT("ExteriorBrightness"), Parameters.X);
		DimmingInstance->SetScalarParameterValue(TEXT("ExteriorVisibleDistance"), Parameters.Y);
		DimmingInstance->SetScalarParameterValue(TEXT("ExteriorFadeSoftness"), Parameters.Z);

		LastSurroundingsParameters = Parameters;
		bHasSurroundingsParameters = true;
	}
}

// 실내 초점 보정 옵션이 켜져 있으면 캐릭터 거리에 맞춰 카메라 초점을 조정한다.
void UNUWalkableInteriorComponent::UpdateInteriorFocus()
{
	if (!bFocusInteriorCamera || !PresentationCamera.IsValid() || !LocalCharacter.IsValid())
	{
		return;
	}
	UCameraComponent* Camera = PresentationCamera.Get();
	FPostProcessSettings& Settings = Camera->PostProcessSettings;
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = FMath::Max(1.0f, FVector::DotProduct(
		LocalCharacter->GetActorLocation() - Camera->GetComponentLocation(), Camera->GetForwardVector()));
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = 16.0f;
	Settings.bOverride_DepthOfFieldSensorWidth = true;
	Settings.DepthOfFieldSensorWidth = 24.0f;
	Settings.bOverride_DepthOfFieldDepthBlurAmount = true;
	Settings.DepthOfFieldDepthBlurAmount = 0.0f;
	Camera->PostProcessBlendWeight = 1.0f;
}

// 실내외 위치에 따라 메쉬·이름표를 숨기거나 복구하며, 표시 상태가 같으면 순회를 생략한다.
void UNUWalkableInteriorComponent::RefreshForegroundActor(AActor* Actor, bool bRefreshComponents)
{
	APlayerController* Controller = PresentationController.Get();
	if (!IsValid(Controller) || !IsValid(Actor) || Actor == LocalCharacter.Get() || Actor == GetOwner())
	{
		return;
	}
	const bool bCharacter = IsInteriorPresentationActor(Actor);
	bool bHide = false;
	if (bCharacter)
	{
		const UNUInteriorParticipantComponent* Participant = Actor->FindComponentByClass<UNUInteriorParticipantComponent>();
		const UNUWalkableInteriorComponent* OtherInterior = Participant ? Participant->GetCurrentInterior() : nullptr;
		bHide = OtherInterior ? OtherInterior != this : !ContainsParticipant(Actor, !HiddenCharacters.Contains(Actor));

		const bool* Previous = PresentationVisibility.Find(Actor);
		if (!bRefreshComponents && Previous && *Previous == bHide)
		{
			return;
		}

		PresentationVisibility.Add(Actor, bHide);
		if (bHide)
		{
			HiddenCharacters.Add(Actor);
		}
		else
		{
			HiddenCharacters.Remove(Actor);
		}
	}
	else
	{
		// 분리된 방에 배치한 벽·가구·문은 전경 소품으로 취급하지 않는다.
		if (GetOwner()->GetLevel() != GetWorld()->PersistentLevel && Actor->GetLevel() == GetOwner()->GetLevel()) { return; }
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			ForegroundMeshes.Add(Mesh);
			SetForegroundOccluded(Mesh, IsForegroundOccluder(Mesh));
		}
		ObservePresentationActor(Actor);
		return;
	}
	TInlineComponentArray<UPrimitiveComponent*> Components(Actor, true);
	for (UPrimitiveComponent* Component : Components)
	{
		if (bHide && !Controller->HiddenPrimitiveComponents.Contains(Component))
		{
			Controller->HiddenPrimitiveComponents.Add(Component);
			HiddenForegroundComponents.Add(Component);
		}
		else if (!bHide && HiddenForegroundComponents.Remove(Component) > 0)
		{
			Controller->HiddenPrimitiveComponents.Remove(Component);
		}
		// 화면 공간의 이름표와 퀘스트 표시는 프리미티브 숨김에 영향을 받지 않는다.
		// 게임 로직의 표시 상태는 유지하고 이 화면에서 그리는 부분만 가린다.
		UWidgetComponent* WidgetComponent = bCharacter ? Cast<UWidgetComponent>(Component) : nullptr;
		if (WidgetComponent && WidgetComponent->GetWidgetSpace() == EWidgetSpace::Screen
			&& WidgetComponent->GetOwnerPlayer() == Controller->GetLocalPlayer())
		{
			if (UUserWidget* Widget = WidgetComponent->GetUserWidgetObject())
			{
				if (bHide)
				{
					if (!HiddenCharacterWidgets.Contains(Widget))
					{
						HiddenCharacterWidgets.Add(Widget, Widget->GetRenderOpacity());
					}
					Widget->SetRenderOpacity(0.0f);
				}
				else if (const float* Opacity = HiddenCharacterWidgets.Find(Widget))
				{
					Widget->SetRenderOpacity(*Opacity);
					HiddenCharacterWidgets.Remove(Widget);
				}
			}
		}
	}
}

// 퇴장 시 타이머·이벤트·가림 효과를 정리하고 기존 카메라와 위젯 표시를 복구한다.
void UNUWalkableInteriorComponent::RestoreExteriorView()
{
	GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	ActorSpawnedHandle.Reset();
	GetWorld()->GetTimerManager().ClearTimer(ChangedCharactersTimer);
	GetWorld()->GetTimerManager().ClearTimer(HiddenComponentsTimer);
	GetWorld()->GetTimerManager().ClearTimer(PresentationSettingsTimer);

	for (const auto& Root : ObservedPresentationRoots)
	{
		if (Root.IsValid())
		{
			Root->TransformUpdated.RemoveAll(this);
			if (AActor* Actor = Root->GetOwner())
			{
				Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandlePresentationActorEndPlay);
			}
		}
	}

	ObservedPresentationRoots.Reset();
	ChangedPresentationActors.Reset();
	PresentationVisibility.Reset();
	PendingPresentationComponents.Reset();
	bHasSurroundingsParameters = false;

	for (const auto& Entry : HiddenCharacterWidgets)
	{
		if (Entry.Key.IsValid())
		{
			Entry.Key->SetRenderOpacity(Entry.Value);
		}
	}
	HiddenCharacterWidgets.Reset();
	HiddenCharacters.Reset();

	// 정리 시점에는 폰의 카메라가 이미 제거 중일 수 있으므로 카메라와 별도로 복구한다.
	if (bOwnsSceneryPresentation)
	{
		if (SceneryPresentationParameters)
		{
			GetWorld()->GetParameterCollectionInstance(SceneryPresentationParameters)->SetScalarParameterValue(TEXT("InteriorActive"), 0.0f);
		}
		bOwnsSceneryPresentation = false;
	}
	if (PresentationCamera.IsValid())
	{
		if (bFocusInteriorCamera)
		{
			FPostProcessSettings& Settings = PresentationCamera->PostProcessSettings;
			Settings.bOverride_DepthOfFieldFocalDistance = OriginalFocusSettings.bOverride_DepthOfFieldFocalDistance;
			Settings.DepthOfFieldFocalDistance = OriginalFocusSettings.DepthOfFieldFocalDistance;
			Settings.bOverride_DepthOfFieldFstop = OriginalFocusSettings.bOverride_DepthOfFieldFstop;
			Settings.DepthOfFieldFstop = OriginalFocusSettings.DepthOfFieldFstop;
			Settings.bOverride_DepthOfFieldSensorWidth = OriginalFocusSettings.bOverride_DepthOfFieldSensorWidth;
			Settings.DepthOfFieldSensorWidth = OriginalFocusSettings.DepthOfFieldSensorWidth;
			Settings.bOverride_DepthOfFieldDepthBlurAmount = OriginalFocusSettings.bOverride_DepthOfFieldDepthBlurAmount;
			Settings.DepthOfFieldDepthBlurAmount = OriginalFocusSettings.DepthOfFieldDepthBlurAmount;
		}
		PresentationCamera->PostProcessSettings.RemoveBlendable(DimmingInstance);
		PresentationCamera->PostProcessBlendWeight = OriginalPostProcessWeight;
	}
	if (PresentationController.IsValid())
	{
		for (const auto& Component : HiddenForegroundComponents)
		{
			PresentationController->HiddenPrimitiveComponents.Remove(Component);
		}
	}
	HiddenForegroundComponents.Reset();
	ForegroundMeshes.Reset();
	HiddenOccluders.Reset();
	ForegroundRoomProjection.Init();
	PresentationCamera.Reset();
	PresentationController.Reset();
	DimmingInstance = nullptr;
}
