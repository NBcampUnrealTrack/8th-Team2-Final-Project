// Fill out your copyright notice in the Description page of Project Settings.


#include "NUFleeAbility.h"

#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"

UNUFleeAbility::UNUFleeAbility()
{
	AbilityId = NU::Ability::Battle::Flee;
	
	// Encounter의 TriggerBattleAbility가 보내는 GameplayEvent 수신
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = NU::Ability::Battle::Flee;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
	
	TargetPolicy = ENUTargetPolicy::Self;
	ActionPriority = 200;
	Motion = ENUBattleActionMotion::MoveToCenter;
	
	ApproachTime = 0.25f;
	ActionTime = 0.4f;
	ReturnTime = 0.25f;
	HitTimeNormalized = 0.8f;
}

void UNUFleeAbility::PreApplyAbilityEffect()
{
	UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(SourceASC)) return;

	ANUBattleEncounter* Encounter = SourceASC->GetBattleEncounter();
	if (!IsValid(Encounter)) return;
	
	// 도주 성공/실패 판정 여기 넣으면 됨
	
	ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(GetAvatarActorFromActorInfo());
	Encounter->RequestFlee(Avatar ? Avatar->GetCombatantId() : INDEX_NONE);
}
