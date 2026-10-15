#include "Quest/Integration/NUQuestBattleEventBridgeSubsystem.h"

#include "Battle/Event/NUBattleEventSubsystem.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/NUGameplayTags.h"
#include "Quest/Ability/NUQuestCalmAbility.h"
#include "Quest/Ability/NUQuestMoveStoneAbility.h"
#include "Quest/Ability/NUQuestWaitAbility.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Encounter/NUQuestBattleSession.h"
#include "Quest/Data/NUQuestEvent.h"

#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
	// 전투 참가자별로 완료 가능한 목표에만 진행 이벤트를 전달한다.
	void DispatchQuestEvent(const TArray<TWeakObjectPtr<APlayerState>>& EligiblePlayerStates, const FGameplayTag& ObjectiveType, FName TargetId)
	{
		FNUQuestEvent QuestEvent;
		QuestEvent.ObjectiveType = ObjectiveType;
		QuestEvent.TargetId = TargetId;
		QuestEvent.Amount = 1;

		for (const TWeakObjectPtr<APlayerState>& PlayerStatePtr : EligiblePlayerStates)
		{
			ANUPlayerState* PlayerState = Cast<ANUPlayerState>(PlayerStatePtr.Get());
			UNUQuestComponent* QuestComponent = IsValid(PlayerState)
				? PlayerState->GetQuestComponent()
				: nullptr;

			// 해당 플레이어에게 유효하지 않은 이벤트면 다음 참가자를 확인한다.
			if (!IsValid(QuestComponent) || !QuestComponent->HandleQuestEvent(QuestEvent))
			{
				continue;
			}

			UE_LOG(LogNUQuest, Log, TEXT("Quest BattleBridge: Player=%s Type=%s TargetId=%s"),
				*PlayerState->GetPlayerName(),
				*ObjectiveType.ToString(),
				*TargetId.ToString());
		}
	}
}

void UNUQuestBattleEventBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UNUBattleEventSubsystem>();
	Super::Initialize(Collection);

	if (UNUBattleEventSubsystem* BattleEvents = GetGameInstance()->GetSubsystem<UNUBattleEventSubsystem>())
	{
		EnemyDefeatedHandle = BattleEvents->OnEnemyDefeated.AddUObject(this, &ThisClass::HandleEnemyDefeated);
		NaiaCapturedHandle = BattleEvents->OnNaiaCaptured.AddUObject(this, &ThisClass::HandleNaiaCaptured);
		BattleWonHandle = BattleEvents->OnBattleWon.AddUObject(this, &ThisClass::HandleBattleWon);
	}
}

void UNUQuestBattleEventBridgeSubsystem::Deinitialize()
{
	if (UNUBattleEventSubsystem* BattleEvents = GetGameInstance()->GetSubsystem<UNUBattleEventSubsystem>())
	{
		BattleEvents->OnEnemyDefeated.Remove(EnemyDefeatedHandle);
		BattleEvents->OnNaiaCaptured.Remove(NaiaCapturedHandle);
		BattleEvents->OnBattleWon.Remove(BattleWonHandle);
	}

	EnemyDefeatedHandle.Reset();
	NaiaCapturedHandle.Reset();
	BattleWonHandle.Reset();

	for (UNUQuestBattleSession* Session : Sessions)
	{
		if (Session != nullptr)
		{
			Session->Shutdown();
		}
	}
	Sessions.Reset();
	Super::Deinitialize();
}

// 일반 전투 결과를 퀘스트 목표 이벤트로 변환한다.
void UNUQuestBattleEventBridgeSubsystem::HandleEnemyDefeated(const FNUBattleEnemyDefeatedEvent& Event)
{
	DispatchQuestEvent(Event.EligiblePlayerStates, NU::Quest::Objective::EnemyDefeated, Event.EnemyId);
}

void UNUQuestBattleEventBridgeSubsystem::HandleNaiaCaptured(const FNUBattleNaiaCapturedEvent& Event)
{
	DispatchQuestEvent(Event.EligiblePlayerStates, NU::Quest::Objective::GrowNaia, Event.NaiaId);
}

void UNUQuestBattleEventBridgeSubsystem::HandleBattleWon(const FNUBattleWonEvent& Event)
{
	DispatchQuestEvent(Event.EligiblePlayerStates, NU::Quest::Objective::BattleWon, Event.EnemyId);
}

// 연결·상태 조회: 현재 인카운터에 연결된 세션만 사용한다.
UNUQuestBattleSession* UNUQuestBattleEventBridgeSubsystem::FindSession(const ANUBattleEncounter* Encounter) const
{
	if (!IsValid(Encounter))
	{
		return nullptr;
	}

	for (UNUQuestBattleSession* Session : Sessions)
	{
		if (Session != nullptr && Session->IsRunning() && Session->GetEncounter() == Encounter)
		{
			return Session;
		}
	}
	return nullptr;
}

UNUQuestBattleSession* UNUQuestBattleEventBridgeSubsystem::FindSessionForQuest(const UNUQuestComponent* QuestComponent) const
{
	for (UNUQuestBattleSession* Session : Sessions)
	{
		if (QuestComponent != nullptr && Session != nullptr && Session->IsRunning() && Session->GetQuestComponent() == QuestComponent)
		{
			return Session;
		}
	}
	return nullptr;
}

void UNUQuestBattleEventBridgeSubsystem::EndSessionsForQuest(const UNUQuestComponent* QuestComponent)
{
	for (UNUQuestBattleSession* Session : Sessions)
	{
		if (Session != nullptr && Session->GetQuestComponent() == QuestComponent)
		{
			Session->Shutdown();
		}
	}
	PruneSessions();
}

void UNUQuestBattleEventBridgeSubsystem::PruneSessions()
{
	Sessions.RemoveAll([](const UNUQuestBattleSession* Session) { return Session == nullptr || !Session->IsRunning(); });
}

bool UNUQuestBattleEventBridgeSubsystem::PrepareParticipants(ANUBattleEncounter* Encounter)
{
	if (!IsValid(Encounter) || Encounter->GetNetMode() != NM_Standalone)
	{
		return false;
	}

	// 현재 외부 서버 연동 전의 로컬 단독 플레이 경로다. 파티 공용 상태로 확대하지 않는다.
	const FNUBattleParticipant* Player = Encounter->FindFirstParticipant(ENUBattleTeam::Ally);
	ANUPlayerState* State = Player != nullptr ? Cast<ANUPlayerState>(Player->OwningPlayerState) : nullptr;
	UNUQuestComponent* Quest = State != nullptr ? State->GetQuestComponent() : nullptr;
	UNUQuestBattleDefinition* Definition = Cast<UNUQuestBattleDefinition>(Encounter->GetBattleDefinition());
	if (Quest == nullptr)
	{
		return false;
	}

	// 한 플레이어는 퀘스트 전투 하나에만 연결한다.
	PruneSessions();
	if (const UNUQuestBattleSession* Existing = FindSessionForQuest(Quest))
	{
		return Existing->GetEncounter() == Encounter && Existing->GetDefinition() == Definition;
	}

	UNUQuestBattleSession* Session = NewObject<UNUQuestBattleSession>(this);
	if (!Session->Initialize(Encounter, Definition, Quest))
	{
		return false;
	}
	Sessions.Add(Session);
	return true;
}

void UNUQuestBattleEventBridgeSubsystem::BeginTurn(ANUBattleEncounter* Encounter, int32 TurnNumber)
{
	if (UNUQuestBattleSession* Session = FindSession(Encounter))
	{
		Session->BeginTurn(TurnNumber);
	}
}

// 턴·행동 요청은 세션의 전투 규칙으로 전달한다.
void UNUQuestBattleEventBridgeSubsystem::AdjustActionContext(ANUBattleEncounter* Encounter, const FNUBattleCommandRequest& Command, FNUBattleActionContext& Context) const
{
	if (const UNUQuestBattleSession* Session = FindSession(Encounter))
	{
		Session->AdjustAction(Command, Context);
	}
}

float UNUQuestBattleEventBridgeSubsystem::AdjustSpeed(ANUBattleEncounter* Encounter, bool bEnemy, float Speed) const
{
	const UNUQuestBattleSession* Session = FindSession(Encounter);
	return Session != nullptr ? Session->AdjustSpeed(bEnemy, Speed) : Speed;
}

bool UNUQuestBattleEventBridgeSubsystem::InterceptDamage(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Target, float& Damage)
{
	UNUQuestBattleSession* Session = FindSession(Encounter);
	return Session != nullptr && Session->InterceptDamage(Target, Damage);
}

void UNUQuestBattleEventBridgeSubsystem::NotifyAbilityHit(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Actor, FGameplayTag AbilityId)
{
	if (UNUQuestBattleSession* Session = FindSession(Encounter))
	{
		Session->NotifyHit(Actor, AbilityId);
	}
}

void UNUQuestBattleEventBridgeSubsystem::NotifyReaction(ANUBattleEncounter* Encounter, UNUAbilitySystemComponent* Actor, bool bParry)
{
	if (UNUQuestBattleSession* Session = FindSession(Encounter))
	{
		Session->NotifyReaction(Actor, bParry);
	}
}

// 목표 성공 판정과 전투 제한은 일반 승리 보상과 분리한다.
ENUBattleResult UNUQuestBattleEventBridgeSubsystem::GetForcedResult(ANUBattleEncounter* Encounter) const
{
	const UNUQuestBattleSession* Session = FindSession(Encounter);
	return Session != nullptr && Session->HasSucceeded() ? ENUBattleResult::Victory : ENUBattleResult::None;
}

FGameplayTag UNUQuestBattleEventBridgeSubsystem::GetAttackAbilityOverride(ANUBattleEncounter* Encounter, int32 CombatantId) const
{
	const UNUQuestBattleSession* Session = FindSession(Encounter);
	return Session != nullptr && Session->IsCalming() && Encounter->IsPlayerCombatant(CombatantId)
		? NU::Quest::Ability::Calm : FGameplayTag();
}

FGameplayTag UNUQuestBattleEventBridgeSubsystem::GetSkillAbilityOverride(ANUBattleEncounter* Encounter, int32 CombatantId) const
{
	const UNUQuestBattleSession* Session = FindSession(Encounter);
	const UNUQuestBattleDefinition* Definition = Session != nullptr ? Session->GetDefinition() : nullptr;
	if (Session == nullptr || !Session->IsCalming() || Definition == nullptr || !Encounter->IsPlayerCombatant(CombatantId))
	{
		return {};
	}

	return Definition->Rule == ENUQuestCalmingRule::Earth ? NU::Quest::Ability::MoveStone : NU::Quest::Ability::Wait;
}

bool UNUQuestBattleEventBridgeSubsystem::IsAbilityAllowed(ANUBattleEncounter* Encounter, FGameplayTag AbilityId) const
{
	const UNUQuestBattleSession* Session = FindSession(Encounter);
	return Session != nullptr && Session->IsAbilityAllowed(AbilityId);
}

bool UNUQuestBattleEventBridgeSubsystem::IsCaptureAllowed(const ANUBattleEncounter* Encounter) const { return false; }

// 진정 상대는 HP 바를 숨기고 퀘스트가 별도 동요 위젯을 소유한다.
bool UNUQuestBattleEventBridgeSubsystem::ShouldHideVitalBar(const ANUBattleEncounter* Encounter, ENUBattleTeam Team) const
{
	const UNUQuestBattleDefinition* Definition = Encounter != nullptr ? Cast<UNUQuestBattleDefinition>(Encounter->GetBattleDefinition()) : nullptr;
	return Team == ENUBattleTeam::Enemy && Definition != nullptr && Definition->Kind == ENUQuestBattleKind::Calming;
}

bool UNUQuestBattleEventBridgeSubsystem::ShouldGrantStandardRewards(const ANUBattleEncounter* Encounter) const { return false; }

bool UNUQuestBattleEventBridgeSubsystem::ShouldPublishStandardVictory(const ANUBattleEncounter* Encounter) const { return false; }
