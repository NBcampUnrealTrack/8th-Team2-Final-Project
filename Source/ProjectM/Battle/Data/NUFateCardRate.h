#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUFateCardRate.generated.h"


// 갬블러의 전직 갈래, 카드 확률표를 갈라 읽음
UENUM(BlueprintType)
enum class ENUFateArcana : uint8
{
	Major,
	Minor
};

// [운명 읽기] 카드 확률 한 줄, LUK 구간마다 한 줄 (FateCardRateTable)
// 구간 사이의 LUK 는 앞뒤 줄을 선형으로 이음 (기획 확정)
// 네 확률의 합은 100 이고 변환기가 검사함
USTRUCT(BlueprintType)
struct PROJECTM_API FNUFateCardRateRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	int32 CardRateId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	ENUFateArcana ArcanaType = ENUFateArcana::Major;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	int32 LUK = 0;

	// 퍼센트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	float Rate_GreatFortune = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	float Rate_Fortune = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	float Rate_Misfortune = 0.f;

	// 마이너 아르카나에만 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	float Rate_GreatMisfortune = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|FateCard")
	FText Description;
};
