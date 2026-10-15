#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Recovery/Data/NURecoveryTypes.h"
#include "NURecoverySettings.generated.h"

class UDataTable;
struct FNURecoveryCostRow;

// Project Settings에서 회복 비용 DataTable 경로를 관리하고 레벨별 행을 조회한다.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "NU Recovery"))
class PROJECTM_API UNURecoverySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 로컬 실행의 회복 시간으로, 온라인 회복 시간은 서버 응답을 사용한다.
	// HP 0%에서 100%까지의 시간. 실제 적용 주기는 1초
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Rest", meta = (ClampMin = "1"))
	float FullRecoveryDurationSeconds = 3600.0f;

	// 즉시 회복과 사망 비용이 같은 유료 전환 조건을 사용한다.
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Unlock", meta = (ClampMin = "1"))
	int32 PaidRecoveryRequiredLevel = 10;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Presentation", meta = (ClampMin = "0.1"))
	float WakeUpSeconds = 0.25f;

	// 거점 로딩이 끝나지 않을 때 재시도를 종료하는 제한 시간
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Travel", meta = (ClampMin = "1"))
	float DestinationLoadTimeoutSeconds = 15.0f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	bool TryGetRecoveryCostRow(int32 Level, FNURecoveryCostRow& OutRow) const;
	bool ValidateRecoveryCostTable(FString& OutError) const;
	bool TryGetRecoveryPoint(int32 LocationId, FNURecoveryPointDefinition& OutPoint) const;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Points")
	TArray<FNURecoveryPointDefinition> RecoveryPoints;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Death")
	TArray<FName> DeathPenaltyRequiredQuestIds;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Data")
	TSoftObjectPtr<UDataTable> RecoveryCostTable;
};
