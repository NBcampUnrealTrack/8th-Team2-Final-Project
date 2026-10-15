#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectMPaperZDMigrationLibrary.generated.h"

/** Deterministic editor-only rebuild of ProjectM's generated PaperZD assets. */
UCLASS()
class PROJECTMEDITOR_API UProjectMPaperZDMigrationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns a concise success report or a list of validation errors. */
	UFUNCTION(BlueprintCallable, Category = "ProjectM|PaperZD")
	static FString RebuildPaperZDAssets();
};
