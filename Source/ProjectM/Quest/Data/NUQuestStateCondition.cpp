#include "Quest/Data/NUQuestStateCondition.h"

#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestProgressTypes.h"

bool FNUQuestStateCondition::IsSet() const
{
	return QuestId > 0;
}

bool FNUQuestStateCondition::Validate(FString& OutError) const
{
	OutError.Reset();
	if (QuestId < 0 || ObjectiveId < 0)
	{
		OutError = TEXT("퀘스트와 목표 ID는 0 이상이어야 합니다.");
		return false;
	}
	if (!IsSet() && ObjectiveId != 0)
	{
		OutError = TEXT("목표 ID를 쓰려면 퀘스트 ID도 지정합니다.");
		return false;
	}
	if ((State == ENUQuestConditionState::Available || State == ENUQuestConditionState::AvailableOrInProgress) && ObjectiveId != 0)
	{
		OutError = TEXT("수락 전을 포함하는 조건은 퀘스트 전체에만 사용합니다.");
		return false;
	}
	return true;
}

bool FNUQuestStateCondition::IsMet(TFunctionRef<const FNUQuestProgressState*(FName QuestId)> FindProgress, TFunctionRef<const UNUQuestDefinition*(FName QuestId)> FindDefinition) const
{
	const FName QuestName = IsSet() ? FName(*LexToString(QuestId)) : NAME_None;
	const FName ObjectiveName = ObjectiveId > 0 ? FName(*LexToString(ObjectiveId)) : NAME_None;
	const FNUQuestProgressState* Progress = IsSet() ? FindProgress(QuestName) : nullptr;
	if (Progress == nullptr)
	{
		return false;
	}

	const bool bQuestCompleted = Progress->QuestState == ENUQuestState::Completed;
	if (ObjectiveId == 0)
	{
		switch (State)
		{
		case ENUQuestConditionState::Accepted:
			return bQuestCompleted || Progress->IsInProgress();
		case ENUQuestConditionState::InProgress:
			return Progress->IsInProgress();
		case ENUQuestConditionState::Completed:
			return bQuestCompleted;
		case ENUQuestConditionState::Available:
			return Progress->QuestState == ENUQuestState::Available;
		case ENUQuestConditionState::AvailableOrInProgress:
			return Progress->QuestState == ENUQuestState::Available || Progress->IsInProgress();
		}
		return false;
	}

	const UNUQuestDefinition* Definition = FindDefinition(QuestName);
	const FNUQuestObjectiveSpec* Objective = Definition != nullptr
		? Definition->Objectives.FindByPredicate([ObjectiveName](const FNUQuestObjectiveSpec& Candidate)
		{
			return Candidate.ObjectiveId == ObjectiveName;
		})
		: nullptr;
	if (Objective == nullptr)
	{
		return false;
	}

	// 완료한 퀘스트는 모든 목표를 끝낸 것으로 본다.
	const FNUQuestObjectiveProgress* ObjectiveProgress = Progress->FindObjectiveProgress(ObjectiveName);
	const bool bDone = bQuestCompleted || (ObjectiveProgress != nullptr && ObjectiveProgress->IsComplete(Objective->RequiredCount));
	const bool bOpen = !bDone && ObjectiveProgress != nullptr && Progress->IsInProgress() && Progress->IsObjectivePrerequisiteMet(*Definition, *Objective);
	switch (State)
	{
	case ENUQuestConditionState::Accepted:
		return bDone || bOpen;
	case ENUQuestConditionState::InProgress:
		return bOpen;
	case ENUQuestConditionState::Completed:
		return bDone;
	}
	return false;
}

bool FNUQuestStateCondition::HasTarget(TFunctionRef<const UNUQuestDefinition*(FName QuestId)> FindDefinition) const
{
	if (!IsSet())
	{
		return true;
	}

	const UNUQuestDefinition* Definition = FindDefinition(FName(*LexToString(QuestId)));
	const FName ObjectiveName(*LexToString(ObjectiveId));
	return Definition != nullptr && (ObjectiveId == 0 || Definition->Objectives.ContainsByPredicate([ObjectiveName](const FNUQuestObjectiveSpec& Candidate)
	{
		return Candidate.ObjectiveId == ObjectiveName;
	}));
}

FString FNUQuestStateCondition::ToString() const
{
	return FString::Printf(TEXT("%d/%d/%s"), QuestId, ObjectiveId, *UEnum::GetValueAsString(State));
}
