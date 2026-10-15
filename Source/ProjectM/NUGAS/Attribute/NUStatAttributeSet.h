// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "NUStatAttributeSet.generated.h"


// 스테이터스 관련 수치
UCLASS()
class PROJECTM_API UNUStatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData &Data) override;
	
	/* ACCESSORS 설정 */
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Strength);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Dexterity);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Intelligence);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Luck);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Vitality);
	
protected:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	
protected:
	/* 스탯 수치 */
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength)
	FGameplayAttributeData Strength;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Dexterity)
	FGameplayAttributeData Dexterity;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intelligence)
	FGameplayAttributeData Intelligence;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Luck)
	FGameplayAttributeData Luck;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Vitality)
	FGameplayAttributeData Vitality;
	
	
	/* OnRep 함수 */
	
	UFUNCTION()
	void OnRep_Strength(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_Dexterity(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_Intelligence(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_Luck(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_Vitality(const FGameplayAttributeData& OldValue) const;
};
