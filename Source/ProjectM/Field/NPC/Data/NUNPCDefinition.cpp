#include "Field/NPC/Data/NUNPCDefinition.h"

bool FNUNPCDefinition::TryParseNpcId(FName RowName, int32& OutNpcId)
{
	OutNpcId = 0;
	return !RowName.IsNone()
		&& LexTryParseString(OutNpcId, *RowName.ToString())
		&& OutNpcId > 0;
}

bool FNUNPCDefinition::ValidateRow(FName RowName, FString& OutError) const
{
	OutError.Reset();

	int32 ParsedNpcId = 0;
	if (!TryParseNpcId(RowName, ParsedNpcId))
	{
		OutError = TEXT("RowName은 0보다 큰 숫자 NpcId여야 합니다.");
		return false;
	}

	if (DisplayName.IsEmptyOrWhitespace())
	{
		OutError = TEXT("DisplayName이 비어 있습니다.");
		return false;
	}

	if (Species == ENUNPCSpecies::Unknown)
	{
		OutError = TEXT("Species가 지정되지 않았습니다.");
		return false;
	}

	if (Gender == ENUNPCGender::Unknown)
	{
		OutError = TEXT("Gender가 지정되지 않았습니다.");
		return false;
	}

	if (AreaId < 0 || LocationId < 0)
	{
		OutError = TEXT("AreaId와 LocationId는 0 이상이어야 합니다.");
		return false;
	}

	if (ShopId < 0)
	{
		OutError = TEXT("ShopId는 0 이상이어야 합니다.");
		return false;
	}

	return true;
}
