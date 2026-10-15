// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "NUCombatAttributeSet.generated.h"


// 전투용 수치
UCLASS()
class PROJECTM_API UNUCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData &Data) override;
	
	/* ACCESSORS 설정 */
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, AttackPower);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, MagicAttackPower);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Defense);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Speed);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, CriticalRate);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, CriticalDamage);
	
	/* 스탯 수치 */
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicAttackPower)
	FGameplayAttributeData MagicAttackPower;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Speed)
	FGameplayAttributeData Speed;
	
	// 0~1 비율, 표기만 100 곱해서 표기
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalRate)
	FGameplayAttributeData CriticalRate;
	
	// 1.x 배율 그대로 적용, 표기만 100 곱해서 표기
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalDamage)
	FGameplayAttributeData CriticalDamage;
	
protected:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	
protected:
	
	/* OnRep 함수 */
	
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_MagicAttackPower(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_Speed(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_CriticalRate(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_CriticalDamage(const FGameplayAttributeData& OldValue) const;
};
