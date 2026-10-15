#include "WorldLocation/Data/NUAreaDefinition.h"

bool FNUAreaDefinition::ValidateRow(FName RowName, FString& OutError) const
{
	OutError.Reset();

	if (AreaId <= 0 || RowName.ToString() != LexToString(AreaId))
	{
		OutError = TEXT("RowName은 AreaId와 동일한 양의 정수 문자열이어야 합니다.");
		return false;
	}

	if (AreaKey.IsNone() || DisplayName.IsEmptyOrWhitespace())
	{
		OutError = TEXT("AreaKey 또는 DisplayName이 비어 있습니다.");
		return false;
	}

	if (AreaType <= ENUAreaType::Unknown || AreaType > ENUAreaType::Dungeon)
	{
		OutError = TEXT("AreaType이 유효하지 않습니다.");
		return false;
	}

	if (Chapter <= 0 || MinLevel <= 0 || MaxLevel < MinLevel)
	{
		OutError = TEXT("Chapter와 MinLevel은 1 이상이고 MaxLevel은 MinLevel 이상이어야 합니다.");
		return false;
	}

	if (ParentAreaId < 0 || ParentAreaId == AreaId || UnlockQuestId < 0 || RespawnLocationId < 0)
	{
		OutError = TEXT("참조 ID는 0 이상이어야 하며 ParentAreaId는 자기 자신일 수 없습니다.");
		return false;
	}

	TSet<int32> ConnectedIds;
	for (int32 ConnectedAreaId : ConnectedAreas)
	{
		if (ConnectedAreaId <= 0 || ConnectedAreaId == AreaId || ConnectedIds.Contains(ConnectedAreaId))
		{
			OutError = TEXT("ConnectedAreas에 잘못된 ID, 자기 참조 또는 중복 ID가 있습니다.");
			return false;
		}
		ConnectedIds.Add(ConnectedAreaId);
	}

	return true;
}
