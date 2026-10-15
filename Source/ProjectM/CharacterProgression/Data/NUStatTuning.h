#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"

// 스탯 테이블을 읽어 기본 Secondary Stat과 Primary Stat 1당 증가량을 제공한다.
struct PROJECTM_API FNUStatTuning
{
	FNUSecondaryStats BaseSecondaryStats;

	FNUSecondaryStats PerStrength;
	FNUSecondaryStats PerDexterity;
	FNUSecondaryStats PerIntelligence;
	FNUSecondaryStats PerLuck;
	FNUSecondaryStats PerVitality;

	FNUSecondaryStats CalculateSecondaryStats(const FNUPrimaryStats& Primary) const;
	bool Validate(FString& OutError) const;

	static const FNUStatTuning& Get();

private:
	FNUStatTuning();

	bool bLoadedFromTable = false;
};
