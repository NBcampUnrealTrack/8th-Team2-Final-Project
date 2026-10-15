// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleGameplayAbility.h"
#include "NUUseItemAbility.generated.h"


// 아이템 사용 공용 어빌리티
UCLASS()
class PROJECTM_API UNUUseItemAbility : public UNUBattleGameplayAbility
{
	GENERATED_BODY()
	
public:
	UNUUseItemAbility();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
	virtual void ApplyRuntimeEffect(AActor* TargetActor) override;

	// 인카운터가 아이템 정의의 정책으로 이미 대상을 확정했으므로 다시 타겟을 정하면 안됨
	virtual void ResolveEffectTargets(TArray<AActor*>& OutTargets) const override;

private:
	// 교감 아이템일 때 GE 대신 포획 판정으로 감
	void ApplyCaptureEffect(AActor* TargetActor);

private:
	UPROPERTY()
	TSubclassOf<UGameplayEffect> ItemEffectClass;

	float ItemMagnitude;

	// 인카운터가 페이로드에 넣어 보낸 아이템 분류 태그, 포획 분기 판정에 사용함
	FGameplayTagContainer ItemTags;
};
