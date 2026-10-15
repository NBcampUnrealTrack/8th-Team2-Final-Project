#include "Quest/Ability/NUQuestBurrowAbility.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Quest/Encounter/NUQuestBattleSession.h"
#include "Quest/Integration/NUQuestBattleEventBridgeSubsystem.h"

#include "Engine/GameInstance.h"

namespace NU::Quest::Ability
{
	UE_DEFINE_GAMEPLAY_TAG(Burrow, "NU.Ability.Battle.Quest.Burrow");
}

UNUQuestBurrowAbility::UNUQuestBurrowAbility()
{
	AbilityId = NU::Quest::Ability::Burrow;
	DisplayName = NSLOCTEXT("NUQuest", "BurrowAction", "땅속으로 숨기");
	TargetPolicy = ENUTargetPolicy::Self;
	Motion = ENUBattleActionMotion::InPlace;
	ActionTime = 0.8f;
	TelegraphTime = 0.f;
	AbilityEffects.Reset();
}

void UNUQuestBurrowAbility::ApplySelfRuntimeEffects()
{
	if (ANUBattleEncounter* Encounter = GetBattleEncounter())
	{
		if (UNUQuestBattleEventBridgeSubsystem* Bridge = Encounter->GetGameInstance()->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>())
		{
			if (UNUQuestBattleSession* Session = Bridge->FindSession(Encounter)) { Session->ApplyBurrow(); }
		}
	}
}
