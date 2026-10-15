#include "CharacterProgression/Net/NUCharacterStatServerTypes.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool TryReadInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
	{
		double Number = 0.0;
		if (!Object.IsValid()
			|| !Object->TryGetNumberField(FieldName, Number)
			|| !FMath::IsFinite(Number)
			|| Number < static_cast<double>(MIN_int32)
			|| Number > static_cast<double>(MAX_int32)
			|| FMath::TruncToDouble(Number) != Number)
		{
			return false;
		}

		OutValue = static_cast<int32>(Number);
		return true;
	}

	bool TryReadFloat(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, float& OutValue)
	{
		double Number = 0.0;
		if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, Number) || !FMath::IsFinite(Number))
		{
			return false;
		}

		OutValue = static_cast<float>(Number);
		return FMath::IsFinite(OutValue);
	}

	bool TryReadInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue)
	{
		return Object.IsValid() && Object->TryGetNumberField(FieldName, OutValue);
	}

	template <typename ValueType>
	bool ReadRequired(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, ValueType& OutValue, FString& OutError, bool (*Reader)(const TSharedPtr<FJsonObject>&, const TCHAR*, ValueType&))
	{
		if (Reader(Object, FieldName, OutValue))
		{
			return true;
		}

		OutError = FString::Printf(TEXT("%s 필드가 없거나 숫자 형식이 올바르지 않습니다."), FieldName);
		return false;
	}
}

bool NUCharacterStatServer::ParseSnapshot(const FString& ResponseData, FNUCharacterStatSnapshot& OutSnapshot, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("응답 JSON을 읽지 못했습니다.");
		return false;
	}

	FNUCharacterStatSnapshot Parsed;
	// 서버 계약에 포함된 값은 하나라도 누락되면 불완전한 스냅샷으로 처리한다.
	if (!ReadRequired(Root, TEXT("Level"), Parsed.Level, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("TotalExperience"), Parsed.TotalExperience, OutError, TryReadInt64)
		|| !ReadRequired(Root, TEXT("ExperienceIntoLevel"), Parsed.ExperienceIntoLevel, OutError, TryReadInt64)
		|| !ReadRequired(Root, TEXT("RequiredExperienceToNext"), Parsed.RequiredExperienceToNext, OutError, TryReadInt64)
		|| !ReadRequired(Root, TEXT("MaxLevel"), Parsed.MaxLevel, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("Str"), Parsed.Strength, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("Dex"), Parsed.Dexterity, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("Int"), Parsed.Intelligence, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("Luk"), Parsed.Luck, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("Vit"), Parsed.Vitality, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("TotalPoint"), Parsed.TotalPoint, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("FreePoint"), Parsed.FreePoint, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("CapStr"), Parsed.StrengthCap, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("CapDex"), Parsed.DexterityCap, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("CapInt"), Parsed.IntelligenceCap, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("CapLuk"), Parsed.LuckCap, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("CapVit"), Parsed.VitalityCap, OutError, TryReadInt32)
		|| !ReadRequired(Root, TEXT("MaxHp"), Parsed.MaxHealth, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("MaxSp"), Parsed.MaxSP, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("Attack"), Parsed.AttackPower, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("MagicAttack"), Parsed.MagicAttackPower, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("Defense"), Parsed.Defense, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("Speed"), Parsed.Speed, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("CritRate"), Parsed.CriticalRate, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("CritDamage"), Parsed.CriticalDamage, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("ParryRate"), Parsed.ParryRate, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("DodgeRate"), Parsed.DodgeRate, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("DamageReduce"), Parsed.DamageReduce, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("HealBonus"), Parsed.HealBonus, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("EquipAttackRate"), Parsed.EquipAttackRate, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("EquipMagicAttackRate"), Parsed.EquipMagicAttackRate, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("EquipMaxHpRate"), Parsed.EquipMaxHealthRate, OutError, TryReadFloat)
		|| !ReadRequired(Root, TEXT("EquipDefenseFlat"), Parsed.EquipDefenseFlat, OutError, TryReadFloat))
	{
		return false;
	}

	// 숫자 형식을 통과한 뒤 성장 도메인에서 허용하지 않는 음수 값을 거부한다.
	const int64 AssignedPoints = static_cast<int64>(Parsed.Strength)
		+ Parsed.Dexterity
		+ Parsed.Intelligence
		+ Parsed.Luck
		+ Parsed.Vitality;
	FNUPrimaryStats PrimaryStats;
	PrimaryStats.Strength = Parsed.Strength;
	PrimaryStats.Dexterity = Parsed.Dexterity;
	PrimaryStats.Intelligence = Parsed.Intelligence;
	PrimaryStats.Luck = Parsed.Luck;
	PrimaryStats.Vitality = Parsed.Vitality;
	if (Parsed.Level < 1
		|| Parsed.MaxLevel < Parsed.Level
		|| Parsed.TotalExperience < 0
		|| Parsed.ExperienceIntoLevel < 0
		|| Parsed.RequiredExperienceToNext < 0
		|| !NUProgression::ArePrimaryStatsValid(PrimaryStats)
		|| Parsed.TotalPoint < 0
		|| Parsed.FreePoint < 0
		|| Parsed.StrengthCap < 0
		|| Parsed.DexterityCap < 0
		|| Parsed.IntelligenceCap < 0
		|| Parsed.LuckCap < 0
		|| Parsed.VitalityCap < 0
		|| Parsed.StrengthCap > NUProgression::MaxPrimaryStat
		|| Parsed.DexterityCap > NUProgression::MaxPrimaryStat
		|| Parsed.IntelligenceCap > NUProgression::MaxPrimaryStat
		|| Parsed.LuckCap > NUProgression::MaxPrimaryStat
		|| Parsed.VitalityCap > NUProgression::MaxPrimaryStat
		|| Parsed.Strength > Parsed.StrengthCap
		|| Parsed.Dexterity > Parsed.DexterityCap
		|| Parsed.Intelligence > Parsed.IntelligenceCap
		|| Parsed.Luck > Parsed.LuckCap
		|| Parsed.Vitality > Parsed.VitalityCap
		|| AssignedPoints + Parsed.FreePoint != Parsed.TotalPoint
		|| (Parsed.Level == Parsed.MaxLevel && Parsed.RequiredExperienceToNext != 0)
		|| (Parsed.Level < Parsed.MaxLevel && (Parsed.RequiredExperienceToNext <= 0 || Parsed.ExperienceIntoLevel >= Parsed.RequiredExperienceToNext)))
	{
		OutError = TEXT("레벨, 배분 스탯, 포인트 또는 상한 값이 유효 범위를 벗어났습니다.");
		return false;
	}

	OutSnapshot = Parsed;
	OutError.Reset();
	return true;
}
