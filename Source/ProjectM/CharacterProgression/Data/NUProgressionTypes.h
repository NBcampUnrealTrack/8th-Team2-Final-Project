#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Data/NUProgressionVersion.h"
#include "Engine/DataTable.h"
#include "NUProgressionTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNUProgression, Log, All);

// 저장과 데이터 테이블에서는 int32 ID를 사용하고 코드에서만 의미를 구분한다.
enum class ENUPrimaryStatId : int32
{
	Strength		= 1,
	Dexterity		= 2,
	Intelligence	= 3,
	Luck			= 4,
	Vitality		= 5
};

// ---------------------------------------------------------
// 스탯
// ---------------------------------------------------------

// Primary Stat 5종
// 성장 노드가 1씩 올리는 값이라 정수로 둠
USTRUCT(BlueprintType)
struct PROJECTM_API FNUPrimaryStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int32 Strength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int32 Dexterity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int32 Intelligence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int32 Luck = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int32 Vitality = 0;

	bool operator==(const FNUPrimaryStats& Other) const
	{
		return Strength == Other.Strength
			&& Dexterity == Other.Dexterity
			&& Intelligence == Other.Intelligence
			&& Luck == Other.Luck
			&& Vitality == Other.Vitality;
	}

	bool operator!=(const FNUPrimaryStats& Other) const
	{
		return !(*this == Other);
	}
};

namespace NUProgression
{
	inline constexpr int32 MaxPrimaryStat = 29;

	PROJECTM_API bool ArePrimaryStatsValid(const FNUPrimaryStats& Stats);
}


// Primary Stat과 GAS 효과로 계산되는 Secondary Stat 8종
USTRUCT(BlueprintType)
struct PROJECTM_API FNUSecondaryStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float MaxHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float MaxSP = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float AttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float MagicAttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float Defense = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float Speed = 0.f;

	// 확률 0~1, 0.1 -> 10%
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float CriticalRate = 0.f;

	// 치명타 배율, 1.5 -> 150%
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float CriticalDamage = 0.f;
};

// 기본 Secondary 값과 Primary Stat 1당 증가량을 담는 테이블 행
USTRUCT(BlueprintType)
struct PROJECTM_API FNUStatProgressionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int32 StatId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float MaxHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float MaxSP = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float AttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float MagicAttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float Defense = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float Speed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float CriticalRate = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	float CriticalDamage = 0.f;

	FNUSecondaryStats ToSecondaryStats() const
	{
		FNUSecondaryStats Stats;

		Stats.MaxHealth = MaxHealth;
		Stats.MaxSP = MaxSP;
		Stats.AttackPower = AttackPower;
		Stats.MagicAttackPower = MagicAttackPower;
		Stats.Defense = Defense;
		Stats.Speed = Speed;
		Stats.CriticalRate = CriticalRate;
		Stats.CriticalDamage = CriticalDamage;

		return Stats;
	}
};

// ---------------------------------------------------------
// 경험치
// ---------------------------------------------------------

// 현재 레벨에서 다음 레벨로 올라가는 데 필요한 경험치 한 행
USTRUCT(BlueprintType)
struct PROJECTM_API FNULevelProgressionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int32 Level = 1;

	// 다음 레벨로 넘어가기 위한 경험치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int64 RequiredExperienceToNext = 0;

	// 이 레벨이 시작되는 누적 경험치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Progression")
	int64 RequiredCumulativeExperience = 0;
};

// 서버가 확정한 성장 경로의 연결선 한 개
USTRUCT(BlueprintType)
struct PROJECTM_API FNUProgressionNodeLinkState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 ParentNodeId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 ChildNodeId = 0;

	bool operator==(const FNUProgressionNodeLinkState& Other) const
	{
		return ParentNodeId == Other.ParentNodeId && ChildNodeId == Other.ChildNodeId;
	}
};


// 서버에서 확정한 플레이어 성장 상태
USTRUCT(BlueprintType)
struct PROJECTM_API FNUProgressionState
{
	GENERATED_BODY()

	// 정적 노드 ID와 보상 의미를 해석할 때 사용하는 계약 버전
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 GraphVersion = NUProgressionGraphVersion;

	// 외부 서버가 성장 변경을 확정할 때마다 증가시키는 낙관적 동시성 버전
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int64 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression")
	int32 Level = 1;

	// 서버와 저장소에서 사용하는 기준 경험치
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression")
	int64 TotalExperience = 0;

	// TotalExperience에서 현재 레벨 시작 총경험치를 뺀 파생값
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression")
	int64 ExperienceIntoLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression")
	FNUPrimaryStats PrimaryStats;

	// 서버가 확정한 남은 성장 포인트
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 AvailablePoints = 0;

	// 현재 레벨까지 서버가 지급한 누적 성장 포인트
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 TotalPoints = 0;

	// 이미 획득한 노드는 다시 적용하지 않음
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	TArray<int32> AcquiredNodeIds;

	// 현재 진행 중인 줄기 
	// 줄기 사이를 이동할 수 있는 상태에서는 0
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 ActiveStemId = 0;

	// 전직 또는 메인 노드까지 도달해 더 이상 진행하지 않는 완주 줄기 ID
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	TArray<int32> CompletedStemIds;

	// 획득 순서와 별개로 실제 선택한 연결선을 서버 상태에 보존한다.
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	TArray<FNUProgressionNodeLinkState> AcquiredNodeLinks;

	// 현재 줄기에서 마지막으로 획득한 노드
	// 줄기 사이에서는 직전 완료 노드를 유지한다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 CurrentNodeId = 0;

	// 최종 노드를 선택하기 전에는 0, 선택한 뒤에는 해당 Final NodeId다.
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	int32 SelectedFinalNodeId = 0;

	// 경험치를 뺀 나머지가 같은지
	// 서버 계약상 Revision 은 성장 상태(직업·노드·포인트)가 바뀔 때만 오르고 경험치만 늘면
	// 안 오른다. 그래서 "같은 리비전인데 경험치만 다른" 응답이 정상이고, 그걸 가려내야 한다
	// 레벨업은 포인트가 늘어 리비전이 같이 오르므로 Level 은 엄격한 쪽에 남겨둔다
	bool EqualsIgnoringExperience(const FNUProgressionState& Other) const
	{
		return GraphVersion == Other.GraphVersion
			&& Revision == Other.Revision
			&& Level == Other.Level
			&& PrimaryStats == Other.PrimaryStats
			&& AvailablePoints == Other.AvailablePoints
			&& TotalPoints == Other.TotalPoints
			&& AcquiredNodeIds == Other.AcquiredNodeIds
			&& ActiveStemId == Other.ActiveStemId
			&& CompletedStemIds == Other.CompletedStemIds
			&& AcquiredNodeLinks == Other.AcquiredNodeLinks
			&& CurrentNodeId == Other.CurrentNodeId
			&& SelectedFinalNodeId == Other.SelectedFinalNodeId;
	}

	bool operator==(const FNUProgressionState& Other) const
	{
		return EqualsIgnoringExperience(Other)
			&& TotalExperience == Other.TotalExperience
			&& ExperienceIntoLevel == Other.ExperienceIntoLevel;
	}

	bool operator!=(const FNUProgressionState& Other) const
	{
		return !(*this == Other);
	}
};
