#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Data/NUJobTypes.h"
#include "CharacterProgression/Data/NUProgressionNodeRewardTypes.h"
#include "CharacterProgression/Data/NUProgressionNodeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUProgressionDataSubsystem.generated.h"

class UDataTable;

// DataTable 행을 검증하고 런타임 조회용 인덱스로 변환한다.
// 플레이어별 획득 상태나 포인트는 소유하지 않는다.
class PROJECTM_API FNUProgressionGraphData
{
public:
	bool BuildFromTable(const UDataTable* Table, FString& OutError);
	bool BuildFromRows(const TArray<FNUProgressionNodeDefinitionRow>& Rows, FString& OutError);
	void Reset();

	const FNUProgressionNodeDefinitionRow* FindNode(int32 NodeId) const;
	TArray<const FNUProgressionNodeDefinitionRow*> GetAllNodes() const;
	TArray<const FNUProgressionNodeDefinitionRow*> GetNodesByGraph(int32 GraphId) const;
	TArray<const FNUProgressionNodeDefinitionRow*> GetParentNodes(int32 NodeId) const;
	TArray<const FNUProgressionNodeDefinitionRow*> GetChildNodes(int32 NodeId) const;
	ENUProgressionNodeState EvaluateNodeState(int32 NodeId, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context) const;

	bool ValidateGraph(int32 GraphId, FString& OutError) const;
	int32 Num() const { return NodesById.Num(); }
	bool IsEmpty() const { return NodesById.IsEmpty(); }

private:
	TMap<int32, FNUProgressionNodeDefinitionRow> NodesById;
	TMap<int32, TArray<int32>> NodeIdsByGraph;
	TMap<int32, TArray<int32>> ChildNodeIdsByNode;
};

// 직업, 전직, 노드 보상을 검증하고 ID 기반 조회 맵으로 보관한다.
class PROJECTM_API FNUProgressionStaticData
{
public:
	bool BuildFromTables(
		const UDataTable* JobTable,
		const UDataTable* AdvancementTable,
		const UDataTable* RewardTable,
		const FNUProgressionGraphData& GraphData,
		FString& OutError);
	void Reset();

	const FNUJobDefinitionRow* FindJob(int32 JobId) const;
	// 직업 선택 화면에서 사용할 전체 직업을 JobId 순서로 반환한다.
	TArray<const FNUJobDefinitionRow*> GetAllJobs() const;
	const FNUJobAdvancementDefinitionRow* FindAdvancement(int32 AdvancementId) const;
	TArray<const FNUJobAdvancementDefinitionRow*> GetAdvancementsByJob(int32 JobId) const;
	TArray<const FNUProgressionNodeRewardRow*> GetRewardsByNode(int32 NodeId) const;

	int32 GetJobCount() const { return JobsById.Num(); }
	int32 GetAdvancementCount() const { return AdvancementsById.Num(); }
	int32 GetRewardCount() const { return RewardsById.Num(); }
	bool IsReady() const { return !JobsById.IsEmpty() && !AdvancementsById.IsEmpty(); }

private:
	TMap<int32, FNUJobDefinitionRow> JobsById;
	TMap<int32, FNUJobAdvancementDefinitionRow> AdvancementsById;
	TMap<int32, TArray<int32>> AdvancementIdsByJob;
	TMap<int32, FNUProgressionNodeRewardRow> RewardsById;
	TMap<int32, TArray<int32>> RewardIdsByNode;
};

// 모든 플레이어가 공유하는 정적 성장 그래프를 GameInstance 수명으로 관리한다.
// 포인트와 획득 NodeId는 플레이어별 FNUProgressionState에서 따로 관리한다.
UCLASS()
class PROJECTM_API UNUProgressionDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FNUProgressionNodeDefinitionRow* FindNode(int32 NodeId) const;
	TArray<const FNUProgressionNodeDefinitionRow*> GetNodesByGraph(int32 GraphId) const;
	TArray<const FNUProgressionNodeDefinitionRow*> GetParentNodes(int32 NodeId) const;
	TArray<const FNUProgressionNodeDefinitionRow*> GetChildNodes(int32 NodeId) const;
	ENUProgressionNodeState EvaluateNodeState(int32 NodeId, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context) const;
	bool ValidateGraph(int32 GraphId, FString& OutError) const;

	const FNUJobDefinitionRow* FindJob(int32 JobId) const;
	// 검증이 끝난 전체 직업 정의를 JobId 순서로 반환한다.
	TArray<const FNUJobDefinitionRow*> GetAllJobs() const;
	const FNUJobAdvancementDefinitionRow* FindAdvancement(int32 AdvancementId) const;
	TArray<const FNUJobAdvancementDefinitionRow*> GetAdvancementsByJob(int32 JobId) const;
	TArray<const FNUProgressionNodeRewardRow*> GetRewardsByNode(int32 NodeId) const;

	bool IsReady() const { return !GraphData.IsEmpty() && StaticData.IsReady(); }

private:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ProgressionNodeTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ProgressionNodeRewardTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> JobDefinitionTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> JobAdvancementTable;

	FNUProgressionGraphData GraphData;
	FNUProgressionStaticData StaticData;
};
