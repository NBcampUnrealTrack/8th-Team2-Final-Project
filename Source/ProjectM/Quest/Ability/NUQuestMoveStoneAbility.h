#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "NUGAS/Ability/NUPlayerBattleAbility.h"
#include "NUQuestMoveStoneAbility.generated.h"

namespace NU::Quest::Ability
{
	PROJECTM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MoveStone);
}

// 땅 진정 전투에서 메뉴로 선택하는 돌 옮기기 행동
UCLASS()
class PROJECTM_API UNUQuestMoveStoneAbility : public UNUPlayerBattleAbility
{
	GENERATED_BODY()
public:
	UNUQuestMoveStoneAbility();
protected:
	virtual void ApplySelfRuntimeEffects() override;

	// 공격이 아니므로 전투 공격 모션과 직업 공격 연출 없이 앞으로만 나간다.
	virtual void BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const override;
};
