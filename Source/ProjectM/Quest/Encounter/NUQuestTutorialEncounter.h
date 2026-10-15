#pragma once

#include "CoreMinimal.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "NUQuestTutorialEncounter.generated.h"

// 연습·진정 전투 등 튜토리얼 적의 행동을 소유한다.
UCLASS(NotPlaceable, Blueprintable)
class PROJECTM_API ANUQuestTutorialEncounter : public ANUBattleEncounter
{
	GENERATED_BODY()

public:
	virtual bool ConfigureBattleDefinition(UNUBattleDefinition* Definition) override;

protected:
	virtual bool ShouldWaitForPlayerCommands() const override;
	virtual void GenerateEnemyCommands() override;
	virtual void SortActionQueue() override;
	virtual FNUEnemySelectedAction SelectEnemyAbility(const FNUBattleParticipant& Participant) override;

private:
	bool bEnemyOnlyPractice = false;
};
