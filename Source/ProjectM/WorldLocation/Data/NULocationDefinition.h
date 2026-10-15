#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NULocationDefinition.generated.h"

UENUM(BlueprintType)
enum class ENULocationType : uint8
{
	Unknown,
	Zone,
	Terrain,
	Building,
	Facility,
	Shop,
	Landmark
};

UENUM(BlueprintType)
enum class ENULocationServiceType : uint8
{
	None,
	Lodging,
	Shop,
	Quest,
	Heal,
	Craft,
	JobChange,
	Travel,
	Rest
};

// 장소 정적 정의. 실제 좌표, 방문 기록, 서비스 실행은 포함하지 않는다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNULocationDefinition : public FTableRowBase
{
	GENERATED_BODY()

	// 숫자 RowName과 동일한 장소 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	int32 LocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	FName LocationKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	FString EnglishName;

	// 이 장소가 속한 AreaTable의 지역 ID다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Reference")
	int32 AreaId = 0;

	// 기획 표시용 구역명. 소속 지역 판정에는 AreaId를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	FText ZoneName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Classification")
	ENULocationType LocationType = ENULocationType::Unknown;

	// 서비스 분류만 제공한다. 실제 선택지와 실행은 Interaction Provider가 담당한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Classification")
	ENULocationServiceType ServiceType = ENULocationServiceType::None;

	// 완료해야 하는 퀘스트 ID. 0이면 장소 자체의 해금 조건은 없다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Reference")
	int32 UnlockQuestId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Rules")
	bool bIndoor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Rules")
	bool bRestPoint = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Development")
	FString Description;

	bool ValidateRow(FName RowName, FString& OutError) const;
};
