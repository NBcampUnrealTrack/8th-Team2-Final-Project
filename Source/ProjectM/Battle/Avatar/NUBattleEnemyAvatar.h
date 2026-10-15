// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleAvatar.h"
#include "NUBattleEnemyAvatar.generated.h"

class UNUCombatAttributeSet;
class UNUVitalAttributeSet;
class UNUAbilitySystemComponent;
struct FNUNaiaDefinition;
struct FNUNaiaStats;

UCLASS()
class PROJECTM_API ANUBattleEnemyAvatar : public ANUBattleAvatar
{
	GENERATED_BODY()

public:
	ANUBattleEnemyAvatar();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UNUAbilitySystemComponent* GetNUASC() const override;
	
	void InitializeEnemy(const FNUNaiaDefinition& NaiaDefinition);
	
	void ApplyInitialVitals(const FNUNaiaStats& NaiaStats);
	void ApplyInitialCombatStats(const FNUNaiaStats& NaiaStats);
	
	int32 GetLevel() const;
	
protected:
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Ability", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UNUVitalAttributeSet> VitalSet;
	
	UPROPERTY()
	TObjectPtr<UNUCombatAttributeSet> CombatSet;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|GameplayEffect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UGameplayEffect> InitVitalsEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|GameplayEffect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> InitCombatStatsEffect;
	
	int32 Level = 1;
};
