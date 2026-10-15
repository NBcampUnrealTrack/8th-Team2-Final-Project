#include "Battle/Event/NUBattleEventSubsystem.h"

void UNUBattleEventSubsystem::PublishEnemyDefeated(const FNUBattleEnemyDefeatedEvent& Event)
{
	OnEnemyDefeated.Broadcast(Event);
}

void UNUBattleEventSubsystem::PublishBattleWon(const FNUBattleWonEvent& Event)
{
	OnBattleWon.Broadcast(Event);
}

void UNUBattleEventSubsystem::PublishNaiaCaptured(const FNUBattleNaiaCapturedEvent& Event)
{
	OnNaiaCaptured.Broadcast(Event);
}
