// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUActiveSkillDefinition.h"

struct FNUBattleActionContext;
class UNUBattleDataSubsystem;

// 스킬 정의와 효과 행을 행동 컨텍스트로 조립하는 곳
// 스킬 테이블의 규칙(어떤 EffectType이 어떤 GE로 가는지, 모션을 무엇으로 고르는지)을 선택함
namespace NUActiveSkill
{
	// 피해 줄이 들고 있는 값들
	struct FDamageParams
	{
		// 기준이 되는 피해 줄을 찾았는지, 못 찾으면 이 타격에 피해 항목을 달지 않음
		bool bHasDamage = false;

		// 조건이 붙었든 아니든 피해 줄이 하나라도 있는지, 중앙으로 갈지만 정함
		bool bHasAnyDamageRow = false;

		float Coefficient = 0.f;
		float DefenseIgnore = 0.f;
		float Lifesteal = 0.f;
	};

	// 피해 줄과 거기 딸린 줄에서 값을 모음
	// DamageCondition 은 어느 피해 줄을 기준으로 삼을지 — 보통 조건 없는 줄이고,
	// 상태가 풀릴 때 터지는 예약 피해만 OnStateEnd 줄을 봄 (2210 개화) 
	PROJECTM_API FDamageParams GatherDamageParams(const TArray<FNUActiveEffectRow>* Effects,
		ENUEffectCondition DamageCondition = ENUEffectCondition::None);

	// 데이터로 실행할 수 있는 스킬이면 true
	// 전용 구현이 필요한 Special 행이 있으면 false, 이때 Out은 건드리지 않음
	PROJECTM_API bool BuildActionContext(int32 SkillId, const UNUBattleDataSubsystem& Data,
		FNUBattleActionContext& Out);
}
