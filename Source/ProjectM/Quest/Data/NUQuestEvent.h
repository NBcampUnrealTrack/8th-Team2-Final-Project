#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NUQuestEvent.generated.h"

// 퀘스트 목표에 전달되는 공통 이벤트
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestEvent
{
	GENERATED_BODY()

	// 목표 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Quest|Event",meta = (Categories = "NU.Quest.Objective"))
	FGameplayTag ObjectiveType;

	// NPC, 아이템, 오브젝트 등의 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Quest|Event")
	FName TargetId = NAME_None;

	// 증가시킬 진행량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Quest|Event",meta = (ClampMin = "1", UIMin = "1"))
	int32 Amount = 1;

	// 같은 TargetId를 쓰는 월드 액터 여러 개를 각각 한 번만 집계할 때 사용하는 런타임 식별자
	// 비워 두면 기존처럼 횟수 이벤트로 처리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Quest|Event")
	FName SourceInstanceId = NAME_None;

	bool IsValid() const
	{
		return ObjectiveType.IsValid() && Amount > 0;
	}
};
