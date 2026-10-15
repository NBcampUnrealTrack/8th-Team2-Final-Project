#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FNUBattleActionContext;
class UNUQuestBattleDefinition;
enum class ENUQuestBattleKind : uint8;
enum class ENUQuestCalmingRule : uint8;

// 달래기 한 번의 결과, 전투 세션이 결과 큐를 고를 때 씀
enum class ENUQuestCalmResult : uint8
{
	None,		// 진정 전투가 아님
	Calmed,		// 동요가 줄었음
	Failed,		// 숨어 있어 닿지 않았음 (땅 매몰)
	Backfired	// 연달아 달래서 동요가 올랐음 (불)
};

// 한 번의 연습·진정 전투에서만 유지되는 판정 상태. 일반 전투와 영구 퀘스트 기록은 소유하지 않는다.
struct FNUQuestTutorialBattleRules
{
	// 전투 시작과 종료 시 상태를 준비하거나 초기화한다.
	bool Initialize(const UNUQuestBattleDefinition* Definition, int32 CompletedGrassRecoveries = 0);
	void Reset();

	// 현재 전투 유형과 UI에 필요한 값을 조회한다.
	bool IsActive() const;
	bool IsTraining() const;
	bool IsCalming() const;
	bool HasSucceeded() const { return bSucceeded; }
	float GetAgitation() const { return Agitation; }
	float GetInitialAgitation() const { return InitialAgitation; }
	float GetAgitationRatio() const;
	// 연습 전투(기본 공격·패링·회피)의 지금 횟수와 필요 횟수. 연습이 아니면 둘 다 0이다.
	ENUQuestBattleKind GetKind() const { return Kind; }
	int32 GetPracticeCount() const;
	int32 GetPracticeRequired() const;

	// 물 안개는 동요와 별개로 관리하며 전투 성공 시 완전히 걷힌다.
	float GetMistRatio() const { return bSucceeded ? 0.f : MistRatio; }

	int32 GetTurnNumber() const { return TurnNumber; }
	bool IsEnemyBurrowed() const;
	bool ShouldEnemyBurrow() const;
	bool EnterBurrow();

	// 턴과 적 행동에 적용할 연습 전투 규칙이다.
	void BeginTurn(int32 InTurnNumber);
	void AdjustEnemyAction(FNUBattleActionContext& Context) const;
	bool InterceptDamage(bool bEnemyTarget, float TargetHealth, float& Damage);
	bool ShouldRestorePlayerHealthOnHit(bool bEnemyActor) const;

	// 실제 명중, 방어 반응, 달래기 실행으로 성공 조건을 갱신한다.
	void NotifyPlayerAbilityHit(FGameplayTag AbilityId);
	void NotifyPlayerReaction(bool bParry);
	ENUQuestCalmResult ApplyCalm();
	float AdjustSpeed(bool bEnemy, float Speed) const;
	bool CanMoveStone() const;
	bool MoveStone();

private:
	void ReduceAgitation(float Amount);

	// 인카운터 하나에만 속하는 진행 상태다.
	ENUQuestBattleKind Kind{};
	ENUQuestCalmingRule Rule{};
	int32 LocationId = 0;
	int32 TurnNumber = 0;
	int32 BasicAttackCommandCount = 0;
	int32 LastBasicAttackCommandTurn = INDEX_NONE;
	int32 ReactionCount = 0;
	int32 LastCalmTurn = INDEX_NONE;
	int32 StoneTurn = INDEX_NONE;
	int32 EarthBurrowIntervalTurns = 0;
	float InitialAgitation = 0.f;
	float Agitation = 0.f;
	float MistRatio = 1.f;
	float CalmAmount = 0.f;
	float ReactionCalmAmount = 0.f;
	float EarthStoneCalmAmount = 0.f;
	float EarthEmergeChanceAfterFailedCalm = 0.5f;
	float FireDecayPerTurn = 0.f;
	float RepeatedCalmPenalty = 0.f;
	float WindPlayerSpeedMultiplier = 1.f;
	float WindParryCalmMultiplier = 1.f;
	bool bEnemyBurrowed = false;
	bool bSucceeded = false;
	bool bRequiresEarthStone = false;
};
