#include "CharacterProgression/Component/NUJobProviderComponent.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestProviderComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace NUJobProvider
{
	const FName InitialJobSelectionOptionId(TEXT("InitialJobSelection"));
}

UNUJobProviderComponent::UNUJobProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UNUJobProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!CanProvideJobSelection(Context))
	{
		return;
	}

	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
	FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Handle.OptionType = NU::Interaction::Option::Job;
	Option.Handle.OptionId = NUJobProvider::InitialJobSelectionOptionId;
	Option.DisplayText = Settings->JobSelectionOptionText;
	Option.CategoryText = Settings->JobCategoryText;
	Option.Group = ENUInteractionOptionGroup::Service;
	Option.SortOrder = 0;
	Option.bEnabled = true;
}

bool UNUJobProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	return IsJobSelectionHandle(Handle) && CanProvideJobSelection(Context);
}

bool UNUJobProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!CanExecuteInteraction_Implementation(Context, Handle) || !Context.Interactor->HasAuthority())
	{
		return false;
	}

	UNUJobComponent* JobComponent = FindJobComponent(Context.Interactor);
	const APawn* InteractorPawn = Cast<APawn>(Context.Interactor);
	ANUPlayerController* PlayerController = InteractorPawn != nullptr ? Cast<ANUPlayerController>(InteractorPawn->GetController()) : nullptr;
	if (JobComponent == nullptr || PlayerController == nullptr || !JobComponent->TryAuthorizeInitialJobSelection(GetQuestActorId()))
	{
		return false;
	}

	PlayerController->ClientBeginInitialJobSelection();
	return true;
}

bool UNUJobProviderComponent::IsJobSelectionHandle(const FNUInteractionOptionHandle& Handle) const
{
	return Handle.OptionType == NU::Interaction::Option::Job
		&& Handle.OptionId == NUJobProvider::InitialJobSelectionOptionId;
}

FName UNUJobProviderComponent::GetQuestActorId() const
{
	const UNUQuestProviderComponent* QuestProvider = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UNUQuestProviderComponent>()
		: nullptr;
	return QuestProvider != nullptr ? QuestProvider->QuestActorId : NAME_None;
}

UNUJobComponent* UNUJobProviderComponent::FindJobComponent(AActor* Interactor) const
{
	const APawn* InteractorPawn = Cast<APawn>(Interactor);
	AController* Controller = InteractorPawn != nullptr ? InteractorPawn->GetController() : nullptr;
	ANUPlayerState* PlayerState = Controller != nullptr ? Controller->GetPlayerState<ANUPlayerState>() : nullptr;
	return PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr;
}

bool UNUJobProviderComponent::CanProvideJobSelection(const FNUInteractionContext& Context) const
{
	if (!Context.IsValid() || Context.Target.Get() != GetOwner())
	{
		return false;
	}

	const UNUJobComponent* JobComponent = FindJobComponent(Context.Interactor);
	return JobComponent != nullptr && JobComponent->CanAuthorizeInitialJobSelection(GetQuestActorId());
}
