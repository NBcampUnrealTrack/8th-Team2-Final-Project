#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Net/NUExperienceGrowth.h"
#include "NUQuestServerTypes.generated.h"

struct PROJECTM_API FNUQuestServerObjective
{
	int32 ObjectiveId = 0;
	int32 CurrentCount = 0;
};

struct PROJECTM_API FNUQuestServerProgress
{
	int32 QuestId = 0;
	int32 State = 0;
	int32 CompletionCount = 0;
	int64 Revision = 0;
	bool bCanAccept = false;
	TArray<FNUQuestServerObjective> Objectives;
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestServerReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Reward")
	int64 Experience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Reward")
	int64 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Reward")
	bool bHasGrowth = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Reward")
	FNUExperienceGrowth Growth;
};

namespace NUQuestServer
{
	PROJECTM_API bool TryConvertQuestId(int64 QuestId, int32& OutQuestId);

	PROJECTM_API bool ParseProgress(
		const FString& ResponseData,
		FNUQuestServerProgress& OutProgress,
		FString& OutError);

	PROJECTM_API bool ParseList(
		const FString& ResponseData,
		TArray<FNUQuestServerProgress>& OutProgressList,
		FString& OutError);

	PROJECTM_API bool ParseComplete(
		const FString& ResponseData,
		FNUQuestServerProgress& OutProgress,
		FNUQuestServerReward& OutReward,
		FString& OutError);
}
