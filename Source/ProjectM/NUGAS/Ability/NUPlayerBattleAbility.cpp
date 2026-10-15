// Fill out your copyright notice in the Description page of Project Settings.


#include "NUPlayerBattleAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Encounter/NUBattleEncounter.h"


UNUPlayerBattleAbility::UNUPlayerBattleAbility()
{
	SPCost = 0;
	SPCostPerHit = 0;
	HitInterval = 0.3f;
}

int32 UNUPlayerBattleAbility::GetSPCost() const
{
	return SPCost;
}

int32 UNUPlayerBattleAbility::GetSPCostPerHit() const
{
	return SPCostPerHit;
}

bool UNUPlayerBattleAbility::IsRepeatable() const
{
	return SPCostPerHit > 0;
}

void UNUPlayerBattleAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 인카운터가 EventMagnitude에 확정된 타수를 실어 보냄
	// Super에서 타격 대기를 예약하므로 그 전에 타격 횟수를 정해둬야 함
	// 여기서 투입 SP로 타수를 다시 구하면 상태이상 보정과 어긋나게 됨
	RemainingHits = TriggerEventData
		? FMath::Max(1, FMath::FloorToInt(TriggerEventData->EventMagnitude))
		: 1;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UNUPlayerBattleAbility::OnHitApplied()
{
	--RemainingHits;

	// 타격횟수가 남아있고 적이 남아있을때만 반복
	if (RemainingHits > 0 && RetargetToLivingTarget())
	{
		UAbilityTask_WaitDelay* WaitNext = UAbilityTask_WaitDelay::WaitDelay(this, HitInterval);
		WaitNext->OnFinish.AddDynamic(this, &UNUPlayerBattleAbility::HandleRepeatHit);
		WaitNext->ReadyForActivation();
		return;
	}

	Super::OnHitApplied();
}

void UNUPlayerBattleAbility::BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const
{
	Super::BuildActionMotionSpec(OutSpec);
	
	// 아바타가 반복타격이 끝날때까지 공격 액션을 유지하도록 알려줌
	OutSpec.HitCount = RemainingHits;
	OutSpec.HitInterval = HitInterval;
}

int32 UNUPlayerBattleAbility::GetActionHitCount() const
{
	return RemainingHits;
}

void UNUPlayerBattleAbility::HandleRepeatHit()
{
	ApplyHitEffects();
	OnHitApplied();
}

bool UNUPlayerBattleAbility::RetargetToLivingTarget()
{
	ANUBattleEncounter* Encounter = GetBattleEncounter();
	if (!IsValid(Encounter))
	{
		return false;
	}
	
	// 옮겨갈 진영은 지금 대상에서 유도함
	// 기본은 적 대상
	ENUBattleTeam TargetTeam = ENUBattleTeam::Enemy;

	// 지금 대상이 아직 살아 있으면 그대로 이어서 때림
	if (const ANUBattleAvatar* TargetAvatar = Cast<ANUBattleAvatar>(CachedTargetActor))
	{
		FNUBattleParticipant Current;
		if (Encounter->GetParticipant(TargetAvatar->GetCombatantId(), Current))
		{
			TargetTeam = Current.Team;

			TArray<int32> Living;
			Encounter->GetLivingCombatantIds(TargetTeam, Living);

			if (Living.Contains(TargetAvatar->GetCombatantId()))
			{
				return true;
			}
		}
	}

	// 죽었으면 같은 진영의 살아있는 다른 전투원으로 남은 타수를 넘김
	TArray<int32> LivingTargets;
	Encounter->GetLivingCombatantIds(TargetTeam, LivingTargets);

	for (int32 CombatantId : LivingTargets)
	{
		FNUBattleParticipant Next;
		if (Encounter->GetParticipant(CombatantId, Next) && IsValid(Next.AvatarActor))
		{
			CachedTargetActor = Next.AvatarActor;
			return true;
		}
	}

	// 다음 타겟을 찾을 수 없음, 그 진영이 전멸함
	return false;
}
