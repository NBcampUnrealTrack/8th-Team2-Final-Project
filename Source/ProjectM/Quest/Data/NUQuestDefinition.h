#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestDefinition.generated.h"

class UNUQuestDialogueDefinition;

// 퀘스트 정의
// 퀘스트 규칙, 표시 데이터 등
UCLASS(BlueprintType)
class PROJECTM_API UNUQuestDefinition : public UObject
{
	GENERATED_BODY()
	
public:
	// 퀘스트 검증
	bool ValidateDefinition(FString& OutError) const;

	// 보고 NPC와의 대화로 끝나는 목표. 이 목표를 채우는 대화에서 바로 보상을 받으므로 "돌아가 보고하기" 단계가 없다.
	// 퀘스트를 끝내는 필수 목표(다른 필수 목표의 선행이 아닌 것)가 이것 하나뿐일 때만 돌려준다.
	const FNUQuestObjectiveSpec* FindTurnInTalkObjective() const;

	// Quest ID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Identity")
	FName QuestId = NAME_None;

	// 서버/DB에서 사용하는 숫자 ID. DataTable을 쓰면 Quest_ID(RowName)로 덮어쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Identity", meta = (ClampMin = "0"))
	int64 QuestNumericId = 0;

	// 여러 퀫트를 하나의 그룹으로 묶는 ID
	// 그룹 필요하지 않으면 None
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Identity")
	FName QuestGroupId = NAME_None;

	// 퀘스트 분류
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Identity", meta = (Categories = "NU.Quest.Category"))
	FGameplayTag CategoryTag;

	// 이 퀘스트를 제안하는 액터의 QuestActorId. NPC와 오브젝트 모두 가능
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Identity")
	FName OfferActorId = NAME_None;

	// 완료 보고를 받는 액터의 QuestActorId. 비우면 목표 달성 즉시 완료 가능 상태로만 둔다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Identity")
	FName TurnInActorId = NAME_None;

	// 클라이언트 위치/도달 판정용 ID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Identity")
	FName LocationId = NAME_None;

	// 퀘스트 제목
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Display")
	FText Title;

	// 퀘스트 요약
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Display")
	FText Summary;

	// 퀘스트 설명
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Display", meta = (MultiLine = "true"))
	FText Description;

	// 권장 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Display", meta = (ClampMin = "1", UIMin = "1"))
	int32 RecommendedLevel = 1;

	// 퀘스트 우선 순위
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Display")
	int32 SortOrder = 0;

	// 단계별 대사를 묶은 클라이언트 콘텐츠. CSV의 QuestDialogue 열에서 연결한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Display")
	TSoftObjectPtr<UNUQuestDialogueDefinition> QuestDialogue;

	// 퀘스트 활성화 필요 조건
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Rules")
	TArray<FNUQuestRequirementSpec> StartRequirements;

	// 퀘스트 진행 목표 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Rules")
	TArray<FNUQuestObjectiveSpec> Objectives;

	// 퀘스트 보상 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Rules")
	TArray<FNUQuestRewardSpec> Rewards;

	// 퀘스트 반복 가능 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Rules")
	ENUQuestRepeatPolicy RepeatPolicy = ENUQuestRepeatPolicy::Once;

	// 퀘스트 조건 충족 시, 퀘스트 자동 수락 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Rules")
	bool bAutoAccept = false;

	// 퀘스트 완료 시, 퀘스트 자동 완수 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|Rules")
	bool bAutoComplete = false;
};
