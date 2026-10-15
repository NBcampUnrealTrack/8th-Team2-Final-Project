#pragma once

#include "CoreMinimal.h"
#include "UI/Controller/NUUIController.h"
#include "NUStarterNaiaSelectionUIController.generated.h"

class UNUStarterNaiaSelectionWidget;

UCLASS()
class PROJECTM_API UNUStarterNaiaSelectionUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	void OpenSelection();
	void CloseSelection();

protected:
	virtual void OnPlayerControllerChanged(APlayerController* NewPC) override;
	virtual void OnDeinitialize() override;

private:
	void HandleSelectionFinished();

	UPROPERTY(Transient)
	TObjectPtr<UNUStarterNaiaSelectionWidget> SelectionWidget;
};
