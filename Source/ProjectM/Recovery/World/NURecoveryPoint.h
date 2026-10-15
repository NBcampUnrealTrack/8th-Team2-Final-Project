#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NURecoveryPoint.generated.h"

class UCapsuleComponent;

// RespawnLocation 캡슐을 드래그하여 부활 위치를 편집한다.
UCLASS(Blueprintable)
class PROJECTM_API ANURecoveryPoint : public AActor
{
	GENERATED_BODY()

public:
	ANURecoveryPoint();

	// 에디터에서 옮긴 캡슐의 월드 Transform을 반환한다.
	UFUNCTION(BlueprintPure, Category = "NU|Recovery")
	FTransform GetRecoveryTransform() const;

	// RecoverySettings의 거점 ID와 일치해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery")
	int32 LocationId = 0;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Recovery")
	TObjectPtr<UCapsuleComponent> RespawnLocation;
};
