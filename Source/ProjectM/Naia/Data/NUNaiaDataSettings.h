#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "NUNaiaDataSettings.generated.h"

class UNUEnemyPatternRegistry;
class ANUSpiritState;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Naia Data"))
class PROJECTM_API UNUNaiaDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
    virtual FName GetCategoryName() const override
    {
        return TEXT("Game");
    }

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Naia Data")
    TSoftObjectPtr<UDataTable> NaiaDefinitionTable;

	// 모든 PlayerState 로스터가 공통으로 생성하는 런타임 나이아 상태 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Naia Runtime")
	TSoftClassPtr<ANUSpiritState> NaiaStateClass;

	// Q9 계약으로 처음 지급되는 나이아 레벨, 야생 개체 SpawnLevel과 무관함
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Starter Contract", meta = (ClampMin = 1))
	int32 StarterContractLevel = 10;

    // NaiaId -> 적 행동 패턴 매핑
    // 등록되지 않은 몬스터는 기본 공격만 하므로 비워둬도 동작함
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Naia Data")
    TSoftObjectPtr<UNUEnemyPatternRegistry> EnemyPatternRegistry;
};
