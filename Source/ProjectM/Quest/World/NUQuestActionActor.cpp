#include "Quest/World/NUQuestActionActor.h"

#include "Quest/Encounter/NUQuestEncounterSubsystem.h"
#include "Quest/World/NUQuestNaiaActor.h"
#include "CharacterState/NUPlayerState.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "PlayerController/NUPlayerController.h"
#include "NUGAS/NUGameplayTags.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Component/NUQuestMarkerComponent.h"
#include "Quest/Component/NUQuestProviderComponent.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Field/Presence/NUActorPresenceRow.h"
#include "Quest/UI/NUQuestAgitationWidget.h"
#include "Field/Presence/NUActorPresenceComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "PaperFlipbookComponent.h"
#include "PaperSpriteComponent.h"
#include "UObject/ConstructorHelpers.h"

ANUQuestActionActor::ANUQuestActionActor()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(65.f);
	InteractionVolume->SetCollisionProfileName(TEXT("NU_Interactable"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(InteractionVolume);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Shape.Object);
	Mesh->SetRelativeScale3D(FVector(0.4f));

	StateSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("StateSprite"));
	StateSprite->SetupAttachment(InteractionVolume);
	StateSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StateSprite->SetGenerateOverlapEvents(false);
	StateSprite->SetVisibility(false);
	StateSprite->SetCastShadow(false);

	QuestProvider = CreateDefaultSubobject<UNUQuestProviderComponent>(TEXT("QuestProvider"));
	QuestProvider->ObjectiveType = NU::Quest::Objective::UseObject;

	QuestMarker = CreateDefaultSubobject<UNUQuestMarkerComponent>(TEXT("QuestMarker"));
	QuestMarker->SetupAttachment(InteractionVolume);
	QuestMarker->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
}

UNUQuestComponent* ANUQuestActionActor::FindQuest(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const ANUPlayerState* State = Pawn != nullptr ? Pawn->GetPlayerState<ANUPlayerState>() : nullptr;
	return State != nullptr ? State->GetQuestComponent() : nullptr;
}

// 진행 중인 목표와 기획에 지정된 선행 조건을 함께 확인한다.
bool ANUQuestActionActor::CanInteract_Implementation(AActor* Interactor) const
{
	if (IsHidden()) { return false; }
	if (bMoveStoneDuringBattle)
	{
		return false; // 전투 메뉴의 돌 옮기기 행동으로만 실행한다.
	}

	const UNUQuestComponent* Quest = FindQuest(Interactor);
	const UNUQuestDataSubsystem* Data = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
	const UNUQuestDefinition* Definition = Data != nullptr ? Data->FindDefinition(QuestId) : nullptr;
	const FNUQuestProgressState* Progress = Quest != nullptr ? Quest->FindQuestProgress(QuestId) : nullptr;
	if (Definition == nullptr || Progress == nullptr || Progress->QuestState != ENUQuestState::Active || GetWorld()->GetTimeSeconds() < NextInteractionTime) { return false; }

	const FNUQuestObjectiveSpec* Objective = Definition->Objectives.FindByPredicate([this](const FNUQuestObjectiveSpec& Item) { return Item.ObjectiveId == ObjectiveId; });
	const FNUQuestObjectiveProgress* ObjectiveProgress = Progress->FindObjectiveProgress(ObjectiveId);
	if (Objective == nullptr || ObjectiveProgress == nullptr || ObjectiveProgress->IsComplete(Objective->RequiredCount)) { return false; }
	const UNUQuestBattleDefinition* Battle = Cast<UNUQuestBattleDefinition>(EncounterRequest.BattleDefinition);
	if (Battle != nullptr && Battle->Rule == ENUQuestCalmingRule::Grass && Battle->QuestId == QuestId && Battle->ObjectiveId == ObjectiveId
		&& Battle->GetCompletedGrassRecoveries(*Definition, *Progress) < Battle->GrassRecoveryObjectiveIds.Num())
	{
		return !Battle->GrassRecoveryDialogue.IsNull();
	}

	for (const FName RequiredId : RequiredObjectiveIds)
	{
		const FNUQuestObjectiveSpec* Required = Definition->Objectives.FindByPredicate([RequiredId](const FNUQuestObjectiveSpec& Item) { return Item.ObjectiveId == RequiredId; });
		const FNUQuestObjectiveProgress* Completed = Progress->FindObjectiveProgress(RequiredId);
		if (Required == nullptr || Completed == nullptr || !Completed->IsComplete(Required->RequiredCount)) { return false; }
	}

	return Progress->IsObjectivePrerequisiteMet(*Definition, *Objective);
}

// 일반 작업은 제공자에, 특수 전투 진입은 퀘스트 인카운터에 위임한다.
bool ANUQuestActionActor::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor)) { return false; }
	if (EncounterRequest.BattleDefinition != nullptr)
	{
		APawn* Pawn = Cast<APawn>(Interactor);
		ANUPlayerState* Player = Pawn != nullptr ? Pawn->GetPlayerState<ANUPlayerState>() : nullptr;
		const UNUQuestComponent* Quest = FindQuest(Interactor);
		const UNUQuestBattleDefinition* Battle = Cast<UNUQuestBattleDefinition>(EncounterRequest.BattleDefinition);
		UNUQuestEncounterSubsystem* Encounters = GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>();
		if (Player == nullptr || Quest == nullptr || Battle == nullptr || Encounters == nullptr || Battle->QuestId != QuestId || Battle->ObjectiveId != ObjectiveId) { return false; }
		if (Battle->Rule == ENUQuestCalmingRule::Grass)
		{
			const UNUQuestDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
			const UNUQuestDefinition* Definition = Data != nullptr ? Data->FindDefinition(QuestId) : nullptr;
			const FNUQuestProgressState* Progress = Quest->FindQuestProgress(QuestId);
			if (Definition != nullptr && Progress != nullptr && Battle->GetCompletedGrassRecoveries(*Definition, *Progress) < Battle->GrassRecoveryObjectiveIds.Num())
			{
				ANUPlayerController* PC = Cast<ANUPlayerController>(Pawn->GetController());
				UNUDialogueDefinition* Dialogue = Battle->GrassRecoveryDialogue.LoadSynchronous();
				if (PC == nullptr || Dialogue == nullptr) { return false; }
				PC->ClientOpenStandaloneDialogue(Dialogue);
				NextInteractionTime = GetWorld()->GetTimeSeconds() + InteractionCooldown;
				return true;
			}
		}

		FNUBattleEncounterRequest Request = EncounterRequest;
		Request.Source = ENUBattleEncounterSource::Symbol;
		Request.SourceActor = this;
		const bool bStarted = Encounters->TryStartQuestEncounter(Player, Request, Quest->IsServerBackedSession());
		if (bStarted) { NextInteractionTime = GetWorld()->GetTimeSeconds() + InteractionCooldown; }
		return bStarted;
	}

	if (!OfferQuestOnUse.IsNone())
	{
		UNUQuestComponent* Quest = FindQuest(Interactor);
		if (Quest == nullptr) { return false; }
		TWeakObjectPtr<AActor> WeakInteractor(Interactor);
		Quest->OfferQuestAfterObjective(OfferQuestOnUse, FSimpleDelegate::CreateWeakLambda(this, [this, WeakInteractor]()
		{
			if (WeakInteractor.IsValid()) { QuestProvider->HandleInteraction(WeakInteractor.Get()); }
		}));
		NextInteractionTime = GetWorld()->GetTimeSeconds() + InteractionCooldown;
		return true;
	}

	const bool bChanged = QuestProvider->HandleInteraction(Interactor);
	if (bChanged) { NextInteractionTime = GetWorld()->GetTimeSeconds() + InteractionCooldown; }
	return bChanged;
}

void ANUQuestActionActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	StateSprite->SetSprite(IncompleteSprite);
	StateSprite->SetRelativeLocation(StateSpriteOffset);
	StateSprite->SetVisibility(IncompleteSprite != nullptr);
}

void ANUQuestActionActor::BeginPlay()
{
	InitialEncounterRequest = EncounterRequest;
	InitialQuestId = QuestId;
	InitialObjectiveId = ObjectiveId;
	InitialRequiredObjectiveIds = RequiredObjectiveIds;
	// 등장 컴포넌트는 Super::BeginPlay 안에서 첫 단계를 알리므로 그 전에 구독한다.
	if (UNUActorPresenceComponent* Presence = FindComponentByClass<UNUActorPresenceComponent>())
	{
		Presence->OnPresenceChanged.AddUObject(this, &ThisClass::HandleActorPresence);
	}

	Super::BeginPlay();
	// 온라인 진행 스냅샷이 오기 전에는 이전 상태를 잠깐 잘못 보여주지 않는다.
	StateSprite->SetVisibility(false);

	if (bMoveStoneDuringBattle)
	{
		QuestMarker->SetVisibility(false);
		QuestMarker->SetHiddenInGame(true);
		InteractionVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BattleStoneRestLocation = Mesh->GetRelativeLocation();
		BattleStoneRestScale = Mesh->GetRelativeScale3D();
		SetActorHiddenInGame(true);
		return;
	}
	BindQuest();
}

void ANUQuestActionActor::MoveBattleStone()
{
	if (bMoveStoneDuringBattle && bBattleBurrowed) { bStoneMovedForEmergence = true; }
}

void ANUQuestActionActor::SetBattleBurrowed(bool bBurrowed, UPaperFlipbookComponent* Sprite)
{
	if (!bMoveStoneDuringBattle || bBurrowed == bBattleBurrowed) { return; }
	if (BurrowSprite.Get() != Sprite)
	{
		RestoreBurrowSprite();
		BurrowSprite = Sprite;
		if (Sprite != nullptr)
		{
			BurrowSpriteRestLocation = Sprite->GetRelativeLocation();
			BurrowSpriteRestScale = Sprite->GetRelativeScale3D();
			bBurrowSpriteWasVisible = Sprite->IsVisible();
		}
	}
	bBattleBurrowed = bBurrowed;
	if (bBurrowed) { bStoneMovedForEmergence = false; }
	FVector SlideDirection = FVector::ForwardVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC != nullptr && PC->PlayerCameraManager != nullptr)
	{
		SlideDirection = PC->PlayerCameraManager->GetActorRightVector();
		SlideDirection.Z = 0.f;
		SlideDirection = SlideDirection.GetSafeNormal();
	}
	StoneSlideOffset = GetActorTransform().InverseTransformVector(SlideDirection * 60.f);
	Mesh->SetRelativeLocation(BattleStoneRestLocation);
	Mesh->SetRelativeScale3D(BattleStoneRestScale);
	SetActorHiddenInGame(false);
	BurrowPresentationStart = GetWorld()->GetTimeSeconds();
	UpdateBurrowPresentation();
	GetWorldTimerManager().SetTimer(BurrowPresentationTimer, this, &ThisClass::UpdateBurrowPresentation, 1.f / 60.f, true);
}

void ANUQuestActionActor::RestoreBurrowSprite()
{
	if (UPaperFlipbookComponent* Sprite = BurrowSprite.Get())
	{
		Sprite->SetRelativeLocation(BurrowSpriteRestLocation);
		Sprite->SetRelativeScale3D(BurrowSpriteRestScale);
		Sprite->SetVisibility(bBurrowSpriteWasVisible);
	}
}

void ANUQuestActionActor::UpdateBurrowPresentation()
{
	// 기존 행동의 타격 이후 여유 시간 안에서 끝낸다. 전투 속도는 변경하지 않는다.
	const float Duration = bBattleBurrowed ? 0.36f : 0.28f;
	const float Alpha = FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - BurrowPresentationStart) / Duration, 0.f, 1.f);
	const auto Smooth = [](float Value) { const float T = FMath::Clamp(Value, 0.f, 1.f); return T * T * (3.f - 2.f * T); };
	float Depth = 0.f;
	float Tremble = 0.f;
	if (bBattleBurrowed)
	{
		// 몸을 짧게 떨고 내려간 후 기존 돌무더기 하나를 남긴다.
		Depth = Smooth((Alpha - 0.2f) / 0.8f);
		Tremble = Alpha < 0.2f ? FMath::Sin(Alpha / 0.2f * 4.f * PI) * 2.f : 0.f;
		Mesh->SetVisibility(Alpha >= 0.55f);
		Mesh->SetRelativeScale3D(BattleStoneRestScale * FMath::Lerp(0.75f, 1.f, Smooth((Alpha - 0.55f) / 0.45f)));
	}
	else
	{
		// 돌 옮기기는 먼저 옆으로 밀고, 자발적으로 나올 때는 먼저 돌을 들썩인다.
		// 옮긴 돌은 옆에 남겨 두고 다음 턴 시작에 치운다(ClearMovedBattleStone). 스스로 나올 때만 바로 사라진다.
		const float StoneAlpha = Smooth(Alpha / 0.45f);
		const float Rise = Smooth((Alpha - 0.45f) / 0.55f);
		Depth = 1.f - Rise;
		const FVector Offset = bStoneMovedForEmergence ? StoneSlideOffset * StoneAlpha
			: FVector(0.f, 0.f, FMath::Sin(StoneAlpha * PI) * 9.f);
		const float Shrink = bStoneMovedForEmergence ? 0.f : Smooth((Alpha - 0.75f) / 0.25f);
		Mesh->SetRelativeLocation(BattleStoneRestLocation + Offset);
		Mesh->SetRelativeScale3D(BattleStoneRestScale * (1.f - Shrink));
		Mesh->SetVisibility(true);
	}
	if (UPaperFlipbookComponent* Sprite = BurrowSprite.Get())
	{
		const FTransform ParentTransform = Sprite->GetAttachParent() != nullptr ? Sprite->GetAttachParent()->GetComponentTransform() : FTransform::Identity;
		const FVector Offset = ParentTransform.InverseTransformVector(FVector(0.f, 0.f, -18.f * Depth)) + FVector(Tremble, 0.f, 0.f);
		Sprite->SetRelativeLocation(BurrowSpriteRestLocation + Offset);
		Sprite->SetRelativeScale3D(BurrowSpriteRestScale * FVector(1.f, 1.f, FMath::Lerp(1.f, 0.15f, Depth)));
		Sprite->SetVisibility(bBurrowSpriteWasVisible && Depth < 0.995f);
	}
	if (Alpha >= 1.f)
	{
		GetWorldTimerManager().ClearTimer(BurrowPresentationTimer);
		if (!bBattleBurrowed)
		{
			RestoreBurrowSprite();
			if (!bStoneMovedForEmergence) { SetActorHiddenInGame(true); }
		}
	}
}

// 옆으로 밀어 둔 돌은 다음 턴이 시작될 때 작아지며 치운다.
void ANUQuestActionActor::ClearMovedBattleStone()
{
	if (!bMoveStoneDuringBattle || bBattleBurrowed || !bStoneMovedForEmergence) { return; }
	if (GetWorldTimerManager().IsTimerActive(BurrowPresentationTimer))
	{
		// 미는 연출이 끝나기 전에 턴이 넘어가면 끝난 모습으로 맞춘 뒤 치운다.
		GetWorldTimerManager().ClearTimer(BurrowPresentationTimer);
		RestoreBurrowSprite();
		Mesh->SetRelativeLocation(BattleStoneRestLocation + StoneSlideOffset);
	}
	BurrowPresentationStart = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(BurrowPresentationTimer, this, &ThisClass::UpdateMovedStoneClear, 1.f / 60.f, true);
}

void ANUQuestActionActor::UpdateMovedStoneClear()
{
	const float Alpha = FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - BurrowPresentationStart) / 0.25f, 0.f, 1.f);
	Mesh->SetRelativeScale3D(BattleStoneRestScale * (1.f - Alpha * Alpha));
	if (Alpha >= 1.f)
	{
		GetWorldTimerManager().ClearTimer(BurrowPresentationTimer);
		SetActorHiddenInGame(true);
		Mesh->SetRelativeLocation(BattleStoneRestLocation);
		Mesh->SetRelativeScale3D(BattleStoneRestScale);
		bStoneMovedForEmergence = false;
	}
}

// 플레이어 상태 준비를 기다린 뒤 진행 이벤트를 한 번만 구독한다.
void ANUQuestActionActor::BindQuest()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	UNUQuestComponent* Quest = PC != nullptr ? FindQuest(PC->GetPawn()) : nullptr;
	if (Quest == nullptr)
	{
		GetWorldTimerManager().SetTimer(BindTimer, this, &ThisClass::BindQuest, 0.25f, false);
		return;
	}

	BoundQuest = Quest;
	Quest->OnQuestProgressChanged.AddUniqueDynamic(this, &ThisClass::RefreshState);
	RefreshState();
}

// 연습 나이아와 설치형 작업은 저장된 목표 진행에 맞춰 표시를 복원한다.
void ANUQuestActionActor::RefreshState()
{
	RefreshGrassRecovery();
	RefreshStateSprite();
	const UNUQuestBattleDefinition* Battle = Cast<UNUQuestBattleDefinition>(EncounterRequest.BattleDefinition);
	if (IsA<ANUQuestNaiaActor>() && FindComponentByClass<UNUActorPresenceComponent>() == nullptr
		&& Battle != nullptr && NUQuestBattle::IsPracticeKind(Battle->Kind)
		&& BoundQuest.IsValid() && BoundQuest->IsQuestProgressReady())
	{
		const UNUQuestDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
		const UNUQuestDefinition* Definition = Data != nullptr ? Data->FindDefinition(QuestId) : nullptr;
		const FNUQuestProgressState* Progress = BoundQuest->FindQuestProgress(QuestId);
		const FNUQuestObjectiveSpec* Spec = Definition != nullptr ? Definition->Objectives.FindByPredicate(
			[this](const FNUQuestObjectiveSpec& Item) { return Item.ObjectiveId == ObjectiveId; }) : nullptr;
		const FNUQuestObjectiveProgress* Completed = Progress != nullptr ? Progress->FindObjectiveProgress(ObjectiveId) : nullptr;
		const bool bDone = Progress != nullptr && (Progress->QuestState == ENUQuestState::Completed
			|| (Spec != nullptr && Completed != nullptr && Completed->IsComplete(Spec->RequiredCount)));
		SetActorHiddenInGame(bDone);
		SetActorEnableCollision(!bDone);
	}
	if (!bShowInstalledState || !BoundQuest.IsValid()) { return; }

	const FNUQuestProgressState* Progress = BoundQuest->FindQuestProgress(QuestId);
	const FNUQuestObjectiveProgress* Objective = Progress != nullptr ? Progress->FindObjectiveProgress(ObjectiveId) : nullptr;
	const bool bInstalled = Objective != nullptr && Objective->CurrentCount > 0;
	Mesh->SetVisibility(bInstalled);
}

void ANUQuestActionActor::RefreshStateSprite()
{
	if (IncompleteSprite == nullptr) { return; }
	if (!BoundQuest.IsValid() || !BoundQuest->IsQuestProgressReady())
	{
		StateSprite->SetVisibility(false);
		return;
	}

	const UNUQuestDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	const UNUQuestDefinition* Definition = Data != nullptr ? Data->FindDefinition(QuestId) : nullptr;
	const FNUQuestProgressState* Progress = BoundQuest->FindQuestProgress(QuestId);
	const FNUQuestObjectiveSpec* Spec = Definition != nullptr ? Definition->Objectives.FindByPredicate(
		[this](const FNUQuestObjectiveSpec& Item) { return Item.ObjectiveId == ObjectiveId; }) : nullptr;
	const FNUQuestObjectiveProgress* Objective = Progress != nullptr ? Progress->FindObjectiveProgress(ObjectiveId) : nullptr;
	const bool bDone = Progress != nullptr && (Progress->QuestState == ENUQuestState::Completed
		|| (Spec != nullptr && Objective != nullptr && Objective->IsComplete(Spec->RequiredCount)));
	StateSprite->SetSprite(bDone && CompletedSprite != nullptr ? CompletedSprite : IncompleteSprite);
	StateSprite->SetRelativeLocation(StateSpriteOffset + (bDone ? CompletedSpriteOffset : FVector::ZeroVector));
	StateSprite->SetVisibility(true);
}

// 풀 회수 단계는 영구 목표에서 복원하며 전투 시작값과 같은 계산을 사용한다.
void ANUQuestActionActor::RefreshGrassRecovery()
{
	const UNUQuestBattleDefinition* Battle = Cast<UNUQuestBattleDefinition>(EncounterRequest.BattleDefinition);
	if (Battle == nullptr || Battle->Rule != ENUQuestCalmingRule::Grass || !BoundQuest.IsValid() || BoundQuest->IsServerBackedSession()) { return; }
	const UNUQuestDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	const UNUQuestDefinition* Definition = Data != nullptr ? Data->FindDefinition(QuestId) : nullptr;
	const FNUQuestProgressState* Progress = BoundQuest->FindQuestProgress(QuestId);
	const FNUQuestObjectiveProgress* Final = Progress != nullptr ? Progress->FindObjectiveProgress(ObjectiveId) : nullptr;
	const bool bActive = Definition != nullptr && Progress != nullptr && Progress->QuestState == ENUQuestState::Active && Final != nullptr && Final->CurrentCount == 0;
	if (!bActive)
	{
		if (GrassAgitationWidget != nullptr) { GrassAgitationWidget->SetVisibility(false); }
		return;
	}

	const int32 Stage = Battle->GetCompletedGrassRecoveries(*Definition, *Progress);
	if (Stage > 0 && GrassAgitationWidget == nullptr && Battle->AgitationWidgetClass != nullptr)
	{
		GrassAgitationWidget = NewObject<UWidgetComponent>(this);
		GrassAgitationWidget->SetupAttachment(GetRootComponent());
		GrassAgitationWidget->SetWidgetSpace(EWidgetSpace::Screen);
		GrassAgitationWidget->SetDrawSize(FVector2D(260.f, 48.f));
		GrassAgitationWidget->SetRelativeLocation(Battle->AgitationWidgetOffset);
		GrassAgitationWidget->SetWidgetClass(Battle->AgitationWidgetClass);
		GrassAgitationWidget->RegisterComponent();
		GrassAgitationWidget->InitWidget();
	}
	if (GrassAgitationWidget != nullptr)
	{
		GrassAgitationWidget->SetVisibility(Stage > 0);
		if (UNUQuestAgitationWidget* Widget = Cast<UNUQuestAgitationWidget>(GrassAgitationWidget->GetUserWidgetObject()))
		{
			Widget->SetAgitation(Battle->GetGrassAgitation(Stage), Battle->InitialAgitation);
		}
	}
}

// 진행할 퀘스트·목표는 전투 정의가 갖고, 대상 ID와 목표 타입은 등장 컴포넌트가 퀘스트 제공자에 이미 넣었다.
// 선행 조건은 퀘스트 데이터의 목표 선행(PrerequisiteObjectiveId)이 정하므로 배치값 RequiredObjectiveIds는 비운다.
void ANUQuestActionActor::HandleActorPresence(const FNUActorPresenceRow* Step)
{
	if (Step == nullptr || !Step->bOverrideQuestInteraction)
	{
		if (bActorPresenceOverrodeQuest)
		{
			EncounterRequest = InitialEncounterRequest;
			QuestId = InitialQuestId;
			ObjectiveId = InitialObjectiveId;
			RequiredObjectiveIds = InitialRequiredObjectiveIds;
			bActorPresenceOverrodeQuest = false;
			RefreshState();
		}
		return;
	}
	bActorPresenceOverrodeQuest = true;
	UNUQuestBattleDefinition* Battle = Step != nullptr ? Step->Battle.LoadSynchronous() : nullptr;
	EncounterRequest.BattleDefinition = Battle;
	QuestId = Battle != nullptr ? Battle->QuestId : NAME_None;
	ObjectiveId = Battle != nullptr ? Battle->ObjectiveId : NAME_None;
	RequiredObjectiveIds.Reset();

	if (Step != nullptr && Battle == nullptr && Step->QuestActorId > 0)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestActionActor: 전투 없는 퀘스트 대상 자리는 상호작용하지 않습니다 [%s] QuestActorId=%d"), *GetName(), Step->QuestActorId);
	}

	RefreshState();
}

void ANUQuestActionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNUActorPresenceComponent* Presence = FindComponentByClass<UNUActorPresenceComponent>())
	{
		Presence->OnPresenceChanged.RemoveAll(this);
	}

	GetWorldTimerManager().ClearTimer(BurrowPresentationTimer);
	RestoreBurrowSprite();
	BurrowSprite.Reset();
	GetWorldTimerManager().ClearTimer(BindTimer);
	if (BoundQuest.IsValid()) { BoundQuest->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::RefreshState); }

	Super::EndPlay(EndPlayReason);
}
