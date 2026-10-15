#pragma once

#include "CoreMinimal.h"
#include "NUExperienceGrowth.generated.h"

class FJsonObject;

// 경험치 지급 응답에 포함되는 서버 확정 레벨, 경험치 변화
USTRUCT(BlueprintType)
struct PROJECTM_API FNUExperienceGrowth
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Experience")
	int32 OldLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Experience")
	int32 NewLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Experience")
	int64 OldTotalExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Experience")
	int64 NewTotalExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Experience")
	int64 ExperienceIntoLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Experience")
	int64 RequiredExperienceToNext = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Experience")
	int32 MaxLevel = 1;

	bool IsLevelUp() const { return NewLevel > OldLevel; }
};

namespace NUExperienceGrowthServer
{
	PROJECTM_API bool Parse(const TSharedPtr<FJsonObject>& Object, FNUExperienceGrowth& OutGrowth, FString& OutError);
}
