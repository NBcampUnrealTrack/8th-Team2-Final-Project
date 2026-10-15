// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleGameplayAbility.h"
#include "NUEnemyAttackAbility.generated.h"


// 적 공격, 기존 동작은 기본 Ability로 옮김
// 적 공격의 기본값만 지정함
UCLASS()
class PROJECTM_API UNUEnemyAttackAbility : public UNUBattleGameplayAbility
{
	GENERATED_BODY()
	
public:
	UNUEnemyAttackAbility();

};
