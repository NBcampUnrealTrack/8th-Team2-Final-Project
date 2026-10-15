#include "WorldTransition/World/NUWorldPortal.h"

#include "WorldTransition/World/NUWorldTransitionSubsystem.h"
#include "WorldTransition/Data/NUWorldPortalDataSubsystem.h"
#include "Engine/GameInstance.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Engine/Level.h"
#include "WorldTransition/Data/NUWorldTransitionSettings.h"
#include "WorldLocation/Data/NUWorldDataSettings.h"
#include "WorldTransition/World/NUWorldEntryPoint.h"
#endif

ANUWorldPortal::ANUWorldPortal()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bGenerateOverlapEventsDuringLevelStreaming = true;

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->SetBoxExtent(FVector(70.f, 70.f, 90.f));
	InteractionVolume->SetCollisionProfileName(TEXT("NU_Interactable"));
	InteractionVolume->SetGenerateOverlapEvents(true);

	EntranceBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("EntranceBlocker"));
	EntranceBlocker->SetupAttachment(InteractionVolume);
	EntranceBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EntranceBlocker->SetGenerateOverlapEvents(false);

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(InteractionVolume);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ANUWorldPortal::CanInteract_Implementation(AActor* Interactor) const
{
	const UWorld* World = GetWorld();
	const UNUWorldTransitionSubsystem* Transition = World != nullptr ? World->GetSubsystem<UNUWorldTransitionSubsystem>() : nullptr;
	return GetActivationMode() == ENUWorldPortalActivation::Interaction && !PortalId.IsNone() && (UsesPortalTable() || ReturnsToEntrance() || Destination.IsConfigured()) && Transition != nullptr && (UsesLocationMove() ? Transition->CanRequestLocationTransition(Interactor, this) : Transition->CanRequestTransition(Interactor));
}

void ANUWorldPortal::BeginPlay()
{
	Super::BeginPlay();
	// 테이블 재임포트로 활성화 방식이 달라져도 Overlap 경로를 유지한다.
	InteractionVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandlePortalBeginOverlap);
	InteractionVolume->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandlePortalEndOverlap);
}

void ANUWorldPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AutomaticTransitionTimer);
	InteractionVolume->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandlePortalBeginOverlap);
	InteractionVolume->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandlePortalEndOverlap);
	PendingCharacter.Reset();
	SuppressedCharacters.Reset();
	Super::EndPlay(EndPlayReason);
}

ENUWorldPortalActivation ANUWorldPortal::GetActivationMode() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUWorldPortalDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldPortalDataSubsystem>() : nullptr;
	const FNUWorldPortalDefinition* Definition = UsesPortalTable() && Data != nullptr ? Data->FindPortalDefinition(PortalId) : nullptr;
	if (Definition != nullptr)
	{
		return Definition->ActivationMode == ENUWorldPortalActivation::Interact ? ENUWorldPortalActivation::Interaction : Definition->ActivationMode;
	}
	// 테이블 포탈은 양쪽 모두 정의된 활성화 방식을 사용한다.
	return ActivationMode == ENUWorldPortalActivation::Interact ? ENUWorldPortalActivation::Interaction : ActivationMode;
}

bool ANUWorldPortal::UsesPortalTable() const
{
	return !bLocationTransition && PortalId.ToString().IsNumeric();
}

void ANUWorldPortal::HandlePortalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	const APlayerController* Controller = IsValid(Character) ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (GetActivationMode() != ENUWorldPortalActivation::Overlap || !IsValid(Controller) || !Controller->IsLocalController() || OtherComponent != Character->GetCapsuleComponent())
	{
		return;
	}
	const UNUWorldTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>();
	if (Transition != nullptr && Transition->IsTransitionInProgress())
	{
		SuppressUntilExit(Character);
		return;
	}
	if (SuppressedCharacters.Contains(Character))
	{
		return;
	}

	// Overlap 처리 중 재호출을 피하도록 다음 프레임에 이동한다.
	PendingCharacter = Character;
	GetWorldTimerManager().ClearTimer(AutomaticTransitionTimer);
	AutomaticTransitionTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TryAutomaticTransition);
}

void ANUWorldPortal::TryAutomaticTransition()
{
	ACharacter* Character = PendingCharacter.Get();
	PendingCharacter.Reset();
	const APlayerController* Controller = IsValid(Character) ? Cast<APlayerController>(Character->GetController()) : nullptr;
	UNUWorldTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>();

	if (GetActivationMode() == ENUWorldPortalActivation::Overlap && IsValid(Controller) && Controller->IsLocalController() && !SuppressedCharacters.Contains(Character) && InteractionVolume->IsOverlappingComponent(Character->GetCapsuleComponent()) && !PortalId.IsNone() && (UsesPortalTable() || ReturnsToEntrance() || Destination.IsConfigured()) && Transition != nullptr && Transition->CanRequestTransition(Character))
	{
		if (UsesLocationMove()) { Transition->RequestLocationTransition(Character, this); }
		else if (UsesPortalTable()) { Transition->RequestPortalTransition(Character, this); }
		else { Transition->RequestTransition(Character, Destination); }
	}
}

// 도착 직후 자동 재진입을 막고 예약된 이동을 취소한다.
void ANUWorldPortal::SuppressUntilExit(ACharacter* Character)
{
	if (GetActivationMode() == ENUWorldPortalActivation::Overlap && IsValid(Character))
	{
		SuppressedCharacters.Add(Character);
		if (PendingCharacter.Get() == Character)
		{
			PendingCharacter.Reset();
			GetWorldTimerManager().ClearTimer(AutomaticTransitionTimer);
		}
	}
}

void ANUWorldPortal::HandlePortalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (IsValid(Character) && OtherComponent == Character->GetCapsuleComponent())
	{
		SuppressedCharacters.Remove(Character);
		if (PendingCharacter.Get() == Character)
		{
			PendingCharacter.Reset();
		}
	}
}

bool ANUWorldPortal::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !INUInteractableInterface::Execute_CanInteract(this, Interactor))
	{
		return false;
	}

	UNUWorldTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>();
	return Transition != nullptr && (UsesLocationMove() ? Transition->RequestLocationTransition(Interactor, this) : (UsesPortalTable() ? Transition->RequestPortalTransition(Interactor, this) : Transition->RequestTransition(Interactor, Destination)));
}

FName ANUWorldPortal::GetPortalId() const
{
	return PortalId;
}

const FNUWorldDestination& ANUWorldPortal::GetDestination() const
{
	return Destination;
}

UBoxComponent* ANUWorldPortal::GetInteractionVolume() const
{
	return InteractionVolume;
}

#if WITH_EDITOR
EDataValidationResult ANUWorldPortal::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (PortalId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("PortalId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}
	if (!UsesPortalTable() && !ReturnsToEntrance() && !Destination.IsConfigured())
	{
		Context.AddError(FText::FromString(TEXT("Destination.EntryPointId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}
	if (UsesPortalTable())
	{
		const UNUWorldTransitionSettings* Settings = GetDefault<UNUWorldTransitionSettings>();
		const UNUWorldDataSettings* WorldData = GetDefault<UNUWorldDataSettings>();
		UDataTable* Table = Settings->PortalDefinitionTable.LoadSynchronous();
		TArray<FString> Errors;
		if (!UNUWorldPortalDataSubsystem::ValidateTable(Table, WorldData->AreaDefinitionTable.LoadSynchronous(), WorldData->LocationDefinitionTable.LoadSynchronous(), Errors))
		{
			for (const FString& Error : Errors) { Context.AddError(FText::FromString(Error)); }
			return EDataValidationResult::Invalid;
		}
		const FNUWorldPortalDefinition* Row = Table->FindRow<FNUWorldPortalDefinition>(PortalId, TEXT("ValidatePortalActor"), false);
		FNUWorldPortalRoute Route;
		FString Error;
		TArray<FNUWorldPortalEntryBinding> Bindings;
		if (!UNUWorldPortalDataSubsystem::ReadEntryAssets(Settings->PortalEntryAssets, Bindings, Errors))
		{
			for (const FString& EntryError : Errors) { Context.AddError(FText::FromString(EntryError)); }
			return EDataValidationResult::Invalid;
		}
		const FString SourceMap = GetLevel() != nullptr ? GetLevel()->GetOutermost()->GetName() : FString();
		UNUWorldPortalDataSubsystem::ApplyMapVariant(SourceMap, Bindings);
		if (Row == nullptr || !UNUWorldPortalDataSubsystem::ResolveRoute(*Row, PlacementSide == ENUWorldPortalSide::Destination, Bindings, Route, Error))
		{
			Context.AddError(FText::FromString(Row == nullptr ? FString::Printf(TEXT("PortalId=%s: 테이블에 존재하지 않는 ID입니다."), *PortalId.ToString()) : Error));
			return EDataValidationResult::Invalid;
		}
		auto CountEntries = [](const ULevel* Level, FName EntryId)
		{
			int32 Count = 0;
			if (Level != nullptr)
			{
				for (const AActor* Actor : Level->Actors)
				{
					const ANUWorldEntryPoint* Entry = Cast<ANUWorldEntryPoint>(Actor);
					if (IsValid(Entry) && Entry->GetEntryPointId() == EntryId) { ++Count; }
				}
			}
			return Count;
		};
		const FNUWorldDestination& Source = Route.Source.Destination;
		if (WorldData->GetAreaIdForMap(SourceMap) != Route.Source.AreaId || (!Source.Level.IsNull() && Source.Level.ToSoftObjectPath().GetLongPackageName() != SourceMap) || CountEntries(GetLevel(), Source.EntryPointId) != 1)
		{
			Context.AddError(FText::FromString(TEXT("출발 지역/레벨이 배치 설정과 다르거나 출발 EntryPoint가 없거나 중복입니다.")));
			Result = EDataValidationResult::Invalid;
		}
		const FNUWorldDestination& Target = Route.Target.Destination;
		int32 TargetCount = 0;
		if (!Target.Level.IsNull())
		{
			const UWorld* TargetWorld = Target.Level.LoadSynchronous();
			TargetCount = CountEntries(TargetWorld != nullptr ? TargetWorld->PersistentLevel : nullptr, Target.EntryPointId);
			if (WorldData->GetAreaIdForMap(Target.Level.ToSoftObjectPath().GetLongPackageName()) != Route.Target.AreaId)
			{
				Context.AddError(FText::FromString(TEXT("목적 레벨의 지역이 배치 설정과 다릅니다.")));
				Result = EDataValidationResult::Invalid;
			}
		}
		else
		{
			// 기본 레벨로 복귀하는 연결은 해당 지역의 등록된 맵에서 도착 지점을 찾는다.
			for (const auto& MapArea : WorldData->MapAreaIds)
			{
				if (MapArea.Value != Route.Target.AreaId) { continue; }
				const UWorld* TargetWorld = MapArea.Key.LoadSynchronous();
				TargetCount += CountEntries(TargetWorld != nullptr ? TargetWorld->PersistentLevel : nullptr, Target.EntryPointId);
			}
		}
		if (TargetCount != 1)
		{
			Context.AddError(FText::FromString(TEXT("목적 EntryPoint가 없거나 중복입니다. 목적 레벨 경로와 도착 지점 ID를 확인하십시오.")));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif
