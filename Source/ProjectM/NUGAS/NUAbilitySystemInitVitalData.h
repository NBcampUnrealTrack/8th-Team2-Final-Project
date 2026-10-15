// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUAbilitySystemInitVitalData.generated.h"

USTRUCT(BlueprintType)
struct FNUAbilitySystemInitVitalData
{
	GENERATED_BODY()
	
	UPROPERTY()
	float MaxHealth = 100.f;
	
	UPROPERTY()
	float Health = 100.f;
	
	UPROPERTY()
	float MaxSP = 50.f;

	UPROPERTY()
	float SP = 50.f;
};

USTRUCT(BlueprintType)
struct FNUAbilitySystemInitCombatData
{
	GENERATED_BODY()
	
	UPROPERTY()
	float AttackPower = 10.f;
	
	UPROPERTY()
	float MagicAttackPower = 10.f;
	
	UPROPERTY()
	float Defense = 5.f;
	
	UPROPERTY()
	float Speed = 10.f;
	
	UPROPERTY()
	float CriticalRate = 0.1f;
	
	UPROPERTY()
	float CriticalDamage = 1.5f;
};
