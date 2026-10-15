#include "Quest/Data/NUQuestBattleDefinition.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Quest/Integration/NUQuestBattleEventBridgeSubsystem.h"
#include "Quest/Encounter/NUQuestTutorialEncounter.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestTutorialTags.h"
#include "Quest/Data/NUQuestProgressTypes.h"

#include "Engine/GameInstance.h"

bool NUQuestBattle::IsPracticeKind(ENUQuestBattleKind Kind)
{
	return Kind == ENUQuestBattleKind::BasicAttack || Kind == ENUQuestBattleKind::Parry || Kind == ENUQuestBattleKind::Dodge;
}

bool NUQuestBattle::IsTutorialKind(ENUQuestBattleKind Kind)
{
	return IsPracticeKind(Kind) || Kind == ENUQuestBattleKind::Calming;
}

int32 NUQuestBattle::ToServerBattleMode(ENUQuestBattleKind Kind)
{
	switch (Kind)
	{
	case ENUQuestBattleKind::BasicAttack:	return 1;
	case ENUQuestBattleKind::Parry:			return 2;
	case ENUQuestBattleKind::Dodge:			return 3;
	case ENUQuestBattleKind::Calming:		return 4;
	case ENUQuestBattleKind::Sparring:		return 5;
	default:								return 0;
	}
}

ENUQuestBattleKind NUQuestBattle::FromServerBattleMode(int32 BattleMode)
{
	switch (BattleMode)
	{
	case 1:		return ENUQuestBattleKind::BasicAttack;
	case 2:		return ENUQuestBattleKind::Parry;
	case 3:		return ENUQuestBattleKind::Dodge;
	case 4:		return ENUQuestBattleKind::Calming;
	case 5:		return ENUQuestBattleKind::Sparring;
	default:	return ENUQuestBattleKind::None;
	}
}

int32 UNUQuestBattleDefinition::GetCompletedGrassRecoveries(const UNUQuestDefinition& Quest, const FNUQuestProgressState& Progress) const
{
	int32 Count = 0;
	for (FName Id : GrassRecoveryObjectiveIds)
	{
		const FNUQuestObjectiveSpec* Objective = Quest.Objectives.FindByPredicate([Id](const FNUQuestObjectiveSpec& Item) { return Item.ObjectiveId == Id; });
		const FNUQuestObjectiveProgress* State = Progress.FindObjectiveProgress(Id);
		if (Objective != nullptr && State != nullptr && State->IsComplete(Objective->RequiredCount)) { ++Count; }
	}
	return Count;
}

float UNUQuestBattleDefinition::GetGrassAgitation(int32 CompletedRecoveries) const
{
	// 물건 회수만으로 진정 성공이 되지 않도록 마지막 전투 몫을 남긴다.
	return FMath::Clamp(InitialAgitation - FMath::Clamp(CompletedRecoveries, 0, GrassRecoveryObjectiveIds.Num()) * FMath::Max(0.f, GrassRecoveryCalmAmount), FMath::Min(1.f, InitialAgitation), InitialAgitation);
}

FGameplayTag UNUQuestBattleDefinition::GetObjectiveType() const
{
	switch (Kind)
	{
	case ENUQuestBattleKind::BasicAttack:
	case ENUQuestBattleKind::Parry:
	case ENUQuestBattleKind::Dodge:
		return NU::Quest::Objective::TutorialAction;
	case ENUQuestBattleKind::Calming:
		return NU::Quest::Objective::NaiaCalmed;
	case ENUQuestBattleKind::Sparring:
		return NU::Quest::Objective::SparringWon;
	default:
		return FGameplayTag();
	}
}

TScriptInterface<INUBattleEventInterface> UNUQuestBattleDefinition::GetEventHandler(ANUBattleEncounter* Encounter)
{
	if (Encounter == nullptr || Encounter->GetNetMode() != NM_Standalone || Kind == ENUQuestBattleKind::None || (Kind == ENUQuestBattleKind::Calming && InitialAgitation <= 0.f)) { return {}; }
	// 연습·진정 정의가 일반 전투 액터로 실행되는 설정 오류를 거부한다. Q16은 기존 경로를 유지한다.
	if (Kind != ENUQuestBattleKind::Sparring && !Encounter->IsA<ANUQuestTutorialEncounter>()) { return {}; }

	TScriptInterface<INUBattleEventInterface> Handler;
	Handler.SetObject(Encounter->GetGameInstance()->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>());
	Handler.SetInterface(Cast<INUBattleEventInterface>(Handler.GetObject()));
	return Handler;
}
