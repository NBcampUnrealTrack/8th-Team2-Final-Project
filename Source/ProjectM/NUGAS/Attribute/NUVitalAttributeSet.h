// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "NUVitalAttributeSet.generated.h"

struct FNUCombatPopupData;

// 체력, SP 수치
UCLASS()
class PROJECTM_API UNUVitalAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData &Data) override;
	
	/* ACCESSORS 설정 */
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, Health);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, MaxHealth);
	// 전투 자원, 내 행동이 끝날 때마다 전량 리필됨
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, SP);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, MaxSP);

	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, IncomingDamage);
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, IncomingHealing);

	// 이번 피해가 치명타였는지, 팝업에서 Damage와 Critical을 가르는 데 사용
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, IncomingCritical);
	
	// 이번 피해의 속성 상성이 어떻게 되는지 확인, 데미지 팝업에서 Weak/Resist 띄우는데 사용
	ATTRIBUTE_ACCESSORS_BASIC(ThisClass, IncomingElementAffinity);

protected:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	// 최대치가 변했을 때 현재치를 따라가게 하는 함수
	void AdjustVitalToMaxChange(const FGameplayAttribute& VitalAttribute, float CurrentVital,
		float OldMax, float NewMax);

	void HandleDeath(const FGameplayEffectModCallbackData& Data);

	// 전투 아바타 머리 위에 수치를 띄움
	// 전투 밖(필드 액터)에서는 Cast가 실패해서 자연히 걸러짐
	void ShowCombatPopup(const FGameplayEffectModCallbackData& Data, const FNUCombatPopupData& PopupData) const;

	// 흡혈, 가한 피해에 비례해 시전자를 회복시킴
	// 데미지 계산은 Execution 이 하지만 실제로 깎인 양은 여기서만 알 수 있어 이 자리에 둠
	void ApplyLifesteal(const FGameplayEffectModCallbackData& Data, float DamageDealt);

protected:
	
	/* 체력 수치 */

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;

	// 스킬 포인트. 패링/회피/매직 실드, 반복 기본 공격, 스킬 강화 등에 소모되며 내 행동 종료 시 전량 리필된다.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SP)
	FGameplayAttributeData SP;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSP)
	FGameplayAttributeData MaxSP;


	/* 메타 데이터, 복제하지않고 서버에서 판정하기 위한 변수 */
	
	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingDamage;
	
	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingHealing;

	// 치명타 여부 0/1, Execution이 IncomingDamage보다 먼저 실어 보냄
	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingCritical;

	// 속성 상성 여부, 치명타와 같은 방식으로 처리
	UPROPERTY(BlueprintReadOnly)
	FGameplayAttributeData IncomingElementAffinity;
	
	/* OnRep 함수 */
	
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
	
	UFUNCTION()
	void OnRep_SP(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_MaxSP(const FGameplayAttributeData& OldValue) const;
};
