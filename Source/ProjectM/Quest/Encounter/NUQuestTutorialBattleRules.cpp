#include "Quest/Encounter/NUQuestTutorialBattleRules.h"

#include "Battle/NUBattleActionContext.h"
#include "NUGAS/NUGameplayTags.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Data/NUQuestTypes.h"

namespace
{
	constexpr int32 RequiredBasicAttackCommands = 5;
	constexpr int32 RequiredReactions = 3;
}

bool FNUQuestTutorialBattleRules::Initialize(const UNUQuestBattleDefinition* Definition, int32 CompletedGrassRecoveries)
{
	Reset();
	// 동요는 진정 전투에서만 쓴다.
	if (Definition == nullptr || !NUQuestBattle::IsTutorialKind(Definition->Kind) || (Definition->Kind == ENUQuestBattleKind::Calming && Definition->InitialAgitation <= 0.f))
	{
		return false;
	}

	Kind = Definition->Kind;
	Rule = Definition->Rule;
	LocationId = Definition->LocationId;
	InitialAgitation = Definition->InitialAgitation;
	Agitation = IsCalming() && Rule == ENUQuestCalmingRule::Grass ? Definition->GetGrassAgitation(CompletedGrassRecoveries) : InitialAgitation;
	CalmAmount = Definition->CalmAmount;
	ReactionCalmAmount = Definition->ReactionCalmAmount;
	EarthBurrowIntervalTurns = Definition->EarthBurrowIntervalTurns;
	EarthStoneCalmAmount = Definition->EarthStoneCalmAmount;
	EarthEmergeChanceAfterFailedCalm = FMath::Clamp(Definition->EarthEmergeChanceAfterFailedCalm, 0.f, 1.f);
	bRequiresEarthStone = Rule == ENUQuestCalmingRule::Earth && Definition->bRequireEarthStone;
	FireDecayPerTurn = Definition->FireDecayPerTurn;
	RepeatedCalmPenalty = Definition->RepeatedCalmPenalty;
	WindPlayerSpeedMultiplier = Definition->WindPlayerSpeedMultiplier;
	WindParryCalmMultiplier = Definition->WindParryCalmMultiplier;
	return true;
}

void FNUQuestTutorialBattleRules::Reset()
{
	*this = FNUQuestTutorialBattleRules();
}

bool FNUQuestTutorialBattleRules::IsActive() const { return Kind != ENUQuestBattleKind::None; }

bool FNUQuestTutorialBattleRules::IsTraining() const { return NUQuestBattle::IsPracticeKind(Kind); }

bool FNUQuestTutorialBattleRules::IsCalming() const { return Kind == ENUQuestBattleKind::Calming; }

float FNUQuestTutorialBattleRules::GetAgitationRatio() const
{
	return IsCalming() && InitialAgitation > 0.f ? FMath::Clamp(Agitation / InitialAgitation, 0.f, 1.f) : 0.f;
}

int32 FNUQuestTutorialBattleRules::GetPracticeCount() const
{
	if (!IsTraining()) { return 0; }
	return Kind == ENUQuestBattleKind::BasicAttack ? BasicAttackCommandCount : ReactionCount;
}

int32 FNUQuestTutorialBattleRules::GetPracticeRequired() const
{
	if (!IsTraining()) { return 0; }
	return Kind == ENUQuestBattleKind::BasicAttack ? RequiredBasicAttackCommands : RequiredReactions;
}

void FNUQuestTutorialBattleRules::BeginTurn(int32 InTurnNumber)
{
	TurnNumber = InTurnNumber;
	if (IsCalming() && Rule == ENUQuestCalmingRule::Fire && TurnNumber > 1)
	{
		ReduceAgitation(FireDecayPerTurn);
	}
}

bool FNUQuestTutorialBattleRules::IsEnemyBurrowed() const { return bEnemyBurrowed; }

bool FNUQuestTutorialBattleRules::ShouldEnemyBurrow() const
{
	return IsCalming() && Rule == ENUQuestCalmingRule::Earth && !bEnemyBurrowed && TurnNumber > 0
		&& EarthBurrowIntervalTurns > 0 && TurnNumber % EarthBurrowIntervalTurns == 0;
}

bool FNUQuestTutorialBattleRules::EnterBurrow()
{
	if (!ShouldEnemyBurrow()) { return false; }
	bEnemyBurrowed = true;
	return true;
}

void FNUQuestTutorialBattleRules::AdjustEnemyAction(FNUBattleActionContext& Context) const
{
	if (Kind == ENUQuestBattleKind::Parry)
	{
		Context.bReactable = true;
		Context.bParryable = true;
		return;
	}

	if (Kind == ENUQuestBattleKind::Dodge)
	{
		// 회피 연습에서는 패링을 막고 회피 반응만 허용한다.
		Context.bReactable = true;
		Context.bParryable = false;
		return;
	}

	if (IsCalming())
	{
		// New Lv.1~10 퀘스트 라인의 공통 규칙에 따라 Q8 공격은 전부 패링 가능하다.
		Context.bReactable = true;
		Context.bParryable = true;
	}
}

bool FNUQuestTutorialBattleRules::InterceptDamage(bool bEnemyTarget, float TargetHealth, float& Damage)
{
	if (bEnemyTarget && (IsTraining() || IsCalming()))
	{
		// 연습·진정 상대의 HP를 보호한다. 공격 피해로 동요를 줄이지 않는다.
		Damage = 0.f;
		return true;
	}

	if (IsTraining())
	{
		// Q3은 플레이어 HP를 1 이상 보존하고 Q5의 공격 피해는 1로 고정한다.
		Damage = Kind == ENUQuestBattleKind::BasicAttack ? FMath::Min(Damage, FMath::Max(0.f, TargetHealth - 1.f)) : 1.f;
	}

	return false;
}

bool FNUQuestTutorialBattleRules::ShouldRestorePlayerHealthOnHit(bool bEnemyActor) const
{
	return bEnemyActor && (Kind == ENUQuestBattleKind::Parry || Kind == ENUQuestBattleKind::Dodge);
}

void FNUQuestTutorialBattleRules::NotifyPlayerAbilityHit(FGameplayTag AbilityId)
{
	if (Kind == ENUQuestBattleKind::BasicAttack && AbilityId == NU::Ability::Battle::BasicAttack && LastBasicAttackCommandTurn != TurnNumber)
	{
		// 기본 공격의 반복 타격은 같은 내부 턴을 공유하므로 명령 하나로 한 번만 센다.
		LastBasicAttackCommandTurn = TurnNumber;
		bSucceeded = ++BasicAttackCommandCount >= RequiredBasicAttackCommands;
	}
}

void FNUQuestTutorialBattleRules::NotifyPlayerReaction(bool bParry)
{
	if ((bParry && Kind == ENUQuestBattleKind::Parry) || (!bParry && Kind == ENUQuestBattleKind::Dodge))
	{
		bSucceeded = ++ReactionCount >= RequiredReactions;
	}

	// 패링의 동요 감소는 바람 기믹에만 적용한다. 회피 감소는 기존 규칙을 유지한다.
	if (IsCalming() && (!bParry || Rule == ENUQuestCalmingRule::Wind))
	{
		const float Multiplier = bParry ? WindParryCalmMultiplier : 1.f;
		ReduceAgitation(ReactionCalmAmount * Multiplier);
	}
}

ENUQuestCalmResult FNUQuestTutorialBattleRules::ApplyCalm()
{
	if (!IsCalming())
	{
		return ENUQuestCalmResult::None;
	}

	if (IsEnemyBurrowed())
	{
		// 달래기는 실패한다. 동요는 유지하고 이 입력에 대해서만 출현 여부를 추첨한다.
		if (FMath::FRand() < EarthEmergeChanceAfterFailedCalm) { bEnemyBurrowed = false; }
		return ENUQuestCalmResult::Failed;
	}

	const bool bRepeated = Rule == ENUQuestCalmingRule::Fire && LastCalmTurn == TurnNumber - 1;
	LastCalmTurn = TurnNumber;
	ReduceAgitation(bRepeated ? -RepeatedCalmPenalty : CalmAmount);

	// 물 안개는 동요 수치와 분리해 달래기 성공만 반영한다.
	if (Rule == ENUQuestCalmingRule::Water)
	{
		MistRatio = FMath::Max(0.f, MistRatio - CalmAmount / InitialAgitation);
	}

	return bRepeated ? ENUQuestCalmResult::Backfired : ENUQuestCalmResult::Calmed;
}

float FNUQuestTutorialBattleRules::AdjustSpeed(bool bEnemy, float Speed) const
{
	if (!IsCalming()) { return Speed; }
	if (Rule == ENUQuestCalmingRule::Wind && !bEnemy) { return Speed * WindPlayerSpeedMultiplier; }
	return Speed;
}

bool FNUQuestTutorialBattleRules::CanMoveStone() const { return IsEnemyBurrowed() && !bSucceeded && StoneTurn != TurnNumber; }

bool FNUQuestTutorialBattleRules::MoveStone()
{
	if (!CanMoveStone()) { return false; }
	StoneTurn = TurnNumber;
	bEnemyBurrowed = false;
	ReduceAgitation(EarthStoneCalmAmount);
	return true;
}

void FNUQuestTutorialBattleRules::ReduceAgitation(float Amount)
{
	Agitation = FMath::Clamp(Agitation - Amount, 0.f, InitialAgitation);
	bSucceeded = Agitation <= KINDA_SMALL_NUMBER && (!bRequiresEarthStone || StoneTurn != INDEX_NONE);
	UE_LOG(LogNUQuest, Log, TEXT("진정 전투 Location=%d Agitation=%.1f"), LocationId, Agitation);
}
