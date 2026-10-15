// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/Net/NUBattleRewardServerTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleReward, Log, All);


namespace
{
	bool TryReadRewardInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue)
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

	// 드롭 아이템의 키 이름이 계약에 안 적혀 있음
	// 흔한 이름 몇 개를 시도하고, 하나도 안 맞으면 지어내지 않고 원문을 로그로 남김
	bool TryReadRewardItem(const TSharedPtr<FJsonObject>& Object, FNUServerBattleRewardItem& OutItem)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		int64 ItemId = 0;
		if (!TryReadRewardInt64(Object, TEXT("ItemId"), ItemId)
			&& !TryReadRewardInt64(Object, TEXT("Id"), ItemId)
			&& !TryReadRewardInt64(Object, TEXT("ResourceId"), ItemId))
		{
			return false;
		}

		int64 Count = 1;
		if (!TryReadRewardInt64(Object, TEXT("Count"), Count)
			&& !TryReadRewardInt64(Object, TEXT("Amount"), Count)
			&& !TryReadRewardInt64(Object, TEXT("Quantity"), Count))
		{
			Count = 1;
		}

		OutItem.ItemId = static_cast<int32>(ItemId);
		OutItem.Count = FMath::Max(1, static_cast<int32>(Count));

		return OutItem.ItemId > 0;
	}
}

bool NUBattleRewardServer::Parse(const FString& ResponseData, FNUServerBattleReward& OutReward, FString& OutError)
{
	OutError.Reset();

	// 승리가 아니면 보상 자체가 없다, 오류로 다루지 않고 없다고만 알린다
	if (ResponseData.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("보상 JSON을 읽을 수 없습니다.");
		return false;
	}

	// 보상은 Reward 객체 안에 올 수도, 루트에 바로 올 수도 있으므로 둘다 받기
	TSharedPtr<FJsonObject> RewardObject = Root;
	const TSharedPtr<FJsonObject>* Nested = nullptr;
	if (Root->TryGetObjectField(TEXT("Reward"), Nested) && Nested != nullptr && Nested->IsValid())
	{
		RewardObject = *Nested;
	}

	FNUServerBattleReward Parsed;

	// Exp·Gold 가 둘 다 없으면 보상 페이로드가 아니다 (전투 종료 상태만 실려 온 경우)
	const bool bHasExp = TryReadRewardInt64(RewardObject, TEXT("Exp"), Parsed.Experience);
	const bool bHasGold = TryReadRewardInt64(RewardObject, TEXT("Gold"), Parsed.Gold);
	if (!bHasExp && !bHasGold)
	{
		return false;
	}

	Parsed.Experience = FMath::Max<int64>(0, Parsed.Experience);
	Parsed.Gold = FMath::Max<int64>(0, Parsed.Gold);

	const TSharedPtr<FJsonValue>* GrowthValue = RewardObject->Values.Find(TEXT("Growth"));
	if (GrowthValue != nullptr && GrowthValue->IsValid() && (*GrowthValue)->Type != EJson::Null)
	{
		FString GrowthError;
		if (NUExperienceGrowthServer::Parse((*GrowthValue)->AsObject(), Parsed.Growth, GrowthError))
		{
			Parsed.bHasGrowth = true;
		}
		else
		{
			UE_LOG(LogNUBattleReward, Warning,
				TEXT("보상의 Growth를 읽지 못함: %s"), *GrowthError);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ItemArray = nullptr;
	if (RewardObject->TryGetArrayField(TEXT("Items"), ItemArray) && ItemArray != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ItemArray)
		{
			if (!Value.IsValid())
			{
				continue;
			}

			FNUServerBattleRewardItem Item;
			if (TryReadRewardItem(Value->AsObject(), Item))
			{
				Parsed.Items.Add(Item);
				continue;
			}

			UE_LOG(LogNUBattleReward, Warning,
				TEXT("보상 아이템의 모양을 모름, 원문을 확인해 파서를 맞출 것: %s"), *ResponseData);
		}
	}

	OutReward = MoveTemp(Parsed);

	return true;
}
