#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Battle/NUBattleTypes.h"
#include "Engine/DataTable.h"
#include "NUPassiveDefinition.generated.h"

// 패시브가 언제 발동하는지
// 상시는 전투 내내 켜져 있고 나머지는 그 사건이 일어날 때만 판정함
UENUM(BlueprintType)
enum class ENUPassiveTrigger : uint8
{
	None,
	Always,				// 상시
	OnTurnStart,		// 턴시작
	OnBattleStart,		// 전투시작
	OnAttack,			// 공격시
	OnHitTaken,			// 피격시
	OnCriticalTaken,	// 치명피격시
	OnKill,				// 처치시
	OnDefeated,			// 전투불능시
	OnLowHealth,		// 저체력시
	OnStatusEffect		// 상태이상시
};

// 패시브가 무엇을 하는지
// 겹치는 이름은 ENUSkillEffectType 과 같은 뜻으로 맞춰둠
UENUM(BlueprintType)
enum class ENUPassiveEffectType : uint8
{
	None,
	StatModifier,		// 스탯증가
	StatusApply,		// 상태부여
	StatusImmune,		// 상태면역
	Heal,				// 회복
	DamageAmplify,		// 피해증가 — 옛날 표 전용, 새 표는 DamageDealtMod 로 변경됨
	DamageReduce,		// 피해감소 — 옛날 표 전용, 새 표는 DamageTakenMod 로 변경됨
	Evade,				// 회피
	Special,			// 특수 — 옛날 표 전용, 새 표는 아래 이름들로 갈림
	DamageDealtMod,		// 주는 피해 증감, 부호가 방향을 가름 (2803 · 2836)
	DamageTakenMod,		// 받는 피해 증감, 부호가 방향을 가름 (2838)
	ElementPowerMod,	// 속성 위력 증감 (2821~2826)
	StatusDurationMod,	// 건 상태이상의 지속 턴 증감 (2820)
	ReflectStatus,		// 걸린 상태이상을 부여자에게 되돌림 (2832)
	CopyPassive,		// 상대의 보유 패시브를 같이 가짐 (2833)
	ResourceGain,		// 자원 획득 (2834 SP)
	DeathPrevent,		// 죽을 피해를 한 번 버팀 (2835)
	TurnOrderPriority	// 행동 순서 맨 앞 (2837)
};

// 효과를 누구에게 적용하는지
UENUM(BlueprintType)
enum class ENUPassiveTargetScope : uint8
{
	None,
	Self,				// 자신
	Enemy,				// 적
	Attacker,			// 공격자
	RandomEnemy			// 랜덤적
};


// 패시브 효과, PassiveSkillEffectTable 의 RowName 이 SkillEffectId 가 됨
USTRUCT(BlueprintType)
struct PROJECTM_API FNUPassiveEffectRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Identity")
	int32 SkillEffectId = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Identity")
	int32 PassiveId = INDEX_NONE;
	
	// 한 패시브 안에서의 순번, 1부터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Identity")
	int32 EffectIndex = 0;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	ENUPassiveEffectType EffectType = ENUPassiveEffectType::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	ENUPassiveTargetScope TargetScope = ENUPassiveTargetScope::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	ENUEffectStackType StackType = ENUEffectStackType::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	ENUStatusAffectedStat AffectedStat = ENUStatusAffectedStat::None;
	
	// bIsPercent 면 백분율(30 = 30%)이지 비율이 아님
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	float Value = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	bool bIsPercent = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	ENUEffectDurationScope DurationScope = ENUEffectDurationScope::Instant;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	int32 DurationTurns = 0;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Trigger")
	ENUPassiveTrigger Trigger = ENUPassiveTrigger::None;
	
	// 발동 확률, 0이면 조건을 만족할 때 확정 발동
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Trigger")
	float TriggerChance = 0.f;
	
	// 기준치를 "이상" 으로 읽을지 "미만" 으로 읽을지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Trigger")
	ENUThresholdCompare ThresholdCompare = ENUThresholdCompare::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Trigger")
	float TriggerThreshold = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Trigger")
	ENUEffectCondition ConditionType = ENUEffectCondition::None;
	
	// 조건이 값을 요구할 때 쓰는 칸, 상태이상 목록이면 쉼표로 여럿이 올 수 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Trigger")
	FString ConditionValue;
	
	// 전투당 횟수 제한, 0이면 제한 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Trigger")
	int32 MaxPerBattle = 0;
	
	
	// 속성을 가리는 효과가 쓰는 칸 (2821~2826)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	FGameplayTag Element;
	
	// 부여하거나 면역이 되는 상태이상
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	int32 StatusEffectId = 0;
	
	// Code면 전용 구현이 있어야 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|PassiveEffect|Effect")
	ENUEffectImplementBy ImplementBy = ENUEffectImplementBy::Data;
	
	
	// 계산에 쓸 값, 백분율이면 여기서 한 번만 나눔
	float GetScaledValue() const
	{
		return bIsPercent ? Value / 100.f : Value;
	}
};

// 보유 패시브 하나의 효과 줄 하나, 번호와 줄을 같이 들고 다님
// 줄만 들고 다니면 로그에 어느 패시브인지 못 적음
struct FNUOwnedPassiveEffect
{
	int32 PassiveId = 0;
	FNUPassiveEffectRow Row;
};

// 패시브 정의, PassiveTable의 RowName이 PassiveId가 됨
// 효과는 별도 테이블로 분리함
USTRUCT(BlueprintType)
struct PROJECTM_API FNUPassiveDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Passive|Identity")
	int32 PassiveId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Passive|Identity")
	FName PassiveKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Passive|Identity")
	FText KoreanName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Passive|Identity")
	FText EnglishName;


	// 이 패시브를 획득하는데 필요한 나이아의 속성, 비어있으면 속성을 가리지 않음(All)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Passive|Classification")
	FGameplayTag AcquireElement;

	// 기획의 희귀도, 굴림 확률을 나눌 때 쓸 값이라 담아만 둠
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Passive|Classification")
	int32 Grade = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Passive|Text")
	FText Description;
};
