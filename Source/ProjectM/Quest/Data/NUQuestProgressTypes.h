#pragma once

#include "CoreMinimal.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestProgressTypes.generated.h"

class UNUQuestDefinition;

/**
* 퀘스트 목표 진행 상태
* 
* 예: 퀘스트 약초 수집
* 
* 목표1
* ObjectiveId = TalkToChief
* CurrentCount = 1
* 
* 목표 2:
* ObjectiveId = CollectHerb
* RequiredCount = 3
*/
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestObjectiveProgress
{
	GENERATED_BODY()

	// QuestDefinition의 ObjectiveId와 대응
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Progress")
	FName ObjectiveId = NAME_None;

	// 현재 진행 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Progress", meta = (ClampMin = "0"))
	int32 CurrentCount = 0;

	// 같은 TargetId를 가진 월드 액터 중 이미 처리한 인스턴스
	// SourceInstanceId가 없는 일반 처치, 수집 이벤트에는 사용하지 않는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Progress")
	TArray<FName> ProcessedSourceInstanceIds;

	bool IsComplete(int32 RequiredCount) const;
};

/**
* 플레이어가 가진 퀘스트 하나의 전체 진행 정보
*
* 예: 퀘스트 약초 수집
* 
* QuestId: Q_MAIN_001
* State: Active
* 
* 목표 진행:
* - TalkToChief: 1/1
* - CollectHerb: 2/3
* 
* 완료 횟수: 0
* 
*/
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestProgressState
{
	GENERATED_BODY()

	// 퀘스트 ID
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Progress")
	FName QuestId = NAME_None;

	// 현재 퀘스트 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Progress")
	ENUQuestState QuestState = ENUQuestState::Locked;

	// 목표별 진행 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Progress")
	TArray<FNUQuestObjectiveProgress> ObjectiveProgress;

	// 지금까지 완료한 횟수
	// 반복 퀘스트에서는 2 이상이 될 수 있음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Progress", meta = (ClampMin = "0"))
	int32 CompletionCount = 0;

	// 처음 상태를 생성
	void Initialize(const UNUQuestDefinition& Definition, ENUQuestState InitialState = ENUQuestState::Locked);

	// 퀘스트 정의에 맞춰 목표 진행도를 0으로 초기화
	// 반복 퀘스트 다시 수락할 때 사용
	void ResetObjectiveProgress(const UNUQuestDefinition& Definition);

	// 목표 ID를 통해 진행 정보 찾기
	FNUQuestObjectiveProgress* FindObjectiveProgress(FName ObjectiveId);
	const FNUQuestObjectiveProgress* FindObjectiveProgress(FName ObjectiveId) const;	// 읽기용

	// 선택 목표를 제외한 모든 필수 목표가 완료됐는지 확인
	bool IsRequiredObjectivesComplete(const UNUQuestDefinition& Definition) const;

	bool IsObjectivePrerequisiteMet(const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective) const;

	// 목표 목록에 줄이 보일지 판단한다. 준비 단계(bHideInObjectiveList) 줄은 끝나기 전까지만 보이고,
	// 일반 목표 줄은 앞선 준비 단계가 모두 끝나야 보인다. 진행 판정에는 쓰지 않는다.
	bool IsObjectiveListed(const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective) const;

	// 목표 목록에 진행 가능한 줄로 보일지 판단한다. 목록에 보이는 줄이면서 선행 목표가 끝났으면 true다.
	bool IsObjectiveOpenForDisplay(const UNUQuestDefinition& Definition, const FNUQuestObjectiveSpec& Objective) const;

	const FNUQuestObjectiveSpec* FindNextObjective(const UNUQuestDefinition& Definition) const;

	// 수락 안내와 일지에 전체 목표 및 대기·진행·완료 상태를 표시한다.
	// bShowStatus가 false면 [진행 가능] 같은 상태 배지 없이 목표 설명만 나열한다.
	FText GetObjectiveDisplayText(const UNUQuestDefinition& Definition, bool bShowStatus = true) const;

	// 진행 중인 퀘스트인지
	bool IsInProgress() const;
};
