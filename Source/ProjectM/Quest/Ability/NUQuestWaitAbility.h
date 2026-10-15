#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "NUGAS/Ability/NUPlayerBattleAbility.h"
#include "NUQuestWaitAbility.generated.h"

namespace NU::Quest::Ability
{
	PROJECTM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wait);
}

// 진정 전투에서 공격하지 않고 적 행동을 기다린다.
UCLASS()
class PROJECTM_API UNUQuestWaitAbility : public UNUPlayerBattleAbility
{
	GENERATED_BODY()

public:
	UNUQuestWaitAbility();
};
