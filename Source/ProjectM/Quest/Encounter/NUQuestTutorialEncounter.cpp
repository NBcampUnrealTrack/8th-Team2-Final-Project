#include "Quest/Encounter/NUQuestTutorialEncounter.h"

#include "Battle/Enemy/NUEnemyPatternSelector.h"
#include "NUGAS/NUGameplayTags.h"
#include "Quest/Ability/NUQuestBurrowAbility.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Encounter/NUQuestBattleSession.h"
#include "Quest/Integration/NUQuestBattleEventBridgeSubsystem.h"

#include "Engine/GameInstance.h"

bool ANUQuestTutorialEncounter::ConfigureBattleDefinition(UNUBattleDefinition* Definition)
{
	const UNUQuestBattleDefinition* Tutorial = Cast<UNUQuestBattleDefinition>(Definition);
	// 연습·진정 전용 클래스에 일반 대련 데이터가 들어오지 않게 한다.
	if (Tutorial == nullptr || !NUQuestBattle::IsTutorialKind(Tutorial->Kind))
	{
		return false;
	}

	if (!Super::ConfigureBattleDefinition(Definition)) { return false; }
	bEnemyOnlyPractice = Tutorial->Kind == ENUQuestBattleKind::Parry || Tutorial->Kind == ENUQuestBattleKind::Dodge;
	// Q3·Q8은 직접 입력을 기다린다. 시간초과 기본 공격이 연습·진정을 대신하지 않는다.
	MaxCommandSeconds = Tutorial->Kind == ENUQuestBattleKind::BasicAttack || Tutorial->Kind == ENUQuestBattleKind::Calming
		? 0.f : GetClass()->GetDefaultObject<ANUQuestTutorialEncounter>()->MaxCommandSeconds;
	return true;
}

bool ANUQuestTutorialEncounter::ShouldWaitForPlayerCommands() const { return !bEnemyOnlyPractice; }

void ANUQuestTutorialEncounter::GenerateEnemyCommands()
{
	const UNUQuestBattleDefinition* Definition = Cast<UNUQuestBattleDefinition>(GetBattleDefinition());
	if (Definition != nullptr && Definition->Kind == ENUQuestBattleKind::BasicAttack)
	{
		// Q3는 공격 입력만 연습하므로 적 행동을 생성하지 않는다.
		return;
	}

	const UNUQuestBattleEventBridgeSubsystem* Bridge = GetGameInstance()->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>();
	const UNUQuestBattleSession* Session = Bridge != nullptr ? Bridge->FindSession(this) : nullptr;
	// 이미 숨은 턴에는 공격도 재매몰도 예약하지 않는다. 플레이어 행동으로 나와도 그 턴에는 공격하지 않는다.
	if (Session != nullptr && Session->IsEnemyBurrowed()) { return; }
	Super::GenerateEnemyCommands();
}

void ANUQuestTutorialEncounter::SortActionQueue()
{
	Super::SortActionQueue();

	const UNUQuestBattleDefinition* Definition = Cast<UNUQuestBattleDefinition>(GetBattleDefinition());
	if (Definition != nullptr && Definition->Kind == ENUQuestBattleKind::Calming && Definition->Rule == ENUQuestCalmingRule::Lightning)
	{
		// 전투원 기본 속도와 관계없이 매 턴 순서를 한 번 추첨한다.
		for (int32 Index = ActionQueue.Num() - 1; Index > 0; --Index)
		{
			ActionQueue.Swap(Index, FMath::RandRange(0, Index));
		}

		return;
	}

	// 매몰은 속도·선제·둔화·도주 우선도와 무관하게 선행한다. 나머지 순서는 유지한다.
	ActionQueue.StableSort([](const FNUResolvedAction& A, const FNUResolvedAction& B)
	{
		return A.Command.AbilityId == NU::Quest::Ability::Burrow && B.Command.AbilityId != NU::Quest::Ability::Burrow;
	});
}

FNUEnemySelectedAction ANUQuestTutorialEncounter::SelectEnemyAbility(const FNUBattleParticipant& Participant)
{
	const UNUQuestBattleEventBridgeSubsystem* Bridge = GetGameInstance()->GetSubsystem<UNUQuestBattleEventBridgeSubsystem>();
	const UNUQuestBattleSession* Session = Bridge != nullptr ? Bridge->FindSession(this) : nullptr;
	if (Session != nullptr && Session->ShouldEnemyBurrow())
	{
		FNUEnemySelectedAction Action;
		Action.AbilityId = NU::Quest::Ability::Burrow;
		return Action;
	}
	const UNUQuestBattleDefinition* Definition = Cast<UNUQuestBattleDefinition>(GetBattleDefinition());
	if (Definition != nullptr && (Definition->Kind == ENUQuestBattleKind::Parry || Definition->Kind == ENUQuestBattleKind::Dodge))
	{
		// Q5는 나이아별 패턴을 쓰지 않고 반응 연습용 기본 공격만 사용한다.
		FNUEnemySelectedAction Action;
		Action.AbilityId = NU::Ability::Battle::BasicAttack;
		return Action;
	}

	return Super::SelectEnemyAbility(Participant);
}
