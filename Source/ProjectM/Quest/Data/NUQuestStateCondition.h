#pragma once

#include "CoreMinimal.h"
#include "NUQuestStateCondition.generated.h"

class UNUQuestDefinition;
struct FNUQuestProgressState;

// 퀘스트(목표를 지정하면 그 목표)가 어느 상태인지. 연출 표와 월드 상태 표가 같은 뜻으로 쓴다.
UENUM(BlueprintType)
enum class ENUQuestConditionState : uint8
{
	// 수락한 뒤로 계속(목표 지정 시 그 목표가 열린 뒤로 계속)
	Accepted,
	// 수락했고 아직 끝내지 않은 동안(목표 지정 시 그 목표가 열려 있는 동안)
	InProgress,
	// 끝낸 뒤로 계속
	Completed,
	// 선행 조건을 충족했지만 아직 수락하지 않은 퀘스트. 목표에는 사용하지 않는다.
	Available,
	// 수락 가능해진 때부터 진행 중인 동안까지. 의뢰인을 만나러 가는 길의 연출에 사용한다.
	AvailableOrInProgress
};

// 퀘스트 진행 조건 하나. QuestId가 0이면 조건이 없는 것이다.
// 표에서는 한 칸에 (QuestId=10007,ObjectiveId=1000701,State=Completed)처럼 적는다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestStateCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Condition")
	int32 QuestId = 0;

	// 0이면 퀘스트 전체 기준이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Condition")
	int32 ObjectiveId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Condition")
	ENUQuestConditionState State = ENUQuestConditionState::InProgress;

	bool IsSet() const;
	bool Validate(FString& OutError) const;

	// 진행 조회로 판단한다. 진행이 없으면(nullptr) 아직 수락하지 않은 것으로 보고, 실패·포기한 퀘스트는 어느 상태도 아니다.
	bool IsMet(TFunctionRef<const FNUQuestProgressState*(FName QuestId)> FindProgress, TFunctionRef<const UNUQuestDefinition*(FName QuestId)> FindDefinition) const;

	// 가리키는 퀘스트와 목표가 정의에 있는지.
	bool HasTarget(TFunctionRef<const UNUQuestDefinition*(FName QuestId)> FindDefinition) const;

	FString ToString() const;
};
