#include "WorldLocation/Data/NULocationDefinition.h"

bool FNULocationDefinition::ValidateRow(FName RowName, FString& OutError) const
{
	OutError.Reset();

	if (LocationId <= 0 || RowName.ToString() != LexToString(LocationId))
	{
		OutError = TEXT("RowName은 LocationId와 동일한 양의 정수 문자열이어야 합니다.");
		return false;
	}

	if (LocationKey.IsNone() || DisplayName.IsEmptyOrWhitespace())
	{
		OutError = TEXT("LocationKey 또는 DisplayName이 비어 있습니다.");
		return false;
	}

	if (LocationType <= ENULocationType::Unknown || LocationType > ENULocationType::Landmark)
	{
		OutError = TEXT("LocationType이 유효하지 않습니다.");
		return false;
	}

	if (ServiceType > ENULocationServiceType::Rest)
	{
		OutError = TEXT("ServiceType이 유효하지 않습니다.");
		return false;
	}

	// 지역은 필수 참조이며, 해금 퀘스트 0은 조건 없음을 뜻한다.
	if (AreaId <= 0 || UnlockQuestId < 0)
	{
		OutError = TEXT("AreaId는 양수, UnlockQuestId는 0 이상이어야 합니다.");
		return false;
	}

	return true;
}
