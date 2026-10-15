#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUNPCDataSettings.generated.h"

class UDataTable;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU NPC Data"))
class PROJECTM_API UNUNPCDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "NPC Data")
	TSoftObjectPtr<UDataTable> NPCDefinitionTable;
};
