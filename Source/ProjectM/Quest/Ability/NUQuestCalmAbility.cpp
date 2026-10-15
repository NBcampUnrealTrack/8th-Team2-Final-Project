#include "Quest/Ability/NUQuestCalmAbility.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Quest/Encounter/NUQuestBattleSession.h"
#include "Quest/Integration/NUQuestBattleEventBridgeSubsystem.h"

#include "Engine/GameInstance.h"

namespace NU::Quest::Ability
{
	UE_DEFINE_GAMEPLAY_TAG(Calm, "NU.Ability.Battle.Quest.Calm");
}

UNUQuestCalmAbility::UNUQuestCalmAbility()
{
	AbilityId = NU::Quest::Ability::Calm;
	DisplayName = NSLOCTEXT("NUQuest", "CalmAction", "달래기");
	SPCost = 1;
	TargetPolicy = ENUTargetPolicy::SingleEnemy;
	AbilityEffects.Reset();

	// 돌 옮기기와 같은 동작: 중앙까지 나가 도착하자마자 결과 큐(빛 가루)를 띄우고 돌아온다.
	Motion = ENUBattleActionMotion::MoveToCenter;
	HitTimeNormalized = 0.15f;
}

void UNUQuestCalmAbility::BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const
{
	Super::BuildActionMotionSpec(OutSpec);
	OutSpec.bSkipAttackPresentation = true;
}

// SP 지불과 행동 실행을 거친 뒤 전용 규칙에 실제 효과를 전달한다.
void UNUQuestCalmAbility::ApplySelfRuntimeEffects()
{
	if (ANUBattleEncounter* Encounter = GetBattleEncounter())
	{
		if (UNUQuestBattleEventBridgeSubsystem* Bridge = Encounter->GetGameInstance()->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>())
		{
			if (UNUQuestBattleSession* Session = Bridge->FindSession(Encounter)) { Session->ApplyCalm(); }
		}
	}
}
