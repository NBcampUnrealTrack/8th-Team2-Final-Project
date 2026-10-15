#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NUGE_FullRecovery.generated.h"

// 회복 대상의 현재 HP를 Provider가 전달한 최대 HP로 변경한다.
UCLASS()
class PROJECTM_API UNUGE_FullRecovery : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_FullRecovery();
};
