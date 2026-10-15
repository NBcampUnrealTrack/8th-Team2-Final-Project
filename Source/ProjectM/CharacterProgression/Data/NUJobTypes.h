#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUJobTypes.generated.h"

class UTexture2D;

// 서버가 확정한 플레이어의 현재 직업 상태
USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobState
{
	GENERATED_BODY()

	// 기본 직업 ID. 로그인 상태가 오기 전에는 0이다.
	UPROPERTY(BlueprintReadOnly, Category = "NU|Job")
	int32 JobId = 0;

	// 현재 전직 ID. 미전직 상태에서는 0이다.
	UPROPERTY(BlueprintReadOnly, Category = "NU|Job")
	int32 AdvancementId = 0;

	bool operator==(const FNUJobState& Other) const
	{
		return JobId == Other.JobId
			&& AdvancementId == Other.AdvancementId;
	}

	bool operator!=(const FNUJobState& Other) const
	{
		return !(*this == Other);
	}
};

// 직업 선택 화면과 성장 그래프 진입에 필요한 기본 직업 정보.
// 2차 전직과 노드 보상은 별도 테이블에서 관리한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job")
	int32 JobId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job")
	FName JobKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job")
	int32 GraphId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|Display")
	TSoftObjectPtr<UTexture2D> Icon;
};

// 2차 이후 전직 한 갈래를 정의한다.
// ParentAdvancementId로 다음 전직 단계를 이어간다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobAdvancementDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement")
	int32 AdvancementId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement")
	int32 ParentJobId = 0;

	// 2차 전직은 0
	// 나중에 3차 등장 이후는 바로 이전 전직의 AdvancementId를 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement")
	int32 ParentAdvancementId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement", meta = (ClampMin = "2"))
	int32 AdvancementTier = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement")
	FName AdvancementKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement|Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement|Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Advancement|Display")
	TSoftObjectPtr<UTexture2D> Icon;
};
