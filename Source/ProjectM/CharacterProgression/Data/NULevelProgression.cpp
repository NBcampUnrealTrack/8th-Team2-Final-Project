#include "CharacterProgression/Data/NULevelProgression.h"

#include "CharacterProgression/Data/NUProgressionDataSettings.h"

#include "Engine/DataTable.h"

FNULevelProgression::FNULevelProgression()
{
	// Project Settings에 등록된 레벨 테이블을 가져옴
	const UNUProgressionDataSettings* Settings = GetDefault<UNUProgressionDataSettings>();
	if (Settings == nullptr || Settings->LevelProgressionTable.IsNull())
	{
		return;
	}

	// 게임 시작 시 한 번 읽어 계속 사용할 데이터로 보관
	const UDataTable* Table = Settings->LevelProgressionTable.LoadSynchronous();
	if (Table == nullptr || Table->GetRowStruct() != FNULevelProgressionRow::StaticStruct())
	{
		return;
	}

	// DataTable 행을 값으로 복사
	TArray<FNULevelProgressionRow*> Rows;
	Table->GetAllRows(TEXT("FNULevelProgression"), Rows);
	LevelRows.Reserve(Rows.Num());

	for (const FNULevelProgressionRow* Row : Rows)
	{
		if (Row != nullptr)
		{
			LevelRows.Add(*Row);
		}
	}

	// 현재 레벨을 배열 인덱스로 바꿔 조회하므로 레벨 순서로 정렬
	LevelRows.Sort([](const FNULevelProgressionRow& Left, const FNULevelProgressionRow& Right)
	{
		return Left.Level < Right.Level;
	});
}

int32 FNULevelProgression::GetMaxLevel() const
{
	return LevelRows.IsEmpty() ? 1 : LevelRows.Last().Level;
}

int64 FNULevelProgression::GetMaxTotalExperience() const
{
	return LevelRows.IsEmpty() ? 0 : LevelRows.Last().RequiredCumulativeExperience;
}

bool FNULevelProgression::TryGetRequiredExperienceToNext(int32 CurrentLevel, int64& OutRequiredExperience) const
{
	OutRequiredExperience = 0;

	if (LevelRows.IsEmpty() || CurrentLevel < LevelRows[0].Level)
	{
		return false;
	}

	// 레벨이 1부터 연속된다는 전제로 행 조회
	const int32 RowIndex = CurrentLevel - LevelRows[0].Level;
	if (!LevelRows.IsValidIndex(RowIndex) || LevelRows[RowIndex].Level != CurrentLevel)
	{
		return false;
	}

	OutRequiredExperience = LevelRows[RowIndex].RequiredExperienceToNext;
	return OutRequiredExperience > 0;
}

bool FNULevelProgression::TryGetRequiredCumulativeExperience(int32 TargetLevel, int64& OutCumulativeExperience) const
{
	OutCumulativeExperience = 0;

	if (LevelRows.IsEmpty() || TargetLevel < LevelRows[0].Level)
	{
		return false;
	}

	const int32 RowIndex = TargetLevel - LevelRows[0].Level;
	if (!LevelRows.IsValidIndex(RowIndex) || LevelRows[RowIndex].Level != TargetLevel)
	{
		return false;
	}

	OutCumulativeExperience = LevelRows[RowIndex].RequiredCumulativeExperience;
	return true;
}

bool FNULevelProgression::TryResolveExperienceState(int64 TotalExperience, int32& OutLevel, int64& OutExperienceIntoLevel) const
{
	OutLevel = 1;
	OutExperienceIntoLevel = 0;

	if (TotalExperience < 0 || LevelRows.IsEmpty())
	{
		return false;
	}

	// 최대 레벨 이후 경험치는 현재 기획 상한에서 멈춘다.
	if (TotalExperience >= GetMaxTotalExperience())
	{
		OutLevel = GetMaxLevel();
		return true;
	}

	// 총경험치 이하인 가장 높은 레벨 시작값을 찾는다.
	for (int32 RowIndex = LevelRows.Num() - 1; RowIndex >= 0; --RowIndex)
	{
		const FNULevelProgressionRow& Row = LevelRows[RowIndex];
		if (TotalExperience < Row.RequiredCumulativeExperience)
		{
			continue;
		}

		OutLevel = Row.Level;
		OutExperienceIntoLevel = TotalExperience - Row.RequiredCumulativeExperience;
		return true;
	}

	return false;
}

bool FNULevelProgression::ValidateExperienceState(const FNUProgressionState& State, FString& OutError) const
{
	if (State.Level < 1)
	{
		OutError = TEXT("레벨은 1 이상이어야 함");
		return false;
	}

	if (State.TotalExperience < 0)
	{
		OutError = TEXT("총경험치는 음수일 수 없음");
		return false;
	}

	if (State.ExperienceIntoLevel < 0)
	{
		OutError = TEXT("현재 레벨 경험치는 음수일 수 없음");
		return false;
	}

	if (!Validate(OutError))
	{
		return false;
	}

	const int32 MaxLevel = GetMaxLevel();
	const int64 MaxTotalExperience = GetMaxTotalExperience();
	if (State.Level > MaxLevel)
	{
		if (State.TotalExperience < MaxTotalExperience)
		{
			OutError = TEXT("로컬 최대 레벨보다 높은 상태의 총경험치가 현재 상한보다 작음");
			return false;
		}
		return true;
	}

	if (State.TotalExperience > MaxTotalExperience)
	{
		OutError = TEXT("현재 레벨 표의 최대 총경험치를 초과함");
		return false;
	}

	int32 ResolvedLevel = 1;
	int64 ResolvedExperienceIntoLevel = 0;
	if (!TryResolveExperienceState(
		State.TotalExperience,
		ResolvedLevel,
		ResolvedExperienceIntoLevel))
	{
		OutError = TEXT("총경험치에서 레벨 상태를 계산하지 못함");
		return false;
	}

	if (State.Level != ResolvedLevel
		|| State.ExperienceIntoLevel != ResolvedExperienceIntoLevel)
	{
		OutError = FString::Printf(
			TEXT("레벨 경험치 상태 불일치: 입력 Lv.%d Total=%lld Into=%lld, 계산 Lv.%d Into=%lld"),
			State.Level,
			State.TotalExperience,
			State.ExperienceIntoLevel,
			ResolvedLevel,
			ResolvedExperienceIntoLevel);
		return false;
	}

	return true;
}

bool FNULevelProgression::Validate(FString& OutError) const
{
	return ValidateRows(LevelRows, OutError);
}

bool FNULevelProgression::ValidateRows(const TArray<FNULevelProgressionRow>& Rows, FString& OutError)
{
	// 레벨 데이터가 없으면 경험치 계산을 할 수 없음
	if (Rows.IsEmpty())
	{
		OutError = TEXT("레벨 경험치 행이 비어 있음");
		return false;
	}

	// 레벨 1부터 중복이나 누락 없이 이어지는지 검사
	int32 ExpectedLevel = 1;
	int64 ExpectedTotalExperience = 0;
	for (const FNULevelProgressionRow& Row : Rows)
	{
		if (Row.Level != ExpectedLevel)
		{
			OutError = FString::Printf(
				TEXT("레벨 경험치 행 누락 또는 중복: 기대 레벨=%d, 실제 레벨=%d"),
				ExpectedLevel,
				Row.Level);
			return false;
		}

		if (Row.RequiredCumulativeExperience != ExpectedTotalExperience)
		{
			OutError = FString::Printf(
				TEXT("레벨 도달 총경험치가 연속되지 않음: 레벨=%d, 기대=%lld, 실제=%lld"),
				Row.Level,
				ExpectedTotalExperience,
				Row.RequiredCumulativeExperience);
			return false;
		}

		// 일반 레벨은 다음 요구 경험치가 필요하고 최대 레벨은 0이어야 함
		const bool bIsMaxLevelRow = Row.Level == Rows.Last().Level;
		if ((!bIsMaxLevelRow && Row.RequiredExperienceToNext <= 0)
			|| (bIsMaxLevelRow && Row.RequiredExperienceToNext != 0))
		{
			OutError = FString::Printf(
				TEXT("레벨 요구 경험치가 잘못됨: 레벨=%d, 요구량=%lld"),
				Row.Level,
				Row.RequiredExperienceToNext);
			return false;
		}

		if (!bIsMaxLevelRow)
		{
			if (Row.RequiredExperienceToNext > MAX_int64 - ExpectedTotalExperience)
			{
				OutError = FString::Printf(TEXT("누적 경험치 계산 중 int64 오버플로: 레벨=%d"), Row.Level);
				return false;
			}
			ExpectedTotalExperience += Row.RequiredExperienceToNext;
		}

		++ExpectedLevel;
	}

	return true;
}

const FNULevelProgression& FNULevelProgression::Get()
{
	static const FNULevelProgression Progression;
	return Progression;
}

#if !UE_BUILD_SHIPPING
bool FNULevelProgression::SimulateIncreaseExperience(const FNUProgressionState& CurrentState, int64 GainedExperience, FNUProgressionState& OutState, FString& OutError)
{
	if (GainedExperience < 0)
	{
		OutError = TEXT("획득 경험치는 음수일 수 없음");
		return false;
	}

	if (CurrentState.TotalExperience < 0
		|| GainedExperience > MAX_int64 - CurrentState.TotalExperience)
	{
		OutError = TEXT("경험치 합산 중 int64 오버플로 발생");
		return false;
	}

	// 실제 DataTable에서 읽은 레벨 데이터를 사용
	const FNULevelProgression& Progression = Get();
	if (!Progression.ValidateExperienceState(CurrentState, OutError))
	{
		return false;
	}

	const int32 MaxLevel = Progression.GetMaxLevel();
	if (CurrentState.Level > MaxLevel)
	{
		OutError = FString::Printf(
			TEXT("디버그 경험치 표의 최대 레벨을 초과한 상태임: 현재=%d, 최대=%d"),
			CurrentState.Level,
			MaxLevel);
		return false;
	}

	OutState = CurrentState;
	OutState.TotalExperience = FMath::Min(
		CurrentState.TotalExperience + GainedExperience,
		Progression.GetMaxTotalExperience());

	return Progression.TryResolveExperienceState(
		OutState.TotalExperience,
		OutState.Level,
		OutState.ExperienceIntoLevel);
}
#endif
