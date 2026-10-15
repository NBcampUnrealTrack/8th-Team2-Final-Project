#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "NUGameInstance.generated.h"

/**
 * 레벨 전환을 넘어 유지되어야 하는 상태.
 *
 * 전투 중 체력/마나는 ANUPlayerState의 ASC가 그대로 들고 있고(아바타만 교체됨),
 * 아이템은 UNUInventoryComponent가 관리하므로 여기서 다루지 않는다.
 */
UCLASS(BlueprintType)
class PROJECTM_API UNUGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// 격파해서 필드에서 사라진 심볼 몬스터 (액터 FName)
	UPROPERTY(BlueprintReadWrite, Category = "NU|Encounter")
	TArray<FName> DefeatedFieldEnemies;

	UFUNCTION(BlueprintCallable, Category = "NU|Encounter")
	bool IsFieldEnemyDefeated(FName EnemyGuid) const { return DefeatedFieldEnemies.Contains(EnemyGuid); }

	UFUNCTION(BlueprintCallable, Category = "NU|Encounter")
	void MarkFieldEnemyDefeated(FName EnemyGuid);
};
