#include "CharacterProgression/Data/NUProgressionTypes.h"

DEFINE_LOG_CATEGORY(LogNUProgression);

bool NUProgression::ArePrimaryStatsValid(const FNUPrimaryStats& Stats)
{
	return Stats.Strength >= 0 && Stats.Strength <= MaxPrimaryStat
		&& Stats.Dexterity >= 0 && Stats.Dexterity <= MaxPrimaryStat
		&& Stats.Intelligence >= 0 && Stats.Intelligence <= MaxPrimaryStat
		&& Stats.Luck >= 0 && Stats.Luck <= MaxPrimaryStat
		&& Stats.Vitality >= 0 && Stats.Vitality <= MaxPrimaryStat
		&& (Stats.Strength == 0 || Stats.Intelligence == 0);
}
