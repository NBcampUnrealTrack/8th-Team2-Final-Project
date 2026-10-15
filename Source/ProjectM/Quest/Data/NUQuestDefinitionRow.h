#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestDefinitionRow.generated.h"

class UNUQuestDefinition;
class UNUQuestDialogueDefinition;

UENUM(BlueprintType)
enum class ENUQuestClaimType : uint8
{
	Automatic,
	NpcTurnIn,
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	// 정의

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Identity")
	FName Quest_Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Display")
	FText Quest_KR;

	// 분류

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Classification")
	FGameplayTag QuestCategory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Classification")
	FGameplayTag QuestType;

	// 오픈 조건

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Unlock", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Unlock", meta = (ClampMin = "0"))
	int32 PrereqQuest_ID = 0;

	// 기존 단일 선행 ID를 유지하면서 추가 조건을 모두 요구한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Unlock")
	TArray<int32> AdditionalPrereqQuest_IDs;

	// 수락과 보상 수령 방식은 별개다. 기존 행은 수동 수락을 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Unlock")
	bool bAutoAccept = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Unlock")
	ENUQuestRepeatPolicy RepeatType = ENUQuestRepeatPolicy::Once;

	// 의뢰 위치

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Giver")
	int32 QuestGiver_ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Location")
	int32 Location_ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Location")
	FText Region_KR;

	// 목표

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	int32 Target_ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective", meta = (ClampMin = "1", UIMin = "1"))
	int32 TargetCount = 1;

	// 신규 퀘스트는 별도 목표 테이블을 사용한다. false는 기존 단일 목표/디버그 행 호환용이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	bool bUseObjectiveTable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Giver")
	int32 TurnInNPC_ID = 0;

	// 보상

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward", meta = (ClampMin = "0"))
	int32 Reward_EXP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward", meta = (ClampMin = "0"))
	int32 Reward_Gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward", meta = (ClampMin = "0"))
	int32 RewardItem1_ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward", meta = (ClampMin = "0"))
	int32 RewardItem1Count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward", meta = (ClampMin = "0"))
	int32 RewardItem2_ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward", meta = (ClampMin = "0"))
	int32 RewardItem2Count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Reward")
	ENUQuestClaimType ClaimType = ENUQuestClaimType::NpcTurnIn;

	// 설명

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Display")
	FName Desc_Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Display")
	FText Desc_KR;

	// 퀘스트 하나의 Offer/InProgress/TurnIn/Completed/Target 대사 묶음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Display")
	TSoftObjectPtr<UNUQuestDialogueDefinition> QuestDialogue;

	bool ValidateRow(FName RowName, FString& OutError) const;
	bool ApplyTo(UNUQuestDefinition& OutDefinition, FName RowName, FString& OutError) const;

	virtual void OnPostDataImport(
		const UDataTable* InDataTable,
		FName InRowName,
		TArray<FString>& OutCollectedImportProblems) override;
};
