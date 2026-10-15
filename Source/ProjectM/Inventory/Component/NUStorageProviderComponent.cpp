#include "Inventory/Component/NUStorageProviderComponent.h"

#include "CharacterState/NUPlayerState.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Field/NPC/NUNPCBase.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "Inventory/NUInventoryComponent.h"
#include "Net/EmberServerManager.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace NUStorageProvider
{
	const FName DefaultOptionId(TEXT("Default"));
}

UNUStorageProviderComponent::UNUStorageProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UNUStorageProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!IsContextValid(Context))
	{
		return;
	}


	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
	FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Handle.OptionType = NU::Interaction::Option::Storage;
	Option.Handle.OptionId = NUStorageProvider::DefaultOptionId;
	Option.DisplayText = Settings->StorageOptionText;
	Option.CategoryText = Settings->StorageCategoryText;
	Option.Group = ENUInteractionOptionGroup::Service;
	Option.SortOrder = SortOrder;
	Option.bEnabled = true;
	Option.ExecutionDialogue = StorageExecutionDialogue;
}

bool UNUStorageProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	return IsContextValid(Context) && IsStorageHandle(Handle);
}

bool UNUStorageProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!CanExecuteInteraction_Implementation(Context, Handle) || !Context.Interactor->HasAuthority())
	{
		return false;
	}

	ANUPlayerController* PlayerController = FindPlayerController(Context.Interactor);
	if (!IsValid(PlayerController))
	{
		return false;
	}

	if (!IsStorageAvailable(Context.Interactor))
	{
		if (StorageUnavailableDialogue) { PlayerController->ClientOpenStandaloneDialogue(StorageUnavailableDialogue); }
		return false;
	}
	UNUInventoryComponent* InventoryComponent = FindInventoryComponent(Context.Interactor);
	const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
	if (!IsValid(InventoryComponent) || !IsValid(NPC) || NPC->GetNpcId() <= 0)
	{
		return false;
	}

	PlayerController->ClientOpenStorage();
	InventoryComponent->OnStorageAccessed.Broadcast(NPC->GetNpcId());
	return true;
}

bool UNUStorageProviderComponent::IsStorageAvailable(AActor* Interactor) const
{
	const UEmberServerManager* ServerManager = FindServerManager();
	return IsValid(FindInventoryComponent(Interactor)) && IsValid(ServerManager) && ServerManager->IsLoggedIn();
}

bool UNUStorageProviderComponent::IsContextValid(const FNUInteractionContext& Context) const
{
	return Context.IsValid() && Context.Target.Get() == GetOwner();
}

bool UNUStorageProviderComponent::IsStorageHandle(const FNUInteractionOptionHandle& Handle) const
{
	return Handle.OptionType == NU::Interaction::Option::Storage && Handle.OptionId == NUStorageProvider::DefaultOptionId;
}

ANUPlayerController* UNUStorageProviderComponent::FindPlayerController(AActor* Interactor) const
{
	if (ANUPlayerController* PlayerController = Cast<ANUPlayerController>(Interactor))
	{
		return PlayerController;
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	return IsValid(Pawn) ? Cast<ANUPlayerController>(Pawn->GetController()) : nullptr;
}

UNUInventoryComponent* UNUStorageProviderComponent::FindInventoryComponent(AActor* Interactor) const
{
	const ANUPlayerController* PlayerController = FindPlayerController(Interactor);
	const ANUPlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<ANUPlayerState>() : nullptr;
	return IsValid(PlayerState) ? PlayerState->GetInventoryComponent() : nullptr;
}

UEmberServerManager* UNUStorageProviderComponent::FindServerManager() const
{
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UNUStorageProviderComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UNUInteractionSettings* InteractionSettings = GetDefault<UNUInteractionSettings>();
	if (!IsValid(InteractionSettings) || InteractionSettings->StorageOptionText.IsEmpty() || InteractionSettings->StorageCategoryText.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("NUStorage", "MissingStorageInteractionText", "NU Interaction 설정의 창고 선택지 또는 카테고리 문구가 비어 있습니다."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
