#include "Quest/Data/NUQuestProgressTypes.h"

#include "Quest/Data/NUQuestDefinition.h"

bool FNUQuestObjectiveProgress::IsComplete(int32 RequiredCount) const
{
	const int32 SafeRequiredCount = FMath::Max(1, RequiredCount);

	return CurrentCount >= SafeRequiredCount;
}

void FNUQuestProgressState::Initialize(const UNUQuestDefinition& Definition, ENUQuestState InitialState)
{
	QuestId = Definition.QuestId;
	QuestState = InitialState;
	CompletionCount = 0;

	// 퀘스트 진행 데이터 생성
	ResetObjectiveProgress(Definition);
}

void FNUQuestProgressState::ResetObjectiveProgress(const UNUQuestDefinition& Definition)
{
	ObjectiveProgress.Reset();
	ObjectiveProgress.Reserve(Definition.Objectives.Num());

	for (const FNUQuestObjectiveSpec& Objective : Definition.Objectives)
	{
		FNUQuestObjectiveProgress NewProgress;
		NewProgress.ObjectiveId = Objective.ObjectiveId;
		NewProgress.CurrentCount = 0;

		ObjectiveProgress.Add(NewProgress);
	}
}

FNUQuestObjectiveProgress* FNUQuestProgressState::FindObjectiveProgress(FName ObjectiveId)
{
	return ObjectiveProgress.FindByPredicate(
		[ObjectiveId] (const FNUQuestObjectiveProgress& Progress)
		{
			return Progress.ObjectiveId == ObjectiveId;
		}
	);
}

const FNUQuestObjectiveProgress* FNUQuestProgressState::FindObjectiveProgress(FName ObjectiveId) const
{
	return ObjectiveProgress.FindByPredicate(
		[ObjectiveId] (const FNUQuestObjectiveProgress& Progress)
		{
			return Progress.ObjectiveId == ObjectiveId;
		}
	);
}

bool FNUQuestProgressState::IsRequiredObjectivesComplete(const UNUQuestDefinition& Definition) const
{
	for (const FNUQuestObjectiveSpec& Objective : Definition.Objectives)
	{
		// 선택 목표
		if (Objective.bOptional)
		{
			continue;
		}

		// 진행 데이터가 없을 경우
		const FNUQuestObjectiveProgress* Progress = FindObjectiveProgress(Objective.ObjectiveId);
		if (Progress == nullptr)
		{
			return false;
		}

		// 아직 완료되지 않은 필수 목표
		if (!Progress->IsComplete(Objective.RequiredCount))
		{
			return false;
		}
	}

	return true;
}

bool FNUQuestProgressState::IsInProgress() const
{
	return QuestState == ENUQuestState::Active || QuestState == ENUQuestState::ReadyToTurnIn;
}

FText FNUQuestProgressState::GetObjectiveDisplayText(const UNUQuestDefinition& Definition, bool bShowStatus) const
{
	TArray<FString> Lines;
	Lines.Reserve(Definition.Objectives.Num());
	for (const FNUQuestObjectiveSpec& Objective : Definition.Objectives)
	{
		// 수락 안내와 일지에는 대기 중인 후속 목표도 공개한다. 끝난 준비 단계와 아직 공개하지 않은 목표는 뺀다.
		if (!IsObjectiveListed(Definition, Objective))
		{
			continue;
		}

		const FNUQuestObjectiveProgress* Progress = FindObjectiveProgress(Objective.ObjectiveId);
		const int32 Count = Progress != nullptr ? Progress->CurrentCount : 0;
		const FText Optional = Objective.bOptional ? NSLOCTEXT("NUQuest", "OptionalObjective", " · 선택") : FText::GetEmpty();
		const FText ObjectiveLine = Objective.GetProgressText(Count);
		if (!bShowStatus)
		{
			Lines.Add(FText::Format(NSLOCTEXT("NUQuest", "PlainObjectiveLine", "{0}{1}"), ObjectiveLine, Optional).ToString());
			continue;
		}

		const bool bComplete = Progress != nullptr && Progress->IsComplete(Objective.RequiredCount);
		const bool bInProgress = IsInProgress();
		const FText Status = !bComplete && !IsObjectiveOpenForDisplay(Definition, Objective)
			? NSLOCTEXT("NUQuest", "ObjectiveWaiting", "대기")
			: NUQuest::GetStateDisplayText(bComplete ? ENUQuestState::Completed : bInProgress ? ENUQuestState::Active : ENUQuestState::Available);
		Lines.Add(FText::Format(NSLOCTEXT("NUQuest", "JournalObjectiveLine", "[{0}{1}]  {2}"), Status, Optional, ObjectiveLine).ToString());
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

bool FNUQuestProgressState::IsObjectivePrerequisiteMet(const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective) const
{
	if (Objective.PrerequisiteObjectiveId.IsNone())
	{
		return true;
	}
	const FNUQuestObjectiveSpec* Prerequisite = Definition.Objectives.FindByPredicate([&Objective](const FNUQuestObjectiveSpec& Candidate) { return Candidate.ObjectiveId == Objective.PrerequisiteObjectiveId; });
	const FNUQuestObjectiveProgress* Progress = FindObjectiveProgress(Objective.PrerequisiteObjectiveId);
	return Prerequisite != nullptr && Progress != nullptr && Progress->IsComplete(Prerequisite->RequiredCount);
}

bool FNUQuestProgressState::IsObjectiveListed(const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective) const
{
	// 완료한 퀘스트는 모든 목표를 끝낸 것으로 본다.
	const auto IsDone = [this](const FNUQuestObjectiveSpec& Spec)
	{
		const FNUQuestObjectiveProgress* Progress = FindObjectiveProgress(Spec.ObjectiveId);
		return QuestState == ENUQuestState::Completed || (Progress != nullptr && Progress->IsComplete(Spec.RequiredCount));
	};

	// 준비 단계 줄("○○의 심부름")은 그 NPC와 대화하기 전까지만 보이고, 끝나면 뒤 목표 줄이 대신한다.
	if (Objective.bHideInObjectiveList)
	{
		return !IsDone(Objective);
	}

	// 준비 단계가 끝나기 전에는 그 뒤 목표의 내용을 공개하지 않는다. 정의 검증이 순환을 막으므로 목표 수만큼만 따라간다.
	FName PrerequisiteId = Objective.PrerequisiteObjectiveId;
	for (int32 Depth = 0; Depth < Definition.Objectives.Num() && !PrerequisiteId.IsNone(); ++Depth)
	{
		const FNUQuestObjectiveSpec* Prerequisite = Definition.Objectives.FindByPredicate([PrerequisiteId](const FNUQuestObjectiveSpec& Candidate) { return Candidate.ObjectiveId == PrerequisiteId; });
		if (Prerequisite == nullptr)
		{
			break;
		}

		if (Prerequisite->bHideInObjectiveList && !IsDone(*Prerequisite))
		{
			return false;
		}

		PrerequisiteId = Prerequisite->PrerequisiteObjectiveId;
	}

	return true;
}

bool FNUQuestProgressState::IsObjectiveOpenForDisplay(const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective) const
{
	return IsObjectiveListed(Definition, Objective) && IsObjectivePrerequisiteMet(Definition, Objective);
}

const FNUQuestObjectiveSpec* FNUQuestProgressState::FindNextObjective(const UNUQuestDefinition& Definition) const
{
	const FNUQuestObjectiveSpec* OptionalObjective = nullptr;
	for (const FNUQuestObjectiveSpec& Objective : Definition.Objectives)
	{
		const FNUQuestObjectiveProgress* Progress = FindObjectiveProgress(Objective.ObjectiveId);
		if ((Progress != nullptr && Progress->IsComplete(Objective.RequiredCount)) || !IsObjectivePrerequisiteMet(Definition, Objective))
		{
			continue;
		}
		if (!Objective.bOptional)
		{
			return &Objective;
		}
		if (OptionalObjective == nullptr)
		{
			OptionalObjective = &Objective;
		}
	}
	return OptionalObjective;
}
