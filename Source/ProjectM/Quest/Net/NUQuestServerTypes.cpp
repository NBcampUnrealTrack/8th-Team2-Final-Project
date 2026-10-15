#include "Quest/Net/NUQuestServerTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool TryReadInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
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

	bool TryReadInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue)
	{
		double Number = 0.0;
		if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, Number)
			|| !FMath::IsFinite(Number) || Number < -9007199254740991.0 || Number > 9007199254740991.0 || FMath::TruncToDouble(Number) != Number)
		{
			return false;
		}

		OutValue = static_cast<int64>(Number);
		return true;
	}

	bool ParseProgressObject(
		const TSharedPtr<FJsonObject>& Object,
		FNUQuestServerProgress& OutProgress,
		FString& OutError)
	{
		FNUQuestServerProgress Parsed;
		if (!TryReadInt32(Object, TEXT("QuestId"), Parsed.QuestId) || Parsed.QuestId <= 0)
		{
			OutError = TEXT("QuestId가 없거나 양의 int32가 아닙니다.");
			return false;
		}

		if (!TryReadInt32(Object, TEXT("State"), Parsed.State)
			|| Parsed.State < 0 || Parsed.State > 2)
		{
			OutError = FString::Printf(TEXT("QuestId=%d의 State가 0~2 범위가 아닙니다."), Parsed.QuestId);
			return false;
		}

		if (!TryReadInt32(Object, TEXT("CompletionCount"), Parsed.CompletionCount)
			|| Parsed.CompletionCount < 0)
		{
			OutError = FString::Printf(TEXT("QuestId=%d의 CompletionCount가 올바르지 않습니다."), Parsed.QuestId);
			return false;
		}

		if (!TryReadInt64(Object, TEXT("Revision"), Parsed.Revision) || Parsed.Revision < 0)
		{
			OutError = TEXT("퀘스트 Revision이 없거나 올바르지 않습니다.");
			return false;
		}

		Object->TryGetBoolField(TEXT("CanAccept"), Parsed.bCanAccept);

		const TArray<TSharedPtr<FJsonValue>>* JsonObjectives = nullptr;
		if (!Object->TryGetArrayField(TEXT("Objectives"), JsonObjectives) || JsonObjectives == nullptr)
		{
			OutError = FString::Printf(TEXT("QuestId=%d의 Objectives 배열이 없습니다."), Parsed.QuestId);
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *JsonObjectives)
		{
			const TSharedPtr<FJsonObject> ObjectiveObject = Value.IsValid() ? Value->AsObject() : nullptr;
			FNUQuestServerObjective Objective;
			if (!TryReadInt32(ObjectiveObject, TEXT("ObjectiveId"), Objective.ObjectiveId)
				|| Objective.ObjectiveId <= 0
				|| !TryReadInt32(ObjectiveObject, TEXT("CurrentCount"), Objective.CurrentCount)
				|| Objective.CurrentCount < 0)
			{
				OutError = FString::Printf(TEXT("QuestId=%d의 목표 진행 정보가 올바르지 않습니다."), Parsed.QuestId);
				return false;
			}

			Parsed.Objectives.Add(Objective);
		}

		OutProgress = MoveTemp(Parsed);
		return true;
	}

	bool ParseRoot(const FString& ResponseData, TSharedPtr<FJsonObject>& OutRoot, FString& OutError)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			OutError = TEXT("응답 JSON을 읽지 못했습니다.");
			return false;
		}

		return true;
	}
}

bool NUQuestServer::TryConvertQuestId(int64 QuestId, int32& OutQuestId)
{
	if (QuestId <= 0 || QuestId > MAX_int32)
	{
		return false;
	}

	OutQuestId = static_cast<int32>(QuestId);
	return true;
}

bool NUQuestServer::ParseProgress(
	const FString& ResponseData,
	FNUQuestServerProgress& OutProgress,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	return ParseRoot(ResponseData, Root, OutError)
		&& ParseProgressObject(Root, OutProgress, OutError);
}

bool NUQuestServer::ParseList(
	const FString& ResponseData,
	TArray<FNUQuestServerProgress>& OutProgressList,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(ResponseData, Root, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonQuests = nullptr;
	if (!Root->TryGetArrayField(TEXT("Quests"), JsonQuests) || JsonQuests == nullptr)
	{
		OutError = TEXT("Quests 배열이 없습니다.");
		return false;
	}

	TArray<FNUQuestServerProgress> ParsedList;
	TSet<int32> QuestIds;
	for (const TSharedPtr<FJsonValue>& Value : *JsonQuests)
	{
		FNUQuestServerProgress Progress;
		if (!ParseProgressObject(Value.IsValid() ? Value->AsObject() : nullptr, Progress, OutError))
		{
			return false;
		}

		if (QuestIds.Contains(Progress.QuestId))
		{
			OutError = FString::Printf(TEXT("QuestId=%d가 목록에 중복되었습니다."), Progress.QuestId);
			return false;
		}

		QuestIds.Add(Progress.QuestId);
		ParsedList.Add(MoveTemp(Progress));
	}

	OutProgressList = MoveTemp(ParsedList);
	return true;
}

bool NUQuestServer::ParseComplete(
	const FString& ResponseData,
	FNUQuestServerProgress& OutProgress,
	FNUQuestServerReward& OutReward,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(ResponseData, Root, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ProgressObject = nullptr;
	const TSharedPtr<FJsonObject>* RewardObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("Progress"), ProgressObject) || ProgressObject == nullptr
		|| !ParseProgressObject(*ProgressObject, OutProgress, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Progress 객체가 없습니다.");
		}
		return false;
	}

	if (!Root->TryGetObjectField(TEXT("Reward"), RewardObject) || RewardObject == nullptr)
	{
		OutError = TEXT("Reward 객체가 없습니다.");
		return false;
	}

	FNUQuestServerReward ParsedReward;
	if (!TryReadInt64(*RewardObject, TEXT("Exp"), ParsedReward.Experience)
		|| !TryReadInt64(*RewardObject, TEXT("Gold"), ParsedReward.Gold)
		|| ParsedReward.Experience < 0 || ParsedReward.Gold < 0)
	{
		OutError = TEXT("Reward의 Exp 또는 Gold가 올바르지 않습니다.");
		return false;
	}

	const TSharedPtr<FJsonValue>* GrowthValue = (*RewardObject)->Values.Find(TEXT("Growth"));
	if (GrowthValue != nullptr && GrowthValue->IsValid() && (*GrowthValue)->Type != EJson::Null)
	{
		const TSharedPtr<FJsonObject> GrowthObject = (*GrowthValue)->AsObject();
		if (!NUExperienceGrowthServer::Parse(GrowthObject, ParsedReward.Growth, OutError))
		{
			return false;
		}
		ParsedReward.bHasGrowth = true;
	}

	OutReward = ParsedReward;
	return true;
}
