#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"

#include "CharacterProgression/Data/NUProgressionDataSettings.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"

#include "Engine/DataTable.h"

namespace
{
	// DFS로 순환 연결을 검사할 때 사용하는 방문 상태
	enum class ENUVisitState : uint8
	{
		// 아직 검사하지 않음
		Unvisited,
		// 현재 자식 노드를 검사 중
		Visiting,
		// 자식 노드까지 검사 완료
		Visited
	};

	// 잘못된 enum 값이 들어왔는지 확인
	bool IsValidNodeType(ENUProgressionNodeType NodeType)
	{
		const UEnum* Enum = StaticEnum<ENUProgressionNodeType>();
		return Enum != nullptr && Enum->IsValidEnumValue(static_cast<int64>(NodeType));
	}

	bool IsValidLayoutMode(ENUProgressionNodeLayoutMode LayoutMode)
	{
		const UEnum* Enum = StaticEnum<ENUProgressionNodeLayoutMode>();
		return Enum != nullptr && Enum->IsValidEnumValue(static_cast<int64>(LayoutMode));
	}

	bool IsValidStemType(ENUProgressionStemType StemType)
	{
		const UEnum* Enum = StaticEnum<ENUProgressionStemType>();
		return Enum != nullptr && Enum->IsValidEnumValue(static_cast<int64>(StemType));
	}

	bool IsValidRewardType(ENUProgressionNodeRewardType RewardType)
	{
		const UEnum* Enum = StaticEnum<ENUProgressionNodeRewardType>();
		return Enum != nullptr && Enum->IsValidEnumValue(static_cast<int64>(RewardType));
	}

	template <typename RowType>
	bool CopyTableRows(const UDataTable* Table, const TCHAR* Label, TArray<RowType>& OutRows, FString& OutError)
	{
		if (Table == nullptr)
		{
			OutError = FString::Printf(TEXT("%s 테이블이 없음"), Label);
			return false;
		}

		if (Table->GetRowStruct() != RowType::StaticStruct())
		{
			OutError = FString::Printf(
				TEXT("%s RowStruct가 잘못됨: 기대=%s 실제=%s"),
				Label,
				*RowType::StaticStruct()->GetName(),
				*GetNameSafe(Table->GetRowStruct()));
			return false;
		}

		TArray<RowType*> RowPointers;
		Table->GetAllRows(Label, RowPointers);
		OutRows.Reserve(RowPointers.Num());
		for (const RowType* Row : RowPointers)
		{
			if (Row != nullptr)
			{
				OutRows.Add(*Row);
			}
		}
		return true;
	}

	// 요구 레벨 순으로 정렬하고 같은 레벨이면 NodeId 이름순으로 정렬
	bool SortNodeIds(int32 Left, int32 Right, const TMap<int32, FNUProgressionNodeDefinitionRow>& NodesById)
	{
		const FNUProgressionNodeDefinitionRow* LeftNode = NodesById.Find(Left);
		const FNUProgressionNodeDefinitionRow* RightNode = NodesById.Find(Right);
		
		// 정상 데이터라면 두 노드가 모두 존재해야 함
		if (LeftNode == nullptr || RightNode == nullptr)
		{
			return LeftNode != nullptr;
		}
		
		if (LeftNode->RequiredLevel != RightNode->RequiredLevel)
		{
			return LeftNode->RequiredLevel < RightNode->RequiredLevel;
		}
		
		return Left < Right;
	}

	// DFS를 실행해 순환 연결을 찾음
	bool HasCycleFrom(
		int32 NodeId,
		const TMap<int32, TArray<int32>>& ChildNodeIdsByNode,
		TMap<int32, ENUVisitState>& VisitStates,
		int32& OutCycleNodeId)
	{
		// 처음 방문한 노드는 Map에 없으므로 기본값 Unvisited가 나옴
		const ENUVisitState State = VisitStates.FindRef(NodeId);

		// 검사 중인 노드를 다시 만나면 순환 연결
		if (State == ENUVisitState::Visiting)
		{
			OutCycleNodeId = NodeId;
			return true;
		}
		// 자식까지 검사가 끝나면 확인 X
		if (State == ENUVisitState::Visited)
		{
			return false;
		}

		// 자식 노드 확인 전 현재 노드 검사중으로 표시
		VisitStates.Add(NodeId, ENUVisitState::Visiting);

		// 현재 노드에 연결된 모든 자식 노드를 한 경로씩 검사함
		if (const TArray<int32>* Children = ChildNodeIdsByNode.Find(NodeId))
		{
			for (const int32 ChildId : *Children)
			{
				if (HasCycleFrom(ChildId, ChildNodeIdsByNode, VisitStates, OutCycleNodeId))
				{
					return true;
				}
			}
		}

		// 모든 자식 검사가 끝나면 완료 상태
		VisitStates.Add(NodeId, ENUVisitState::Visited);
		return false;
	}

	// 성장 그래프가 사용할 수 있는 구조인지 검사
	bool ValidateGraphMaps(
		int32 GraphId,
		const TMap<int32, FNUProgressionNodeDefinitionRow>& NodesById,
		const TMap<int32, TArray<int32>>& NodeIdsByGraph,
		const TMap<int32, TArray<int32>>& ChildNodeIdsByNode,
		FString& OutError)
	{
		const TArray<int32>* GraphNodeIds = NodeIdsByGraph.Find(GraphId);
		if (GraphId <= 0 || GraphNodeIds == nullptr || GraphNodeIds->IsEmpty())
		{
			OutError = FString::Printf(TEXT("그래프가 없거나 비어 있음: GraphId=%d"), GraphId);
			return false;
		}

		// 그래프에 존재하는 루트 노드
		TArray<int32> RootNodeIds;
		for (const int32 NodeId : *GraphNodeIds)
		{
			const FNUProgressionNodeDefinitionRow* Node = NodesById.Find(NodeId);
			if (Node != nullptr && Node->NodeType == ENUProgressionNodeType::Root)
			{
				RootNodeIds.Add(NodeId);
			}
		}

		// 시작점이 하나여야 됨
		if (RootNodeIds.Num() != 1)
		{
			OutError = FString::Printf(TEXT("루트 노드는 정확히 하나여야 함: GraphId=%d 루트 수=%d"), GraphId, RootNodeIds.Num());
			return false;
		}

		// 모든 노드를 시작점으로 DFS 실행
		TMap<int32, ENUVisitState> VisitStates;
		int32 CycleNodeId = 0;
		for (const int32 NodeId : *GraphNodeIds)
		{
			if (HasCycleFrom(NodeId, ChildNodeIdsByNode, VisitStates, CycleNodeId))
			{
				OutError = FString::Printf(TEXT("순환 연결 발견: GraphId=%d NodeId=%d"), GraphId, CycleNodeId);
				return false;
			}
		}

		// root에서 시작해 실제로 도달할 수 있는 노드
		TSet<int32> ReachableNodeIds;
		TArray<int32> PendingNodeIds = RootNodeIds;
		while (!PendingNodeIds.IsEmpty())
		{
			// tarray의 마지막 노드를 꺼내 stack 방식의 dfs
			const int32 CurrentNodeId = PendingNodeIds.Pop(EAllowShrinking::No);
			// 중복 방문 금지
			if (ReachableNodeIds.Contains(CurrentNodeId))
			{
				continue;
			}
			ReachableNodeIds.Add(CurrentNodeId);

			// 다음에 확인할 자식 노드 stack에 추가
			if (const TArray<int32>* Children = ChildNodeIdsByNode.Find(CurrentNodeId))
			{
				PendingNodeIds.Append(*Children);
			}
		}

		// 전체 노드 수와 root에서 도달한 노드 수가 다르면 끊어진 노드가 존재
		if (ReachableNodeIds.Num() != GraphNodeIds->Num())
		{
			for (const int32 NodeId : *GraphNodeIds)
			{
				if (!ReachableNodeIds.Contains(NodeId))
				{
					OutError = FString::Printf(TEXT("루트에서 도달할 수 없는 노드: GraphId=%d NodeId=%d"), GraphId, NodeId);
					return false;
				}
			}
		}

		// root 1개, 순환 없음, 모든 노드 도달 가능
		return true;
	}
}

bool FNUProgressionGraphData::BuildFromTable(const UDataTable* Table, FString& OutError)
{
	OutError.Reset();

	// 설정된 DataTable이 실제로 존재하는지 확인
	if (Table == nullptr)
	{
		OutError = TEXT("성장 노드 테이블이 없음");
		return false;
	}

	// 다른 종류의 DataTable이 잘못 들어오는 경우를 막음
	if (Table->GetRowStruct() != FNUProgressionNodeDefinitionRow::StaticStruct())
	{
		OutError = FString::Printf(
			TEXT("성장 노드 RowStruct가 잘못됨: 기대=%s 실제=%s"),
			*FNUProgressionNodeDefinitionRow::StaticStruct()->GetName(),
			*GetNameSafe(Table->GetRowStruct()));
		return false;
	}

	// DataTable의 행 포인터를 가져옴
	TArray<FNUProgressionNodeDefinitionRow*> RowPointers;
	Table->GetAllRows(TEXT("FNUProgressionGraphData::BuildFromTable"), RowPointers);

	// 런타임 조회 데이터가 DataTable 행 포인터에 의존하지 않도록 값으로 복사
	TArray<FNUProgressionNodeDefinitionRow> Rows;
	Rows.Reserve(RowPointers.Num());
	for (const FNUProgressionNodeDefinitionRow* Row : RowPointers)
	{
		if (Row != nullptr)
		{
			Rows.Add(*Row);
		}
	}

	// 실제 검증과 조회 Map 생성은 한 곳에서 처리
	return BuildFromRows(Rows, OutError);
}

bool FNUProgressionGraphData::BuildFromRows(const TArray<FNUProgressionNodeDefinitionRow>& Rows, FString& OutError)
{
	// 이전에 만든 조회 데이터가 섞이지 않도록 먼저 비움
	Reset();
	OutError.Reset();

	// 노드가 하나도 없으면 그래프를 만들 수 없음
	if (Rows.IsEmpty())
	{
		OutError = TEXT("성장 노드 행이 비어 있음");
		return false;
	}

	// 같은 그래프에서 배치 좌표가 겹치는지 검사할 때만 사용
	TMap<int32, TSet<FString>> UsedLayoutsByGraph;
	// 먼저 각 행만으로 확인 가능한 값과 기본 조회 맵을 만든다.
	for (const FNUProgressionNodeDefinitionRow& Row : Rows)
	{
		// 노드를 찾는 데 필요한 ID는 비어 있을 수 없음
		if (Row.NodeId <= 0 || Row.GraphId <= 0 || Row.StemId < 0)
		{
			OutError = FString::Printf(TEXT("필수 노드 식별자가 잘못됨: NodeId=%d GraphId=%d StemId=%d"),
				Row.NodeId, Row.GraphId, Row.StemId);
			Reset();
			return false;
		}

		// NodeId는 전체 테이블에서 하나만 존재해야 함
		if (NodesById.Contains(Row.NodeId))
		{
			OutError = FString::Printf(TEXT("중복 NodeId: %d"), Row.NodeId);
			Reset();
			return false;
		}

		// CSV에서 enum 범위를 벗어난 값이 들어오는 경우를 막음
		if (!IsValidNodeType(Row.NodeType))
		{
			OutError = FString::Printf(TEXT("노드 타입이 잘못됨: NodeId=%d"), Row.NodeId);
			Reset();
			return false;
		}
		if (!IsValidLayoutMode(Row.LayoutMode)
			|| !IsValidStemType(Row.StemType)
			|| Row.StemStage < 0
			|| !FMath::IsFinite(Row.LayoutRadius)
			|| !FMath::IsFinite(Row.LayoutAngleDegrees)
			|| !FMath::IsFinite(Row.LayoutOffsetX)
			|| !FMath::IsFinite(Row.LayoutOffsetY)
			|| (Row.LayoutMode == ENUProgressionNodeLayoutMode::Radial && Row.LayoutRadius < 0.f))
		{
			OutError = FString::Printf(TEXT("노드 배치 값이 잘못됨: NodeId=%d"), Row.NodeId);
			Reset();
			return false;
		}

		// RequiredLevel 0은 레벨 조건 없음, 포인트 비용은 0 이상
		if (Row.RequiredLevel < 0 || Row.PointCost < 0)
		{
			OutError = FString::Printf(TEXT("요구 레벨 또는 포인트 비용이 잘못됨: NodeId=%d"), Row.NodeId);
			Reset();
			return false;
		}

		// 자기 자신을 선행 노드로 지정할 수 없음
		if (Row.ParentNodeId1 == Row.NodeId
			|| Row.ParentNodeId2 == Row.NodeId
			|| Row.ParentNodeId3 == Row.NodeId)
		{
			OutError = FString::Printf(TEXT("자기 자신을 부모로 참조함: NodeId=%d"), Row.NodeId);
			Reset();
			return false;
		}

		// 부모 연결에 같은 NodeId를 두 번 넣을 수 없음
		TSet<int32> UniqueParentIds;
		const int32 RowParentIds[] = { Row.ParentNodeId1, Row.ParentNodeId2, Row.ParentNodeId3 };
		for (const int32 ParentId : RowParentIds)
		{
			if (ParentId <= 0)
			{
				continue;
			}
			if (UniqueParentIds.Contains(ParentId))
			{
				OutError = FString::Printf(TEXT("부모 NodeId가 중복됨: NodeId=%d ParentId=%d"),
					Row.NodeId, ParentId);
				Reset();
				return false;
			}
			UniqueParentIds.Add(ParentId);
		}

		// Root는 부모가 없어야 하고 나머지 노드는 부모가 있어야 함
		const bool bHasParent = Row.ParentNodeId1 > 0
			|| Row.ParentNodeId2 > 0
			|| Row.ParentNodeId3 > 0;
		if ((Row.NodeType == ENUProgressionNodeType::Root && bHasParent)
			|| (Row.NodeType != ENUProgressionNodeType::Root && !bHasParent))
		{
			OutError = FString::Printf(TEXT("루트와 부모 관계가 잘못됨: NodeId=%d"), Row.NodeId);
			Reset();
			return false;
		}

		// 직업 노드는 어떤 직업을 요구하는지 지정해야 함
		if (Row.RequiredJobId < 0 || Row.RequiredAdvancementId < 0)
		{
			OutError = FString::Printf(TEXT("직업 또는 전직 요구 ID가 음수임: NodeId=%d"), Row.NodeId);
			Reset();
			return false;
		}
		if ((Row.NodeType == ENUProgressionNodeType::Advancement
				|| Row.NodeType == ENUProgressionNodeType::Job)
			&& Row.RequiredJobId <= 0)
		{
			OutError = FString::Printf(TEXT("전직 노드에는 RequiredJobId가 필요함: NodeId=%d"), Row.NodeId);
			Reset();
			return false;
		}

		// 같은 좌표에 노드가 두 개 배치되면 UI가 겹치므로 막음
		const FString LayoutKey = Row.LayoutMode == ENUProgressionNodeLayoutMode::Radial
			? FString::Printf(TEXT("R:%.3f:%.3f:%.3f:%.3f"),
				Row.LayoutRadius, Row.LayoutAngleDegrees, Row.LayoutOffsetX, Row.LayoutOffsetY)
			: FString::Printf(TEXT("G:%d:%d:%.3f:%.3f"),
				Row.LayoutRow, Row.LayoutColumn, Row.LayoutOffsetX, Row.LayoutOffsetY);
		TSet<FString>& UsedLayouts = UsedLayoutsByGraph.FindOrAdd(Row.GraphId);
		if (UsedLayouts.Contains(LayoutKey))
		{
			OutError = FString::Printf(TEXT("노드 배치 좌표가 중복됨: GraphId=%d Layout=%s"),
				Row.GraphId, *LayoutKey);
			Reset();
			return false;
		}
		UsedLayouts.Add(LayoutKey);


		// NodeId 조회와 GraphId별 조회에 사용할 Map 생성
		NodesById.Add(Row.NodeId, Row);
		NodeIdsByGraph.FindOrAdd(Row.GraphId).Add(Row.NodeId);
	}

	// 전체 NodeId가 모인 뒤 부모 참조와 부모 레벨을 검사한다.
	for (const TPair<int32, FNUProgressionNodeDefinitionRow>& Pair : NodesById)
	{
		const FNUProgressionNodeDefinitionRow& Node = Pair.Value;
		const int32 ParentIds[] = { Node.ParentNodeId1, Node.ParentNodeId2, Node.ParentNodeId3 };
		for (const int32 ParentId : ParentIds)
		{
			if (ParentId <= 0)
			{
				continue;
			}

			const FNUProgressionNodeDefinitionRow* Parent = NodesById.Find(ParentId);

			// 테이블에 없는 부모 NodeId를 참조할 수 없음
			if (Parent == nullptr)
			{
				OutError = FString::Printf(TEXT("부모 노드가 존재하지 않음: NodeId=%d ParentId=%d"),
					Node.NodeId, ParentId);
				Reset();
				return false;
			}

			// 다른 성장 그래프의 노드를 부모로 연결할 수 없음
			if (Parent->GraphId != Node.GraphId)
			{
				OutError = FString::Printf(TEXT("부모 노드가 다른 그래프에 속함: NodeId=%d ParentId=%d"),
					Node.NodeId, ParentId);
				Reset();
				return false;
			}

			// 자식 노드의 요구 레벨이 부모보다 낮을 수 없음
			if (Parent->RequiredLevel > Node.RequiredLevel)
			{
				OutError = FString::Printf(TEXT("노드 요구 레벨이 부모보다 낮음: NodeId=%d ParentId=%d"),
					Node.NodeId, ParentId);
				Reset();
				return false;
			}


			// 원본 행은 부모만 알고 있으므로 반대 방향인 자식 조회 Map도 생성
			ChildNodeIdsByNode.FindOrAdd(ParentId).Add(Node.NodeId);
		}
	}

	// 테이블 행 순서와 관계없이 항상 같은 순서로 조회되게 한다.
	for (TPair<int32, TArray<int32>>& Pair : NodeIdsByGraph)
	{
		Pair.Value.Sort([this](int32 Left, int32 Right)
		{
			return SortNodeIds(Left, Right, NodesById);
		});
	}
	for (TPair<int32, TArray<int32>>& Pair : ChildNodeIdsByNode)
	{
		Pair.Value.Sort([this](int32 Left, int32 Right)
		{
			return SortNodeIds(Left, Right, NodesById);
		});
	}

	// 마지막으로 루트 개수, 순환 연결, 도달할 수 없는 노드를 검사한다.
	for (const TPair<int32, TArray<int32>>& Pair : NodeIdsByGraph)
	{
		if (!ValidateGraphMaps(Pair.Key, NodesById, NodeIdsByGraph, ChildNodeIdsByNode, OutError))
		{
			Reset();
			return false;
		}
	}

	return true;
}

void FNUProgressionGraphData::Reset()
{
	NodesById.Reset();
	NodeIdsByGraph.Reset();
	ChildNodeIdsByNode.Reset();
}

const FNUProgressionNodeDefinitionRow* FNUProgressionGraphData::FindNode(int32 NodeId) const
{
	return NodeId <= 0 ? nullptr : NodesById.Find(NodeId);
}

TArray<const FNUProgressionNodeDefinitionRow*> FNUProgressionGraphData::GetAllNodes() const
{
	TArray<const FNUProgressionNodeDefinitionRow*> Result;
	Result.Reserve(NodesById.Num());
	for (const TPair<int32, FNUProgressionNodeDefinitionRow>& Pair : NodesById)
	{
		Result.Add(&Pair.Value);
	}
	Result.Sort([](const FNUProgressionNodeDefinitionRow& Left, const FNUProgressionNodeDefinitionRow& Right)
	{
		return Left.NodeId < Right.NodeId;
	});
	return Result;
}

TArray<const FNUProgressionNodeDefinitionRow*> FNUProgressionGraphData::GetNodesByGraph(int32 GraphId) const
{
	TArray<const FNUProgressionNodeDefinitionRow*> Result;
	const TArray<int32>* NodeIds = NodeIdsByGraph.Find(GraphId);
	if (NodeIds == nullptr)
	{
		return Result;
	}

	Result.Reserve(NodeIds->Num());
	for (const int32 NodeId : *NodeIds)
	{
		if (const FNUProgressionNodeDefinitionRow* Node = NodesById.Find(NodeId))
		{
			Result.Add(Node);
		}
	}
	return Result;
}

TArray<const FNUProgressionNodeDefinitionRow*> FNUProgressionGraphData::GetParentNodes(int32 NodeId) const
{
	TArray<const FNUProgressionNodeDefinitionRow*> Result;
	const FNUProgressionNodeDefinitionRow* Node = FindNode(NodeId);
	if (Node == nullptr)
	{
		return Result;
	}

	const int32 ParentIds[] = { Node->ParentNodeId1, Node->ParentNodeId2, Node->ParentNodeId3 };
	for (const int32 ParentId : ParentIds)
	{
		if (const FNUProgressionNodeDefinitionRow* Parent = FindNode(ParentId))
		{
			Result.Add(Parent);
		}
	}
	return Result;
}

TArray<const FNUProgressionNodeDefinitionRow*> FNUProgressionGraphData::GetChildNodes(int32 NodeId) const
{
	TArray<const FNUProgressionNodeDefinitionRow*> Result;
	const TArray<int32>* ChildIds = ChildNodeIdsByNode.Find(NodeId);
	if (ChildIds == nullptr)
	{
		return Result;
	}

	Result.Reserve(ChildIds->Num());
	for (const int32 ChildId : *ChildIds)
	{
		if (const FNUProgressionNodeDefinitionRow* Child = FindNode(ChildId))
		{
			Result.Add(Child);
		}
	}
	return Result;
}

ENUProgressionNodeState FNUProgressionGraphData::EvaluateNodeState(int32 NodeId, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context) const
{
	const FNUProgressionNodeDefinitionRow* Node = FindNode(NodeId);
	if (Node == nullptr)
	{
		return ENUProgressionNodeState::Locked;
	}

	return FNUProgressionNodeRules::Evaluate(*Node, State, Context);
}

bool FNUProgressionGraphData::ValidateGraph(int32 GraphId, FString& OutError) const
{
	OutError.Reset();
	return ValidateGraphMaps(GraphId, NodesById, NodeIdsByGraph, ChildNodeIdsByNode, OutError);
}

bool FNUProgressionStaticData::BuildFromTables(
	const UDataTable* JobTable,
	const UDataTable* AdvancementTable,
	const UDataTable* RewardTable,
	const FNUProgressionGraphData& GraphData,
	FString& OutError)
{
	Reset();
	OutError.Reset();

	TArray<FNUJobDefinitionRow> JobRows;
	TArray<FNUJobAdvancementDefinitionRow> AdvancementRows;
	TArray<FNUProgressionNodeRewardRow> RewardRows;
	if (!CopyTableRows(JobTable, TEXT("직업"), JobRows, OutError)
		|| !CopyTableRows(AdvancementTable, TEXT("전직"), AdvancementRows, OutError)
		|| !CopyTableRows(RewardTable, TEXT("노드 보상"), RewardRows, OutError))
	{
		return false;
	}

	if (JobRows.IsEmpty())
	{
		OutError = TEXT("직업 행이 비어 있음");
		return false;
	}

	for (const FNUJobDefinitionRow& Row : JobRows)
	{
		if (Row.JobId <= 0 || Row.GraphId <= 0)
		{
			OutError = FString::Printf(TEXT("직업 필수 ID가 잘못됨: JobId=%d GraphId=%d"),
				Row.JobId, Row.GraphId);
			Reset();
			return false;
		}
		if (JobsById.Contains(Row.JobId))
		{
			OutError = FString::Printf(TEXT("중복 JobId: %d"), Row.JobId);
			Reset();
			return false;
		}
		JobsById.Add(Row.JobId, Row);
	}

	if (AdvancementRows.IsEmpty())
	{
		OutError = TEXT("전직 행이 비어 있음");
		Reset();
		return false;
	}

	for (const FNUJobAdvancementDefinitionRow& Row : AdvancementRows)
	{
		if (Row.AdvancementId <= 0 || Row.ParentJobId <= 0 || Row.AdvancementTier < 2)
		{
			OutError = FString::Printf(TEXT("전직 필수 값이 잘못됨: AdvancementId=%d ParentJobId=%d Tier=%d"),
				Row.AdvancementId, Row.ParentJobId, Row.AdvancementTier);
			Reset();
			return false;
		}
		if (!JobsById.Contains(Row.ParentJobId))
		{
			OutError = FString::Printf(TEXT("전직의 기본 직업이 존재하지 않음: AdvancementId=%d ParentJobId=%d"),
				Row.AdvancementId, Row.ParentJobId);
			Reset();
			return false;
		}
		if (AdvancementsById.Contains(Row.AdvancementId))
		{
			OutError = FString::Printf(TEXT("중복 AdvancementId: %d"), Row.AdvancementId);
			Reset();
			return false;
		}
		if ((Row.AdvancementTier == 2 && Row.ParentAdvancementId != 0)
			|| (Row.AdvancementTier > 2 && Row.ParentAdvancementId <= 0))
		{
			OutError = FString::Printf(TEXT("전직 단계와 부모 전직이 맞지 않음: AdvancementId=%d Tier=%d ParentAdvancementId=%d"),
				Row.AdvancementId, Row.AdvancementTier, Row.ParentAdvancementId);
			Reset();
			return false;
		}

		AdvancementsById.Add(Row.AdvancementId, Row);
		AdvancementIdsByJob.FindOrAdd(Row.ParentJobId).Add(Row.AdvancementId);
	}

	for (const TPair<int32, FNUJobAdvancementDefinitionRow>& Pair : AdvancementsById)
	{
		const FNUJobAdvancementDefinitionRow& Row = Pair.Value;
		if (Row.ParentAdvancementId <= 0)
		{
			continue;
		}

		const FNUJobAdvancementDefinitionRow* Parent = AdvancementsById.Find(Row.ParentAdvancementId);
		if (Parent == nullptr
			|| Parent->ParentJobId != Row.ParentJobId
			|| Parent->AdvancementTier + 1 != Row.AdvancementTier)
		{
			OutError = FString::Printf(TEXT("부모 전직 관계가 잘못됨: AdvancementId=%d ParentAdvancementId=%d"),
				Row.AdvancementId, Row.ParentAdvancementId);
			Reset();
			return false;
		}

		TSet<int32> VisitedIds;
		const FNUJobAdvancementDefinitionRow* Current = &Row;
		while (Current != nullptr && Current->ParentAdvancementId > 0)
		{
			if (VisitedIds.Contains(Current->AdvancementId))
			{
				OutError = FString::Printf(TEXT("전직 순환 참조 발견: AdvancementId=%d"), Row.AdvancementId);
				Reset();
				return false;
			}
			VisitedIds.Add(Current->AdvancementId);
			Current = AdvancementsById.Find(Current->ParentAdvancementId);
		}
	}

	for (TPair<int32, TArray<int32>>& Pair : AdvancementIdsByJob)
	{
		Pair.Value.Sort([this](int32 LeftId, int32 RightId)
		{
			const FNUJobAdvancementDefinitionRow* Left = AdvancementsById.Find(LeftId);
			const FNUJobAdvancementDefinitionRow* Right = AdvancementsById.Find(RightId);
			if (Left == nullptr || Right == nullptr)
			{
				return Left != nullptr;
			}
			return Left->AdvancementTier == Right->AdvancementTier
				? LeftId < RightId
				: Left->AdvancementTier < Right->AdvancementTier;
		});
	}

	for (const FNUProgressionNodeDefinitionRow* Node : GraphData.GetAllNodes())
	{
		if (Node == nullptr)
		{
			continue;
		}
		if (Node->RequiredJobId > 0 && !JobsById.Contains(Node->RequiredJobId))
		{
			OutError = FString::Printf(TEXT("노드의 요구 직업이 존재하지 않음: NodeId=%d JobId=%d"),
				Node->NodeId, Node->RequiredJobId);
			Reset();
			return false;
		}
		if (Node->RequiredAdvancementId > 0)
		{
			const FNUJobAdvancementDefinitionRow* Advancement = AdvancementsById.Find(Node->RequiredAdvancementId);
			if (Advancement == nullptr
				|| (Node->RequiredJobId > 0 && Advancement->ParentJobId != Node->RequiredJobId))
			{
				OutError = FString::Printf(TEXT("노드의 요구 전직이 잘못됨: NodeId=%d AdvancementId=%d"),
					Node->NodeId, Node->RequiredAdvancementId);
				Reset();
				return false;
			}
		}
	}

	TMap<int32, TSet<int32>> UsedSortOrdersByNode;
	TMap<int32, TSet<int32>> AdvancementChoiceIdsByNode;
	TMap<int64, int32> AdvancementRewardCountsByChoice;
	TMap<int64, int32> AdvancementSkillRewardCountsByChoice;
	TMap<int64, int32> AdvancementTotalRewardCountsByChoice;
	for (const FNUProgressionNodeRewardRow& Row : RewardRows)
	{
		if (Row.RewardRowId <= 0
			|| Row.NodeId <= 0
			|| Row.SortOrder < 0
			|| Row.ChoiceAdvancementId < 0
			|| !FMath::IsFinite(Row.Value))
		{
			OutError = FString::Printf(TEXT("노드 보상 필수 값이 잘못됨: RewardRowId=%d NodeId=%d SortOrder=%d"),
				Row.RewardRowId, Row.NodeId, Row.SortOrder);
			Reset();
			return false;
		}
		if (!IsValidRewardType(Row.RewardType) || Row.RewardType == ENUProgressionNodeRewardType::None)
		{
			OutError = FString::Printf(TEXT("노드 보상 타입이 잘못됨: RewardRowId=%d"), Row.RewardRowId);
			Reset();
			return false;
		}
		const FNUProgressionNodeDefinitionRow* RewardNode = GraphData.FindNode(Row.NodeId);
		if (Row.RewardId <= 0 || RewardNode == nullptr)
		{
			OutError = FString::Printf(TEXT("노드 보상 참조가 잘못됨: RewardRowId=%d NodeId=%d RewardId=%d"),
				Row.RewardRowId, Row.NodeId, Row.RewardId);
			Reset();
			return false;
		}
		if ((RewardNode->NodeType == ENUProgressionNodeType::Advancement) != (Row.ChoiceAdvancementId > 0))
		{
			OutError = FString::Printf(TEXT("전직 선택 보상 범위가 잘못됨: RewardRowId=%d NodeId=%d ChoiceAdvancementId=%d"),
				Row.RewardRowId, Row.NodeId, Row.ChoiceAdvancementId);
			Reset();
			return false;
		}
		if (Row.ChoiceAdvancementId > 0)
		{
			const FNUJobAdvancementDefinitionRow* Choice = AdvancementsById.Find(Row.ChoiceAdvancementId);
			if (Choice == nullptr
				|| (RewardNode->RequiredJobId > 0 && Choice->ParentJobId != RewardNode->RequiredJobId)
				|| Choice->ParentAdvancementId != RewardNode->RequiredAdvancementId)
			{
				OutError = FString::Printf(TEXT("전직 선택 보상 참조가 잘못됨: RewardRowId=%d ChoiceAdvancementId=%d"),
					Row.RewardRowId, Row.ChoiceAdvancementId);
				Reset();
				return false;
			}
			AdvancementChoiceIdsByNode.FindOrAdd(Row.NodeId).Add(Row.ChoiceAdvancementId);
			const int64 ChoiceKey = (static_cast<int64>(Row.NodeId) << 32)
				| static_cast<uint32>(Row.ChoiceAdvancementId);
			AdvancementTotalRewardCountsByChoice.FindOrAdd(ChoiceKey)++;
			if (Row.RewardType == ENUProgressionNodeRewardType::ActiveSkill
				|| Row.RewardType == ENUProgressionNodeRewardType::PassiveSkill)
			{
				AdvancementSkillRewardCountsByChoice.FindOrAdd(ChoiceKey)++;
			}
		}
		if (Row.RewardType == ENUProgressionNodeRewardType::PrimaryStat
			&& (Row.RewardId < 1 || Row.RewardId > 5 || Row.Value <= 0.f))
		{
			OutError = FString::Printf(TEXT("1차 스탯 보상이 잘못됨: RewardRowId=%d RewardId=%d Value=%g"),
				Row.RewardRowId, Row.RewardId, Row.Value);
			Reset();
			return false;
		}
		if (Row.RewardType == ENUProgressionNodeRewardType::Advancement)
		{
			const FNUJobAdvancementDefinitionRow* Advancement = AdvancementsById.Find(Row.RewardId);
			if (RewardNode->NodeType != ENUProgressionNodeType::Advancement
				|| Advancement == nullptr
				|| Row.ChoiceAdvancementId != Row.RewardId
				|| (RewardNode->RequiredJobId > 0 && Advancement->ParentJobId != RewardNode->RequiredJobId))
			{
				OutError = FString::Printf(TEXT("전직 노드 보상이 잘못됨: RewardRowId=%d NodeId=%d AdvancementId=%d"),
					Row.RewardRowId, Row.NodeId, Row.RewardId);
				Reset();
				return false;
			}
			const int64 ChoiceKey = (static_cast<int64>(Row.NodeId) << 32)
				| static_cast<uint32>(Row.ChoiceAdvancementId);
			AdvancementRewardCountsByChoice.FindOrAdd(ChoiceKey)++;
		}
		if (RewardsById.Contains(Row.RewardRowId))
		{
			OutError = FString::Printf(TEXT("중복 RewardRowId: %d"), Row.RewardRowId);
			Reset();
			return false;
		}

		TSet<int32>& UsedSortOrders = UsedSortOrdersByNode.FindOrAdd(Row.NodeId);
		if (UsedSortOrders.Contains(Row.SortOrder))
		{
			OutError = FString::Printf(TEXT("노드 보상 순서가 중복됨: NodeId=%d SortOrder=%d"), Row.NodeId, Row.SortOrder);
			Reset();
			return false;
		}
		UsedSortOrders.Add(Row.SortOrder);
		RewardsById.Add(Row.RewardRowId, Row);
		RewardIdsByNode.FindOrAdd(Row.NodeId).Add(Row.RewardRowId);
	}

	for (const FNUProgressionNodeDefinitionRow* Node : GraphData.GetAllNodes())
	{
		if (Node == nullptr || Node->NodeType != ENUProgressionNodeType::Advancement)
		{
			continue;
		}

		const TSet<int32>* ChoiceIds = AdvancementChoiceIdsByNode.Find(Node->NodeId);
		if (ChoiceIds == nullptr || ChoiceIds->IsEmpty())
		{
			OutError = FString::Printf(TEXT("전직 노드에 선택 갈래가 없음: NodeId=%d"), Node->NodeId);
			Reset();
			return false;
		}
		for (const int32 ChoiceAdvancementId : *ChoiceIds)
		{
			const int64 ChoiceKey = (static_cast<int64>(Node->NodeId) << 32)
				| static_cast<uint32>(ChoiceAdvancementId);
			if (AdvancementTotalRewardCountsByChoice.FindRef(ChoiceKey) != 4
				|| AdvancementRewardCountsByChoice.FindRef(ChoiceKey) != 1)
			{
				OutError = FString::Printf(TEXT("전직 갈래에는 스킬 3개와 전직 보상 1개만 필요함: NodeId=%d AdvancementId=%d"),
					Node->NodeId, ChoiceAdvancementId);
				Reset();
				return false;
			}
			if (AdvancementSkillRewardCountsByChoice.FindRef(ChoiceKey) != 3)
			{
				OutError = FString::Printf(TEXT("전직 갈래에는 시작 스킬 보상이 정확히 3개 필요함: NodeId=%d AdvancementId=%d"),
					Node->NodeId, ChoiceAdvancementId);
				Reset();
				return false;
			}
		}
	}

	for (const FNUProgressionNodeDefinitionRow* Node : GraphData.GetAllNodes())
	{
		if (Node == nullptr || Node->NodeType == ENUProgressionNodeType::Advancement)
		{
			continue;
		}

		int32 PrimaryStatRewardCount = 0;
		int32 SkillRewardCount = 0;
		const TArray<int32>* RewardIds = RewardIdsByNode.Find(Node->NodeId);
		if (RewardIds != nullptr)
		{
			for (const int32 RewardId : *RewardIds)
			{
				const FNUProgressionNodeRewardRow* Reward = RewardsById.Find(RewardId);
				if (Reward == nullptr)
				{
					continue;
				}
				PrimaryStatRewardCount += Reward->RewardType == ENUProgressionNodeRewardType::PrimaryStat ? 1 : 0;
				const bool bIsSkillReward = Reward->RewardType == ENUProgressionNodeRewardType::ActiveSkill
					|| Reward->RewardType == ENUProgressionNodeRewardType::PassiveSkill;
				SkillRewardCount += bIsSkillReward ? 1 : 0;
			}
		}

		const int32 ExpectedPrimaryStatRewardCount = Node->NodeType == ENUProgressionNodeType::Stat ? 1 : 0;
		int32 ExpectedSkillRewardCount = 0;
		switch (Node->NodeType)
		{
		case ENUProgressionNodeType::Root:
			ExpectedSkillRewardCount = 3;
			break;
		case ENUProgressionNodeType::Skill:
		case ENUProgressionNodeType::Main:
			ExpectedSkillRewardCount = 1;
			break;
		case ENUProgressionNodeType::Final:
			ExpectedSkillRewardCount = 2;
			break;
		default:
			break;
		}
		if (PrimaryStatRewardCount != ExpectedPrimaryStatRewardCount || SkillRewardCount != ExpectedSkillRewardCount)
		{
			OutError = FString::Printf(TEXT("노드 타입과 보상 구성이 다름: NodeId=%d Stat=%d/%d Skill=%d/%d"),
				Node->NodeId,
				PrimaryStatRewardCount,
				ExpectedPrimaryStatRewardCount,
				SkillRewardCount,
				ExpectedSkillRewardCount);
			Reset();
			return false;
		}
	}

	for (TPair<int32, TArray<int32>>& Pair : RewardIdsByNode)
	{
		Pair.Value.Sort([this](int32 LeftId, int32 RightId)
		{
			const FNUProgressionNodeRewardRow* Left = RewardsById.Find(LeftId);
			const FNUProgressionNodeRewardRow* Right = RewardsById.Find(RightId);
			if (Left == nullptr || Right == nullptr)
			{
				return Left != nullptr;
			}
			return Left->SortOrder == Right->SortOrder
				? LeftId < RightId
				: Left->SortOrder < Right->SortOrder;
		});
	}

	return true;
}

void FNUProgressionStaticData::Reset()
{
	JobsById.Reset();
	AdvancementsById.Reset();
	AdvancementIdsByJob.Reset();
	RewardsById.Reset();
	RewardIdsByNode.Reset();
}

const FNUJobDefinitionRow* FNUProgressionStaticData::FindJob(int32 JobId) const
{
	return JobId <= 0 ? nullptr : JobsById.Find(JobId);
}

TArray<const FNUJobDefinitionRow*> FNUProgressionStaticData::GetAllJobs() const
{
	TArray<const FNUJobDefinitionRow*> Result;
	Result.Reserve(JobsById.Num());
	for (const TPair<int32, FNUJobDefinitionRow>& Pair : JobsById)
	{
		Result.Add(&Pair.Value);
	}
	// TMap 순회 순서와 무관하게 선택 화면의 직업 순서를 고정한다.
	Result.Sort([](const FNUJobDefinitionRow& Left, const FNUJobDefinitionRow& Right)
	{
		return Left.JobId < Right.JobId;
	});
	return Result;
}

const FNUJobAdvancementDefinitionRow* FNUProgressionStaticData::FindAdvancement(int32 AdvancementId) const
{
	return AdvancementId <= 0 ? nullptr : AdvancementsById.Find(AdvancementId);
}

TArray<const FNUJobAdvancementDefinitionRow*> FNUProgressionStaticData::GetAdvancementsByJob(int32 JobId) const
{
	TArray<const FNUJobAdvancementDefinitionRow*> Result;
	const TArray<int32>* AdvancementIds = AdvancementIdsByJob.Find(JobId);
	if (AdvancementIds == nullptr)
	{
		return Result;
	}

	Result.Reserve(AdvancementIds->Num());
	for (const int32 AdvancementId : *AdvancementIds)
	{
		if (const FNUJobAdvancementDefinitionRow* Advancement = AdvancementsById.Find(AdvancementId))
		{
			Result.Add(Advancement);
		}
	}
	return Result;
}

TArray<const FNUProgressionNodeRewardRow*> FNUProgressionStaticData::GetRewardsByNode(int32 NodeId) const
{
	TArray<const FNUProgressionNodeRewardRow*> Result;
	const TArray<int32>* RewardIds = RewardIdsByNode.Find(NodeId);
	if (RewardIds == nullptr)
	{
		return Result;
	}

	Result.Reserve(RewardIds->Num());
	for (const int32 RewardId : *RewardIds)
	{
		if (const FNUProgressionNodeRewardRow* Reward = RewardsById.Find(RewardId))
		{
			Result.Add(Reward);
		}
	}
	return Result;
}

void UNUProgressionDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Project Settings에 등록된 정적 성장 데이터 테이블 조회
	const UNUProgressionDataSettings* Settings = GetDefault<UNUProgressionDataSettings>();
	if (Settings == nullptr
		|| Settings->ProgressionNodeTable.IsNull()
		|| Settings->ProgressionNodeRewardTable.IsNull()
		|| Settings->JobDefinitionTable.IsNull()
		|| Settings->JobAdvancementTable.IsNull())
	{
		UE_LOG(LogNUProgression, Log, TEXT("성장 정적 데이터 테이블이 모두 설정되지 않음"));
		return;
	}

	// 모든 화면과 플레이어가 공유할 정적 데이터라 GameInstance 시작 시 한 번 로드한다.
	UDataTable* LoadedNodeTable = Settings->ProgressionNodeTable.LoadSynchronous();
	UDataTable* LoadedRewardTable = Settings->ProgressionNodeRewardTable.LoadSynchronous();
	UDataTable* LoadedJobTable = Settings->JobDefinitionTable.LoadSynchronous();
	UDataTable* LoadedAdvancementTable = Settings->JobAdvancementTable.LoadSynchronous();
	if (LoadedNodeTable == nullptr
		|| LoadedRewardTable == nullptr
		|| LoadedJobTable == nullptr
		|| LoadedAdvancementTable == nullptr)
	{
		UE_LOG(LogNUProgression, Warning, TEXT("성장 정적 데이터 테이블 로드 실패"));
		return;
	}

	// 검증과 조회 Map 생성에 실패하면 준비되지 않은 상태로 둠
	FString Error;
	if (!GraphData.BuildFromTable(LoadedNodeTable, Error))
	{
		UE_LOG(LogNUProgression, Error, TEXT("잘못된 성장 노드 테이블: %s, 이유=%s"),
			*LoadedNodeTable->GetName(), *Error);
		return;
	}
	if (!StaticData.BuildFromTables(
		LoadedJobTable,
		LoadedAdvancementTable,
		LoadedRewardTable,
		GraphData,
		Error))
	{
		GraphData.Reset();
		UE_LOG(LogNUProgression, Error, TEXT("잘못된 성장 정적 데이터: %s"), *Error);
		return;
	}

	// 검증이 끝난 테이블만 런타임 조회에 사용한다.
	ProgressionNodeTable = LoadedNodeTable;
	ProgressionNodeRewardTable = LoadedRewardTable;
	JobDefinitionTable = LoadedJobTable;
	JobAdvancementTable = LoadedAdvancementTable;

	UE_LOG(LogNUProgression, Log, TEXT("성장 정적 데이터 준비 완료: 노드=%d 직업=%d 전직=%d 보상=%d"),
		GraphData.Num(),
		StaticData.GetJobCount(),
		StaticData.GetAdvancementCount(),
		StaticData.GetRewardCount());
}

void UNUProgressionDataSubsystem::Deinitialize()
{
	ProgressionNodeTable = nullptr;
	ProgressionNodeRewardTable = nullptr;
	JobDefinitionTable = nullptr;
	JobAdvancementTable = nullptr;
	GraphData.Reset();
	StaticData.Reset();
	Super::Deinitialize();
}

const FNUProgressionNodeDefinitionRow* UNUProgressionDataSubsystem::FindNode(int32 NodeId) const
{
	return GraphData.FindNode(NodeId);
}

TArray<const FNUProgressionNodeDefinitionRow*> UNUProgressionDataSubsystem::GetNodesByGraph(int32 GraphId) const
{
	return GraphData.GetNodesByGraph(GraphId);
}

TArray<const FNUProgressionNodeDefinitionRow*> UNUProgressionDataSubsystem::GetParentNodes(int32 NodeId) const
{
	return GraphData.GetParentNodes(NodeId);
}

TArray<const FNUProgressionNodeDefinitionRow*> UNUProgressionDataSubsystem::GetChildNodes(int32 NodeId) const
{
	return GraphData.GetChildNodes(NodeId);
}

ENUProgressionNodeState UNUProgressionDataSubsystem::EvaluateNodeState(int32 NodeId, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context) const
{
	return GraphData.EvaluateNodeState(NodeId, State, Context);
}

bool UNUProgressionDataSubsystem::ValidateGraph(int32 GraphId, FString& OutError) const
{
	return GraphData.ValidateGraph(GraphId, OutError);
}

const FNUJobDefinitionRow* UNUProgressionDataSubsystem::FindJob(int32 JobId) const
{
	return StaticData.FindJob(JobId);
}

TArray<const FNUJobDefinitionRow*> UNUProgressionDataSubsystem::GetAllJobs() const
{
	return StaticData.GetAllJobs();
}

const FNUJobAdvancementDefinitionRow* UNUProgressionDataSubsystem::FindAdvancement(int32 AdvancementId) const
{
	return StaticData.FindAdvancement(AdvancementId);
}

TArray<const FNUJobAdvancementDefinitionRow*> UNUProgressionDataSubsystem::GetAdvancementsByJob(int32 JobId) const
{
	return StaticData.GetAdvancementsByJob(JobId);
}

TArray<const FNUProgressionNodeRewardRow*> UNUProgressionDataSubsystem::GetRewardsByNode(int32 NodeId) const
{
	return StaticData.GetRewardsByNode(NodeId);
}
