#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUWorldDataSettings.generated.h"

class UDataTable;
class UWorld;

// 지역과 장소 테이블, 맵별 기본 지역을 지정하는 프로젝트 설정
// 맵 경로로 지역을 조회하며 레벨 로딩은 담당하지 않는다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU World Data"))
class PROJECTM_API UNUWorldDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	// 월드 기본 맵의 지역 조회
	int32 GetAreaIdForWorld(const UWorld* World) const;

	// 맵 경로로 지역 조회. 미등록 시 0 반환
	int32 GetAreaIdForMap(const FString& PackageName) const;

	// 한 맵의 기본 지역
	UPROPERTY(Config, EditAnywhere, Category = "World Data", meta = (ClampMin = "1"))
	TMap<TSoftObjectPtr<UWorld>, int32> MapAreaIds;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "World Data")
	TSoftObjectPtr<UDataTable> AreaDefinitionTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "World Data")
	TSoftObjectPtr<UDataTable> LocationDefinitionTable;
};
