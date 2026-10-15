// Fill out your copyright notice in the Description page of Project Settings.


#include "NUEnemyAttackAbility.h"

#include "Battle/NUBattleTypes.h"

UNUEnemyAttackAbility::UNUEnemyAttackAbility()
{
	TargetPolicy = ENUTargetPolicy::AllEnemies;
	Motion = ENUBattleActionMotion::MoveToCenter;
	
	ApproachTime = 0.4f;
	ActionTime = 0.8f;
	ReturnTime = 0.3f;
	HitTimeNormalized = 0.5f;
	
	bParryable = true;
}
