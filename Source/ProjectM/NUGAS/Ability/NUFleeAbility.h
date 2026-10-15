// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleGameplayAbility.h"
#include "NUFleeAbility.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTM_API UNUFleeAbility : public UNUBattleGameplayAbility
{
	GENERATED_BODY()
	
public:
	UNUFleeAbility();
	
protected:
	virtual void PreApplyAbilityEffect() override;
};
