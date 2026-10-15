// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Battle/NUBattleTypes.h"

class ANUBattleEncounter;
class UNUAbilitySystemComponent;
struct FNUEnemyPattern;
struct FNUEnemyPatternRuntime;

// 패턴 선택 시 넘기는 입력 묶음
struct FNUEnemyPatternContext
{
	// AnyOpponentHasTag 조건에서 상대 진영을 확인할때 사용
	ANUBattleEncounter* Encounter = nullptr;
	
	UNUAbilitySystemComponent* SourceASC = nullptr;

	ENUBattleTeam SourceTeam = ENUBattleTeam::Enemy;

	int32 TurnNumber = 0;

	const FNUEnemyPattern* Pattern = nullptr;

	// 쿨타임, 사용 횟수, 현재 페이즈. 선택 결과가 여기에 기록됨
	FNUEnemyPatternRuntime* Runtime = nullptr;
};

struct FNUEnemySelectedAction
{
	// 기본공격, 스킬 등
	FGameplayTag AbilityId;
	
	// 0이면 기본 공격 같은 태그 기반 행동
	// 페이즈 진입에도 0
	int32 SkillId = 0;
	
	// 패링 타이밍 관련 값, 
	// 해당 규칙이 기존 패링 타이밍을 덮어쓰는지
	// 페이즈 진입 행동은 항상 false
	bool bOverrideParryTiming = false;
	float ChallengeDuration = 0.f;
	float SuccessAnchor = 0.f;
	float SuccessWindow = 0.f;
	
	bool IsValid() const
	{
		return AbilityId.IsValid();
	}
};

// Battle Encounter가 호출해서 사용함
// 나중에 보스용으로 다른 구현을 붙이더라도 인카운터 코드는 그대로 둘 수 있게 여기만 교체하면 됨
namespace NUEnemyPattern
{
	// 선택한 행동을 돌려줌
	// 유효하지 않으면 (AbilityId가 없음) 인카운터가 기본공격으로 채움
	PROJECTM_API FNUEnemySelectedAction SelectAbility(const FNUEnemyPatternContext& Context);
}
