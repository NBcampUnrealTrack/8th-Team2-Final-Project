#include "Quest/Component/NUQuestProviderComponent.h"

#include "CharacterState/NUPlayerState.h"
#include "Dialogue/Data/NUQuestDialogueDefinition.h"
#include "Dialogue/Data/NUDialogueDataSubsystem.h"
#include "Field/FieldPlayerCharacter.h"
#include "Field/NPC/NUNPCBase.h"
#include "Field/Presence/NUActorPresenceComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Interaction/Component/NUInteractorComponent.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestEvent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
	// 퀘스트 하나의 단계를 판정한다. 역할(제안/완료) 구분은 아래 필터가 맡는다
	ENUQuestInteractionPhase ResolveInteractionPhase(const UNUQuestComponent* QuestComponent, const UNUQuestDefinition* Definition, const FNUQuestProgressState* Progress)
	{
		if (Progress == nullptr)
		{
			const bool bCanOffer = IsValid(QuestComponent)
				&& Definition != nullptr
				&& QuestComponent->AreStartRequirementsMet(Definition);

			return bCanOffer
				? ENUQuestInteractionPhase::Offer
				: ENUQuestInteractionPhase::Locked;
		}

		switch (Progress->QuestState)
		{
		case ENUQuestState::Locked:
			return ENUQuestInteractionPhase::Locked;

		case ENUQuestState::Available:
			return ENUQuestInteractionPhase::Offer;

		case ENUQuestState::Active:
			return ENUQuestInteractionPhase::InProgress;

		case ENUQuestState::ReadyToTurnIn:
			return ENUQuestInteractionPhase::TurnIn;

		case ENUQuestState::Completed:
			// 반복 퀘스트는 완료 후 재상호작용이 곧 다음 제안이다
			return Definition != nullptr && Definition->RepeatPolicy == ENUQuestRepeatPolicy::Repeatable
				? ENUQuestInteractionPhase::Offer
				: ENUQuestInteractionPhase::Completed;

		default:
			return ENUQuestInteractionPhase::Idle;
		}
	}

	// 완료만 받는 액터가 퀘스트를 제안해버리는 것을 막는다
	ENUQuestInteractionPhase FilterOfferRole(ENUQuestInteractionPhase Phase)
	{
		return Phase == ENUQuestInteractionPhase::TurnIn
			? ENUQuestInteractionPhase::Idle
			: Phase;
	}

	// 제안만 하는 액터가 완료 보고를 받아버리는 것을 막는다
	ENUQuestInteractionPhase FilterTurnInRole(ENUQuestInteractionPhase Phase)
	{
		return Phase == ENUQuestInteractionPhase::Offer || Phase == ENUQuestInteractionPhase::Locked
			? ENUQuestInteractionPhase::Idle
			: Phase;
	}

	// 값이 작을수록 우선. 머리 위에는 하나만 그릴 수 있어 순서가 필요하다
	int32 GetPhaseSortOrder(ENUQuestInteractionPhase Phase)
	{
		switch (Phase)
		{
		case ENUQuestInteractionPhase::TurnIn:		return 0;
		case ENUQuestInteractionPhase::Offer:		return 1;
		case ENUQuestInteractionPhase::Target:		return 2;
		case ENUQuestInteractionPhase::InProgress:	return 3;
		case ENUQuestInteractionPhase::Locked:		return 4;
		case ENUQuestInteractionPhase::Completed:	return 5;
		default:									return 6;
		}
	}

	ENUInteractionOptionGroup GetInteractionOptionGroup(ENUQuestInteractionPhase Phase)
	{
		switch (Phase)
		{
		case ENUQuestInteractionPhase::TurnIn:		return ENUInteractionOptionGroup::QuestTurnIn;
		case ENUQuestInteractionPhase::Offer:		return ENUInteractionOptionGroup::QuestOffer;
		case ENUQuestInteractionPhase::Target:		return ENUInteractionOptionGroup::QuestTarget;
		case ENUQuestInteractionPhase::InProgress:	return ENUInteractionOptionGroup::QuestInProgress;
		default:								return ENUInteractionOptionGroup::Other;
		}
	}

	FText GetInteractionOptionCategory(const UNUInteractionSettings* Settings, ENUQuestInteractionPhase Phase)
	{
		switch (Phase)
		{
		case ENUQuestInteractionPhase::Offer:
			return Settings->QuestCategoryText;
		case ENUQuestInteractionPhase::InProgress:
			return Settings->QuestInProgressCategoryText;
		case ENUQuestInteractionPhase::TurnIn:
			return Settings->QuestTurnInCategoryText;
		case ENUQuestInteractionPhase::Target:
			return Settings->QuestTargetCategoryText;
		default:
			return Settings->QuestCategoryText;
		}
	}
}

UNUQuestProviderComponent::UNUQuestProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ObjectiveType = NU::Quest::Objective::Talk;
	ObjectiveInteractionLabel = NSLOCTEXT("NUQuest", "ObjectiveInteraction", "조사하기");
}

void UNUQuestProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!Context.IsValid() || Context.Target.Get() != GetOwner())
	{
		return;
	}

	const TArray<FNUQuestInteractionOption> QuestOptions = CollectQuestInteractionOptions(Context.Interactor);
	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();

	for (const FNUQuestInteractionOption& QuestOption : QuestOptions)
	{
		// 지금 실제로 처리할 수 없는 퀘스트 상태는 NPC 선택지에 노출하지 않는다.
		if (!IsProviderOptionVisible(Context.Interactor, QuestOption))
		{
			continue;
		}

		FText DisplayText = ObjectiveInteractionLabel;
		int32 SortOrder = 0;
		if (!QuestOption.QuestId.IsNone())
		{
			const UNUQuestDefinition* Definition = FindQuestDefinitionById(QuestOption.QuestId);
			if (Definition == nullptr)
			{
				continue;
			}

			DisplayText = Definition->Title.IsEmpty() ? FText::FromName(QuestOption.QuestId) : Definition->Title;
			SortOrder = Definition->SortOrder;
		}

		FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
		Option.Handle = MakeProviderHandle(QuestOption);
		Option.DisplayText = DisplayText;
		Option.CategoryText = GetInteractionOptionCategory(Settings, QuestOption.Phase);
		Option.Group = GetInteractionOptionGroup(QuestOption.Phase);
		Option.SortOrder = SortOrder;
		Option.bEnabled = true;
	}
}

bool UNUQuestProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	if (!Context.IsValid() || Context.Target.Get() != GetOwner())
	{
		return false;
	}

	FNUQuestInteractionOption Option;
	return ResolveProviderOption(Context.Interactor, Handle, Option)
		&& IsProviderOptionVisible(Context.Interactor, Option)
		&& IsInteractionOptionValid(Context.Interactor, Option);
}

bool UNUQuestProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!Context.Interactor->HasAuthority() || !CanExecuteInteraction_Implementation(Context, Handle))
	{
		return false;
	}

	FNUQuestInteractionOption Option;
	if (!ResolveProviderOption(Context.Interactor, Handle, Option))
	{
		return false;
	}

	return RequestDialogue(Context.Interactor, Option) || HandleQuestInteraction(Context.Interactor, Option);
}

// -----------------------------------------------------------------------------
// 상호작용
// -----------------------------------------------------------------------------

bool UNUQuestProviderComponent::HandleInteraction(AActor* Interactor)
{
	FNUQuestInteractionOption Option;
	ResolveInteractionTarget(Interactor, Option);

	// 대화가 설정된 액터는 대사를 모두 확인한 뒤 퀘스트 처리로 넘어간다.
	// 대화 에셋을 사용하지 않으면 즉시 퀘스트를 처리한다.
	if (RequestDialogue(Interactor, Option))
	{
		return true;
	}

	return HandleQuestInteraction(Interactor, Option);
}

bool UNUQuestProviderComponent::HandleDialogueFinished(AActor* Interactor, const FNUQuestInteractionOption& Option)
{
	return HandleQuestInteraction(Interactor, Option);
}

bool UNUQuestProviderComponent::ContinueAcceptedQuest(AActor* Interactor, FName QuestId)
{
	const UNUQuestComponent* QuestComponent = FindQuestComponent(Interactor);
	if (QuestComponent == nullptr || ObjectiveType != NU::Quest::Objective::Talk
		|| QuestComponent->FindActiveObjectiveId(QuestId, ObjectiveType, QuestActorId, GetObjectiveSourceInstanceId()).IsNone())
	{
		return false;
	}

	// 수락 뒤 자리를 떠났으면 다음에 직접 말을 걸 때 진행한다.
	const AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(Interactor);
	if (FieldCharacter == nullptr || FieldCharacter->InteractionComponent == nullptr
		|| !FieldCharacter->InteractionComponent->RevalidateInteractionTarget(GetOwner()))
	{
		return false;
	}

	// 대화창에서 이 퀘스트 선택지를 고른 것과 같은 경로로 실행한다.
	FNUInteractionContext Context;
	Context.Interactor = Interactor;
	Context.Target = GetOwner();

	FNUInteractionOptionHandle Handle;
	Handle.OptionType = NU::Interaction::Option::Quest;
	Handle.OptionId = QuestId;
	return ExecuteInteraction_Implementation(Context, Handle);
}

bool UNUQuestProviderComponent::HandleQuestInteraction(AActor* Interactor, const FNUQuestInteractionOption& Option)
{
	UNUQuestComponent* QuestComponent = FindQuestComponent(Interactor);
	if (QuestComponent == nullptr)
	{
		return false;
	}

	// 대화를 열 때 정한 조합이 아직 유효한지만 확인한다. 다시 고르지는 않는다
	if (!IsInteractionOptionValid(Interactor, Option))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestProvider: 유효하지 않은 상호작용 QuestId=%s Phase=%s"),
			*Option.QuestId.ToString(),
			*UEnum::GetValueAsString(Option.Phase)
		);
		return false;
	}

	const bool bInteractionHandled = Option.Phase == ENUQuestInteractionPhase::Target
		&& (!GrantedItemRowName.IsNone() ? GrantConfiguredTargetItem(Interactor) : SendObjectiveEvent(*QuestComponent));

	if (Option.QuestId.IsNone())
	{
		return bInteractionHandled;
	}

	const UNUQuestDefinition* Definition = FindQuestDefinitionById(Option.QuestId);
	if (Definition == nullptr)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestProvider: 정의를 찾지 못함 QuestId=%s"), *Option.QuestId.ToString());
		return bInteractionHandled;
	}

	switch (Option.Phase)
	{
	case ENUQuestInteractionPhase::TurnIn:
	{
		const bool bTurnedIn = HandleTurnInQuest(*QuestComponent, *Definition);
		return bTurnedIn || bInteractionHandled;
	}

	case ENUQuestInteractionPhase::Offer:
	case ENUQuestInteractionPhase::Locked:
	{
		const bool bOffered = HandleOfferedQuest(*QuestComponent, *Definition);
		return bOffered || bInteractionHandled;
	}

	default:
		return bInteractionHandled;
	}
}

bool UNUQuestProviderComponent::GrantConfiguredTargetItem(AActor* Interactor) const
{
	const APawn* InteractorPawn = Cast<APawn>(Interactor);
	const ANUPlayerState* PlayerState = InteractorPawn != nullptr
		? InteractorPawn->GetPlayerState<ANUPlayerState>()
		: nullptr;
	UNUInventoryComponent* Inventory = PlayerState != nullptr
		? PlayerState->FindComponentByClass<UNUInventoryComponent>()
		: nullptr;

	if (!IsValid(Inventory) || GrantedItemRowName.IsNone() || GrantedItemCount <= 0)
	{
		return false;
	}

	const bool bGranted = Inventory->AddItem(GrantedItemRowName, GrantedItemCount);
	if (bGranted)
	{
		UE_LOG(LogNUQuest, Log, TEXT("QuestProvider: 목표 아이템 획득 ItemId=%s Count=%d"),
			*GrantedItemRowName.ToString(),
			GrantedItemCount);
	}

	return bGranted;
}

FName UNUQuestProviderComponent::FindTableDialogue(const FNUQuestInteractionOption& Option) const
{
	const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUDialogueDataSubsystem* DialogueData = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUDialogueDataSubsystem>() : nullptr;
	if (NPC == nullptr || DialogueData == nullptr) { return NAME_None; }
	const FName DialogueId = DialogueData->FindQuestDialogue(NPC->GetNpcId(), Option);
	if (!DialogueId.IsNone() || Option.Phase != ENUQuestInteractionPhase::Target) { return DialogueId; }

	// 마지막 대화 목표가 곧 보상 수령이면 완료 보고 대사를 그 대화에서 먼저 읽는다.
	const UNUQuestDefinition* Definition = FindQuestDefinitionById(Option.QuestId);
	const FNUQuestObjectiveSpec* TurnInTalk = Definition != nullptr ? Definition->FindTurnInTalkObjective() : nullptr;
	if (TurnInTalk == nullptr || TurnInTalk->ObjectiveId != Option.ObjectiveId) { return NAME_None; }
	FNUQuestInteractionOption ReportOption = Option;
	ReportOption.Phase = ENUQuestInteractionPhase::TurnIn;
	ReportOption.ObjectiveId = NAME_None;
	return DialogueData->FindQuestDialogue(NPC->GetNpcId(), ReportOption);
}

bool UNUQuestProviderComponent::HasDialogueFor(const FNUQuestInteractionOption& Option) const
{
	return !FindTableDialogue(Option).IsNone() || IsValid(SelectDialogueDefinition(Option));
}

bool UNUQuestProviderComponent::RequestDialogue(AActor* Interactor, const FNUQuestInteractionOption& Option) const
{
	const FName DialogueId = FindTableDialogue(Option);
	if (!DialogueId.IsNone())
	{
		const APawn* Pawn = Cast<APawn>(Interactor);
		ANUPlayerController* PC = Pawn != nullptr ? Cast<ANUPlayerController>(Pawn->GetController()) : nullptr;
		if (IsValid(PC) && Interactor->HasAuthority())
		{
			PC->ClientOpenQuestDialogueSequence(DialogueId, GetOwner(), Option);
			return true;
		}
		return false;
	}

	UNUQuestDialogueDefinition* SelectedDialogue = SelectDialogueDefinition(Option);
	if (!IsValid(SelectedDialogue))
	{
		return false;
	}

	if (!SelectedDialogue->HasPhase(Option.Phase))
	{
		return false;
	}

	FString ValidationError;
	if (!SelectedDialogue->ValidateDefinition(ValidationError))
	{
		UE_LOG(LogNUQuest, Warning,
			TEXT("QuestProvider: 대사 정의가 유효하지 않아 대화를 건너뜁니다. Dialogue=%s Error=%s"),
			*GetNameSafe(SelectedDialogue),
			*ValidationError);
		return false;
	}

	if (!IsValid(Interactor) || !Interactor->HasAuthority())
	{
		return false;
	}

	const APawn* InteractorPawn = Cast<APawn>(Interactor);
	ANUPlayerController* PlayerController = InteractorPawn != nullptr
		? Cast<ANUPlayerController>(InteractorPawn->GetController())
		: nullptr;
	AActor* InteractionOwner = GetOwner();

	if (!IsValid(PlayerController) || !IsValid(InteractionOwner))
	{
		return false;
	}

	PlayerController->ClientOpenQuestDialogue(SelectedDialogue, InteractionOwner, Option);
	return true;
}

ENUQuestInteractionPhase UNUQuestProviderComponent::ResolvePhaseForQuest(const UNUQuestComponent* QuestComponent, FName QuestId) const
{
	if (QuestId.IsNone() || QuestComponent == nullptr)
	{
		return ENUQuestInteractionPhase::Idle;
	}

	const UNUQuestDefinition* Definition = FindQuestDefinitionById(QuestId);
	if (Definition == nullptr)
	{
		return ENUQuestInteractionPhase::Idle;
	}

	return ResolveInteractionPhase(QuestComponent, Definition, QuestComponent->FindQuestProgress(QuestId));
}

TArray<FNUQuestInteractionOption> UNUQuestProviderComponent::CollectQuestInteractionOptions(AActor* Interactor) const
{
	TArray<FNUQuestInteractionOption> Options;
	const UNUActorPresenceComponent* Presence = GetOwner()->FindComponentByClass<UNUActorPresenceComponent>();
	if (Presence != nullptr && Presence->IsPlacementTransitionPending()) { return Options; }

	const UNUQuestDataSubsystem* QuestDataSubsystem = FindQuestDataSubsystem();
	const UNUQuestComponent* QuestComponent = FindQuestComponentForQuery(Interactor);

	if (QuestDataSubsystem == nullptr || !IsValid(QuestComponent) || QuestActorId.IsNone())
	{
		return Options;
	}

	auto AddOption = [&Options](FName QuestId, ENUQuestInteractionPhase Phase, FName ObjectiveId = NAME_None)
	{
		if (Phase == ENUQuestInteractionPhase::Idle)
		{
			return;
		}

		const bool bAlreadyAdded = Options.ContainsByPredicate(
			[QuestId, Phase](const FNUQuestInteractionOption& ExistingOption)
			{
				return ExistingOption.QuestId == QuestId && ExistingOption.Phase == Phase;
			});
		if (bAlreadyAdded)
		{
			return;
		}

		FNUQuestInteractionOption& Option = Options.AddDefaulted_GetRef();
		Option.QuestId = QuestId;
		Option.Phase = Phase;
		Option.ObjectiveId = ObjectiveId;
	};

	for (const FName QuestId : QuestDataSubsystem->GetTurnInQuestsForActor(QuestActorId))
	{
		AddOption(QuestId, FilterTurnInRole(ResolvePhaseForQuest(QuestComponent, QuestId)));
	}

	for (const FName QuestId : QuestDataSubsystem->GetOfferedQuestsForActor(QuestActorId))
	{
		AddOption(QuestId, FilterOfferRole(ResolvePhaseForQuest(QuestComponent, QuestId)));
	}

	// 테이블에 연결한 주민의 안내 대사도 기존 선택지 수집 규칙에 포함한다.
	const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUDialogueDataSubsystem* DialogueData = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUDialogueDataSubsystem>() : nullptr;
	if (NPC != nullptr && DialogueData != nullptr)
	{
		for (FName QuestId : DialogueData->GetInProgressQuestIds(NPC->GetNpcId()))
		{
			const FNUQuestProgressState* Progress = QuestComponent->FindQuestProgress(QuestId);
			if (Progress != nullptr && Progress->QuestState == ENUQuestState::Active) { AddOption(QuestId, ENUQuestInteractionPhase::InProgress); }
		}
	}

	// 기존 에셋을 사용하는 의뢰의 안내도 유지한다.
	for (const auto& Pair : QuestDialogueOverrides)
	{
		const FNUQuestProgressState* Progress = QuestComponent->FindQuestProgress(Pair.Key);
		if (Progress != nullptr && Progress->QuestState == ENUQuestState::Active && IsValid(Pair.Value) && Pair.Value->HasPhase(ENUQuestInteractionPhase::InProgress))
		{
			AddOption(Pair.Key, ENUQuestInteractionPhase::InProgress);
		}
	}

	// 목표 상호작용도 QuestId를 보존한다. 같은 퀘스트의 일반 진행 중 항목 대신
	// Target 항목을 사용해야 어느 퀘스트의 Target 대사인지 정확히 선택할 수 있다.
	for (const FName QuestId : QuestComponent->GetActiveObjectiveQuestIds(
		ObjectiveType,
		QuestActorId,
		GetObjectiveSourceInstanceId()))
	{
		Options.RemoveAll([QuestId](const FNUQuestInteractionOption& ExistingOption)
		{
			return ExistingOption.QuestId == QuestId
				&& ExistingOption.Phase == ENUQuestInteractionPhase::InProgress;
		});
		AddOption(QuestId, ENUQuestInteractionPhase::Target, QuestComponent->FindActiveObjectiveId(QuestId, ObjectiveType, QuestActorId, GetObjectiveSourceInstanceId()));
	}

	// 단계와 기획 정렬값이 같을 때만 QuestId로 동률을 깬다.
	Options.Sort([this](const FNUQuestInteractionOption& A, const FNUQuestInteractionOption& B)
	{
		const int32 PhaseOrderA = GetPhaseSortOrder(A.Phase);
		const int32 PhaseOrderB = GetPhaseSortOrder(B.Phase);

		if (PhaseOrderA != PhaseOrderB)
		{
			return PhaseOrderA < PhaseOrderB;
		}

		const UNUQuestDefinition* DefinitionA = FindQuestDefinitionById(A.QuestId);
		const UNUQuestDefinition* DefinitionB = FindQuestDefinitionById(B.QuestId);
		if (IsValid(DefinitionA) && IsValid(DefinitionB) && DefinitionA->SortOrder != DefinitionB->SortOrder)
		{
			return DefinitionA->SortOrder < DefinitionB->SortOrder;
		}
		if (IsValid(DefinitionA) != IsValid(DefinitionB))
		{
			return IsValid(DefinitionA);
		}

		return A.QuestId.LexicalLess(B.QuestId);
	});

	return Options;
}

ENUQuestInteractionPhase UNUQuestProviderComponent::GetOfferedQuestPhase(AActor* Interactor) const
{
	const TArray<FNUQuestInteractionOption> Options = CollectQuestInteractionOptions(Interactor);

	return Options.IsEmpty() ? ENUQuestInteractionPhase::Idle : Options[0].Phase;
}

bool UNUQuestProviderComponent::IsActiveObjectiveTarget(AActor* Interactor) const
{
	if (QuestActorId.IsNone() || !ObjectiveType.IsValid())
	{
		return false;
	}

	const UNUQuestComponent* QuestComponent = FindQuestComponentForQuery(Interactor);

	return IsValid(QuestComponent)
		&& QuestComponent->HasActiveObjectiveFor(
			ObjectiveType,
			QuestActorId,
			GetObjectiveSourceInstanceId());
}

ENUQuestInteractionPhase UNUQuestProviderComponent::GetInteractionPhase(AActor* Interactor) const
{
	FNUQuestInteractionOption Option;
	ResolveInteractionTarget(Interactor, Option);

	return Option.Phase;
}

void UNUQuestProviderComponent::ResolveInteractionTarget(AActor* Interactor, FNUQuestInteractionOption& OutOption) const
{
	OutOption = FNUQuestInteractionOption();

	const TArray<FNUQuestInteractionOption> Options = CollectQuestInteractionOptions(Interactor);
	if (!Options.IsEmpty())
	{
		OutOption = Options[0];
	}
}

bool UNUQuestProviderComponent::IsInteractionOptionValid(AActor* Interactor, const FNUQuestInteractionOption& Option) const
{
	const UNUActorPresenceComponent* Presence = GetOwner()->FindComponentByClass<UNUActorPresenceComponent>();
	if (Presence != nullptr && Presence->IsPlacementTransitionPending()) { return false; }
	if (Option.Phase == ENUQuestInteractionPhase::Idle)
	{
		return Option.QuestId.IsNone()
			&& CollectQuestInteractionOptions(Interactor).IsEmpty()
			&& !IsActiveObjectiveTarget(Interactor);
	}

	if (Option.Phase == ENUQuestInteractionPhase::Target)
	{
		const UNUQuestComponent* QuestComponent = FindQuestComponentForQuery(Interactor);
		return !Option.QuestId.IsNone()
			&& IsValid(QuestComponent)
			&& !Option.ObjectiveId.IsNone()
			&& QuestComponent->FindActiveObjectiveId(Option.QuestId, ObjectiveType, QuestActorId, GetObjectiveSourceInstanceId()) == Option.ObjectiveId;
	}

	return CollectQuestInteractionOptions(Interactor).ContainsByPredicate(
		[&Option] (const FNUQuestInteractionOption& Candidate)
		{
			return Candidate.QuestId == Option.QuestId && Candidate.Phase == Option.Phase;
		}
	);
}

UNUQuestDialogueDefinition* UNUQuestProviderComponent::SelectDialogueDefinition(const FNUQuestInteractionOption& Option) const
{
	if (Option.QuestId.IsNone())
	{
		return nullptr;
	}

	if (Option.Phase == ENUQuestInteractionPhase::Target && !Option.ObjectiveId.IsNone())
	{
		if (const TObjectPtr<UNUQuestDialogueDefinition>* Override = ObjectiveDialogueOverrides.Find(Option.ObjectiveId))
		{
			UNUQuestDialogueDefinition* Dialogue = Override->Get();
			if (IsValid(Dialogue) && Dialogue->QuestId == Option.QuestId && Dialogue->HasPhase(Option.Phase)) { return Dialogue; }
		}
	}

	if (const TObjectPtr<UNUQuestDialogueDefinition>* Override = QuestDialogueOverrides.Find(Option.QuestId))
	{
		UNUQuestDialogueDefinition* Dialogue = Override->Get();
		if (IsValid(Dialogue) && Dialogue->QuestId == Option.QuestId && Dialogue->HasPhase(Option.Phase))
		{
			return Dialogue;
		}

		UE_LOG(LogNUQuest, Verbose, TEXT("QuestProvider: NPC별 대사가 없어 공용 대사를 조회 Actor=%s Dialogue=%s QuestId=%s Phase=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Dialogue),
			*Option.QuestId.ToString(),
			*UEnum::GetValueAsString(Option.Phase));
	}

	if (Option.Phase == ENUQuestInteractionPhase::Target && IsValid(TargetDialogueOverride))
	{
		if (TargetDialogueOverride->QuestId == Option.QuestId
			&& TargetDialogueOverride->HasPhase(ENUQuestInteractionPhase::Target))
		{
			return TargetDialogueOverride;
		}

		UE_LOG(LogNUQuest, Warning, TEXT("QuestProvider: Target 대사 오버라이드가 현재 퀘스트와 맞지 않음 Actor=%s Dialogue=%s QuestId=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(TargetDialogueOverride),
			*Option.QuestId.ToString());
	}

	const UNUQuestDefinition* Definition = FindQuestDefinitionById(Option.QuestId);
	if (!IsValid(Definition) || Definition->QuestDialogue.IsNull())
	{
		return nullptr;
	}

	UNUQuestDialogueDefinition* Dialogue = Definition->QuestDialogue.LoadSynchronous();
	if (!IsValid(Dialogue) || Dialogue->QuestId != Option.QuestId)
	{
		return nullptr;
	}

	return Dialogue->HasPhase(Option.Phase) ? Dialogue : nullptr;
}

bool UNUQuestProviderComponent::IsProviderOptionVisible(AActor* Interactor, const FNUQuestInteractionOption& Option) const
{
	switch (Option.Phase)
	{
	case ENUQuestInteractionPhase::Offer:
	{
		const UNUQuestDefinition* Definition = FindQuestDefinitionById(Option.QuestId);
		const UNUQuestComponent* QuestComponent = FindQuestComponentForQuery(Interactor);
		return IsValid(Definition)
			&& IsValid(QuestComponent)
			&& QuestComponent->AreStartRequirementsMet(Definition);
	}

	case ENUQuestInteractionPhase::TurnIn:
		return !Option.QuestId.IsNone();

	// 진행 중 항목은 안내 대사를 들려주는 것 말고 하는 일이 없다. 진행 중 퀘스트의 제안·보고 NPC라도
	// 이 NPC의 진행 중 대사가 없으면 골라도 아무 일이 일어나지 않으므로 보여주지 않는다.
	case ENUQuestInteractionPhase::InProgress:
		return !Option.QuestId.IsNone() && HasDialogueFor(Option);

	case ENUQuestInteractionPhase::Target:
		return IsInteractionOptionValid(Interactor, Option);

	default:
		return false;
	}
}

bool UNUQuestProviderComponent::ResolveProviderOption(AActor* Interactor, const FNUInteractionOptionHandle& Handle, FNUQuestInteractionOption& OutOption) const
{
	OutOption = FNUQuestInteractionOption();
	if (Handle.OptionType != NU::Interaction::Option::Quest || Handle.OptionId.IsNone())
	{
		return false;
	}

	const TArray<FNUQuestInteractionOption> Options = CollectQuestInteractionOptions(Interactor);
	const FNUQuestInteractionOption* FoundOption = Options.FindByPredicate([this, &Handle](const FNUQuestInteractionOption& Option)
	{
		return MakeProviderHandle(Option) == Handle;
	});
	if (FoundOption == nullptr)
	{
		return false;
	}

	OutOption = *FoundOption;
	return true;
}

FNUInteractionOptionHandle UNUQuestProviderComponent::MakeProviderHandle(const FNUQuestInteractionOption& Option) const
{
	FNUInteractionOptionHandle Handle;
	Handle.OptionType = NU::Interaction::Option::Quest;
	Handle.OptionId = Option.QuestId;
	return Handle;
}

UNUQuestComponent* UNUQuestProviderComponent::FindQuestComponent(AActor* Interactor) const
{
	// 퀘스트 변경은 서버 권한에서만 한다
	if (!IsValid(Interactor) || !Interactor->HasAuthority())
	{
		return nullptr;
	}

	return FindQuestComponentForQuery(Interactor);
}

UNUQuestComponent* UNUQuestProviderComponent::FindQuestComponentForQuery(AActor* Interactor) const
{
	if (!IsValid(Interactor))
	{
		return nullptr;
	}

	APawn* InteractorPawn = Cast<APawn>(Interactor);
	if (InteractorPawn == nullptr)
	{
		return nullptr;
	}

	ANUPlayerState* PlayerState = InteractorPawn->GetPlayerState<ANUPlayerState>();
	if (PlayerState == nullptr)
	{
		return nullptr;
	}

	return PlayerState->GetQuestComponent();
}

const UNUQuestDataSubsystem* UNUQuestProviderComponent::FindQuestDataSubsystem() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;

	return GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
}

const UNUQuestDefinition* UNUQuestProviderComponent::FindQuestDefinitionById(FName QuestId) const
{
	if (QuestId.IsNone())
	{
		return nullptr;
	}

	const UNUQuestDataSubsystem* QuestDataSubsystem = FindQuestDataSubsystem();

	return QuestDataSubsystem != nullptr ? QuestDataSubsystem->FindDefinition(QuestId) : nullptr;
}

bool UNUQuestProviderComponent::HandleOfferedQuest(UNUQuestComponent& QuestComponent, const UNUQuestDefinition& Definition)
{
	const FNUQuestProgressState* Progress = QuestComponent.FindQuestProgress(Definition.QuestId);

	switch (FilterOfferRole(ResolveInteractionPhase(&QuestComponent, &Definition, Progress)))
	{
	case ENUQuestInteractionPhase::Offer:
	{
		// 아직 등록되지 않았으면 먼저 등록한다.
		// 여기서 바로 수락하지 않아 제안과 수락을 서로 다른 입력으로 분리한다
		if (Progress == nullptr)
		{
			if (!QuestComponent.AddQuest(&Definition, ENUQuestState::Available))
			{
				return false;
			}

			Progress = QuestComponent.FindQuestProgress(Definition.QuestId);

	UE_LOG(LogNUQuest, Log, TEXT("QuestProvider: 퀘스트 제안 QuestId=%s State=%s"),
				*Definition.QuestId.ToString(),
				Progress != nullptr
				? *UEnum::GetValueAsString(Progress->QuestState)
				: TEXT("Unknown")
			);

			// bAutoAccept 등으로 Available이 아닐 수 있다
			if (Progress == nullptr || Progress->QuestState != ENUQuestState::Available)
			{
				return true;
			}
		}

		QuestComponent.RememberOfferSource(Definition.QuestId, GetOwner());
		QuestComponent.BroadcastQuestOffer(Definition.QuestId);
		return true;
	}

	case ENUQuestInteractionPhase::Locked:
	{
		// Locked로 등록해두면 조건이 채워질 때 EvaluateAvailableQuests가 풀어준다
		if (Progress == nullptr)
		{
			return QuestComponent.AddQuest(&Definition, ENUQuestState::Available);
		}

		UE_LOG(LogNUQuest, Log, TEXT("QuestProvider: 조건 미달 QuestId=%s"), *Definition.QuestId.ToString());
		return false;
	}

	default:
		return false;
	}
}

bool UNUQuestProviderComponent::HandleTurnInQuest(UNUQuestComponent& QuestComponent, const UNUQuestDefinition& Definition)
{
	const FNUQuestProgressState* Progress = QuestComponent.FindQuestProgress(Definition.QuestId);

	if (ResolveInteractionPhase(&QuestComponent, &Definition, Progress) != ENUQuestInteractionPhase::TurnIn)
	{
		return false;
	}

	const bool bCompleted = QuestComponent.CompleteQuest(&Definition, QuestActorId);

	if (bCompleted)
	{
	UE_LOG(LogNUQuest, Log, TEXT("QuestProvider: 퀘스트 완료 QuestId=%s"), *Definition.QuestId.ToString());
	}

	return bCompleted;
}

bool UNUQuestProviderComponent::SendObjectiveEvent(UNUQuestComponent& QuestComponent) const
{
	if (QuestActorId.IsNone() || !ObjectiveType.IsValid() || EventAmount <= 0)
	{
		return false;
	}

	FNUQuestEvent QuestEvent;
	QuestEvent.ObjectiveType = ObjectiveType;
	QuestEvent.TargetId = QuestActorId;
	QuestEvent.Amount = EventAmount;
	QuestEvent.SourceInstanceId = GetObjectiveSourceInstanceId();

	const bool bProgressChanged = QuestComponent.HandleQuestEvent(QuestEvent);

	if (bProgressChanged)
	{
	UE_LOG(LogNUQuest, Log, TEXT("QuestProvider: 목표 진행 TargetId=%s Amount=%d"),
			*QuestActorId.ToString(),
			EventAmount
		);
	}

	return bProgressChanged;
}

FName UNUQuestProviderComponent::GetObjectiveSourceInstanceId() const
{
	const AActor* Owner = GetOwner();
	return bCountEachActorOnce && IsValid(Owner)
		? Owner->GetFName()
		: NAME_None;
}
