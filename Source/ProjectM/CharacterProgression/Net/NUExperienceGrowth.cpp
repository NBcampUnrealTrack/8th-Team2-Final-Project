#include "CharacterProgression/Net/NUExperienceGrowth.h"

#include "Dom/JsonObject.h"

namespace
{
	bool TryReadGrowthInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
	{
		double Number = 0.0;
		if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, Number)
			|| Number < static_cast<double>(MIN_int32)
			|| Number > static_cast<double>(MAX_int32)
			|| FMath::TruncToDouble(Number) != Number)
		{
			return false;
		}

		OutValue = static_cast<int32>(Number);
		return true;
	}

	bool TryReadGrowthInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue)
	{
		double Number = 0.0;
		if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, Number)
			|| !FMath::IsFinite(Number)
			|| FMath::TruncToDouble(Number) != Number)
		{
			return false;
		}

		OutValue = static_cast<int64>(Number);
		return true;
	}
}

bool NUExperienceGrowthServer::Parse(const TSharedPtr<FJsonObject>& Object, FNUExperienceGrowth& OutGrowth, FString& OutError)
{
	FNUExperienceGrowth Parsed;
	if (!TryReadGrowthInt32(Object, TEXT("OldLv"), Parsed.OldLevel)
		|| !TryReadGrowthInt32(Object, TEXT("NewLv"), Parsed.NewLevel)
		|| !TryReadGrowthInt64(Object, TEXT("OldExp"), Parsed.OldTotalExperience)
		|| !TryReadGrowthInt64(Object, TEXT("NewExp"), Parsed.NewTotalExperience)
		|| !TryReadGrowthInt64(Object, TEXT("ExperienceIntoLevel"), Parsed.ExperienceIntoLevel)
		|| !TryReadGrowthInt64(Object, TEXT("RequiredExperienceToNext"), Parsed.RequiredExperienceToNext)
		|| !TryReadGrowthInt32(Object, TEXT("MaxLevel"), Parsed.MaxLevel))
	{
		OutError = TEXT("Growth 필드가 없거나 정수가 아닙니다.");
		return false;
	}

	if (Parsed.OldLevel < 1 || Parsed.NewLevel < Parsed.OldLevel || Parsed.MaxLevel < 1
		|| Parsed.OldLevel > Parsed.MaxLevel || Parsed.NewLevel > Parsed.MaxLevel
		|| Parsed.OldTotalExperience < 0 || Parsed.NewTotalExperience < Parsed.OldTotalExperience
		|| Parsed.ExperienceIntoLevel < 0 || Parsed.RequiredExperienceToNext < 0)
	{
		OutError = TEXT("Growth의 레벨 또는 경험치 범위가 올바르지 않습니다.");
		return false;
	}

	if ((Parsed.NewLevel == Parsed.MaxLevel
			&& (Parsed.ExperienceIntoLevel != 0 || Parsed.RequiredExperienceToNext != 0))
		|| (Parsed.NewLevel < Parsed.MaxLevel
			&& (Parsed.RequiredExperienceToNext <= 0 || Parsed.ExperienceIntoLevel >= Parsed.RequiredExperienceToNext)))
	{
		OutError = TEXT("Growth의 현재 경험치와 다음 레벨 요구치가 서로 모순됩니다.");
		return false;
	}

	OutGrowth = Parsed;
	return true;
}
