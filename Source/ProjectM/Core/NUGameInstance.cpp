#include "Core/NUGameInstance.h"

void UNUGameInstance::MarkFieldEnemyDefeated(FName EnemyGuid)
{
	DefeatedFieldEnemies.AddUnique(EnemyGuid);
}
