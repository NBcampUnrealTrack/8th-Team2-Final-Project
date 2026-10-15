#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"

// 레벨 테이블 조회와 디버그용 경험치 증가 계산을 담당한다.
struct PROJECTM_API FNULevelProgression
{
	TArray<FNULevelProgressionRow> LevelRows;

	// 현재 최대 레벨 60
	int32 GetMaxLevel() const;
	int64 GetMaxTotalExperience() const;

	// 다음 레벨에 필요한 경험치 조회
	bool TryGetRequiredExperienceToNext(int32 CurrentLevel, int64& OutRequiredExperience) const;
	bool TryGetRequiredCumulativeExperience(int32 TargetLevel, int64& OutCumulativeExperience) const;
	bool TryResolveExperienceState(int64 TotalExperience, int32& OutLevel, int64& OutExperienceIntoLevel) const;
	bool ValidateExperienceState(const FNUProgressionState& State, FString& OutError) const;
	bool Validate(FString& OutError) const;

	static bool ValidateRows(const TArray<FNULevelProgressionRow>& Rows, FString& OutError);
	static const FNULevelProgression& Get();

#if !UE_BUILD_SHIPPING
	// 디버깅용 경험치 획득 결과 임시 계산
	static bool SimulateIncreaseExperience(
		const FNUProgressionState& CurrentState,
		int64 GainedExperience,
		FNUProgressionState& OutState,
		FString& OutError);
#endif

private:
	FNULevelProgression();
};
