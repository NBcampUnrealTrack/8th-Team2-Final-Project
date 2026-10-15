#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NUGE_PlayerSecondaryStatScaling.generated.h"

// Primary Stat 변경을 Secondary Stat에 자동 반영하는 상시 GE
UCLASS()
class PROJECTM_API UNUGE_PlayerSecondaryStatScaling : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_PlayerSecondaryStatScaling();
};
