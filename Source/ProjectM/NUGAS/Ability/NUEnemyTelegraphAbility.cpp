// Fill out your copyright notice in the Description page of Project Settings.


#include "NUEnemyTelegraphAbility.h"

#include "Battle/NUBattleTypes.h"

UNUEnemyTelegraphAbility::UNUEnemyTelegraphAbility()
{
	AbilityId = NU::Ability::Battle::Enemy::Telegraph;

	// 기획서에 문구가 없어 임시, 보스마다 다른 문구가 필요해지면 페이즈 표에 칸을 둘 것
	DisplayName = NSLOCTEXT("NUBattle", "EnemyTelegraph", "큰 기술 준비");

	// 제자리에서 행동 한 번만 쓰고 끝남, 대상이 자신이라 반응 창도 안 열림
	TargetPolicy = ENUTargetPolicy::Self;
	Motion = ENUBattleActionMotion::InPlace;
	AbilityEffects.Reset();
	bParryable = false;
}
