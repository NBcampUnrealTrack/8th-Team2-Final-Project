#include "CharacterProgression/Data/NUStatTuning.h"

#include "CharacterProgression/Data/NUProgressionDataSettings.h"

#include "Engine/DataTable.h"

namespace
{
	constexpr int32 BaseStatId = 0;
	constexpr int32 StrengthStatId = 1;
	constexpr int32 DexterityStatId = 2;
	constexpr int32 IntelligenceStatId = 3;
	constexpr int32 LuckStatId = 4;
	constexpr int32 VitalityStatId = 5;

	// Primary Stat 하나가 Secondary Stat에 주는 영향을 Out에 누적한다.
	// 음수 검증은 컴포넌트에서 처리
	void AccumulateSecondaryStats(FNUSecondaryStats& Out, const FNUSecondaryStats& PerPoint, int32 StatValue)
	{
		if (StatValue == 0)
		{
			return;
		}

		const float Count = static_cast<float>(StatValue);

		Out.MaxHealth        += PerPoint.MaxHealth        * Count;
		Out.MaxSP            += PerPoint.MaxSP            * Count;
		Out.AttackPower      += PerPoint.AttackPower      * Count;
		Out.MagicAttackPower += PerPoint.MagicAttackPower * Count;
		Out.Defense          += PerPoint.Defense          * Count;
		Out.Speed            += PerPoint.Speed            * Count;
		Out.CriticalRate     += PerPoint.CriticalRate     * Count;
		Out.CriticalDamage   += PerPoint.CriticalDamage   * Count;
	}
}

FNUStatTuning::FNUStatTuning()
{
	// Project Settings에 등록된 스탯 테이블 조회
	const UNUProgressionDataSettings* Settings = GetDefault<UNUProgressionDataSettings>();
	if (Settings == nullptr || Settings->StatProgressionTable.IsNull())
	{
		return;
	}

	// 게임에서 계속 사용할 고정 데이터라 시작할 때 한 번 로드
	const UDataTable* Table = Settings->StatProgressionTable.LoadSynchronous();
	if (Table == nullptr || Table->GetRowStruct() != FNUStatProgressionRow::StaticStruct())
	{
		return;
	}

	TArray<FNUStatProgressionRow*> Rows;
	Table->GetAllRows(TEXT("FNUStatTuning"), Rows);

	// StatId 0~5가 중복 없이 모두 있는지 확인하면서 각 위치에 저장
	TSet<int32> LoadedIds;
	for (const FNUStatProgressionRow* Row : Rows)
	{
		if (Row == nullptr || LoadedIds.Contains(Row->StatId))
		{
			return;
		}

		const FNUSecondaryStats Stats = Row->ToSecondaryStats();
		switch (Row->StatId)
		{
		case BaseStatId:
			BaseSecondaryStats = Stats;
			break;
		case StrengthStatId:
			PerStrength = Stats;
			break;
		case DexterityStatId:
			PerDexterity = Stats;
			break;
		case IntelligenceStatId:
			PerIntelligence = Stats;
			break;
		case LuckStatId:
			PerLuck = Stats;
			break;
		case VitalityStatId:
			PerVitality = Stats;
			break;
		default:
			return;
		}

		LoadedIds.Add(Row->StatId);
	}

	// 기본값 1개와 Primary Stat 5개가 모두 있어야 사용 가능
	bLoadedFromTable = LoadedIds.Num() == 6;
}

// Primary Stat을 입력받아 예상 Secondary Stat 8개를 계산한다.
FNUSecondaryStats FNUStatTuning::CalculateSecondaryStats(const FNUPrimaryStats& Primary) const
{
	FNUSecondaryStats Result = BaseSecondaryStats;

	AccumulateSecondaryStats(Result, PerStrength,     Primary.Strength);
	AccumulateSecondaryStats(Result, PerDexterity,    Primary.Dexterity);
	AccumulateSecondaryStats(Result, PerIntelligence, Primary.Intelligence);
	AccumulateSecondaryStats(Result, PerLuck,         Primary.Luck);
	AccumulateSecondaryStats(Result, PerVitality,     Primary.Vitality);

	// 기획상 100% 초과분은 치명타 피해로 전환되므로 자르는 위치를 결정해야 됨
	// 현재 UNUCombatAttributeSet::ClampAttribute 가 1.0에서 잘라 초과분이 사라지고 있음
	return Result;
}

bool FNUStatTuning::Validate(FString& OutError) const
{
	if (!bLoadedFromTable)
	{
		OutError = TEXT("스탯 튜닝 테이블을 불러오지 못했거나 행 구성이 잘못됨");
		return false;
	}

	const auto ValidateSecondaryStats = [&OutError](const TCHAR* Label, const FNUSecondaryStats& Stats)
	{
		const float Values[] =
		{
			Stats.MaxHealth, Stats.MaxSP, Stats.AttackPower, Stats.MagicAttackPower,
			Stats.Defense, Stats.Speed, Stats.CriticalRate, Stats.CriticalDamage
		};

		for (const float Value : Values)
		{
			if (!FMath::IsFinite(Value) || Value < 0.f)
			{
				OutError = FString::Printf(TEXT("%s에 음수 또는 비유한 수치가 있음"), Label);
				return false;
			}
		}

		return true;
	};

	if (!ValidateSecondaryStats(TEXT("기본 2차 스탯"), BaseSecondaryStats)
		|| !ValidateSecondaryStats(TEXT("힘 1당 증가량"), PerStrength)
		|| !ValidateSecondaryStats(TEXT("민첩 1당 증가량"), PerDexterity)
		|| !ValidateSecondaryStats(TEXT("지능 1당 증가량"), PerIntelligence)
		|| !ValidateSecondaryStats(TEXT("행운 1당 증가량"), PerLuck)
		|| !ValidateSecondaryStats(TEXT("활력 1당 증가량"), PerVitality))
	{
		return false;
	}

	if (BaseSecondaryStats.MaxHealth <= 0.f || BaseSecondaryStats.MaxSP <= 0.f)
	{
		OutError = TEXT("기본 최대 HP와 최대 SP는 0보다 커야 함");
		return false;
	}

	return true;
}

const FNUStatTuning& FNUStatTuning::Get()
{
	static const FNUStatTuning Tuning;
	return Tuning;
}
