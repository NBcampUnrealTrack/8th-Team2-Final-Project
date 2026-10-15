#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Data/NUProgressionVersion.h"
#include "Engine/DataTable.h"
#include "NUProgressionNodeTypes.generated.h"

class UTexture2D;
struct FNUProgressionState;

// 노드가 그래프에서 맡는 역할과 UI 표시 종류
UENUM(BlueprintType)
enum class ENUProgressionNodeType : uint8
{
	Root,
	Stat,
	Skill,
	Advancement,
	Main,
	Final,
	Job			UMETA(Hidden),
	MinorTrait	UMETA(Hidden),
	MajorTrait	UMETA(Hidden)
};

// 현재 플레이어 기준으로 WBP에 표시할 노드 상태
UENUM(BlueprintType)
enum class ENUProgressionNodeState : uint8
{
	Locked		UMETA(DisplayName = "잠김"),
	Available	UMETA(DisplayName = "획득 가능"),
	Acquired	UMETA(DisplayName = "획득 완료")
};

// 노드 좌표를 배치하는 방식 
// 기존 그래프는 격자, 원형 트리는 극좌표를 사용
UENUM(BlueprintType)
enum class ENUProgressionNodeLayoutMode : uint8
{
	Grid,
	Radial
};

// 위치나 표시 이름과 무관하게 줄기의 해금 규칙을 구분한다.
UENUM(BlueprintType)
enum class ENUProgressionStemType : uint8
{
	None,
	Entry,
	Common,
	BranchA,
	BranchB
};

// 한 행이 그래프 노드 하나를 정의한다. 플레이어별 획득 상태는 이 구조체에 넣지 않는다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUProgressionNodeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 NodeId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 GraphId = 0;

	// 0은 줄기에 속하지 않는 최종 노드, 양수는 그래프 안에서 영구 유지하는 줄기 ID다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 StemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Node")
	ENUProgressionStemType StemType = ENUProgressionStemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Node")
	ENUProgressionNodeType NodeType = ENUProgressionNodeType::Stat;

	// 0은 레벨 조건 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Requirement", meta = (ClampMin = "0"))
	int32 RequiredLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Requirement")
	int32 RequiredJobId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Requirement")
	int32 RequiredAdvancementId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Connection")
	int32 ParentNodeId1 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Connection")
	int32 ParentNodeId2 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Connection")
	int32 ParentNodeId3 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Requirement", meta = (ClampMin = "0"))
	int32 PointCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout")
	ENUProgressionNodeLayoutMode LayoutMode = ENUProgressionNodeLayoutMode::Grid;

	// 줄기 안에서의 진행 단계
	// 진입 줄기는 0이 바깥 시작점, 10이 중앙 전직이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "0"))
	int32 StemStage = 0;

	// 같은 단계에 놓이는 갈래 번호
	// 가운데는 0, 좌우 갈래는 음수와 양수를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout")
	int32 StemLane = 0;

	// 원형 배치에서 그래프 중앙으로부터의 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "0.0"))
	float LayoutRadius = 0.f;

	// 원형 배치 각도
	// 0도는 오른쪽이며 양수 방향은 화면의 위쪽으로 회전한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout")
	float LayoutAngleDegrees = 0.f;

	// 시작 노드를 0으로 두고 진행 단계가 올라갈수록 1씩 증가한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout")
	int32 LayoutRow = 0;

	// 0을 그래프 중앙으로 두고 음수는 왼쪽, 양수는 오른쪽에 배치한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout")
	int32 LayoutColumn = 0;

	// 행과 열만으로 맞추기 어려운 노드의 세부 위치를 조정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout")
	float LayoutOffsetX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Layout")
	float LayoutOffsetY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Display")
	TSoftObjectPtr<UTexture2D> Icon;
};

// 플레이어의 직업 상태를 노드 배치와 분리해 선택 규칙에 전달한다.
struct PROJECTM_API FNUProgressionNodeRuleContext
{
	int32 JobId = 0;
	int32 AdvancementId = 0;
};

// 정적 노드 정의와 플레이어 상태를 비교해 현재 화면 상태를 계산한다.
struct PROJECTM_API FNUProgressionNodeRules
{
	static bool HasCurrentParent(const FNUProgressionNodeDefinitionRow& Node, int32 CurrentNodeId);
	static int32 GetCompletedRadialStemCount(const FNUProgressionState& State);
	static int32 GetEarnedPointCountForLevel(int32 Level);
	static int32 ResolveRequiredLevel(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionState& State);
	static int32 ResolveAvailableParentId(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context);
	static ENUProgressionNodeState Evaluate(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionState& State, const FNUProgressionNodeRuleContext& Context);
};
