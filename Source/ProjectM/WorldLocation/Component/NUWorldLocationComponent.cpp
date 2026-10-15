#include "WorldLocation/Component/NUWorldLocationComponent.h"

#include "WorldLocation/Data/NUWorldDataSubsystem.h"
#include "WorldLocation/Data/NUWorldDataSettings.h"
#include "WorldLocation/World/NULocationTrigger.h"
#include "WorldTransition/World/NUWorldTransitionSubsystem.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"
#include "Quest/Component/NUQuestComponent.h"

#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

UNUWorldLocationComponent::UNUWorldLocationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UNUWorldLocationComponent::IsLocationUnlocked(int32 LocationId) const
{
	const UGameInstance* Instance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUWorldDataSubsystem* Data = Instance != nullptr ? Instance->GetSubsystem<UNUWorldDataSubsystem>() : nullptr;
	const UNUQuestComponent* Quest = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UNUQuestComponent>() : nullptr;
	const FNULocationDefinition* Location = Data != nullptr ? Data->FindLocationDefinition(LocationId) : nullptr;

	if (Location == nullptr)
	{
		return false;
	}

	TArray<FName> RequiredQuests;

	if (Location->UnlockQuestId > 0)
	{
		RequiredQuests.AddUnique(FName(*LexToString(Location->UnlockQuestId)));
	}

	// 장소 자체와 소속 지역 계층의 해금 퀘스트를 함께 확인한다.
	TSet<int32> VisitedAreas;
	int32 AreaId = Location->AreaId;

	while (AreaId > 0)
	{
		const FNUAreaDefinition* Area = Data->FindAreaDefinition(AreaId);

		if (Area == nullptr || VisitedAreas.Contains(AreaId))
		{
			return false;
		}

		VisitedAreas.Add(AreaId);

		if (Area->UnlockQuestId > 0)
		{
			RequiredQuests.AddUnique(FName(*LexToString(Area->UnlockQuestId)));
		}

		AreaId = Area->ParentAreaId;
	}

	return RequiredQuests.IsEmpty() || (Quest != nullptr && Quest->HasCompletedQuests(RequiredQuests));
}

void UNUWorldLocationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerState* PlayerState = Cast<APlayerState>(GetOwner()))
	{
		PlayerState->OnPawnSet.AddUniqueDynamic(this, &ThisClass::HandlePawnSet);
		BindPawn(PlayerState->GetPawn());
	}
}

void UNUWorldLocationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerState* PlayerState = Cast<APlayerState>(GetOwner()))
	{
		PlayerState->OnPawnSet.RemoveDynamic(this, &ThisClass::HandlePawnSet);
	}

	BindPawn(nullptr);
	Super::EndPlay(EndPlayReason);
}

int32 UNUWorldLocationComponent::GetCurrentLocationId() const
{
	return CurrentLocationId;
}

int32 UNUWorldLocationComponent::GetLastKnownAreaId() const
{
	return LastKnownAreaId;
}

bool UNUWorldLocationComponent::ShouldShowEntryNotice() const
{
	return bShowEntryNotice;
}

bool UNUWorldLocationComponent::HasTrackedPawn() const
{
	return BoundPawn.IsValid() && BoundPawnRoot.IsValid();
}

const TArray<FName>& UNUWorldLocationComponent::GetOverlappingLocationIds() const
{
	return OverlappingLocationIds;
}

void UNUWorldLocationComponent::HandlePawnSet(APlayerState* PlayerState, APawn* NewPawn, APawn* OldPawn)
{
	BindPawn(NewPawn);
}

void UNUWorldLocationComponent::BindPawn(APawn* Pawn)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocationRefreshTimer);
	}

	bLocationRefreshPending = false;

	if (BoundPawnRoot.IsValid())
	{
		BoundPawnRoot->TransformUpdated.RemoveAll(this);
		BoundPawnRoot->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandlePawnBeginOverlap);
		BoundPawnRoot->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandlePawnEndOverlap);
	}

	BoundPawn = Pawn;
	BoundPawnRoot = IsValid(Pawn) ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	CurrentLocationId = 0;
	LastKnownAreaId = 0;
	bShowEntryNotice = false;
	OverlappingLocationIds.Reset();

	if (BoundPawnRoot.IsValid())
	{
		BoundPawnRoot->TransformUpdated.AddUObject(this, &ThisClass::HandlePawnTransformUpdated);
		const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
		const UNUWorldDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldDataSubsystem>() : nullptr;
		const int32 MapAreaId = GetDefault<UNUWorldDataSettings>()->GetAreaIdForWorld(GetWorld());
		LastKnownAreaId = Data != nullptr && Data->FindAreaDefinition(MapAreaId) != nullptr ? MapAreaId : 0;
		BoundPawnRoot->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandlePawnBeginOverlap);
		BoundPawnRoot->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandlePawnEndOverlap);
		ScheduleLocationRefresh();
	}

	OnLocationChanged.Broadcast();
	OnLocationOverlapsChanged.Broadcast();
}

void UNUWorldLocationComponent::HandlePawnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const ANULocationTrigger* Trigger = Cast<ANULocationTrigger>(OtherActor);

	if (Trigger != nullptr && OtherComponent == Trigger->GetTriggerBounds())
	{
		ScheduleLocationRefresh();
	}
}

void UNUWorldLocationComponent::HandlePawnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	const ANULocationTrigger* Trigger = Cast<ANULocationTrigger>(OtherActor);

	if (Trigger != nullptr && OtherComponent == Trigger->GetTriggerBounds())
	{
		ScheduleLocationRefresh();
	}
}

void UNUWorldLocationComponent::HandlePawnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	// 순간이동은 같은 범위 안에서도 발생한다. 이동과 겹침 갱신이 끝난 다음 프레임에 판정한다.

	if (Teleport != ETeleportType::None)
	{
		ScheduleLocationRefresh();
	}
}

void UNUWorldLocationComponent::ScheduleLocationRefresh()
{
	UWorld* World = GetWorld();

	if (World != nullptr && !bLocationRefreshPending)
	{
		bLocationRefreshPending = true;
		LocationRefreshTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshLocation);
	}
}

void UNUWorldLocationComponent::RefreshLocation()
{
	UWorld* World = GetWorld();

	if (World == nullptr || !HasTrackedPawn())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(LocationRefreshTimer);
	bLocationRefreshPending = false;
	const UGameInstance* GameInstance = World->GetGameInstance();
	const UNUWorldDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUWorldDataSubsystem>() : nullptr;
	const UNUWorldTransitionSubsystem* Transition = World->GetSubsystem<UNUWorldTransitionSubsystem>();

	// 전환 중에는 장소 판정을 보류한다.

	if (Transition != nullptr && Transition->IsTransitionInProgress())
	{
		return;
	}

	// 현재 활성 맵을 기준으로 지역을 판정한다.
	const FString ActiveMap = Transition != nullptr ? Transition->GetActiveMapPackageName(BoundPawn.Get()) : UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	const int32 TransitionAreaId = Transition != nullptr ? Transition->GetActiveAreaId(BoundPawn.Get()) : 0;
	const int32 ConfiguredAreaId = TransitionAreaId > 0 ? TransitionAreaId : GetDefault<UNUWorldDataSettings>()->GetAreaIdForMap(ActiveMap);
	const int32 MapAreaId = Data != nullptr && Data->FindAreaDefinition(ConfiguredAreaId) != nullptr ? ConfiguredAreaId : 0;
	const UNUInteriorParticipantComponent* Participant = BoundPawn->FindComponentByClass<UNUInteriorParticipantComponent>();
	const UNUWalkableInteriorComponent* Interior = Participant ? Participant->GetCurrentInterior() : nullptr;

	TArray<AActor*> Actors;

	// 실내는 출입에 성공한 방을 사용한다. 벽 접촉이나 실외의 겹친 범위로 장소를 바꾸지 않는다.

	if (Interior == nullptr)
	{
		BoundPawnRoot->GetOverlappingActors(Actors, ANULocationTrigger::StaticClass());
	}

	TArray<FName> NewLocationIds;
	const FNULocationDefinition* SelectedLocation = Interior && Data ? Data->FindLocationDefinition(Interior->LocationId) : nullptr;
	int32 SelectedPriority = MIN_int32;

	if (SelectedLocation != nullptr)
	{
		NewLocationIds.AddUnique(FName(*LexToString(SelectedLocation->LocationId)));
	}

	bool bSelectedNotice = SelectedLocation && Interior && Interior->bShowLocationEntryNotice;

	for (AActor* Actor : Actors)
	{
		const ANULocationTrigger* Trigger = Cast<ANULocationTrigger>(Actor);

		if (!IsValid(Trigger) || Trigger->GetLocationId().IsNone() || !BoundPawnRoot->IsOverlappingComponent(Trigger->GetTriggerBounds()))
		{
			continue;
		}

		// 퀘스트는 실제 겹친 범위의 숫자 ID와 예전 문자열 ID를 모두 사용할 수 있다.
		NewLocationIds.AddUnique(Trigger->GetLocationId());
		int32 LocationId = 0;
		const FString Name = Trigger->GetLocationId().ToString();

		if (Data == nullptr || !Data->IsDataReady() || !LexTryParseString(LocationId, *Name) || LocationId <= 0 || Name != LexToString(LocationId))
		{
			continue;
		}

		const FNULocationDefinition* Location = Data->FindLocationDefinition(LocationId);

		if (Location == nullptr)
		{
			continue;
		}

		const int32 Priority = Trigger->GetLocationPriority();
		const bool bSameRank = Priority == SelectedPriority;
		// 우선순위가 같으면 현재 장소를 유지하고, 나머지는 ID로 순서를 고정한다.
		const bool bPreferCurrent = bSameRank && LocationId == CurrentLocationId;
		const bool bStableTie = bSameRank && SelectedLocation != nullptr && SelectedLocation->LocationId != CurrentLocationId && LocationId < SelectedLocation->LocationId;

		if (SelectedLocation == nullptr || Priority > SelectedPriority || bPreferCurrent || bStableTie)
		{
			SelectedLocation = Location;
			SelectedPriority = Priority;
			bSelectedNotice = Trigger->ShouldShowEntryNotice();
		}
	}

	// 겹침 반환 순서가 달라도 같은 장소 목록으로 비교한다.
	NewLocationIds.Sort(FNameLexicalLess());
	const bool bOverlapsChanged = OverlappingLocationIds != NewLocationIds;
	OverlappingLocationIds = MoveTemp(NewLocationIds);
	const int32 SelectedId = SelectedLocation != nullptr ? SelectedLocation->LocationId : 0;
	const int32 AreaId = MapAreaId > 0 ? MapAreaId : (SelectedLocation != nullptr ? SelectedLocation->AreaId : LastKnownAreaId);
	const bool bLocationChanged = SelectedId != CurrentLocationId || AreaId != LastKnownAreaId;
	CurrentLocationId = SelectedId;
	bShowEntryNotice = bSelectedNotice;
	LastKnownAreaId = AreaId;

	if (bLocationChanged)
	{
		OnLocationChanged.Broadcast();
	}

	if (bOverlapsChanged)
	{
		OnLocationOverlapsChanged.Broadcast();
	}
}
