#include "Quest/Ability/NUQuestWaitAbility.h"

namespace NU::Quest::Ability
{
	UE_DEFINE_GAMEPLAY_TAG(Wait, "NU.Ability.Battle.Quest.Wait");
}

UNUQuestWaitAbility::UNUQuestWaitAbility()
{
	AbilityId = NU::Quest::Ability::Wait;
	DisplayName = NSLOCTEXT("NUQuest", "WaitAction", "기다리기");

	// 피해나 SP 소모 없이 행동 한 번만 소비한다.
	SPCost = 0;
	TargetPolicy = ENUTargetPolicy::Self;
	Motion = ENUBattleActionMotion::InPlace;
	AbilityEffects.Reset();
}
