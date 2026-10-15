#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "NUStarterNaiaSelectionEntryButton.generated.h"

DECLARE_DELEGATE_OneParam(FOnNUStarterNaiaSelectionEntryClicked, int32)

UCLASS()
class PROJECTM_API UNUStarterNaiaSelectionEntryButton : public UButton
{
	GENERATED_BODY()

public:
	UNUStarterNaiaSelectionEntryButton();
	void SetNaiaId(int32 InNaiaId) { NaiaId = InNaiaId; }

	FOnNUStarterNaiaSelectionEntryClicked OnEntryClicked;

private:
	UFUNCTION()
	void HandleClicked();

	int32 NaiaId = 0;
};
