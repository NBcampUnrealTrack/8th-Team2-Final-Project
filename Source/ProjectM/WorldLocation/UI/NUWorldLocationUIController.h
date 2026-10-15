#pragma once

#include "CoreMinimal.h"
#include "UI/Controller/NUUIController.h"
#include "NUWorldLocationUIController.generated.h"

class APawn;
class UNUWorldLocationComponent;
class UNUWorldLocationWidget;

// 위치 변경을 구독해 표시한다. 장소 판정과 퀘스트 진행은 담당하지 않는다.
UCLASS()
class PROJECTM_API UNUWorldLocationUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	virtual void OnPlayerControllerChanged(APlayerController* NewPC) override;
	void RefreshPlayerStateBinding();

protected:
	virtual void OnInitialize() override;
	virtual void OnDeinitialize() override;

private:
	void BindPlayerController(APlayerController* PlayerController);
	void ApplyHUDVisibility();

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleLocationChanged();

	TWeakObjectPtr<APlayerController> BoundPlayerController;
	TWeakObjectPtr<UNUWorldLocationComponent> LocationComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNUWorldLocationWidget> LocationWidget;

	int32 LastNoticeLocationId = 0;
};
