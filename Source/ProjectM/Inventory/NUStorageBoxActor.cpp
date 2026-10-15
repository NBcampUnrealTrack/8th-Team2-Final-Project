// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUStorageBoxActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/Component/NUInteractionRouterComponent.h"
#include "Interaction/Data/NUInteractionTypes.h"
#include "Inventory/Component/NUStorageProviderComponent.h"
#include "PlayerController/NUPlayerController.h"

ANUStorageBoxActor::ANUStorageBoxActor()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->SetBoxExtent(FVector(60.f, 60.f, 60.f));
	InteractionVolume->SetCollisionProfileName(TEXT("NU_Interactable"));
	InteractionVolume->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(InteractionVolume);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionRouter = CreateDefaultSubobject<UNUInteractionRouterComponent>(TEXT("InteractionRouter"));
	StorageProvider = CreateDefaultSubobject<UNUStorageProviderComponent>(TEXT("StorageProvider"));
}

bool ANUStorageBoxActor::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) && IsValid(InteractionRouter) && IsValid(StorageProvider);
}

bool ANUStorageBoxActor::Interact_Implementation(AActor* Interactor)
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
	ANUPlayerController* PlayerController = IsValid(InteractorPawn)
		? Cast<ANUPlayerController>(InteractorPawn->GetController())
		: Cast<ANUPlayerController>(Interactor);
	if (!IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->ClientOpenInteractionConversation(nullptr, this, Options);
	return true;
}
