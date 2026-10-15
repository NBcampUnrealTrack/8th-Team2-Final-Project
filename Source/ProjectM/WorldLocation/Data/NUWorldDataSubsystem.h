#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldLocation/Data/NUAreaDefinition.h"
#include "WorldLocation/Data/NULocationDefinition.h"
#include "NUWorldDataSubsystem.generated.h"

class UDataTable;

// 지역과 장소의 정적 정의만 관리한다. 현재 위치와 플레이어별 해금 상태는 소유하지 않는다.
UCLASS()
class PROJECTM_API UNUWorldDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 반환된 행 포인터는 호출 범위 안에서만 사용한다. 재임포트 이후에는 다시 조회한다.
	const FNUAreaDefinition* FindAreaDefinition(int32 AreaId) const;
	const FNULocationDefinition* FindLocationDefinition(int32 LocationId) const;

	UFUNCTION(BlueprintPure, Category = "NU|WorldLocation|Data")
	bool TryGetAreaDefinition(int32 AreaId, FNUAreaDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category = "NU|WorldLocation|Data")
	bool TryGetLocationDefinition(int32 LocationId, FNULocationDefinition& OutDefinition) const;

	// ID 오름차순으로 반환한다. 없는 지역은 빈 배열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "NU|WorldLocation|Data")
	void GetLocationIdsInArea(int32 AreaId, TArray<int32>& OutLocationIds) const;

	UFUNCTION(BlueprintPure, Category = "NU|WorldLocation|Data")
	bool IsDataReady() const;

	// 설정된 두 테이블을 다시 읽는다. 검증 실패 시 이전 인덱스도 폐기한다.
	bool ReloadData();

	// 두 테이블이 모두 준비된 뒤 행과 교차 참조를 검사한다. Quest/NPC/상점은 로드하지 않는다.
	static bool ValidateTables(const UDataTable* AreaTable, const UDataTable* LocationTable, TArray<FString>& OutErrors);

private:
	void ResetData();
	void BuildDefinitionIndices();
	void HandleDefinitionTableChanged();

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> AreaDefinitionTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LocationDefinitionTable = nullptr;

	TMap<int32, FName> AreaIdToRowName;
	TMap<int32, FName> LocationIdToRowName;
	TMap<int32, TArray<int32>> AreaIdToLocationIds;
	bool bDataReady = false;
};
