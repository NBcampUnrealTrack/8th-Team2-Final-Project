#include "Quest/Data/NUQuestTypes.h"

#include "NUGAS/NUGameplayTags.h"

DEFINE_LOG_CATEGORY(LogNUQuest);

FText NUQuest::GetStateDisplayText(ENUQuestState State)
{
	switch (State)
	{
	case ENUQuestState::Active:
		return NSLOCTEXT("NUQuest", "StateInProgress", "진행 중");
	case ENUQuestState::ReadyToTurnIn:
	case ENUQuestState::Completed:
		return NSLOCTEXT("NUQuest", "StateCompleted", "진행 완료");
	default:
		return NSLOCTEXT("NUQuest", "StateBeforeProgress", "진행 가능");
	}
}

bool FNUQuestObjectiveSpec::ShouldDisplayCount() const
{
	return RequiredCount > 1 || ObjectiveType == NU::Quest::Objective::CollectItem || ObjectiveType == NU::Quest::Objective::EnemyDefeated;
}

FText FNUQuestObjectiveSpec::GetProgressText(int32 CurrentCount) const
{
	const FText Label = Description.IsEmpty() ? NSLOCTEXT("NUQuest", "UnnamedObjective", "목표 확인하기") : Description;
	return ShouldDisplayCount()
		? FText::Format(NSLOCTEXT("NUQuest", "CountedObjective", "{0}  {1}/{2}"), Label, FText::AsNumber(FMath::Clamp(CurrentCount, 0, RequiredCount)), FText::AsNumber(RequiredCount))
		: Label;
}

FText FNUQuestRewardSpec::GetDisplayText() const
{
	if (RewardType == NU::Quest::Reward::Gold)
	{
		return FText::Format(NSLOCTEXT("NUQuest", "GoldReward", "골드: {0}"), FText::AsNumber(Amount));
	}
	if (RewardType == NU::Quest::Reward::Item)
	{
		const FText Name = DisplayName.IsEmpty() ? NSLOCTEXT("NUQuest", "ItemRewardFallback", "아이템") : DisplayName;
		// 수량이 1개면 개수를 따로 적지 않는다.
		return Amount > 1
			? FText::Format(NSLOCTEXT("NUQuest", "ItemReward", "아이템: {0} × {1}"), Name, FText::AsNumber(Amount))
			: FText::Format(NSLOCTEXT("NUQuest", "ItemRewardSingle", "아이템: {0}"), Name);
	}
	if (RewardType == NU::Quest::Reward::Experience)
	{
		return FText::Format(NSLOCTEXT("NUQuest", "ExperienceReward", "경험치: {0}"), FText::AsNumber(Amount));
	}
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	if (RewardType == NU::Quest::Reward::UnlockFeature)
	{
		return NSLOCTEXT("NUQuest", "FeatureReward", "기능 해금");
	}
	return NSLOCTEXT("NUQuest", "AdditionalReward", "추가 보상");
}

bool FNUQuestRequirementSpec::IsApplicable(FString& OutError) const
{
	OutError.Reset();

	if (!RequirementType.IsValid())
	{
		OutError = TEXT("RequirementType이 설정되지 않았습니다.");
		return false;
	}

	if (RequirementType == NU::Quest::Requirement::PlayerLevel)
	{
		if (RequiredValue <= 0)
		{
			OutError = FString::Printf(TEXT("플레이어 레벨 조건의 RequiredValue가 %d입니다. 1 이상이어야 합니다."), RequiredValue);
			return false;
		}

		return true;
	}

	if (RequirementType == NU::Quest::Requirement::QuestCompleted)
	{
		if (TargetId.IsNone())
		{
			OutError = TEXT("퀘스트 완료 조건의 TargetId가 설정되지 않았습니다.");
			return false;
		}

		return true;
	}

	if (RequirementType == NU::Quest::Requirement::QuestGroupCompletedCount)
	{
		if (TargetId.IsNone())
		{
			OutError = TEXT("퀘스트 그룹 완료 조건의 TargetId가 설정되지 않았습니다.");
			return false;
		}

		if (RequiredValue <= 0)
		{
			OutError = FString::Printf(TEXT("퀘스트 그룹 완료 조건의 RequiredValue가 %d입니다. 1 이상이어야 합니다."), RequiredValue);
			return false;
		}

		return true;
	}

	if (RequirementType == NU::Quest::Requirement::FlagSet
		|| RequirementType == NU::Quest::Requirement::FlagNotSet)
	{
		if (TargetId.IsNone())
		{
			OutError = FString::Printf(TEXT("[%s] 조건의 TargetId가 설정되지 않았습니다."), *RequirementType.ToString());
			return false;
		}

		return true;
	}

	OutError = FString::Printf(TEXT("지원하지 않는 선행 조건 타입 [%s]입니다."), *RequirementType.ToString());
	return false;
}

bool FNUQuestRewardSpec::IsApplicable(FString& OutError) const
{
	OutError.Reset();
	if (RewardType == NU::Quest::Reward::Gold || RewardType == NU::Quest::Reward::Item)
	{
		if (Amount <= 0 || (RewardType == NU::Quest::Reward::Item && TargetId.IsNone()))
		{
			OutError = TEXT("골드/아이템 보상의 수량 또는 대상이 유효하지 않습니다.");
			return false;
		}
		return true;
	}

	if (!RewardType.IsValid())
	{
		OutError = TEXT("RewardType이 설정되지 않았습니다.");
		return false;
	}

	if (RewardType == NU::Quest::Reward::Experience)
	{
		if (Amount <= 0)
		{
			OutError = FString::Printf(TEXT("경험치 보상의 Amount가 %d입니다. 1 이상이어야 합니다."), Amount);
			return false;
		}

		return true;
	}

	if (RewardType == NU::Quest::Reward::SetFlag
		|| RewardType == NU::Quest::Reward::UnlockFeature)
	{
		if (TargetId.IsNone())
		{
			OutError = FString::Printf(TEXT("[%s] 보상에 TargetId가 설정되지 않았습니다."), *RewardType.ToString());
			return false;
		}

		return true;
	}

	OutError = FString::Printf(TEXT("지원하지 않는 보상 타입 [%s]입니다."), *RewardType.ToString());
	return false;
}
