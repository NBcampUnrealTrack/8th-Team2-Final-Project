#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUDialogueDataSettings.generated.h"

class UDataTable;

// 현재는 NPCDialogue 한 테이블을 사용한다. 지역 확장 시에도 동일한 행 구조로 등록한다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Dialogue Data"))
class PROJECTM_API UNUDialogueDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category = "NU|Dialogue", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectM.NUDialogueRow"))
	TArray<TSoftObjectPtr<UDataTable>> DialogueTables;
};
