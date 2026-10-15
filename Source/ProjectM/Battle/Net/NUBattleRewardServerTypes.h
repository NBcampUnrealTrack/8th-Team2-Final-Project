// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Net/NUExperienceGrowth.h"
#include "NUBattleRewardServerTypes.generated.h"

// 승리 보상으로 받은 아이템 한 종류
// 드롭 규칙이 아직 없어 서버가 항상 빈 배열을 보냄, 실제로 채워지면 모양부터 확인해야 함
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerBattleRewardItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Reward")
	int32 ItemId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Reward")
	int32 Count = 0;
};

// SVR_Battle_End 의 response_data 로 오는 승리 보상
// 퀘스트 완료 보상과 같은 모양이라 성장 부분은 같은 파서(NUExperienceGrowthServer)를 나눠 씀
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerBattleReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Reward")
	int64 Experience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Reward")
	int64 Gold = 0;

	// Growth 는 null 로 올 수 있다 - 줄 게 없거나 지급에 실패한 경우
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Reward")
	bool bHasGrowth = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Reward")
	FNUExperienceGrowth Growth;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Reward")
	TArray<FNUServerBattleRewardItem> Items;
};

namespace NUBattleRewardServer
{
	// 승리가 아니면 response_data 가 비어 오므로 그때는 false 를 돌려줌(오류가 아님)
	PROJECTM_API bool Parse(
		const FString& ResponseData,
		FNUServerBattleReward& OutReward,
		FString& OutError);
}
