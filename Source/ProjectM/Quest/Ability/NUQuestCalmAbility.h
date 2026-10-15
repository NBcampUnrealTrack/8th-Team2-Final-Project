#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "NUGAS/Ability/NUPlayerBattleAbility.h"
#include "NUQuestCalmAbility.generated.h"

namespace NU::Quest::Ability
{
	PROJECTM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Calm);
}

// 진정 전투 동안만 부여되는 달래기 행동
// 기존 플레이어 전투 어빌리티의 SP 비용/실행 흐름을 사용하고 동요 변경만 플레이어 QuestComponent에 위임
UCLASS()
class PROJECTM_API UNUQuestCalmAbility : public UNUPlayerBattleAbility
{
	GENERATED_BODY()

public:
	UNUQuestCalmAbility();

protected:
	// 전투 실행기가 행동을 실행했을 때만 QuestComponent에 효과를 요청한다.
	virtual void ApplySelfRuntimeEffects() override;

	// 공격이 아니므로 전투 공격 모션과 직업 공격 연출 없이 앞으로만 나간다.
	virtual void BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const override;
};
