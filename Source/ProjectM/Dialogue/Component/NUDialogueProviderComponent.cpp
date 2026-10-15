#include "Dialogue/Component/NUDialogueProviderComponent.h"

#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Data/NUQuestTypes.h"

#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace NUDialogueProvider
{
	const FName DefaultOptionId(TEXT("Default"));
}

UNUDialogueProviderComponent::UNUDialogueProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UNUDialogueProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!IsContextValid(Context))
	{
		return;
	}
	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();

	if (IsValid(DefaultDialogueDefinition))
	{
		FNUInteractionOption& DefaultOption = OutOptions.AddDefaulted_GetRef();
		DefaultOption.Handle.OptionType = NU::Interaction::Option::Dialogue;
		DefaultOption.Handle.OptionId = NUDialogueProvider::DefaultOptionId;
		DefaultOption.DisplayText = Settings->DefaultDialogueOptionText;
		DefaultOption.CategoryText = Settings->DialogueCategoryText;
		DefaultOption.Group = ENUInteractionOptionGroup::Dialogue;
		DefaultOption.SortOrder = 0;
		DefaultOption.bEnabled = true;
	}

	for (const FNUDialogueOptionSpec& Spec : DialogueOptions)
	{
		if (Spec.OptionId.IsNone() || Spec.DisplayText.IsEmpty())
		{
			continue;
		}

		FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
		Option.Handle.OptionType = NU::Interaction::Option::Dialogue;
		Option.Handle.OptionId = Spec.OptionId;
		Option.DisplayText = Spec.DisplayText;
		Option.CategoryText = Settings->DialogueCategoryText;
		Option.Group = ENUInteractionOptionGroup::Dialogue;
		Option.SortOrder = Spec.SortOrder;
		Option.bEnabled = Spec.bEnabled && IsValid(Spec.DialogueDefinition);
		Option.DisabledReason = Spec.DisabledReason;
		if (Spec.bEnabled && !IsValid(Spec.DialogueDefinition) && Option.DisabledReason.IsEmpty())
		{
			Option.DisabledReason = NSLOCTEXT("NUDialogue", "MissingDialogueDefinition", "대화 데이터가 없습니다.");
		}
	}
}

bool UNUDialogueProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	if (!IsContextValid(Context))
	{
		return false;
	}

	if (IsDefaultDialogueHandle(Handle))
	{
		return IsValid(DefaultDialogueDefinition);
	}

	const FNUDialogueOptionSpec* Spec = FindOption(Handle);
	return Spec != nullptr && Spec->bEnabled && IsValid(Spec->DialogueDefinition);
}

bool UNUDialogueProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!CanExecuteInteraction_Implementation(Context, Handle) || !Context.Interactor->HasAuthority())
	{
		return false;
	}

	UNUDialogueDefinition* DialogueDefinition = nullptr;
	if (IsDefaultDialogueHandle(Handle))
	{
		DialogueDefinition = DefaultDialogueDefinition;
	}
	else if (const FNUDialogueOptionSpec* Spec = FindOption(Handle))
	{
		DialogueDefinition = Spec->DialogueDefinition;
	}

	const APawn* InteractorPawn = Cast<APawn>(Context.Interactor);
	ANUPlayerController* PlayerController = InteractorPawn != nullptr ? Cast<ANUPlayerController>(InteractorPawn->GetController()) : nullptr;
	if (!IsValid(DialogueDefinition) || !IsValid(PlayerController))
	{
		return false;
	}

	// NPC 일반 대화도 완료 콜백을 서버로 보내 전직 등 대화 완료 기능을 처리한다.
	PlayerController->ClientOpenDialogue(DialogueDefinition, GetOwner(), FNUQuestInteractionOption());
	return true;
}

UNUDialogueDefinition* UNUDialogueProviderComponent::GetInteractionPromptDialogueDefinition() const
{
	// 별도 질문 대사가 없으면 NPC 기본 대사로 인사해 빈 대화창이 뜨지 않게 한다.
	if (IsValid(MultipleChoicePromptDialogueDefinition))
	{
		return MultipleChoicePromptDialogueDefinition.Get();
	}

	return IsValid(DefaultDialogueDefinition) ? DefaultDialogueDefinition.Get() : nullptr;
}

const FNUDialogueOptionSpec* UNUDialogueProviderComponent::FindOption(const FNUInteractionOptionHandle& Handle) const
{
	if (Handle.OptionType != NU::Interaction::Option::Dialogue || Handle.OptionId.IsNone())
	{
		return nullptr;
	}

	return DialogueOptions.FindByPredicate([&Handle](const FNUDialogueOptionSpec& Spec)
	{
		return Spec.OptionId == Handle.OptionId;
	});
}

bool UNUDialogueProviderComponent::IsDefaultDialogueHandle(const FNUInteractionOptionHandle& Handle) const
{
	return Handle.OptionType == NU::Interaction::Option::Dialogue
		&& Handle.OptionId == NUDialogueProvider::DefaultOptionId;
}

bool UNUDialogueProviderComponent::IsContextValid(const FNUInteractionContext& Context) const
{
	return Context.IsValid() && Context.Target.Get() == GetOwner();
}

#if WITH_EDITOR
EDataValidationResult UNUDialogueProviderComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (IsValid(DefaultDialogueDefinition))
	{
		FString Error;
		if (!DefaultDialogueDefinition->ValidateDefinition(Error))
		{
			Context.AddError(FText::Format(NSLOCTEXT("NUDialogue", "InvalidDefaultDialogue", "기본 대화가 유효하지 않습니다: {0}"), FText::FromString(Error)));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (IsValid(MultipleChoicePromptDialogueDefinition))
	{
		FString Error;
		if (!MultipleChoicePromptDialogueDefinition->ValidateDefinition(Error))
		{
			Context.AddError(FText::Format(NSLOCTEXT("NUDialogue", "InvalidOpeningDialogue", "상호작용 질문 대사가 유효하지 않습니다: {0}"), FText::FromString(Error)));
			Result = EDataValidationResult::Invalid;
		}
	}

	TSet<FName> SeenOptionIds;
	for (int32 Index = 0; Index < DialogueOptions.Num(); ++Index)
	{
		const FNUDialogueOptionSpec& Spec = DialogueOptions[Index];
		if (Spec.OptionId == NUDialogueProvider::DefaultOptionId)
		{
			Context.AddError(FText::Format(NSLOCTEXT("NUDialogue", "ReservedDefaultDialogueOptionId", "추가 대화 주제[{0}]의 OptionId로 예약어 Default를 사용할 수 없습니다."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		else if (Spec.OptionId.IsNone() || Spec.DisplayText.IsEmpty())
		{
			Context.AddError(FText::Format(NSLOCTEXT("NUDialogue", "InvalidDialogueOptionIdentity", "추가 대화 주제[{0}]의 OptionId 또는 DisplayText가 비어 있습니다."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		else if (SeenOptionIds.Contains(Spec.OptionId))
		{
			Context.AddError(FText::Format(NSLOCTEXT("NUDialogue", "DuplicateDialogueOption", "중복된 대화 OptionId가 있습니다: {0}"), FText::FromName(Spec.OptionId)));
			Result = EDataValidationResult::Invalid;
		}
		SeenOptionIds.Add(Spec.OptionId);

		if (Spec.bEnabled && !IsValid(Spec.DialogueDefinition))
		{
			Context.AddError(FText::Format(NSLOCTEXT("NUDialogue", "MissingOptionDialogue", "추가 대화 주제[{0}]의 DialogueDefinition이 비어 있습니다."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
