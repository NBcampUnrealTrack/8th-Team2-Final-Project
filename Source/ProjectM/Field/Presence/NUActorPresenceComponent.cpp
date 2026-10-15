#include "Field/Presence/NUActorPresenceComponent.h"

#include "Field/Presence/NUActorPresenceTransitionSubsystem.h"

#include "CharacterState/NUPlayerState.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Net/NUNaiaNetSubsystem.h"
#include "Quest/Data/NUQuestTutorialTags.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Component/NUQuestMarkerComponent.h"
#include "Quest/Component/NUQuestProviderComponent.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Field/Presence/NUActorPresenceRow.h"
#include "Quest/Data/NUQuestTypes.h"
#include "Field/Presence/NUActorPresenceSpot.h"
#include "Field/NPC/NUNPCBase.h"
#include "Quest/World/NUApproachTrigger.h"
#include "Quest/World/NUQuestNaiaActor.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "PaperFlipbookActor.h"
#include "TimerManager.h"

UNUActorPresenceComponent::UNUActorPresenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UNUActorPresenceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (PresenceId.IsNone() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	if (QuestData == nullptr || QuestData->GetActorPresenceSteps(PresenceId).IsEmpty())
	{
		UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: 등장 표에 단계가 없습니다 [%s] PresenceId=%s"), *GetNameSafe(GetOwner()), *PresenceId.ToString());
		return;
	}

	InitialTransform = GetOwner()->GetActorTransform();
	bInitialCollisionEnabled = GetOwner()->GetActorEnableCollision();
	if (const UNUQuestProviderComponent* Provider = GetOwner()->FindComponentByClass<UNUQuestProviderComponent>())
	{
		InitialQuestActorId = Provider->QuestActorId;
		InitialObjectiveType = Provider->ObjectiveType;
	}

	// 조건 없는 행만 쓰는 액터는 퀘스트 진행을 기다릴 필요가 없다. 시작 계약 조건이 있으면 로스터도 기다린다.
	bProgressKnown = true;
	for (const FName RowName : QuestData->GetActorPresenceSteps(PresenceId))
	{
		const FNUActorPresenceRow* Row = QuestData->FindActorPresence(RowName);
		bNeedsStarterContract |= Row->ContractedStarterNaiaId > 0;
		for (const FNUQuestStateCondition& Condition : Row->Conditions)
		{
			if (Condition.IsSet()) { bProgressKnown = false; }
		}
	}
	bProgressKnown &= !bNeedsStarterContract;
	BindApproachTriggers();
	RefreshPresence();
	BindQuest();
}

void UNUActorPresenceComponent::BindApproachTriggers()
{
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	TSet<FName> ApproachIds;
	for (const FName RowName : QuestData->GetActorPresenceSteps(PresenceId))
	{
		const FNUActorPresenceRow* Row = QuestData->FindActorPresence(RowName);
		if (Row != nullptr && !Row->ApproachActorId.IsNone()) { ApproachIds.Add(Row->ApproachActorId); }
	}

	for (const FName ApproachId : ApproachIds)
	{
		ANUApproachTrigger* Trigger = ANUApproachTrigger::Find(GetWorld(), ApproachId);
		if (Trigger == nullptr)
		{
			// 트리거가 없으면 그 행은 끝내 참이 되지 않는다.
			UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: 레벨에 접근 트리거가 없습니다 [%s] PresenceId=%s ApproachActorId=%s"), *GetNameSafe(GetOwner()), *PresenceId.ToString(), *ApproachId.ToString());
			continue;
		}
		ApproachBindings.Emplace(Trigger, Trigger->OnLocalPlayerEntered.AddUObject(this, &ThisClass::HandleApproachEntered));
	}
}

void UNUActorPresenceComponent::HandleApproachEntered()
{
	RefreshPresence();
}

bool UNUActorPresenceComponent::IsApproachMet(FName RowName, const FNUActorPresenceRow& Row)
{
	if (Row.ApproachActorId.IsNone() || ApproachedRows.Contains(RowName))
	{
		return true;
	}

	// 조건이 참인 동안 범위 안에 있어야 기억한다. 조건이 참이 되기 전에 지나간 일은 세지 않는다.
	const ANUApproachTrigger* Trigger = ANUApproachTrigger::Find(GetWorld(), Row.ApproachActorId);
	if (Trigger == nullptr || !Trigger->IsLocalPlayerInside())
	{
		return false;
	}
	ApproachedRows.Add(RowName);
	return true;
}

void UNUActorPresenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingPlacement();
	GetWorld()->GetTimerManager().ClearTimer(BindTimer);
	for (const auto& Binding : ApproachBindings)
	{
		if (ANUApproachTrigger* Trigger = Binding.Key.Get()) { Trigger->OnLocalPlayerEntered.Remove(Binding.Value); }
	}
	ApproachBindings.Reset();
	if (BoundQuest.IsValid())
	{
		BoundQuest->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::RefreshPresence);
	}
	if (BoundRoster.IsValid())
	{
		BoundRoster->OnRosterChanged.RemoveDynamic(this, &ThisClass::RefreshPresence);
	}

	Super::EndPlay(EndPlayReason);
}

void UNUActorPresenceComponent::BindQuest()
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const ANUPlayerState* PlayerState = PlayerController != nullptr ? PlayerController->GetPlayerState<ANUPlayerState>() : nullptr;
	UNUQuestComponent* Quest = PlayerState != nullptr ? PlayerState->GetQuestComponent() : nullptr;
	UNUNaiaRosterComponent* Roster = PlayerState != nullptr ? PlayerState->GetNaiaRosterComponent() : nullptr;
	// 퀘스트 컴포넌트가 BeginPlay를 마쳐야 서버 세션인지 알 수 있고, 로컬 세션은 그 뒤로 준비 알림이 따로 오지 않는다.
	if (Quest == nullptr || !Quest->HasBegunPlay() || (bNeedsStarterContract && Roster == nullptr))
	{
		GetWorld()->GetTimerManager().SetTimer(BindTimer, this, &ThisClass::BindQuest, 0.25f, false);
		return;
	}

	BoundQuest = Quest;
	Quest->OnQuestProgressChanged.AddUniqueDynamic(this, &ThisClass::RefreshPresence);
	if (bNeedsStarterContract)
	{
		BoundRoster = Roster;
		Roster->OnRosterChanged.AddUniqueDynamic(this, &ThisClass::RefreshPresence);
	}
	RefreshPresence();
}

void UNUActorPresenceComponent::RefreshPresence()
{
	const UNUQuestDataSubsystem* QuestData = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	if (QuestData == nullptr)
	{
		return;
	}

	const UNUQuestComponent* Quest = BoundQuest.Get();
	const UNUNaiaRosterComponent* Roster = BoundRoster.Get();
	// 진행을 한 번 받은 뒤에는 연결이 잠시 끊겨도 마지막 진행으로 보여준다.
	bProgressKnown |= Quest != nullptr && Quest->IsQuestProgressReady() && (!bNeedsStarterContract || IsStarterContractKnown());
	if (!bProgressKnown)
	{
		ApplyStep(NAME_None, nullptr);
		return;
	}

	// StepOrder 오름차순으로 검증된 행 중 마지막 일치 행을 적용한다.
	const int32 StarterNaiaId = Roster != nullptr && Roster->HasSelectedStarterNaia() ? Roster->GetSelectedStarterNaiaId() : 0;
	FName ChosenRowName = NAME_None;
	const FNUActorPresenceRow* ChosenStep = nullptr;
	FNUQuestProgressState AvailableProgress;
	for (const FName RowName : QuestData->GetActorPresenceSteps(PresenceId))
	{
		const FNUActorPresenceRow* Step = QuestData->FindActorPresence(RowName);
		if (Step != nullptr && Step->IsMet(
			[Quest, QuestData, &AvailableProgress](FName QuestId) -> const FNUQuestProgressState*
			{
				if (Quest == nullptr) { return nullptr; }
				if (const FNUQuestProgressState* Progress = Quest->FindQuestProgress(QuestId)) { return Progress; }
				// 아직 제안받지 않은 의뢰도 NPC와 같은 조건으로 표시한다. 실제 수락 상태는 추가하지 않는다.
				if (!Quest->AreStartRequirementsMet(QuestData->FindDefinition(QuestId))) { return nullptr; }
				AvailableProgress.QuestId = QuestId;
				AvailableProgress.QuestState = ENUQuestState::Available;
				return &AvailableProgress;
			},
			[QuestData](FName QuestId) { return QuestData->FindDefinition(QuestId); },
			StarterNaiaId) && IsApproachMet(RowName, *Step))
		{
			ChosenRowName = RowName;
			ChosenStep = Step;
		}
	}

	const int32 DebugSerial = Quest != nullptr ? Quest->GetDebugCompletionSerial() : 0;
	if (bApplied && ChosenRowName == CurrentRowName && AppliedDebugSerial == DebugSerial)
	{
		CancelPendingPlacement();
		return;
	}
	ApplyStep(ChosenRowName, ChosenStep);
}

void UNUActorPresenceComponent::RefreshPresenceImmediately()
{
	if (PresenceId.IsNone() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	CancelPendingPlacement();
	bApplied = false;
	bHasResolvedStep = false;
	RefreshPresence();
}

void UNUActorPresenceComponent::ApplyStep(FName RowName, const FNUActorPresenceRow* Step)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	const int32 DebugSerial = BoundQuest.IsValid() ? BoundQuest->GetDebugCompletionSerial() : 0;
	if (DebugSerial != AppliedDebugSerial)
	{
		LastDebugCompletionFrame = GFrameCounter;
	}
	// 하나의 디버그 완료가 같은 프레임에 여러 목표를 채워도 모든 단계는 즉시 복원한다.
	const bool bTransition = !bApplyingPlacement && bHasResolvedStep && LastDebugCompletionFrame != GFrameCounter;
	AppliedDebugSerial = DebugSerial;
	bHasResolvedStep |= bProgressKnown;

	ANUActorPresenceSpot* Spot = Step != nullptr && !Step->SpotId.IsNone() ? FindSpot(Step->SpotId) : nullptr;
	if (Step != nullptr && !Step->SpotId.IsNone() && Spot == nullptr)
	{
		UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: 레벨에 자리가 없습니다 [%s] Step=%s SpotId=%s"), *GetNameSafe(Owner), *RowName.ToString(), *Step->SpotId.ToString());
	}

	const bool bVisible = Step != nullptr && Step->bVisible && (Step->SpotId.IsNone() || Spot != nullptr);
	const UNUQuestProviderComponent* PreviousProvider = Owner->FindComponentByClass<UNUQuestProviderComponent>();
	const bool bPracticeNaia = Owner->IsA<ANUQuestNaiaActor>() && PreviousProvider != nullptr
		&& PreviousProvider->ObjectiveType == NU::Quest::Objective::TutorialAction;
	// 연습을 마친 나이아는 즉시 퇴장한다. 다른 NPC 퇴장은 기존 암전 규칙을 유지한다.
	if (!bVisible && bPresent && bTransition
		&& !bPracticeNaia
		&& (Owner->IsA<ANUNPCBase>() || Owner->IsA<ANUQuestNaiaActor>() || Owner->IsA<APaperFlipbookActor>()))
	{
		PendingRowName = RowName;
		bPlacementPending = true;
		GetWorld()->GetSubsystem<UNUActorPresenceTransitionSubsystem>()->Enqueue(this);
		return;
	}
	if (bVisible)
	{
		const FVector Location = Spot != nullptr ? Spot->GetActorLocation() : InitialTransform.GetLocation();
		const FQuat Rotation = Spot != nullptr ? Spot->GetActorQuat() : InitialTransform.GetRotation();
		// 이미 그 자리에 있으면 옮기지 않는다. Static 액터(격자문 같은 소품)는 같은 자리로 옮기려 해도 엔진이 경고한다.
		const bool bAtTarget = Owner->GetActorLocation().Equals(Location, 0.5) && Owner->GetActorQuat().Equals(Rotation, 1.e-4);
		if (!bAtTarget)
		{
			if (Owner->IsRootComponentMovable())
			{
				// 숨겨 둔 나이아는 새 자리에서 바로 등장하므로 이동을 가릴 필요가 없다.
				if (bTransition && (bPresent || !Owner->IsA<ANUQuestNaiaActor>()))
				{
					PendingRowName = RowName;
					bPlacementPending = true;
					GetWorld()->GetSubsystem<UNUActorPresenceTransitionSubsystem>()->Enqueue(this);
					return;
				}
				Owner->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			}
			else
			{
				UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: 자리를 옮기려면 루트 컴포넌트가 Movable이어야 합니다 [%s] Step=%s SpotId=%s"), *GetNameSafe(Owner), *RowName.ToString(), *Step->SpotId.ToString());
			}
		}
	}
	else if (Spot != nullptr && Owner->IsRootComponentMovable())
	{
		// 숨는 행의 자리는 숨긴 채로 옮겨 둔다. 보이지 않으므로 바로 옮기고, 나중에 그 자리에서 나타나게 한다.
		Owner->SetActorLocationAndRotation(Spot->GetActorLocation(), Spot->GetActorQuat(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	CancelPendingPlacement();
	// 숨은 액터는 충돌도 꺼서 상호작용과 겹침 판정에서 빠진다.
	Owner->SetActorHiddenInGame(!bVisible);
	Owner->SetActorEnableCollision(bVisible && bInitialCollisionEnabled);
	bPresent = bVisible;

	// 퀘스트 역할을 바꾸지 않는 행은 기존 제공자와 일반 상호작용을 그대로 둔다.
	const FNUActorPresenceRow* AppliedStep = bVisible ? Step : nullptr;
	const bool bOverrideRole = AppliedStep != nullptr && AppliedStep->bOverrideQuestInteraction;
	if (UNUQuestProviderComponent* Provider = Owner->FindComponentByClass<UNUQuestProviderComponent>())
	{
		if (bOverrideRole)
		{
			Provider->QuestActorId = AppliedStep->QuestActorId > 0 ? FName(*LexToString(AppliedStep->QuestActorId)) : NAME_None;
			Provider->ObjectiveType = AppliedStep->ObjectiveType;
		}
		else if (bQuestRoleOverridden)
		{
			Provider->QuestActorId = InitialQuestActorId;
			Provider->ObjectiveType = InitialObjectiveType;
		}
	}
	bQuestRoleOverridden = bOverrideRole;

	const bool bChanged = !bApplied || CurrentRowName != RowName;
	CurrentRowName = RowName;
	bApplied = true;
	if (bChanged && bProgressKnown)
	{
		UE_LOG(LogNUQuest, Log, TEXT("ActorPresence: %s %s -> %s (%s)"), *PresenceId.ToString(), *GetNameSafe(Owner),
			bVisible ? (Step->SpotId.IsNone() ? TEXT("원래 자리") : *Step->SpotId.ToString()) : TEXT("숨김"), RowName.IsNone() ? TEXT("참인 단계 없음") : *RowName.ToString());
	}

	OnPresenceChanged.Broadcast(AppliedStep);

	// 같은 진행 변경 알림에서 마커가 먼저 갱신됐을 수 있으므로 바뀐 대상으로 다시 그린다.
	if (UNUQuestMarkerComponent* Marker = Owner->FindComponentByClass<UNUQuestMarkerComponent>())
	{
		Marker->RefreshMarker();
	}
}

void UNUActorPresenceComponent::CancelPendingPlacement()
{
	if (UNUActorPresenceTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UNUActorPresenceTransitionSubsystem>())
	{
		Transition->Cancel(this);
	}
	bPlacementPending = false;
	PendingRowName = NAME_None;
}

void UNUActorPresenceComponent::ApplyPendingPlacement()
{
	if (!bPlacementPending) { return; }
	const UNUQuestDataSubsystem* Data = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	const FName RowName = PendingRowName;
	TGuardValue<bool> Guard(bApplyingPlacement, true);
	ApplyStep(RowName, Data != nullptr ? Data->FindActorPresence(RowName) : nullptr);
}

bool UNUActorPresenceComponent::IsPlacementVisibleTo(const APlayerController& Controller) const
{
	int32 Width = 0, Height = 0;
	Controller.GetViewportSize(Width, Height);
	const auto IsVisible = [&](const FVector& Location)
	{
		FVector2D Screen;
		return Controller.ProjectWorldLocationToScreen(Location + FVector(0, 0, 50), Screen, true)
			&& Screen.X >= -100 && Screen.Y >= -100 && Screen.X <= Width + 100 && Screen.Y <= Height + 100;
	};
	if (!GetOwner()->IsHidden() && IsVisible(GetOwner()->GetActorLocation())) { return true; }
	const UNUQuestDataSubsystem* Data = GetWorld()->GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	const FNUActorPresenceRow* Step = Data != nullptr ? Data->FindActorPresence(PendingRowName) : nullptr;
	// 접근해서 등장시키는 행은 도착지가 보이는 것이 의도다. 원래 자리가 화면 밖이면 바로 등장시킨다.
	if (Step != nullptr && !Step->ApproachActorId.IsNone()) { return false; }
	const ANUActorPresenceSpot* Spot = Step != nullptr ? FindSpot(Step->SpotId) : nullptr;
	return Step != nullptr && Step->bVisible && Spot != nullptr && IsVisible(Spot->GetActorLocation());
}

bool UNUActorPresenceComponent::IsStarterContractKnown() const
{
	// 로컬 실행은 로스터 컴포넌트가 원본이다. 서버 세션은 목록을 받기 전까지 미계약(0)으로 보이므로 믿지 않는다.
	const UNUNaiaNetSubsystem* NaiaNet = GetWorld()->GetGameInstance()->GetSubsystem<UNUNaiaNetSubsystem>();
	return BoundRoster.IsValid() && (NaiaNet == nullptr || !NaiaNet->HasActiveServerSession() || NaiaNet->HasReceivedRoster());
}

ANUActorPresenceSpot* UNUActorPresenceComponent::FindSpot(FName SpotId) const
{
	ANUActorPresenceSpot* Found = nullptr;
	for (TActorIterator<ANUActorPresenceSpot> It(GetWorld()); It; ++It)
	{
		if (It->GetSpotId() != SpotId)
		{
			continue;
		}
		if (Found != nullptr)
		{
			UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: 같은 SpotId가 여러 곳에 있습니다 SpotId=%s (%s, %s)"), *SpotId.ToString(), *Found->GetName(), *It->GetName());
			return nullptr;
		}
		Found = *It;
	}
	return Found;
}
