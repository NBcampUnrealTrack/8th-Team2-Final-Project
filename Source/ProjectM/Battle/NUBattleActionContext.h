#pragma once

#include "CoreMinimal.h"
#include "NUBattleTypes.h"
#include "Data/NUActiveSkillDefinition.h"
#include "Data/NUJobSkillDefinition.h"
#include "NUBattleActionContext.generated.h"

class UNUBattleGameplayAbility;

// 인카운터가 행동을 실행하기 전에 만들어두는 정보
// 발동 전 조회(이름,SP)와 발동 중 실행(계수,효과)를 담음
USTRUCT()
struct PROJECTM_API FNUBattleActionContext
{
	GENERATED_BODY()
	
	/* 조회용, 발동 전 인카운터와 커맨드 컴포넌트가 읽음 */
	
	UPROPERTY()
	FText DisplayName;
	
	UPROPERTY()
	int32 SPCost = 0;
	
	// 0보다 크면 반복, 지금은 기본공격만 사용
	UPROPERTY()
	int32 SPCostPerHit = 0;

	// 표가 정한 타격 횟수, 비용과 무관하게 고정
	UPROPERTY()
	int32 HitCount = 1;
	
	UPROPERTY()
	int32 ActionPriority = 0;
	
	
	/* 실행용, 어빌리티가 받아서 자기 필드에 옮겨 담음 */

	// 액티브 스킬이면 ActiveSkillTable 의 RowName, 아니면 0
	// VFX 프로필(DT_SkillVFX) 조회용
	UPROPERTY()
	int32 SkillId = 0;

	UPROPERTY()
	ENUTargetPolicy TargetPolicy = ENUTargetPolicy::SingleEnemy;
	
	UPROPERTY()
	ENUBattleActionMotion Motion = ENUBattleActionMotion::InPlace;
	
	UPROPERTY()
	float DamageCoefficient = 0.f;

	// 합연산 풀에 더할 이 행동만의 값, 0.2 면 +20%
	// 풀 전체는 실행기가 상한을 잘라주므로 여기서는 표기값 그대로 넣으면 됨
	UPROPERTY()
	float AdditiveDamageBonus = 0.f;

	// 곱연산 항 하나, 1 이면 곱하지 않음
	// "그 조건이면 2배" 처럼 합연산 풀에 섞으면 안 되는 값만 여기에 둘 것
	UPROPERTY()
	float DamageMultiplier = 1.f;

	// 방어력 무시 비율(0~1), 유효 방어력 = DEF x (1 - 이 값)
	UPROPERTY()
	float DefenseIgnore = 0.f;

	// 이 행동이 소모하는 자기 체력, 최대 HP 비율이며 타격마다 소모함
	// 3033 연속 발현은 타격당 5%, 3타라 15퍼 소모하는 방식
	UPROPERTY()
	float SelfHPCostRatio = 0.f;

	// 흡혈 비율, 가한 피해에 곱한 만큼 시전자가 회복함 (1 이면 피해량 전부)
	UPROPERTY()
	float Lifesteal = 0.f;

	// 남은 SP 를 전부 쓰는 행동 (3203 나이아 개화), 인카운터가 비용을 남은 SP 로 · 계수를 쓴 SP 로 채움
	UPROPERTY()
	bool bSpendAllSP = false;
	
	UPROPERTY()
	FGameplayTag DamageType;

	// 계수가 무엇에 비례하는지, 비어 있으면 공격력, DamageType 이 마법이면 마법 공격력
	// 드루이드는 자신의 최대 HP 비율을 계수로 사용함
	UPROPERTY()
	FGameplayTag DamageBase;
	
	UPROPERTY()
	FGameplayTag AttackElement;
	
	UPROPERTY()
	TArray<FNUAbilityEffectEntry> Effects;
	
	// AbilityEffectEntry로 표현할 수 없는 스킬 효과 (비율 회복, 상태이상 회복 등)
	UPROPERTY()
	TArray<FNUActiveEffectRow> RuntimeEffects;

	// 직업 스킬의 런타임 효과
	UPROPERTY()
	TArray<FNUJobSkillEffectRow> JobRuntimeEffects;
	
	// 이 행동을 실행할 어빌리티 클래스, 비어있다면 AbilityId 태그로 찾음
	UPROPERTY()
	TSubclassOf<UNUBattleGameplayAbility> ExecutorClass;
	
	// false면 어빌리티가 CDO에 작성된 값을 읽음
	// 기본공격, 도주 등이 사용할 수 있음
	UPROPERTY()
	bool bValid = false;
	
	
	/* 패링 관련 변수 */
	
	// 이 행동이 패링or회피와 같이 반응창을 여는 행동인지
	UPROPERTY()
	bool bReactable = false;
	
	// 패링인지, false면 회피
	UPROPERTY()
	bool bParryable = false;
	
	UPROPERTY()
	FNUParryTimingSpec ParryTiming;
};