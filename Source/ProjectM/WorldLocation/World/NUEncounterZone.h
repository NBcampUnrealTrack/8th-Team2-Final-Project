#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUEncounterZone.generated.h"

class UBoxComponent;

// 월드에 배치하는 조우 범위와 출현 가능한 Enemy 설정
USTRUCT(BlueprintType)
struct PROJECTM_API FNUEncounterEnemyEntry
{
    GENERATED_BODY()

    // DT_EnemyDefinitionRow RowName
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
    FName EnemyId = NAME_None;

    // 랜덤 가중치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter", meta = (ClampMin = "0.0"))
    float Weight = 1.0f;

    bool IsValid() const
    {
        return !EnemyId.IsNone() && Weight > 0.0f;
    }
};

UCLASS()
class PROJECTM_API ANUEncounterZone : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ANUEncounterZone();

protected:
    virtual void BeginPlay() override;

public:
    // 가중치에 따라 Enemy 하나만 선택
    bool SelectWeightedEnemy(FName& OutEnemyId) const;

    // 설정된 수만큼 Enemy를 랜덤 선택
    bool SelectRandomEnemies(TArray<FName>& OutEnemyIds) const;

    // 이 지역에서 전투할 때 쓸 배경 서브레벨
    const TSoftObjectPtr<UWorld>& GetBattleStageLevel() const;

protected:
    // Zone 범위
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
    TObjectPtr<UBoxComponent> ZoneBounds;

    // 출현할 Enemy 목록
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
    TArray<FNUEncounterEnemyEntry> EnemyEntries;

    // 최소 Enemy 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter|Count", meta = (ClampMin = "1", ClampMax = "4"))
    int32 MinEnemyCount = 1;

    // 최대 Enemy 수
    // 최대 4마리까지 스폰 가능
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter|Count", meta = (ClampMin = "1", ClampMax = "4"))
    int32 MaxEnemyCount = 2;

    // 이 지역의 전투 배경 서브레벨, 비어 있으면 배경을 바꾸지 않음
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
    TSoftObjectPtr<UWorld> BattleStageLevel;

};
