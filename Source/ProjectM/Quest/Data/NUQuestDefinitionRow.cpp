#include "Quest/Data/NUQuestDefinitionRow.h"

#include "NUGAS/NUGameplayTags.h"
#include "Quest/Data/NUQuestDefinition.h"

namespace NUQuestDefinitionRow
{
	FName NumericIdToName(int32 NumericId)
	{
		return NumericId > 0
			? FName(*LexToString(NumericId))
			: NAME_None;
	}

}

bool FNUQuestDefinitionRow::ValidateRow(FName RowName, FString& OutError) const
{
	OutError.Reset();

	int32 Quest_ID = 0;
	if (!LexTryParseString(Quest_ID, *RowName.ToString()) || Quest_ID <= 0 || RowName.ToString() != LexToString(Quest_ID))
	{
		OutError = FString::Printf(TEXT("Quest_ID RowName [%s]은 양의 int32여야 합니다."), *RowName.ToString());
		return false;
	}

	if (Quest_Key.IsNone())
	{
		OutError = TEXT("Quest_Key가 비어 있습니다.");
		return false;
	}

	if (Quest_KR.IsEmpty())
	{
		OutError = TEXT("Quest_KR이 비어 있습니다.");
		return false;
	}

	if (!QuestCategory.IsValid() || !QuestCategory.MatchesTag(NU::Quest::Category::Root))
	{
		OutError = FString::Printf(
			TEXT("QuestCategory [%s]는 NU.Quest.Category 하위 태그여야 합니다."),
			*QuestCategory.ToString());
		return false;
	}

	if (!bUseObjectiveTable && (!QuestType.IsValid() || !QuestType.MatchesTag(NU::Quest::Objective::Root)))
	{
		OutError = FString::Printf(
			TEXT("QuestType [%s]는 NU.Quest.Objective 하위 태그여야 합니다."),
			*QuestType.ToString());
		return false;
	}

	if (RequiredLevel < 1)
	{
		OutError = TEXT("RequiredLevel은 1 이상이어야 합니다.");
		return false;
	}

	if (PrereqQuest_ID < 0 || PrereqQuest_ID == Quest_ID)
	{
		OutError = TEXT("PrereqQuest_ID는 0 이상이며 자기 Quest_ID와 달라야 합니다.");
		return false;
	}

	TSet<int32> Prerequisites;
	Prerequisites.Add(PrereqQuest_ID);
	for (const int32 Prerequisite : AdditionalPrereqQuest_IDs)
	{
		if (Prerequisite <= 0 || Prerequisite == Quest_ID || Prerequisites.Contains(Prerequisite))
		{
			OutError = TEXT("추가 선행 퀘스트 ID는 양수이며 자기 자신이나 기존 선행 ID와 중복될 수 없습니다.");
			return false;
		}
		Prerequisites.Add(Prerequisite);
	}

	if (QuestGiver_ID < 0 || Location_ID < 0 || Target_ID < 0 || TurnInNPC_ID < 0 || RewardItem1_ID < 0 || RewardItem2_ID < 0)
	{
		OutError = TEXT("참조 ID는 0 이상이어야 합니다. 참조가 없으면 0을 사용합니다.");
		return false;
	}
	if (!bUseObjectiveTable && Quest_ID > (MAX_int32 - 1) / 100)
	{
		OutError = TEXT("자동 생성 목표 ID(Quest_ID * 100 + 1)가 int32 범위를 초과합니다.");
		return false;
	}
	if (!bUseObjectiveTable && TargetCount < 1)
	{
		OutError = TEXT("TargetCount는 1 이상이어야 합니다.");
		return false;
	}
	if (bUseObjectiveTable && (QuestType.IsValid() || Target_ID != 0 || TargetCount != 0))
	{
		OutError = TEXT("별도 목표 테이블 사용 시 QuestType을 비우고 Target_ID와 TargetCount를 0으로 설정해야 합니다. 두 형식을 중복 정의할 수 없습니다.");
		return false;
	}

	const bool bClaimAutomatically = ClaimType == ENUQuestClaimType::Automatic;

	if (!bAutoAccept && QuestGiver_ID == 0)
	{
		OutError = TEXT("NPC수령 퀘스트의 QuestGiver_ID가 비어 있습니다.");
		return false;
	}

	if (!bClaimAutomatically && TurnInNPC_ID == 0)
	{
		OutError = TEXT("NPC수령 퀘스트의 TurnInNPC_ID가 비어 있습니다.");
		return false;
	}

	return true;
}

bool FNUQuestDefinitionRow::ApplyTo(UNUQuestDefinition& OutDefinition, FName RowName, FString& OutError) const
{
	if (!ValidateRow(RowName, OutError))
	{
		return false;
	}

	int32 Quest_ID = 0;
	LexTryParseString(Quest_ID, *RowName.ToString());

	const bool bClaimAutomatically = ClaimType == ENUQuestClaimType::Automatic;

	OutDefinition.QuestId = RowName;
	OutDefinition.QuestNumericId = Quest_ID;
	OutDefinition.CategoryTag = QuestCategory;
	OutDefinition.OfferActorId = NUQuestDefinitionRow::NumericIdToName(QuestGiver_ID);
	OutDefinition.TurnInActorId = NUQuestDefinitionRow::NumericIdToName(TurnInNPC_ID);
	OutDefinition.LocationId = NUQuestDefinitionRow::NumericIdToName(Location_ID);
	OutDefinition.Title = Quest_KR;
	OutDefinition.Summary = Desc_KR;
	OutDefinition.Description = Desc_KR;
	OutDefinition.RecommendedLevel = FMath::Max(RequiredLevel, 1);
	OutDefinition.RepeatPolicy = RepeatType;
	OutDefinition.bAutoComplete = bClaimAutomatically;
	OutDefinition.bAutoAccept = bAutoAccept;
	OutDefinition.QuestDialogue = QuestDialogue;

	OutDefinition.StartRequirements.Reset();
	if (RequiredLevel > 1)
	{
		FNUQuestRequirementSpec& LevelRequirement = OutDefinition.StartRequirements.AddDefaulted_GetRef();
		LevelRequirement.RequirementType = NU::Quest::Requirement::PlayerLevel;
		LevelRequirement.RequiredValue = RequiredLevel;
	}

	if (PrereqQuest_ID > 0)
	{
		FNUQuestRequirementSpec& QuestRequirement = OutDefinition.StartRequirements.AddDefaulted_GetRef();
		QuestRequirement.RequirementType = NU::Quest::Requirement::QuestCompleted;
		QuestRequirement.TargetId = NUQuestDefinitionRow::NumericIdToName(PrereqQuest_ID);
		QuestRequirement.RequiredValue = 1;
	}

	for (const int32 Prerequisite : AdditionalPrereqQuest_IDs)
	{
		FNUQuestRequirementSpec& Requirement = OutDefinition.StartRequirements.AddDefaulted_GetRef();
		Requirement.RequirementType = NU::Quest::Requirement::QuestCompleted;
		Requirement.TargetId = NUQuestDefinitionRow::NumericIdToName(Prerequisite);
	}

	OutDefinition.Objectives.Reset();

	if (!bUseObjectiveTable)
	{
		FNUQuestObjectiveSpec& Objective = OutDefinition.Objectives.AddDefaulted_GetRef();
		// 목표 표를 쓰지 않는 퀘스트의 단일 목표도 서버 규칙(quest_id × 100 + 1)과 같은 ID를 쓴다.
		Objective.ObjectiveId = FName(*FString::Printf(TEXT("%lld"), static_cast<long long>(Quest_ID) * 100 + 1));
		Objective.Description = Desc_KR;
		Objective.ObjectiveType = QuestType;
		Objective.TargetId = NUQuestDefinitionRow::NumericIdToName(Target_ID);
		if (Objective.TargetId.IsNone() && QuestType == NU::Quest::Objective::ReachLocation)
		{
			Objective.TargetId = NUQuestDefinitionRow::NumericIdToName(Location_ID);
		}
		Objective.RequiredCount = FMath::Max(TargetCount, 1);
	}

	// CSV 보상을 로컬 플레이와 퀘스트 표시에서 함께 사용한다.
	OutDefinition.Rewards.RemoveAll([](const FNUQuestRewardSpec& Reward)
	{
		return Reward.RewardType == NU::Quest::Reward::Experience || Reward.RewardType == NU::Quest::Reward::Gold || Reward.RewardType == NU::Quest::Reward::Item;
	});
	if (Reward_EXP > 0)
	{
		FNUQuestRewardSpec& Reward = OutDefinition.Rewards.AddDefaulted_GetRef();
		Reward.RewardType = NU::Quest::Reward::Experience;
		Reward.Amount = Reward_EXP;
	}
	if (Reward_Gold > 0)
	{
		FNUQuestRewardSpec& Reward = OutDefinition.Rewards.AddDefaulted_GetRef();
		Reward.RewardType = NU::Quest::Reward::Gold;
		Reward.Amount = Reward_Gold;
	}
	const auto AddItemReward = [&OutDefinition](int32 ItemId, int32 Count)
	{
		if (ItemId <= 0 || Count <= 0) { return; }
		FNUQuestRewardSpec& Reward = OutDefinition.Rewards.AddDefaulted_GetRef();
		Reward.RewardType = NU::Quest::Reward::Item;
		Reward.TargetId = FName(*LexToString(ItemId));
		Reward.Amount = Count;
	};
	AddItemReward(RewardItem1_ID, RewardItem1Count);
	AddItemReward(RewardItem2_ID, RewardItem2Count);

	return true;
}

void FNUQuestDefinitionRow::OnPostDataImport(const UDataTable* InDataTable, FName InRowName, TArray<FString>& OutCollectedImportProblems)
{
	FString Error;
	if (!ValidateRow(InRowName, Error))
	{
		OutCollectedImportProblems.Add(FString::Printf(
			TEXT("[%s:%s] %s"),
			*GetNameSafe(InDataTable),
			*InRowName.ToString(),
			*Error));
	}
}
