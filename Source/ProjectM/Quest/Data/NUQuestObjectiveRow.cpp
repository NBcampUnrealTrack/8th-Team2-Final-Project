#include "Quest/Data/NUQuestObjectiveRow.h"

#include "NUGAS/NUGameplayTags.h"
#include "Quest/Data/NUQuestTutorialTags.h"

bool FNUQuestObjectiveRow::ValidateRow(FName RowName, FString& OutError) const
{
	OutError.Reset();
	int32 NumericId = 0;
	if (!LexTryParseString(NumericId, *RowName.ToString()) || NumericId <= 0 || RowName.ToString() != LexToString(NumericId))
	{
		OutError = TEXT("ObjectiveId는 양의 int32여야 합니다.");
		return false;
	}
	if (QuestId <= 0 || RequiredCount <= 0 || !ObjectiveType.IsValid() || !ObjectiveType.MatchesTag(NU::Quest::Objective::Root))
	{
		OutError = TEXT("양의 QuestId, RequiredCount와 NU.Quest.Objective 하위 태그가 필요합니다.");
		return false;
	}
	// 서버와 같이 교감·처치·승리 목표의 대상 0은 "아무거나"다.
	const bool bWildcardTarget = ObjectiveType == NU::Quest::Objective::GrowNaia || ObjectiveType == NU::Quest::Objective::EnemyDefeated || ObjectiveType == NU::Quest::Objective::BattleWon;
	if (TargetId < 0 || (TargetId == 0 && !bWildcardTarget && ObjectiveType != NU::Quest::Objective::JobSelected && ObjectiveType != NU::Quest::Objective::StarterContracted))
	{
		OutError = TEXT("직업/시작 나이아 선택과 교감/처치/승리 목표 외에는 TargetId가 필요합니다.");
		return false;
	}
	if (PrerequisiteObjectiveId < 0 || PrerequisiteObjectiveId == NumericId)
	{
		OutError = TEXT("선행 목표 ID는 자기 자신과 다른 양의 int32여야 합니다. 선행 목표가 없으면 0을 사용합니다.");
		return false;
	}
	return true;
}

FNUQuestObjectiveSpec FNUQuestObjectiveRow::MakeObjective(FName RowName) const
{
	FNUQuestObjectiveSpec Objective;
	Objective.ObjectiveId = RowName;
	Objective.Description = Description;
	Objective.ObjectiveType = ObjectiveType;
	Objective.TargetId = TargetId > 0 ? FName(*LexToString(TargetId)) : NAME_None;
	Objective.RequiredCount = RequiredCount;
	Objective.bOptional = bOptional;
	Objective.bHideInObjectiveList = bHideInObjectiveList;
	Objective.PrerequisiteObjectiveId = PrerequisiteObjectiveId > 0 ? FName(*LexToString(PrerequisiteObjectiveId)) : NAME_None;
	return Objective;
}

void FNUQuestObjectiveRow::OnPostDataImport(const UDataTable* InDataTable, FName InRowName, TArray<FString>& OutCollectedImportProblems)
{
	FString Error;
	if (!ValidateRow(InRowName, Error))
	{
		OutCollectedImportProblems.Add(FString::Printf(TEXT("[%s:%s] %s"), *GetNameSafe(InDataTable), *InRowName.ToString(), *Error));
	}
}
