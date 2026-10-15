#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Battle/Interface/NUBattleEventInterface.h"
#include "NUQuestBattleEventBridgeSubsystem.generated.h"

struct FNUBattleEnemyDefeatedEvent;
struct FNUBattleNaiaCapturedEvent;
struct FNUBattleWonEvent;
class ANUBattleEncounter;
class UNUQuestBattleSession;
class UNUQuestComponent;

// GameInstance 소유의 공유 연결부. 처치·포획·승리 알림과 즉시 반환이 필요한 전투 호출을 전달한다.
// 연습·진정 판정 상태는 인카운터마다 만든 퀘스트 전투 세션이 소유하고, 이 연결부는 세션을 찾아 전달만 한다.
UCLASS()
class PROJECTM_API UNUQuestBattleEventBridgeSubsystem : public UGameInstanceSubsystem, public INUBattleEventInterface
{
	GENERATED_BODY()

public:
	// 이 인카운터에 연결된 진행 중 세션. 다른 전투의 임시 상태를 사용하지 않는다.
	UNUQuestBattleSession* FindSession(const ANUBattleEncounter* Encounter) const;

	// 이 플레이어가 진행 중인 퀘스트 전투 세션.
	UNUQuestBattleSession* FindSessionForQuest(const UNUQuestComponent* QuestComponent) const;

	// 플레이어가 사라질 때 그 플레이어의 세션을 정리한다.
	void EndSessionsForQuest(const UNUQuestComponent* QuestComponent);

	// 전투 연결과 턴·행동 규칙을 세션으로 전달한다.
	virtual bool PrepareParticipants(ANUBattleEncounter* Encounter) override;
	virtual void BeginTurn(ANUBattleEncounter* Encounter, int32 TurnNumber) override;
	virtual void AdjustActionContext(ANUBattleEncounter* Encounter, const FNUBattleCommandRequest& Command, FNUBattleActionContext& Context) const override;
	virtual float AdjustSpeed(ANUBattleEncounter* Encounter, bool bEnemy, float Speed) const override;

	// 피해와 반응 판정의 결과는 전투별 규칙이 소유한다.
	virtual bool InterceptDamage(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Target, float& Damage) override;
	virtual void NotifyAbilityHit(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Actor, FGameplayTag AbilityId) override;
	virtual void NotifyReaction(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Actor, bool bParry) override;
	virtual ENUBattleResult GetForcedResult(ANUBattleEncounter* Encounter) const override;

	// 명령과 표시를 제한하고 일반 전투의 보상·승리 이벤트를 억제한다.
	virtual FGameplayTag GetAttackAbilityOverride(ANUBattleEncounter* Encounter, int32 CombatantId) const override;
	virtual FGameplayTag GetSkillAbilityOverride(ANUBattleEncounter* Encounter, int32 CombatantId) const override;
	virtual bool IsAbilityAllowed(ANUBattleEncounter* Encounter, FGameplayTag AbilityId) const override;
	virtual bool IsCaptureAllowed(const ANUBattleEncounter* Encounter) const override;
	virtual bool ShouldHideVitalBar(const ANUBattleEncounter* Encounter, ENUBattleTeam Team) const override;
	virtual bool ShouldGrantStandardRewards(const ANUBattleEncounter* Encounter) const override;
	virtual bool ShouldPublishStandardVictory(const ANUBattleEncounter* Encounter) const override;

protected:
	// 전투 이벤트 구독 및 해제
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// 전투 이벤트 -> 퀘스트 진행 이벤트로 변환
	void HandleEnemyDefeated(const FNUBattleEnemyDefeatedEvent& Event);
	void HandleNaiaCaptured(const FNUBattleNaiaCapturedEvent& Event);
	void HandleBattleWon(const FNUBattleWonEvent& Event);

	// 끝난 세션을 목록에서 뺀다.
	void PruneSessions();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUQuestBattleSession>> Sessions;

	FDelegateHandle EnemyDefeatedHandle;
	FDelegateHandle NaiaCapturedHandle;
	FDelegateHandle BattleWonHandle;
};
