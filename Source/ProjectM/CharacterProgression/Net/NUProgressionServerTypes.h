#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Data/NUJobTypes.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"

// 서버에서 받은 직업 및 성장 상태
struct PROJECTM_API FNUProgressionServerSnapshot
{
	FNUJobState JobState;
	FNUProgressionState ProgressionState;
};

namespace NUProgressionServer
{
	// 서버 JSON 변환 및 오류 문구 처리
	PROJECTM_API bool ParseSnapshot(const FString& ResponseData, FNUProgressionServerSnapshot& OutSnapshot, FString& OutError);
	PROJECTM_API FText GetResultMessage(int32 ResultCode, const FString& ServerMessage);
}
