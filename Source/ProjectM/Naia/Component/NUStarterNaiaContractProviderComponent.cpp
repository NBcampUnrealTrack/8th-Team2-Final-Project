#include "Naia/Component/NUStarterNaiaContractProviderComponent.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Net/NUNaiaNetSubsystem.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestProviderComponent.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace NUStarterNaiaContractProvider
{
	const FName DefaultOptionId(TEXT("Default"));
}

UNUStarterNaiaContractProviderComponent::UNUStarterNaiaContractProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UNUStarterNaiaContractProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!CanProvideContract(Context))
	{
		return;
	}

	const bool bAvailable = IsContractAvailable(Context.Interactor);
	if (!bAvailable && !bShowWhenUnavailable)
	{
		return;
	}

	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
	FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Handle.OptionType = NU::Interaction::Option::NaiaContract;
	Option.Handle.OptionId = NUStarterNaiaContractProvider::DefaultOptionId;
	Option.DisplayText = Settings->StarterNaiaContractOptionText;
	Option.CategoryText = Settings->NaiaContractCategoryText;
	Option.Group = ENUInteractionOptionGroup::Service;
	Option.SortOrder = SortOrder;
	Option.bEnabled = bAvailable;
	if (!bAvailable)
	{
		Option.DisabledReason = GetUnavailableReason(Context.Interactor);
	}
}

bool UNUStarterNaiaContractProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	return IsContractHandle(Handle) && CanProvideContract(Context) && IsContractAvailable(Context.Interactor);
}

bool UNUStarterNaiaContractProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!CanExecuteInteraction_Implementation(Context, Handle) || !Context.Interactor->HasAuthority())
	{
		return false;
	}

	ANUPlayerController* PlayerController = FindPlayerController(Context.Interactor);
	ANUPlayerState* PlayerState = FindPlayerState(Context.Interactor);
	UNUNaiaRosterComponent* RosterComponent = IsValid(PlayerState) ? PlayerState->GetNaiaRosterComponent() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(RosterComponent) || !RosterComponent->AuthorizeStarterNaiaSelection())
	{
		return false;
	}

	PlayerController->ClientBeginStarterNaiaSelection();
	return true;
}

bool UNUStarterNaiaContractProviderComponent::IsContextValid(const FNUInteractionContext& Context) const
{
	return Context.IsValid() && Context.Target.Get() == GetOwner();
}

bool UNUStarterNaiaContractProviderComponent::IsContractHandle(const FNUInteractionOptionHandle& Handle) const
{
	return Handle.OptionType == NU::Interaction::Option::NaiaContract && Handle.OptionId == NUStarterNaiaContractProvider::DefaultOptionId;
}

bool UNUStarterNaiaContractProviderComponent::CanProvideContract(const FNUInteractionContext& Context) const
{
	if (!IsContextValid(Context))
	{
		return false;
	}

	const ANUPlayerState* PlayerState = FindPlayerState(Context.Interactor);
	const UNUJobComponent* JobComponent = IsValid(PlayerState) ? PlayerState->GetJobComponent() : nullptr;
	const UNUNaiaRosterComponent* RosterComponent = IsValid(PlayerState) ? PlayerState->GetNaiaRosterComponent() : nullptr;
	return IsValid(JobComponent)
		&& JobComponent->HasJob()
		&& JobComponent->IsInitialJobSelectionNpc(GetQuestActorId())
		&& IsValid(RosterComponent)
		&& !RosterComponent->HasSelectedStarterNaia();
}

bool UNUStarterNaiaContractProviderComponent::IsContractAvailable(AActor* Interactor) const
{
	const ANUPlayerState* PlayerState = FindPlayerState(Interactor);
	const UNUNaiaRosterComponent* RosterComponent = IsValid(PlayerState) ? PlayerState->GetNaiaRosterComponent() : nullptr;
	const UNUProgressionComponent* Progression = IsValid(PlayerState) ? PlayerState->GetProgressionComponent() : nullptr;
	if (Progression != nullptr && Progression->IsUsingLocalProgressionBackend())
	{
		return IsValid(RosterComponent) && RosterComponent->CanAcceptNaia();
	}
	const UNUNaiaNetSubsystem* NaiaNetSubsystem = FindNaiaNetSubsystem();
	return IsValid(RosterComponent)
		&& RosterComponent->CanAcceptNaia()
		&& IsValid(NaiaNetSubsystem)
		&& NaiaNetSubsystem->HasActiveServerSession()
		&& NaiaNetSubsystem->HasReceivedRoster()
		&& !NaiaNetSubsystem->IsStarterNaiaContractPending();
}

FText UNUStarterNaiaContractProviderComponent::GetUnavailableReason(AActor* Interactor) const
{
	const ANUPlayerState* PlayerState = FindPlayerState(Interactor);
	const UNUNaiaRosterComponent* RosterComponent = IsValid(PlayerState) ? PlayerState->GetNaiaRosterComponent() : nullptr;
	const UNUNaiaNetSubsystem* NaiaNetSubsystem = FindNaiaNetSubsystem();
	if (!IsValid(NaiaNetSubsystem) || !NaiaNetSubsystem->HasActiveServerSession())
	{
		return NSLOCTEXT("NUStarterNaiaContract", "ServerUnavailable", "서버 연결이 필요합니다.");
	}
	if (!NaiaNetSubsystem->HasReceivedRoster())
	{
		return NSLOCTEXT("NUStarterNaiaContract", "RosterSynchronizing", "나이아 정보를 불러오는 중입니다.");
	}
	if (NaiaNetSubsystem->IsStarterNaiaContractPending())
	{
		return NSLOCTEXT("NUStarterNaiaContract", "RequestPending", "계약 요청을 처리 중입니다.");
	}
	if (!IsValid(RosterComponent) || !RosterComponent->CanAcceptNaia())
	{
		return NSLOCTEXT("NUStarterNaiaContract", "RosterFull", "파티와 보관함에 빈자리가 없습니다.");
	}

	return NSLOCTEXT("NUStarterNaiaContract", "Unavailable", "지금은 계약할 수 없습니다.");
}

FName UNUStarterNaiaContractProviderComponent::GetQuestActorId() const
{
	const UNUQuestProviderComponent* QuestProvider = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UNUQuestProviderComponent>() : nullptr;
	return IsValid(QuestProvider) ? QuestProvider->QuestActorId : NAME_None;
}

ANUPlayerController* UNUStarterNaiaContractProviderComponent::FindPlayerController(AActor* Interactor) const
{
	if (ANUPlayerController* PlayerController = Cast<ANUPlayerController>(Interactor))
	{
		return PlayerController;
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	return IsValid(Pawn) ? Cast<ANUPlayerController>(Pawn->GetController()) : nullptr;
}

ANUPlayerState* UNUStarterNaiaContractProviderComponent::FindPlayerState(AActor* Interactor) const
{
	const ANUPlayerController* PlayerController = FindPlayerController(Interactor);
	return IsValid(PlayerController) ? PlayerController->GetPlayerState<ANUPlayerState>() : nullptr;
}

UNUNaiaNetSubsystem* UNUStarterNaiaContractProviderComponent::FindNaiaNetSubsystem() const
{
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUNaiaNetSubsystem>() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UNUStarterNaiaContractProviderComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UNUInteractionSettings* InteractionSettings = GetDefault<UNUInteractionSettings>();
	if (!IsValid(InteractionSettings) || InteractionSettings->StarterNaiaContractOptionText.IsEmpty() || InteractionSettings->NaiaContractCategoryText.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("NUStarterNaiaContract", "MissingInteractionText", "NU Interaction 설정의 스타팅 나이아 계약 선택지 또는 카테고리 문구가 비어 있습니다."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
