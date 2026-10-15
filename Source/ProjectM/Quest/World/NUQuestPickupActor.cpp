#include "Quest/World/NUQuestPickupActor.h"

#include "Field/NUSpriteVisualComponent.h"
#include "Interaction/Component/NUInteractionRouterComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestMarkerComponent.h"
#include "Quest/Component/NUQuestProviderComponent.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PaperFlipbookComponent.h"

ANUQuestPickupActor::ANUQuestPickupActor()
{
	PrimaryActorTick.bCanEverTick = true;

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(45.0f);
	InteractionVolume->SetCollisionProfileName(TEXT("NU_Interactable"));
	InteractionVolume->SetGenerateOverlapEvents(true);

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(InteractionVolume);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, nullptr, NUSpriteVisuals::MakeNPC());

	InteractionRouter = CreateDefaultSubobject<UNUInteractionRouterComponent>(TEXT("InteractionRouter"));

	QuestProvider = CreateDefaultSubobject<UNUQuestProviderComponent>(TEXT("QuestProvider"));
	QuestProvider->ObjectiveType = NU::Quest::Objective::CollectItem;
	QuestProvider->ObjectiveInteractionLabel = NSLOCTEXT("NUQuest", "PickupQuestItem", "줍기");

	QuestMarker = CreateDefaultSubobject<UNUQuestMarkerComponent>(TEXT("QuestMarker"));
	QuestMarker->SetupAttachment(InteractionVolume);
	QuestMarker->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
}

void ANUQuestPickupActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshLocalVisibility();
}

void ANUQuestPickupActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SpriteVisual->UpdateBillboard();
	RefreshLocalVisibility();
}

bool ANUQuestPickupActor::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor)
		&& IsValid(InteractionRouter)
		&& IsValid(QuestProvider)
		&& QuestProvider->IsActiveObjectiveTarget(Interactor);
}

bool ANUQuestPickupActor::Interact_Implementation(AActor* Interactor)
{
	if (!INUInteractableInterface::Execute_CanInteract(this, Interactor) || !HasAuthority())
	{
		return false;
	}

	FNUInteractionContext Context;
	Context.Interactor = Interactor;
	Context.Target = this;

	TArray<FNUInteractionOption> Options;
	const ENUInteractionRouteResult RouteResult = InteractionRouter->RouteInteraction(Context, Options);
	if (RouteResult == ENUInteractionRouteResult::Executed)
	{
		return true;
	}

	if (RouteResult != ENUInteractionRouteResult::SelectionRequired)
	{
		return false;
	}

	const APawn* InteractorPawn = Cast<APawn>(Interactor);
	ANUPlayerController* PlayerController = InteractorPawn != nullptr
		? Cast<ANUPlayerController>(InteractorPawn->GetController())
		: nullptr;
	if (!IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->ClientOpenInteractionConversation(nullptr, this, Options);
	return true;
}

void ANUQuestPickupActor::RefreshLocalVisibility()
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World != nullptr
		? World->GetFirstPlayerController()
		: nullptr;
	APawn* LocalPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;

	const bool bShouldBeVisible = IsValid(LocalPawn)
		&& IsValid(QuestProvider)
		&& QuestProvider->IsActiveObjectiveTarget(LocalPawn);
	Sprite->SetVisibility(bShouldBeVisible);
}
