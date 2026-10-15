#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUProgressionDataSettings.generated.h"

class UDataTable;

// Project Settings에서 성장에 사용하는 DataTable 경로를 관리한다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Progression Data"))
class PROJECTM_API UNUProgressionDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Progression Data")
	TSoftObjectPtr<UDataTable> LevelProgressionTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Progression Data")
	TSoftObjectPtr<UDataTable> StatProgressionTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Progression Data")
	TSoftObjectPtr<UDataTable> ProgressionNodeTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Progression Data")
	TSoftObjectPtr<UDataTable> ProgressionNodeRewardTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Job Data")
	TSoftObjectPtr<UDataTable> JobDefinitionTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Job Data")
	TSoftObjectPtr<UDataTable> JobAdvancementTable;

	// 최초 직업 선택 기능을 제공할 NPC의 QuestActorId
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Job Data")
	FName InitialJobSelectionNpcId = NAME_None;

	// 이 퀘스트를 모두 완료했으면 전직 목표가 열려 있지 않아도 최초 전직을 허용한다. 시험용 성장 퀘스트로 바로 전직할 때 쓴다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Job Data")
	TArray<FName> InitialJobSelectionUnlockQuestIds;

	// 기존 에셋 호환용. 최초 전직은 레벨 대신 활성 전직 목표로 판정한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Job Data", meta = (DeprecatedProperty, DeprecationMessage = "Use the active JobSelected quest objective."))
	int32 InitialJobSelectionRequiredLevel = 10;
};
