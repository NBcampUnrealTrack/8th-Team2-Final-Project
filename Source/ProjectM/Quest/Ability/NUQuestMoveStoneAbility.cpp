#include "Quest/Ability/NUQuestMoveStoneAbility.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Quest/Encounter/NUQuestBattleSession.h"
#include "Quest/Integration/NUQuestBattleEventBridgeSubsystem.h"

#include "Engine/GameInstance.h"

namespace NU::Quest::Ability
{
	UE_DEFINE_GAMEPLAY_TAG(MoveStone, "NU.Ability.Battle.Quest.MoveStone");
}

UNUQuestMoveStoneAbility::UNUQuestMoveStoneAbility()
{
	AbilityId = NU::Quest::Ability::MoveStone;
	DisplayName = NSLOCTEXT("NUQuest", "MoveStoneAction", "돌 옮기기");
	SPCost = 0;
	TargetPolicy = ENUTargetPolicy::Self;
	AbilityEffects.Reset();

	// 대상 선택 없이 중앙까지 나가서, 도착하면 돌이 밀리고 나이아가 나온다.
	Motion = ENUBattleActionMotion::MoveToCenter;
	HitTimeNormalized = 0.15f;
}

void UNUQuestMoveStoneAbility::BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const
{
	Super::BuildActionMotionSpec(OutSpec);
	OutSpec.bSkipAttackPresentation = true;
}

void UNUQuestMoveStoneAbility::ApplySelfRuntimeEffects()
{
	if (ANUBattleEncounter* Encounter = GetBattleEncounter())
	{
		if (UNUQuestBattleEventBridgeSubsystem* Bridge = Encounter->GetGameInstance()->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>())
		{
			if (UNUQuestBattleSession* Session = Bridge->FindSession(Encounter)) { Session->MoveStone(); }
		}
	}
}
