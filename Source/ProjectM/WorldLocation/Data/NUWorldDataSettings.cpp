#include "WorldLocation/Data/NUWorldDataSettings.h"
#include "Engine/World.h"

FName UNUWorldDataSettings::GetCategoryName() const
{
	return TEXT("Game");
}


int32 UNUWorldDataSettings::GetAreaIdForWorld(const UWorld* World) const
{
	if (World == nullptr)
	{
		return 0;
	}
	return GetAreaIdForMap(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
}

int32 UNUWorldDataSettings::GetAreaIdForMap(const FString& PackageName) const
{
	// 레벨 로딩 없이 경로만 비교한다.
	for (const TPair<TSoftObjectPtr<UWorld>, int32>& Entry : MapAreaIds)
	{
		if (Entry.Key.ToSoftObjectPath().GetLongPackageName() == PackageName)
		{
			return Entry.Value;
		}
	}
	return 0;
}
