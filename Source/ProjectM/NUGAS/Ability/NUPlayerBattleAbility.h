// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleGameplayAbility.h"
#include "NUPlayerBattleAbility.generated.h"


// 플레이어가 전투에서 사용하는 SP를 소모할 수 있는 어빌리티
UCLASS()
class PROJECTM_API UNUPlayerBattleAbility : public UNUBattleGameplayAbility
{
	GENERATED_BODY()

public:
	UNUPlayerBattleAbility();

	// 명령 제출 시 서버가 사용 예약할 SP를 계산할 때 사용함
	int32 GetSPCost() const;
	int32 GetSPCostPerHit() const;
	bool IsRepeatable() const;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 남은 타격 횟수가 있으면 다음 타격을 예약하고, 없으면 마무리로 넘김
	virtual void OnHitApplied() override;
	
	virtual void BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const override;

	// 발동 순간의 남은 타격 수가 곧 총 타수, 타격마다 줄어들므로 발동 순간에만 맞는 값
	virtual int32 GetActionHitCount() const override;

	// 2회차 이후 타격, PreApplyAbilityEffect는 행동당 1회라 여기서 부르지 않음
	UFUNCTION()
	void HandleRepeatHit();

	// 대상이 죽었으면 같은 진영의 살아있는 다른 전투원으로 옮김, 남은 대상이 없으면 false
	// 진영을 지금 대상에서 유도함, 환각처럼 아군을 때리는 행동도 아군 안에서만 옮겨져야 함
	bool RetargetToLivingTarget();

protected:
	// 소모하는 SP 고정 비용, 스킬이 사용함 
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Cost", meta = (ClampMin = "0"))
	int32 SPCost;

	// 0보다 크면 투입 SP를 이 값으로 나눈 횟수만큼 반복 타격함, 기본공격이 사용함
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Cost", meta = (ClampMin = "0"))
	int32 SPCostPerHit;

	// 반복 타격 사이 간격, 연타로 보이게 하는 용도
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Cost", meta = (ClampMin = "0"))
	float HitInterval;

private:
	// 이번 발동에 남은 타격 수, ActivateAbility에서 투입 SP로 정해짐
	int32 RemainingHits = 1;
};
