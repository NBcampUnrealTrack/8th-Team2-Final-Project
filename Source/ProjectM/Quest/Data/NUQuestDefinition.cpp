#include "Quest/Data/NUQuestDefinition.h"

#include "Dialogue/Data/NUQuestDialogueDefinition.h"
#include "NUGAS/NUGameplayTags.h"

bool UNUQuestDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();

	if (QuestId.IsNone())
	{
		OutError = TEXT("QuestId가 설정되지 않았습니다.");
		return false;
	}

	if (!CategoryTag.MatchesTag(NU::Quest::Category::Root))
	{
		OutError = TEXT("CategoryTag가 NU.Quest.Category 하위 태그로 설정되지 않았습니다.");
		return false;
	}

	if (!bAutoAccept && OfferActorId.IsNone())
	{
		OutError = TEXT("수동 수락 퀘스트에는 OfferActorId가 필요합니다.");
		return false;
	}

	if (TurnInActorId.IsNone() && !bAutoComplete)
	{
		OutError = TEXT("TurnInActorId가 없습니다. 자동 완료 퀘스트가 아니면 보고받을 액터가 있어야 합니다.");
		return false;
	}

	if (RepeatPolicy == ENUQuestRepeatPolicy::Daily || RepeatPolicy == ENUQuestRepeatPolicy::Weekly)
	{
		OutError = TEXT("Daily/Weekly 반복 정책은 서버 리셋 기준이 구현되기 전까지 사용할 수 없습니다.");
		return false;
	}

	if (!QuestDialogue.IsNull())
	{
		const UNUQuestDialogueDefinition* Dialogue = QuestDialogue.LoadSynchronous();
		if (!IsValid(Dialogue))
		{
			OutError = TEXT("QuestDialogue 에셋을 불러올 수 없습니다.");
			return false;
		}

		if (Dialogue->QuestId != QuestId)
		{
			OutError = FString::Printf(
				TEXT("QuestDialogue의 QuestId [%s]가 퀘스트 [%s]와 일치하지 않습니다."),
				*Dialogue->QuestId.ToString(),
				*QuestId.ToString());
			return false;
		}

		FString DialogueError;
		if (!Dialogue->ValidateDefinition(DialogueError))
		{
			OutError = FString::Printf(TEXT("QuestDialogue가 유효하지 않습니다: %s"), *DialogueError);
			return false;
		}
	}

	for (int32 RequirementIndex = 0; RequirementIndex < StartRequirements.Num(); ++RequirementIndex)
	{
		const FNUQuestRequirementSpec& Requirement = StartRequirements[RequirementIndex];

		FString RequirementError;
		if (!Requirement.IsApplicable(RequirementError))
		{
			OutError = FString::Printf(TEXT("StartRequirements[%d]: %s"), RequirementIndex, *RequirementError);
			return false;
		}
	}

	if (!bAutoComplete && Objectives.Num() == 0)
	{
		OutError = TEXT("자동 완료 퀘스트가 아닌데 Objectives가 비어 있습니다.");
		return false;
	}

	if (!Objectives.IsEmpty()
		&& !Objectives.ContainsByPredicate([](const FNUQuestObjectiveSpec& Objective)
		{
			return !Objective.bOptional;
		}))
	{
		OutError = TEXT("모든 목표가 bOptional입니다. 완료 기준이 되는 필수 목표가 하나 이상 필요합니다.");
		return false;
	}

	// 숨긴 목표만 남으면 목표 창에 할 일이 보이지 않는다.
	if (!Objectives.IsEmpty()
		&& !Objectives.ContainsByPredicate([](const FNUQuestObjectiveSpec& Objective)
		{
			return !Objective.bHideInObjectiveList;
		}))
	{
		OutError = TEXT("모든 목표가 bHideInObjectiveList입니다. 목표 창에 보일 목표가 하나 이상 필요합니다.");
		return false;
	}

	// 준비 단계는 끝나면 목록에서 빠지고 뒤이은 목표 줄이 대신하므로, 표시되는 목표의 선행이어야 한다.
	for (const FNUQuestObjectiveSpec& Hidden : Objectives)
	{
		if (!Hidden.bHideInObjectiveList)
		{
			continue;
		}

		const bool bHasVisibleFollower = Objectives.ContainsByPredicate([&Hidden](const FNUQuestObjectiveSpec& Candidate)
		{
			return !Candidate.bHideInObjectiveList && Candidate.PrerequisiteObjectiveId == Hidden.ObjectiveId;
		});
		if (!bHasVisibleFollower)
		{
			OutError = FString::Printf(TEXT("준비 단계 목표 [%s]를 선행으로 둔 일반 목표가 없습니다. 대화를 마친 뒤 목표 창이 비게 됩니다."), *Hidden.ObjectiveId.ToString());
			return false;
		}
	}

	TMap<FName, FName> ObjectivePrerequisites;

	for (int32 ObjectiveIndex = 0; ObjectiveIndex < Objectives.Num(); ++ObjectiveIndex)
	{
		const FNUQuestObjectiveSpec& Objective = Objectives[ObjectiveIndex];

		if (!Objective.IsConfigured())
		{
			OutError = FString::Printf(TEXT("Objectives[%d]의 필수 값이 설정되지 않았습니다."),ObjectiveIndex);
			return false;
		}

		if (ObjectivePrerequisites.Contains(Objective.ObjectiveId))
		{
			OutError = FString::Printf(TEXT("중복된 ObjectiveId가 있습니다: %s"), *Objective.ObjectiveId.ToString());
			return false;
		}

		ObjectivePrerequisites.Add(Objective.ObjectiveId, Objective.PrerequisiteObjectiveId);
	}

	// 표시 순서와 무관하게 같은 퀘스트의 목표만 참조해야 한다.
	for (const auto& Pair : ObjectivePrerequisites)
	{
		if (!Pair.Value.IsNone() && !ObjectivePrerequisites.Contains(Pair.Value))
		{
			OutError = FString::Printf(TEXT("목표 [%s]의 선행 목표 [%s]가 같은 퀘스트에 없습니다."), *Pair.Key.ToString(), *Pair.Value.ToString());
			return false;
		}
	}

	// 이미 확인한 경로를 재사용하며 자기 참조와 간접 순환을 검사한다.
	TSet<FName> ValidatedIds;
	for (const auto& Pair : ObjectivePrerequisites)
	{
		TSet<FName> Path;
		FName CurrentId = Pair.Key;
		while (!CurrentId.IsNone() && !ValidatedIds.Contains(CurrentId))
		{
			if (Path.Contains(CurrentId))
			{
				OutError = FString::Printf(TEXT("목표 [%s]의 선행 조건에 순환 참조가 있습니다."), *CurrentId.ToString());
				return false;
			}
			Path.Add(CurrentId);
			CurrentId = ObjectivePrerequisites.FindChecked(CurrentId);
		}
		ValidatedIds.Append(Path);
	}

	for (int32 RewardIndex = 0; RewardIndex < Rewards.Num(); ++RewardIndex)
	{
		const FNUQuestRewardSpec& Reward = Rewards[RewardIndex];

		FString RewardError;
		if (!Reward.IsApplicable(RewardError))
		{
			OutError = FString::Printf(TEXT("Rewards[%d]: %s"), RewardIndex, *RewardError);
			return false;
		}
	}

	return true;
}

const FNUQuestObjectiveSpec* UNUQuestDefinition::FindTurnInTalkObjective() const
{
	if (bAutoComplete || TurnInActorId.IsNone())
	{
		return nullptr;
	}

	// 마지막 필수 목표가 둘 이상이면 어느 쪽이 나중에 끝날지 정할 수 없으므로 보고 단계를 그대로 둔다.
	const FNUQuestObjectiveSpec* LastObjective = nullptr;
	for (const FNUQuestObjectiveSpec& Objective : Objectives)
	{
		const bool bHasRequiredFollower = Objectives.ContainsByPredicate([&Objective](const FNUQuestObjectiveSpec& Candidate)
		{
			return !Candidate.bOptional && Candidate.PrerequisiteObjectiveId == Objective.ObjectiveId;
		});
		if (Objective.bOptional || bHasRequiredFollower)
		{
			continue;
		}

		if (LastObjective != nullptr)
		{
			return nullptr;
		}
		LastObjective = &Objective;
	}

	const bool bTalksToTurnInActor = LastObjective != nullptr
		&& LastObjective->ObjectiveType == NU::Quest::Objective::Talk
		&& LastObjective->TargetId == TurnInActorId;
	return bTalksToTurnInActor ? LastObjective : nullptr;
}
