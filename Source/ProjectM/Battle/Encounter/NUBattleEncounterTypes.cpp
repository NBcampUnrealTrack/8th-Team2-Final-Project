#include "Battle/Encounter/NUBattleEncounterTypes.h"

#include "CharacterState/NUPlayerState.h"

bool FNUBattleEncounterRequest::IsValid() const
{
	if (!::IsValid(Initiator.Get()))
	{
		return false;
	}

	if (Enemies.IsEmpty())
	{
		return false;
	}

	for (const FNUEnemySpawnSpec& EnemySpec : Enemies)
	{
		if (!EnemySpec.IsValid())
		{
			return false;
		}
	}

	switch (Source)
	{
	case ENUBattleEncounterSource::Symbol:
		return ::IsValid(SourceActor.Get());

	case ENUBattleEncounterSource::Random:
		return SourceActor == nullptr;

	default:
		return false;
	}
}