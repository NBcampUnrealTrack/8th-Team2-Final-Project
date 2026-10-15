#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NUGE_PlayerBaseSecondaryStats.generated.h"

// 기본 Secondary Stat 8종을 GAS Attribute에 적용하는 상시 GE
UCLASS()
class PROJECTM_API UNUGE_PlayerBaseSecondaryStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_PlayerBaseSecondaryStats();
};
