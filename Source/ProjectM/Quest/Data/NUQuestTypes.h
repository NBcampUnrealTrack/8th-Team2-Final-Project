#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NUQuestTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNUQuest, Log, All);

// 퀘스트 진행 상태
UENUM(BlueprintType)
enum class ENUQuestState : uint8
{
	Locked			UMETA(DisplayName = "잠김"),
	Available		UMETA(DisplayName = "수락 가능"),
	Active			UMETA(DisplayName = "진행 중"),
	ReadyToTurnIn	UMETA(DisplayName = "임무 완료 및 보고 가능"),
	Completed		UMETA(DisplayName = "퀘스트 완료"),
	Failed			UMETA(DisplayName = "실패"),
	Abandoned		UMETA(DisplayName = "포기"),
};

namespace NUQuest
{
	// 내부 보고/보상 처리 상태와 플레이어에게 보여주는 상태를 분리한다.
	PROJECTM_API FText GetStateDisplayText(ENUQuestState State);
}

// 퀘스트 분류는 NU.Quest.Category 하위 태그로 관리한다. (NUGameplayTags.h)

// 다시 수락할 수 있는 주기
UENUM(BlueprintType)
enum class ENUQuestRepeatPolicy : uint8
{
	Once		UMETA(DisplayName = "1회성"),
	Daily		UMETA(DisplayName = "일일"),
	Weekly		UMETA(DisplayName = "주간"),
	Repeatable	UMETA(DisplayName = "반복 가능"),
};


/**
* 퀘스트 시작 조건
*
* RequirementType 예:
* - NU.Quest.Requirement.PlayerLevel
* - NU.Quest.Requirement.QuestCompleted
*
* 예:
* - PlayerLevel: TargetId 사용 안 함, RequiredValue = 10
* - QuestCompleted: TargetId = Q_MAIN_001
*/
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestRequirementSpec
{
	GENERATED_BODY()

	//어떤 조건을 검사할지 나타내는 타입 태그.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Requirement", meta = (Categories = "NU.Quest.Requirement"))
	FGameplayTag RequirementType;

	// 퀘스트 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Requirement")
	FName TargetId = NAME_None;

	// 목표 완료에 필요한 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Requirement")
	int32 RequiredValue = 1;

	bool IsConfigured() const
	{
		return RequirementType.IsValid();
	}

	// 조건 타입별 필수 값과 현재 지원 여부를 검증한다.
	bool IsApplicable(FString& OutError) const;
};


/**
* 퀘스트 목표
*
* ObjectiveType 예:
* - NU.Quest.Objective.Talk
* - NU.Quest.Objective.Interact
* - NU.Quest.Objective.CollectItem
* - NU.Quest.Objective.EnemyDefeated
* - NU.Quest.Objective.BattleWon
* - NU.Quest.Objective.ReachLocation
*
* TargetId는 ObjectiveType에 따라 가리키는 것이 다르다:
* - Talk / UseObject : 액터의 QuestActorId   (NPC_VILLAGE_CHIEF)
* - CollectItem      : 아이템 테이블 RowName (C_VIT_01)
* - EnemyDefeated    : 적 정의 RowName       (MOB_001)
* - BattleWon        : 전투 대표 적 RowName   (MOB_001)
* - ReachLocation    : 월드 위치 ID           (LOC_VILLAGE_GATE)
*/
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestObjectiveSpec
{
	GENERATED_BODY()

	// 진행도 저장에 사용하는 고정 ID. 새 목표 테이블의 RowName이며 표시 순서와 무관하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	FName ObjectiveId = NAME_None;

	// 퀘스트 설명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective", meta = (Categories = "NU.Quest.Objective"))
	FGameplayTag ObjectiveType;

	// 검사할 대상. ObjectiveType에 따라 액터의 QuestActorId / 아이템 RowName / 적 RowName 중 하나.
	// 비우면 해당 타입의 모든 이벤트와 일치한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	FName TargetId = NAME_None;

	// 목표 완료에 필요한 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category = "NU|Quest|Objective", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredCount = 1;

	// 완료에 필수가 아닌 선택 목표 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	bool bOptional = false;

	// 부탁을 받는 대화처럼 다음 목표의 준비 단계. 끝나기 전까지만 목표 목록에 보이고("○○의 심부름"), 끝나면 목록에서 빠진다.
	// 이 목표가 끝나기 전에는 이 목표를 선행으로 둔(직간접) 목표 줄을 목록에 공개하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	bool bHideInObjectiveList = false;

	// 이 목표를 진행하려면 먼저 완료돼야 하는 목표.
	// 비워 두면 순서 제약이 없다. 같은 퀘스트의 목표를 지정하며 표시 순서와 무관하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	FName PrerequisiteObjectiveId = NAME_None;

	bool ShouldDisplayCount() const;
	FText GetProgressText(int32 CurrentCount) const;

	bool IsConfigured() const
	{
		return !ObjectiveId.IsNone() && ObjectiveType.IsValid() && RequiredCount > 0;
	}
};

/**
* 퀘스트 완료 보상
*
* RewardType 예:
* - NU.Quest.Reward.Experience
* - NU.Quest.Reward.SetFlag
*
* TargetId 예:
* - Experience: 사용하지 않음, Amount = 50
* - SetFlag: QUEST_VILLAGE_INTRO_COMPLETED
*/
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestRewardSpec
{
	GENERATED_BODY()

	//어떤 보상을 적용할지 나타내는 타입 태그.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward", meta = (Categories = "NU.Quest.Reward"))
	FGameplayTag RewardType;

	// 보상 대상
	// 플래그 ID, 아이템 ID 등 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward")
	FName TargetId = NAME_None;

	// 보상 수량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward")
	int32 Amount = 1;

	// 플레이어에게 보여줄 이름. 플래그/해금의 내부 ID는 UI에 출력하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward")
	FText DisplayName;

	FText GetDisplayText() const;

	bool IsConfigured() const
	{
		return RewardType.IsValid();
	}

	// 보상 타입별 필수 값까지 검사
	bool IsApplicable(FString& OutError) const;
};


// 상호작용 시점에 이 액터가 플레이어에게 갖는 의미
// 대사 선택, 퀘스트 처리, 머리 위 마커가 모두 이 값을 기준 처리
UENUM(BlueprintType)
enum class ENUQuestInteractionPhase : uint8
{
	Idle		UMETA(DisplayName = "처리할 퀘스트 없음"),
	Offer		UMETA(DisplayName = "수락 가능"),
	InProgress	UMETA(DisplayName = "진행 중"),
	TurnIn		UMETA(DisplayName = "보고 가능"),
	Completed	UMETA(DisplayName = "완료"),
	Locked		UMETA(DisplayName = "선행 조건 미달"),
	Target		UMETA(DisplayName = "진행 중 퀘스트의 목표"),
};

// 이 액터가 특정 퀘스트에 대해 지금 할 수 있는 일
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestInteractionOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Interaction")
	FName QuestId = NAME_None;

	// 대화를 시작한 목표를 보존해 같은 NPC의 다음 목표에 이전 대사가 적용되지 않게 한다.
	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Interaction")
	FName ObjectiveId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Interaction")
	ENUQuestInteractionPhase Phase = ENUQuestInteractionPhase::Idle;
};
