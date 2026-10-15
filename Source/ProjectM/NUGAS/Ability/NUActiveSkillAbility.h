// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUPlayerBattleAbility.h"
#include "NUActiveSkillAbility.generated.h"


// 액티브 스킬 실행기
// 스킬 하나당 클래스 하나가 아니라 실행 방식 하나당 클래스 하나이므로
// 데이터로 도는 스킬 전부가 이 클래스 하나로 처리됨
//
// 계수, 속성, 부여 효과는 인카운터가 만든 FNUBattleActionContext 에서 오고
// 부모의 ApplyActionContext 가 멤버에 저장함
// 이 클래스는 해당 컨텍스트 없이는 발동하지 않고, 항목으로 표현할 수 없는 효과만 별도로 처리함
UCLASS()
class PROJECTM_API UNUActiveSkillAbility : public UNUPlayerBattleAbility
{
	GENERATED_BODY()

public:
	UNUActiveSkillAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 항목으로 표현할 수 없는 효과 처리 (상태이상 해제, 특수 스킬 효과 등)
	virtual void ApplyRuntimeEffect(AActor* TargetActor) override;
	virtual void ApplySelfRuntimeEffects() override;
	virtual void ApplyCrossSideRuntimeEffects() override;

private:
	// 런타임 줄 하나를 적용함, 누구에게 갈지는 부르는 쪽이 정해서 넘김
	void ApplyRuntimeRow(const FNUActiveEffectRow& Row, UNUAbilitySystemComponent* SourceASC,
		UNUAbilitySystemComponent* TargetASC);

	// 직업 스킬의 런타임 효과, 같은 일을 하지만 행 타입이 달라 함수를 분리함
	void ApplyJobRuntimeRow(const FNUJobSkillEffectRow& Row, UNUAbilitySystemComponent* SourceASC,
		UNUAbilitySystemComponent* TargetASC);
};
