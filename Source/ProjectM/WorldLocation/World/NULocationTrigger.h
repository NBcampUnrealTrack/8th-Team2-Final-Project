#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NULocationTrigger.generated.h"

class UBoxComponent;

// 장소 ID와 범위만 제공한다. 위치 선택, 퀘스트 진행, HUD는 각 담당이 처리한다.
UCLASS(Blueprintable)
class PROJECTM_API ANULocationTrigger : public AActor
{
	GENERATED_BODY()

public:
	ANULocationTrigger();
	FName GetLocationId() const;
	bool ShouldShowEntryNotice() const;
	int32 GetLocationPriority() const;
	UBoxComponent* GetTriggerBounds() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation")
	TObjectPtr<UBoxComponent> TriggerBounds;

	// LocationTable의 숫자 ID. 기존 퀘스트의 문자열 TargetId도 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation")
	FName LocationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation")
	bool bShowEntryNotice = true;

	// 겹친 범위에서는 높은 값, 같은 값에서는 하위 장소를 우선한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation")
	int32 LocationPriority = 0;
};
