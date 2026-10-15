#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "NUJobSelectionEntryButton.generated.h"

DECLARE_DELEGATE_OneParam(FOnNUJobSelectionEntryClicked, int32)

UCLASS()
class PROJECTM_API UNUJobSelectionEntryButton : public UButton
{
	GENERATED_BODY()

public:
	UNUJobSelectionEntryButton();
	void SetJobId(int32 InJobId) { JobId = InJobId; }

	FOnNUJobSelectionEntryClicked OnEntryClicked;

private:
	UFUNCTION()
	void HandleClicked();

	int32 JobId = 0;
};
