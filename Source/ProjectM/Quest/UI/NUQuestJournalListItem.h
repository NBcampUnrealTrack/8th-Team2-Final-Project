#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestJournalListItem.generated.h"

/**
 * ListView에 표시할 퀘스트 한 건의 화면 데이터.
 * 퀘스트 원본과 진행 상태를 직접 소유하지 않고, 목록을 갱신할 때 필요한 값만 복사한다.
 */
UCLASS(BlueprintType)
class PROJECTM_API UNUQuestJournalListItem : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(FName InQuestId, const FText& InTitle, FGameplayTag InCategoryTag, ENUQuestState InQuestState, int32 InSortOrder);

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Journal")
	FName QuestId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Journal")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Journal")
	FText StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Journal")
	FGameplayTag CategoryTag;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Journal")
	ENUQuestState QuestState = ENUQuestState::Locked;

	// Required objectives only: completed count / total required (0 / 0 when the quest has none).
	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Journal")
	int32 ProgressCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Journal")
	int32 ProgressRequired = 0;

	int32 SortOrder = 0;
};
