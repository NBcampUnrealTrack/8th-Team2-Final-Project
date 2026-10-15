#include "CharacterProgression/Data/NUProgressionNodeTypes.h"

#include "CharacterProgression/Data/NUProgressionTypes.h"

bool FNUProgressionNodeRules::HasCurrentParent(const FNUProgressionNodeDefinitionRow& Node, int32 CurrentNodeId)
{
	if (CurrentNodeId <= 0)
	{
		return false;
	}

	return (Node.ParentNodeId1 > 0 && Node.ParentNodeId1 == CurrentNodeId)
		|| (Node.ParentNodeId2 > 0 && Node.ParentNodeId2 == CurrentNodeId)
		|| (Node.ParentNodeId3 > 0 && Node.ParentNodeId3 == CurrentNodeId);
}

int32 FNUProgressionNodeRules::GetCompletedRadialStemCount(const FNUProgressionState& State)
{
	// 전직 노드가 진입 줄기를 완료 처리하므로 첫 완료 줄기는 방사형 줄기 개수에서 제외한다.
	return FMath::Max(0, State.CompletedStemIds.Num() - 1);
}

int32 FNUProgressionNodeRules::GetEarnedPointCountForLevel(int32 Level)
{
	int32 EarnedPointCount = 0;
	for (int32 CandidateLevel = 11; CandidateLevel <= FMath::Min(Level, 59); ++CandidateLevel)
	{
		const int32 Stage = CandidateLevel % 10;
		const bool bAwardsPoint = (Stage >= 1 && Stage <= 4) || (Stage >= 6 && Stage <= 9);
		EarnedPointCount += bAwardsPoint ? 1 : 0;
	}
	return EarnedPointCount;
}

int32 FNUProgressionNodeRules::ResolveRequiredLevel(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionState& State)
{
	const bool bIsRadialStem = Node.StemType == ENUProgressionStemType::Common
		|| Node.StemType == ENUProgressionStemType::BranchA
		|| Node.StemType == ENUProgressionStemType::BranchB;
	if (!bIsRadialStem)
	{
		return Node.RequiredLevel;
	}

	// 방사형 줄기는 선택 순서가 고정되지 않는다. 완료한 줄기 수로 현재 10레벨 구간을 계산한다.
	return 20 + GetCompletedRadialStemCount(State) * 10 + Node.StemStage;
}

int32 FNUProgressionNodeRules::ResolveAvailableParentId(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context)
{
	if (Node.NodeType == ENUProgressionNodeType::Root)
	{
		return 0;
	}
	if (Node.NodeType == ENUProgressionNodeType::Advancement && Context.AdvancementId > 0)
	{
		return 0;
	}

	if ((Node.RequiredJobId > 0 && Node.RequiredJobId != Context.JobId)
		|| (Node.RequiredAdvancementId > 0 && Node.RequiredAdvancementId != Context.AdvancementId))
	{
		return 0;
	}

	const int32 CompletedRadialStemCount = GetCompletedRadialStemCount(State);
	const bool bIsRadialStem = Node.StemType == ENUProgressionStemType::Common
		|| Node.StemType == ENUProgressionStemType::BranchA
		|| Node.StemType == ENUProgressionStemType::BranchB;
	if (bIsRadialStem && Context.AdvancementId <= 0)
	{
		return 0;
	}
	if (bIsRadialStem && Node.StemStage == 1 && CompletedRadialStemCount >= 4)
	{
		return 0;
	}

	if (Node.NodeType == ENUProgressionNodeType::Final)
	{
		if (State.SelectedFinalNodeId > 0
			|| State.ActiveStemId > 0
			|| CompletedRadialStemCount != 4
			|| Context.AdvancementId <= 0
			|| State.CurrentNodeId <= 0
			|| !State.AcquiredNodeIds.Contains(State.CurrentNodeId))
		{
			return 0;
		}

		// 최종 2택은 고정 줄기에 속하지 않고 네 번째로 완주한 줄기의 메인 노드 뒤에서 선택한다.
		return State.CurrentNodeId;
	}

	if (Node.StemId > 0)
	{
		if (State.CompletedStemIds.Contains(Node.StemId))
		{
			return 0;
		}

		if (State.ActiveStemId > 0 && State.ActiveStemId != Node.StemId)
		{
			return 0;
		}
	}

	if (HasCurrentParent(Node, State.CurrentNodeId))
	{
		return State.CurrentNodeId;
	}

	// 진행 중인 줄기에서는 현재 노드에서만 다음 단계로 갈 수 있다.
	if (State.ActiveStemId > 0 || Node.StemId <= 0 || Node.StemStage != 1)
	{
		return 0;
	}

	// 줄기 하나를 완주한 뒤에는 이전에 획득한 중앙 노드에서 새 줄기를 시작한다.
	const int32 ParentNodeIds[] = { Node.ParentNodeId1, Node.ParentNodeId2, Node.ParentNodeId3 };
	for (const int32 ParentNodeId : ParentNodeIds)
	{
		if (ParentNodeId > 0 && State.AcquiredNodeIds.Contains(ParentNodeId))
		{
			return ParentNodeId;
		}
	}

	return 0;
}

ENUProgressionNodeState FNUProgressionNodeRules::Evaluate(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context)
{
	// NodeId가 없으면 정상 노드로 판단할 수 없음
	if (Node.NodeId <= 0)
	{
		return ENUProgressionNodeState::Locked;
	}

	// 레벨이나 포인트가 바뀌어도 이미 획득한 노드는 완료 상태를 유지한다.
	if (State.AcquiredNodeIds.Contains(Node.NodeId))
	{
		return ENUProgressionNodeState::Acquired;
	}
	if (Node.NodeType == ENUProgressionNodeType::Advancement && Context.AdvancementId > 0)
	{
		return ENUProgressionNodeState::Locked;
	}

	// 방사형 줄기는 선택 순서에 따라 같은 노드의 실제 요구 레벨이 달라진다.
	const int32 RequiredLevel = ResolveRequiredLevel(Node, State);
	const bool bLevelLocked = RequiredLevel > 0 && State.Level < RequiredLevel;
	if (bLevelLocked || State.AvailablePoints < Node.PointCost)
	{
		return ENUProgressionNodeState::Locked;
	}
	if ((Node.RequiredJobId > 0 && Node.RequiredJobId != Context.JobId)
		|| (Node.RequiredAdvancementId > 0 && Node.RequiredAdvancementId != Context.AdvancementId))
	{
		return ENUProgressionNodeState::Locked;
	}

	const bool bIsRoot = Node.NodeType == ENUProgressionNodeType::Root;
	if (bIsRoot)
	{
		return State.CurrentNodeId <= 0
			&& State.ActiveStemId <= 0
			&& State.AcquiredNodeIds.IsEmpty()
			? ENUProgressionNodeState::Available
			: ENUProgressionNodeState::Locked;
	}

	return ResolveAvailableParentId(Node, State, Context) > 0
		? ENUProgressionNodeState::Available
		: ENUProgressionNodeState::Locked;
}
