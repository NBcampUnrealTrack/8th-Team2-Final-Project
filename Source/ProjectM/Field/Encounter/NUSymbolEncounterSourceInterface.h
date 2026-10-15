#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "UObject/Interface.h"
#include "NUSymbolEncounterSourceInterface.generated.h"

UINTERFACE(BlueprintType)
class PROJECTM_API UNUSymbolEncounterSourceInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTM_API INUSymbolEncounterSourceInterface
{
	GENERATED_BODY()

public:
	// 심볼이 발생시킨 전투가 시작될 때 호출
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NU|Field|Encounter")
	void HandleSymbolEncounterStarted(AActor* Initiator);

	// 심볼과의 전투가 종료될 때 호출
	// 심볼 재등장, 제거, 재도전 등 처리
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NU|Field|Encounter")
	void HandleSymbolEncounterCompleted(ENUBattleResult Result);
};
