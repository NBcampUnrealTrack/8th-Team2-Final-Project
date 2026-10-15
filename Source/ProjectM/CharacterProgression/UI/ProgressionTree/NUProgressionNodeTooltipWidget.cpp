#include "CharacterProgression/UI/ProgressionTree/NUProgressionNodeTooltipWidget.h"

#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "CharacterProgression/Data/NUJobTypes.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UNUProgressionNodeTooltipWidget::SetNodeDefinition(const FNUProgressionNodeDefinitionRow& InNode)
{
	NodeDefinition = InNode;
	RefreshContent();
}

void UNUProgressionNodeTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshContent();
}

void UNUProgressionNodeTooltipWidget::RefreshContent()
{
	if (NodeNameText == nullptr || NodeTypeText == nullptr || NodeDescriptionText == nullptr)
	{
		return;
	}

	FText NodeName = NodeDefinition.DisplayName.IsEmpty()
		? FText::AsNumber(NodeDefinition.NodeId)
		: NodeDefinition.DisplayName;
	FText NodeType = GetNodeTypeText(NodeDefinition);
	FText NodeDescription = NodeDefinition.Description.IsEmpty()
		? NSLOCTEXT("NUProgression", "EmptyNodeDescription", "설명 없음")
		: NodeDefinition.Description;
	TryResolveRewardContent(NodeName, NodeType, NodeDescription);

	NodeNameText->SetText(NodeName);
	NodeTypeText->SetText(NodeType);
	NodeDescriptionText->SetText(NodeDescription);
}

bool UNUProgressionNodeTooltipWidget::TryResolveRewardContent(
	FText& OutName,
	FText& OutType,
	FText& OutDescription) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* ProgressionData = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	const UNUBattleDataSubsystem* BattleData = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	if (ProgressionData == nullptr)
	{
		return false;
	}

	const TArray<const FNUProgressionNodeRewardRow*> Rewards = ProgressionData->GetRewardsByNode(NodeDefinition.NodeId);
	if (NodeDefinition.NodeType == ENUProgressionNodeType::Skill)
	{
		if (BattleData == nullptr)
		{
			return false;
		}
		for (const FNUProgressionNodeRewardRow* Reward : Rewards)
		{
			if (Reward == nullptr
				|| (Reward->RewardType != ENUProgressionNodeRewardType::ActiveSkill
					&& Reward->RewardType != ENUProgressionNodeRewardType::PassiveSkill))
			{
				continue;
			}

			const FNUJobSkillDefinition* Skill = BattleData->FindJobSkillDefinition(Reward->RewardId);
			if (Skill == nullptr)
			{
				continue;
			}

			OutName = Skill->Name_KR;
			OutType = Skill->SkillType.IsNone()
				? GetNodeTypeText(NodeDefinition)
				: FText::FromString(Skill->SkillType.ToString());
			OutDescription = Skill->Description.IsEmpty()
				? NSLOCTEXT("NUProgression", "EmptySkillDescription", "설명 없음")
				: Skill->Description;
			return true;
		}
		return false;
	}

	if (NodeDefinition.NodeType != ENUProgressionNodeType::Advancement)
	{
		return false;
	}

	TArray<int32> ChoiceOrder;
	TMap<int32, const FNUJobAdvancementDefinitionRow*> AdvancementByChoice;
	TMap<int32, TArray<FText>> SkillDescriptionsByChoice;
	for (const FNUProgressionNodeRewardRow* Reward : Rewards)
	{
		if (Reward == nullptr || Reward->ChoiceAdvancementId <= 0)
		{
			continue;
		}
		ChoiceOrder.AddUnique(Reward->ChoiceAdvancementId);
		if (Reward->RewardType == ENUProgressionNodeRewardType::Advancement)
		{
			if (const FNUJobAdvancementDefinitionRow* Advancement = ProgressionData->FindAdvancement(Reward->ChoiceAdvancementId))
			{
				AdvancementByChoice.Add(Reward->ChoiceAdvancementId, Advancement);
			}
			continue;
		}
		if ((Reward->RewardType != ENUProgressionNodeRewardType::ActiveSkill
				&& Reward->RewardType != ENUProgressionNodeRewardType::PassiveSkill)
			|| BattleData == nullptr)
		{
			continue;
		}

		const FNUJobSkillDefinition* Skill = BattleData->FindJobSkillDefinition(Reward->RewardId);
		if (Skill == nullptr)
		{
			continue;
		}

		const FText SkillName = Skill->Name_KR.IsEmpty()
			? FText::FromName(Reward->RewardKey)
			: Skill->Name_KR;
		SkillDescriptionsByChoice.FindOrAdd(Reward->ChoiceAdvancementId).Add(FText::Format(
			NSLOCTEXT("NUProgression", "AdvancementSkillTooltip", "{0} · {1}\n{2}"),
			SkillName,
			Skill->SkillType.IsNone() ? FText::GetEmpty() : FText::FromString(Skill->SkillType.ToString()),
			Skill->Description));
	}

	TArray<FText> ChoiceDescriptions;
	for (const int32 ChoiceAdvancementId : ChoiceOrder)
	{
		const FNUJobAdvancementDefinitionRow* const* AdvancementPtr = AdvancementByChoice.Find(ChoiceAdvancementId);
		const FNUJobAdvancementDefinitionRow* Advancement = AdvancementPtr != nullptr ? *AdvancementPtr : nullptr;
		if (Advancement == nullptr)
		{
			continue;
		}

		TArray<FText> Details;
		if (!Advancement->Description.IsEmpty())
		{
			Details.Add(Advancement->Description);
		}
		if (const TArray<FText>* SkillDescriptions = SkillDescriptionsByChoice.Find(ChoiceAdvancementId))
		{
			Details.Append(*SkillDescriptions);
		}

		ChoiceDescriptions.Add(Details.IsEmpty()
			? Advancement->DisplayName
			: FText::Format(
				NSLOCTEXT("NUProgression", "AdvancementChoiceTooltip", "{0}\n{1}"),
				Advancement->DisplayName,
				FText::Join(FText::FromString(TEXT("\n\n")), Details)));
	}
	OutType = NSLOCTEXT("NUProgression", "TooltipTypeSecondAdvancementChoice", "2차 전직 선택 노드");
	if (!ChoiceDescriptions.IsEmpty())
	{
		OutDescription = FText::Join(FText::FromString(TEXT("\n\n────────\n\n")), ChoiceDescriptions);
	}
	return !ChoiceDescriptions.IsEmpty();
}

FText UNUProgressionNodeTooltipWidget::GetNodeTypeText(const FNUProgressionNodeDefinitionRow& Node)
{
	switch (Node.NodeType)
	{
	case ENUProgressionNodeType::Root: return NSLOCTEXT("NUProgression", "TooltipTypeRoot", "직업 시작 노드");
	case ENUProgressionNodeType::Stat: return NSLOCTEXT("NUProgression", "TooltipTypeStat", "스탯 노드");
	case ENUProgressionNodeType::Skill: return NSLOCTEXT("NUProgression", "TooltipTypeSkill", "스킬 노드");
	case ENUProgressionNodeType::Advancement: return NSLOCTEXT("NUProgression", "TooltipTypeAdvancement", "전직 노드");
	case ENUProgressionNodeType::Main: return NSLOCTEXT("NUProgression", "TooltipTypeMain", "메인 노드");
	case ENUProgressionNodeType::Final: return NSLOCTEXT("NUProgression", "TooltipTypeFinal", "최종 노드");
	case ENUProgressionNodeType::Job: return NSLOCTEXT("NUProgression", "TooltipTypeJob", "직업 노드");
	case ENUProgressionNodeType::MinorTrait: return NSLOCTEXT("NUProgression", "TooltipTypeMinor", "소형 특이효과");
	case ENUProgressionNodeType::MajorTrait:
		return Node.RequiredLevel >= 60
			? NSLOCTEXT("NUProgression", "TooltipTypeFinal", "최종 노드")
			: NSLOCTEXT("NUProgression", "TooltipTypeMajor", "대형 특이효과");
	default: return FText::GetEmpty();
	}
}
