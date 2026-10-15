#include "CharacterProgression/Net/NUProgressionServerTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace NUProgressionServerTypesPrivate
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

	bool TryReadInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue)
	{
		return Object.IsValid() && Object->TryGetNumberField(FieldName, OutValue);
	}

	bool ReadRequiredInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue, FString& OutError)
	{
		if (TryReadInt32(Object, FieldName, OutValue))
		{
			return true;
		}

		OutError = FString::Printf(TEXT("%s 필드가 없거나 int32 형식이 아닙니다."), FieldName);
		return false;
	}

	bool ReadRequiredInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue, FString& OutError)
	{
		if (TryReadInt64(Object, FieldName, OutValue))
		{
			return true;
		}

		OutError = FString::Printf(TEXT("%s 필드가 없거나 int64 형식이 아닙니다."), FieldName);
		return false;
	}

	bool ReadInt32Array(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, TArray<int32>& OutValues, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, JsonValues) || JsonValues == nullptr)
		{
			OutError = FString::Printf(TEXT("%s 배열이 없습니다."), FieldName);
			return false;
		}

		TArray<int32> ParsedValues;
		TSet<int32> UniqueValues;
		ParsedValues.Reserve(JsonValues->Num());
		for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValues)
		{
			double Number = 0.0;
			if (!JsonValue.IsValid()
				|| !JsonValue->TryGetNumber(Number)
				|| !FMath::IsFinite(Number)
				|| Number <= 0.0
				|| Number > static_cast<double>(MAX_int32)
				|| FMath::TruncToDouble(Number) != Number)
			{
				OutError = FString::Printf(TEXT("%s 배열에 유효하지 않은 ID가 있습니다."), FieldName);
				return false;
			}

			const int32 Value = static_cast<int32>(Number);
			if (UniqueValues.Contains(Value))
			{
				OutError = FString::Printf(TEXT("%s 배열에 중복 ID가 있습니다."), FieldName);
				return false;
			}
			UniqueValues.Add(Value);
			ParsedValues.Add(Value);
		}

		OutValues = MoveTemp(ParsedValues);
		return true;
	}

	bool ReadNodeLinks(const TSharedPtr<FJsonObject>& Object, TArray<FNUProgressionNodeLinkState>& OutLinks, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* JsonLinks = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(TEXT("AcquiredNodeLinks"), JsonLinks) || JsonLinks == nullptr)
		{
			OutError = TEXT("AcquiredNodeLinks 배열이 없습니다.");
			return false;
		}

		TArray<FNUProgressionNodeLinkState> ParsedLinks;
		TSet<int32> ChildNodeIds;
		ParsedLinks.Reserve(JsonLinks->Num());
		for (const TSharedPtr<FJsonValue>& JsonLink : *JsonLinks)
		{
			if (!JsonLink.IsValid() || JsonLink->Type != EJson::Object)
			{
				OutError = TEXT("AcquiredNodeLinks 배열에 객체가 아닌 값이 있습니다.");
				return false;
			}

			const TSharedPtr<FJsonObject> LinkObject = JsonLink->AsObject();
			FNUProgressionNodeLinkState Link;
			if (!ReadRequiredInt32(LinkObject, TEXT("ParentNodeId"), Link.ParentNodeId, OutError)
				|| !ReadRequiredInt32(LinkObject, TEXT("ChildNodeId"), Link.ChildNodeId, OutError)
				|| Link.ParentNodeId <= 0
				|| Link.ChildNodeId <= 0
				|| Link.ParentNodeId == Link.ChildNodeId
				|| ChildNodeIds.Contains(Link.ChildNodeId))
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("AcquiredNodeLinks에 유효하지 않거나 중복된 연결이 있습니다.");
				}
				return false;
			}

			ChildNodeIds.Add(Link.ChildNodeId);
			ParsedLinks.Add(Link);
		}

		OutLinks = MoveTemp(ParsedLinks);
		return true;
	}

}

bool NUProgressionServer::ParseSnapshot(const FString& ResponseData, FNUProgressionServerSnapshot& OutSnapshot, FString& OutError)
{
	using NUProgressionServerTypesPrivate::ReadInt32Array;
	using NUProgressionServerTypesPrivate::ReadNodeLinks;
	using NUProgressionServerTypesPrivate::ReadRequiredInt32;
	using NUProgressionServerTypesPrivate::ReadRequiredInt64;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("성장 응답 JSON을 읽지 못했습니다.");
		return false;
	}

	FNUProgressionServerSnapshot Parsed;
	FNUProgressionState& Progression = Parsed.ProgressionState;
	if (!ReadRequiredInt32(Root, TEXT("GraphVersion"), Progression.GraphVersion, OutError)
		|| !ReadRequiredInt64(Root, TEXT("ProgressionRevision"), Progression.Revision, OutError)
		|| !ReadRequiredInt32(Root, TEXT("JobId"), Parsed.JobState.JobId, OutError)
		|| !ReadRequiredInt32(Root, TEXT("AdvancementId"), Parsed.JobState.AdvancementId, OutError)
		|| !ReadRequiredInt32(Root, TEXT("Level"), Progression.Level, OutError)
		|| !ReadRequiredInt64(Root, TEXT("TotalExperience"), Progression.TotalExperience, OutError)
		|| !ReadRequiredInt64(Root, TEXT("ExperienceIntoLevel"), Progression.ExperienceIntoLevel, OutError)
		|| !ReadRequiredInt32(Root, TEXT("AvailablePoints"), Progression.AvailablePoints, OutError)
		|| !ReadRequiredInt32(Root, TEXT("TotalPoints"), Progression.TotalPoints, OutError)
		|| !ReadRequiredInt32(Root, TEXT("ActiveStemId"), Progression.ActiveStemId, OutError)
		|| !ReadRequiredInt32(Root, TEXT("CurrentNodeId"), Progression.CurrentNodeId, OutError)
		|| !ReadRequiredInt32(Root, TEXT("SelectedFinalNodeId"), Progression.SelectedFinalNodeId, OutError)
		|| !ReadInt32Array(Root, TEXT("AcquiredNodeIds"), Progression.AcquiredNodeIds, OutError)
		|| !ReadInt32Array(Root, TEXT("CompletedStemIds"), Progression.CompletedStemIds, OutError)
		|| !ReadNodeLinks(Root, Progression.AcquiredNodeLinks, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PrimaryStatsObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("PrimaryStats"), PrimaryStatsObject) || PrimaryStatsObject == nullptr
		|| !ReadRequiredInt32(*PrimaryStatsObject, TEXT("Str"), Progression.PrimaryStats.Strength, OutError)
		|| !ReadRequiredInt32(*PrimaryStatsObject, TEXT("Dex"), Progression.PrimaryStats.Dexterity, OutError)
		|| !ReadRequiredInt32(*PrimaryStatsObject, TEXT("Int"), Progression.PrimaryStats.Intelligence, OutError)
		|| !ReadRequiredInt32(*PrimaryStatsObject, TEXT("Luk"), Progression.PrimaryStats.Luck, OutError)
		|| !ReadRequiredInt32(*PrimaryStatsObject, TEXT("Vit"), Progression.PrimaryStats.Vitality, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("PrimaryStats 객체가 없습니다.");
		}
		return false;
	}

	if (Progression.GraphVersion <= 0
		|| Progression.Revision < 0
		|| Parsed.JobState.JobId < 0
		|| Parsed.JobState.AdvancementId < 0
		|| (Parsed.JobState.JobId == 0 && Parsed.JobState.AdvancementId != 0)
		|| Progression.Level < 1
		|| Progression.TotalExperience < 0
		|| Progression.ExperienceIntoLevel < 0
		|| Progression.AvailablePoints < 0
		|| Progression.TotalPoints < 0
		|| Progression.AvailablePoints > Progression.TotalPoints
		|| Progression.ActiveStemId < 0
		|| Progression.CurrentNodeId < 0
		|| Progression.SelectedFinalNodeId < 0)
	{
		OutError = TEXT("성장 응답에 음수 값 또는 서로 모순되는 상태가 있습니다.");
		return false;
	}

	const FNUPrimaryStats& Primary = Progression.PrimaryStats;
	if (!NUProgression::ArePrimaryStatsValid(Primary))
	{
		OutError = TEXT("1차 스탯이 상한 29를 넘었거나 STR과 INT가 동시에 설정되었습니다.");
		return false;
	}

	OutSnapshot = MoveTemp(Parsed);
	OutError.Reset();
	return true;
}

FText NUProgressionServer::GetResultMessage(int32 ResultCode, const FString& ServerMessage)
{
	if (!ServerMessage.IsEmpty())
	{
		return FText::FromString(ServerMessage);
	}

	switch (ResultCode)
	{
	case 2100: return NSLOCTEXT("NUProgressionServer", "GraphUnavailable", "서버 성장 그래프를 사용할 수 없습니다.");
	case 2101: return NSLOCTEXT("NUProgressionServer", "VersionMismatch", "성장 데이터 버전이 다릅니다. 클라이언트 업데이트가 필요합니다.");
	case 2102: return NSLOCTEXT("NUProgressionServer", "RevisionConflict", "성장 상태가 변경되어 최신 상태로 다시 맞췄습니다.");
	case 2103: return NSLOCTEXT("NUProgressionServer", "InvalidJob", "선택할 수 없는 직업입니다.");
	case 2104: return NSLOCTEXT("NUProgressionServer", "JobAlreadySet", "이미 직업을 선택했습니다.");
	case 2105: return NSLOCTEXT("NUProgressionServer", "JobNotSet", "직업을 먼저 선택해야 합니다.");
	case 2106: return NSLOCTEXT("NUProgressionServer", "InvalidNode", "선택할 수 없는 성장 노드입니다.");
	case 2107: return NSLOCTEXT("NUProgressionServer", "NodeAlreadyAcquired", "이미 획득한 성장 노드입니다.");
	case 2108: return NSLOCTEXT("NUProgressionServer", "ParentNotMet", "선행 노드를 먼저 획득해야 합니다.");
	case 2109: return NSLOCTEXT("NUProgressionServer", "LevelTooLow", "노드 요구 레벨이 부족합니다.");
	case 2110: return NSLOCTEXT("NUProgressionServer", "NoPoint", "성장 포인트가 부족합니다.");
	case 2111: return NSLOCTEXT("NUProgressionServer", "InvalidAdvancement", "선택할 수 없는 2차 전직입니다.");
	case 2112: return NSLOCTEXT("NUProgressionServer", "AdvancementRequired", "현재 전직으로 획득할 수 없는 노드입니다.");
	case 2113: return NSLOCTEXT("NUProgressionServer", "StemBusy", "현재 진행 중인 줄기를 먼저 완료해야 합니다.");
	case 2114: return NSLOCTEXT("NUProgressionServer", "StemLimit", "방사 줄기는 네 개까지만 선택할 수 있습니다.");
	case 2115: return NSLOCTEXT("NUProgressionServer", "FinalNotReady", "최종 노드 조건을 충족하지 못했습니다.");
	case 2116: return NSLOCTEXT("NUProgressionServer", "FinalAlreadySet", "이미 최종 노드를 선택했습니다.");
	case 2117: return NSLOCTEXT("NUProgressionServer", "ManagedStat", "직업 선택 후에는 성장 노드로만 스탯을 올릴 수 있습니다.");
	case 2118: return NSLOCTEXT("NUProgressionServer", "DatabaseError", "성장 상태를 저장하지 못했습니다.");
	case 2119: return NSLOCTEXT("NUProgressionServer", "StageTaken", "이 단계에서는 이미 다른 노드를 선택했습니다.");
	default: return FText::Format(NSLOCTEXT("NUProgressionServer", "UnknownError", "성장 요청에 실패했습니다. ({0})"), FText::AsNumber(ResultCode));
	}
}
