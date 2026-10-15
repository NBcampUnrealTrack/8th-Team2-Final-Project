// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NUCombatantInterface.generated.h"

class UNUAbilitySystemComponent;
class ANUBattleEncounter;

UINTERFACE()
class UNUCombatantInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTM_API INUCombatantInterface
{
	GENERATED_BODY()
	
public:
	virtual UNUAbilitySystemComponent* GetNUASC() const = 0;
	
	AActor* GetCurrentAvatar() const;
	void SetCurrentAvatar(AActor* NewAvatar);
	
	ANUBattleEncounter* GetBattleEncounter() const;
	void SetBattleEncounter(ANUBattleEncounter* NewEncounter);
};
