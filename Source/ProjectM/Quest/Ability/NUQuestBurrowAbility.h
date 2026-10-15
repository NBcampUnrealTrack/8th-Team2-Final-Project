#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"
#include "NUQuestBurrowAbility.generated.h"

namespace NU::Quest::Ability
{
	PROJECTM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burrow);
}

// 땅 나이아가 공격 대신 수행하는 매몰 행동
UCLASS()
class PROJECTM_API UNUQuestBurrowAbility : public UNUBattleGameplayAbility
{
	GENERATED_BODY()
public:
	UNUQuestBurrowAbility();
protected:
	virtual void ApplySelfRuntimeEffects() override;
};
