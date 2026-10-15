// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Map/NUMapImageSettings.h"

FName UNUMapImageSettings::GetCategoryName() const
{
	return TEXT("Game");
}

const FNUMapImageEntry* UNUMapImageSettings::FindForPackage(const FString& PackageName) const
{
	// 레벨 로딩 없이 경로만 비교한다 (NUWorldDataSettings::GetAreaIdForMap 과 같은 방식)
	for (const FNUMapImageEntry& Entry : Maps)
	{
		if (Entry.IsValidEntry() && Entry.Map.ToSoftObjectPath().GetLongPackageName() == PackageName)
		{
			return &Entry;
		}
	}
	return nullptr;
}

FVector2D UNUMapImageSettings::WorldToMapUV(const FNUMapImageEntry& Entry, const FVector& LevelLocation)
{
	// 지도 오른쪽이 -X 라 U 는 Max 에서 빼고, 텍스처 V 는 아래로 자라는데 지도 위가 +Y 라 V 도 Max 에서 뺀다
	const FVector2D Size = Entry.WorldMax - Entry.WorldMin;
	return FVector2D(
		(Entry.WorldMax.X - LevelLocation.X) / Size.X,
		(Entry.WorldMax.Y - LevelLocation.Y) / Size.Y);
}
