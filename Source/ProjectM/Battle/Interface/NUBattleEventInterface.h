#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "NUBattleEventInterface.generated.h"

class ANUBattleEncounter;
class UNUAbilitySystemComponent;
struct FNUBattleActionContext;
struct FNUBattleCommandRequest;

// 공용 전투가 외부 기능에 알림 및 판정을 요청하는 계약
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UNUBattleEventInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTM_API INUBattleEventInterface
{
	GENERATED_BODY()

public:
	// 하나의 구현 객체가 여러 전투를 처리할 수 있으므로 항상 호출한 인카운터를 전달한다.
	virtual bool PrepareParticipants(ANUBattleEncounter* Encounter) { return true; }
	virtual void BeginTurn(ANUBattleEncounter* Encounter, int32 TurnNumber) {}

	// 피해, 행동 / 허용, 승패는 반환값이 필요하므로 일반 완료 알림과 구분한다.
	virtual void AdjustActionContext(ANUBattleEncounter* Encounter, const FNUBattleCommandRequest& Command, FNUBattleActionContext& Context) const {}
	virtual float AdjustSpeed(ANUBattleEncounter* Encounter, bool bEnemy, float Speed) const { return Speed; }
	virtual bool InterceptDamage(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Target, float& Damage) { return false; }
	virtual void NotifyAbilityHit(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Actor, FGameplayTag AbilityId) {}
	virtual void NotifyReaction(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Actor, bool bParry) {}
	virtual ENUBattleResult GetForcedResult(ANUBattleEncounter* Encounter) const { return ENUBattleResult::None; }

	// 기본 공격 교체는 실행 상태를 가진 외부 기능에 조회한다.
	virtual FGameplayTag GetAttackAbilityOverride(ANUBattleEncounter* Encounter, int32 CombatantId) const { return FGameplayTag(); }
	virtual FGameplayTag GetSkillAbilityOverride(ANUBattleEncounter* Encounter, int32 CombatantId) const { return FGameplayTag(); }

	// 상태 변경 없이 전투의 허용 조건과 표시, 보상 여부를 조회한다.
	virtual bool IsAbilityAllowed(ANUBattleEncounter* Encounter, FGameplayTag AbilityId) const { return true; }
	virtual bool IsCaptureAllowed(const ANUBattleEncounter* Encounter) const { return true; }
	virtual bool ShouldHideVitalBar(const ANUBattleEncounter* Encounter, ENUBattleTeam Team) const { return false; }
	virtual bool ShouldGrantStandardRewards(const ANUBattleEncounter* Encounter) const { return true; }
	virtual bool ShouldPublishStandardVictory(const ANUBattleEncounter* Encounter) const { return true; }
};
