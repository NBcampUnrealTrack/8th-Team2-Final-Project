// Fill out your copyright notice in the Description page of Project Settings.

#include "Shop/NUShopTypes.h"

#include "Inventory/NUInventoryTypes.h"

namespace NUShop
{

static const UDataTable* GetShopItemTable()
{
	return GetDefault<UNUShopSettings>()->ShopItemTable.LoadSynchronous();
}

int32 GetSellPrice(const FNUItemDefRow& Def, const FNUShopRow& Shop)
{
	const UDataTable* Table = GetShopItemTable();
	if (Table == nullptr)
	{
		return 0;
	}

	// 기준 행: 이 상점 행이 있으면 그 값(이 상점 구매가), 없으면 가장 싼 행(지역 물가 1.0의 원본 가격)
	int32 BasePrice = 0;
	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FNUShopItemRow* Item = reinterpret_cast<const FNUShopItemRow*>(Row.Value);
		if (Item->RefId != Def.NumericId)
		{
			continue;
		}
		if (Item->ShopId == Shop.ShopId)
		{
			BasePrice = Item->Price;
			break;
		}
		if (BasePrice == 0 || Item->Price < BasePrice)
		{
			BasePrice = Item->Price;
		}
	}

	// 되팔기율이 float라 100 x 0.35f = 34.99999 처럼 정수 아래로 떨어질 수 있어
	// 천분율 정수로 고친 뒤 정수 나눗셈으로 버린다 --- 십진으로 계산하는 서버와 같은 값이 나온다
	return static_cast<int32>(static_cast<int64>(BasePrice) * FMath::RoundToInt32(Shop.BuybackRate * 1000.f) / 1000);
}

const FNUShopRow* FindShopRow(int32 ShopId)
{
	const UDataTable* Table = GetDefault<UNUShopSettings>()->ShopTable.LoadSynchronous();
	return Table != nullptr
		? Table->FindRow<FNUShopRow>(FName(*FString::FromInt(ShopId)), TEXT("NUShop"), false)
		: nullptr;
}

const FNUShopItemRow* FindShopItemRow(int32 ShopItemId)
{
	const UDataTable* Table = GetShopItemTable();
	return Table != nullptr
		? Table->FindRow<FNUShopItemRow>(FName(*FString::FromInt(ShopItemId)), TEXT("NUShop"), false)
		: nullptr;
}

TArray<const FNUShopItemRow*> GetShopItems(int32 ShopId)
{
	TArray<const FNUShopItemRow*> Items;
	const UDataTable* Table = GetShopItemTable();
	if (Table == nullptr)
	{
		return Items;
	}
	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FNUShopItemRow* Item = reinterpret_cast<const FNUShopItemRow*>(Row.Value);
		if (Item->ShopId == ShopId)
		{
			Items.Add(Item);
		}
	}
	Items.Sort([](const FNUShopItemRow& A, const FNUShopItemRow& B) { return A.SlotIndex < B.SlotIndex; });
	return Items;
}

}
