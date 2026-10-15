#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUProgressionNodeRewardTypes.generated.h"

// 노드 획득 시 지급할 데이터의 종류
UENUM(BlueprintType)
enum class ENUProgressionNodeRewardType : uint8
{
	None,
	PrimaryStat,
	ActiveSkill,
	PassiveSkill,
	Advancement
};

// 노드 하나에 여러 보상을 연결할 수 있도록 보상 한 건을 별도 행으로 정의
USTRUCT(BlueprintType)
struct PROJECTM_API FNUProgressionNodeRewardRow : public FTableRowBase
{
	GENERATED_BODY()

	// NodeId나 SortOrder에서 계산하지 않는 독립적인 영구 식별자다. 신규 보상은 기존 ID를 재사용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	int32 RewardRowId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	int32 NodeId = 0;

	// 한 노드가 여러 보상을 줄 때 표시와 적용 순서를 고정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	ENUProgressionNodeRewardType RewardType = ENUProgressionNodeRewardType::None;

	// RewardType에 따라 스탯, 스킬, 특성, 전직의 영구 ID를 참조한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	int32 RewardId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	FName RewardKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	float Value = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression|Reward")
	int32 ChoiceAdvancementId = 0;
};
