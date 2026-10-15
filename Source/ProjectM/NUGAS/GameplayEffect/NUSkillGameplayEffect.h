// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NUSkillGameplayEffect.generated.h"


// 액티브 스킬 피해 GE
UCLASS()
class PROJECTM_API UNUGE_SkillDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_SkillDamage();
};

// 액티브 스킬 회복 GE
// 즉발회복, 턴회복 둘다 이 GE 사용
UCLASS()
class PROJECTM_API UNUGE_SkillHeal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_SkillHeal();
};
