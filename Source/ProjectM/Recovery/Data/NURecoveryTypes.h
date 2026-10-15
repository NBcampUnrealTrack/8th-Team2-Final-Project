#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NURecoveryTypes.generated.h"

// 레벨별 즉시 회복 비용과 사망 시 부활 손실 골드를 정의한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNURecoveryCostRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery", meta = (ClampMin = "0"))
	int32 RecoveryCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery", meta = (ClampMin = "0"))
	int32 ResurrectionGoldLoss = 0;
};

class UWorld;

// 맵과 거점 ID를 연결
// 부활 위치는 해당 맵의 NURecoveryPoint 액터로 편집한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNURecoveryPointDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	int32 LocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TSoftObjectPtr<UWorld> Map;

	// 다른 맵을 현재 UWorld에 스트리밍할 때 사용할 배치 Transform.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	FTransform LevelTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0"))
	float RegistrationRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TArray<FName> RequiredQuestIds;

	// 이 퀘스트를 완료한 뒤 최초 한 번만 등록한다. 이후 수동으로 등록한 거점을 덮지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	FName AutoRegisterQuestId;

	// UE 로컬 실행에서 현재 맵의 첫 기본 거점으로 사용한다.
	// RequiredQuestIds를 모두 완료하고 등록 거점이 없을 때만 적용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	bool bRegisterOnStart = false;
};
