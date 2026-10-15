// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "NUDamageExecution.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTM_API UNUDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
	
public:
	UNUDamageExecution();
	
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	
private:
	float CalculateElementMultiplier(const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const;
	float CalculateCriticalMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams, 
		const FAggregatorEvaluateParameters& Params, bool& bOutCritical) const;
	
	// 합연산 풀, 1 + Σ 를 반환함
	// "주는 피해 +X%" 는 전부 여기 하나로 모여 기본 피해에 한 번만 곱해짐
	float CalculateAdditivePool(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		const FGameplayTagContainer& AttackTags) const;
	
	// 방어 감쇠 배율, 100 / (100 + 유효 방어력)
	// 기획 문서가 상성·치명타와 같은 곱연산 묶음에 넣으므로 여기도 곱연산 항 하나로 둠
	float CalculateDefenseMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		const FAggregatorEvaluateParameters& Params) const;
	
	// 대상이 받는 피해 감소 배율, 감소가 없으면 1
	float CalculateDamageTakenMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams) const;
	
	// 대상이 그 속성으로 받는 피해 배수, 없으면 1 (2109 수중 감옥)
	float CalculateElementDamageTakenMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		const FGameplayTagContainer& AttackTags) const;
	
	// 상시 직업 스킬의 곱연산 "주는 피해" 항, 공격자 쪽
	// 합연산 풀(3번)과 자리가 다름 — 표의 StackType 이 둘을 가름
	float CalculateJobDamageMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams) const;
	
	// 공격자에게 걸린 속성 위력 배수, 없으면 1
	// 패시브의 속성 위력은 합연산, 이쪽은 스킬이 건 곱연산
	float CalculateElementPowerMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		const FGameplayTagContainer& AttackTags) const;
	
	// 대상이 받는 추가 피해 (균열), 없으면 1
	// 상태이상 정의를 그대로 읽음
	float CalculateIncomingDamageAmp(const FGameplayEffectCustomExecutionParameters& ExecutionParams) const;
};
