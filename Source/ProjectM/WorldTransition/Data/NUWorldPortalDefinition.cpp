#include "WorldTransition/Data/NUWorldPortalDefinition.h"

FText FNUWorldPortalDefinition::GetRequirementFailureText(int32 PlayerLevel, bool bUnlockQuestCompleted, int32 RequiredItemCount) const
{
	TArray<FText> Reasons;
	if (UnlockQuestId > 0 && !bUnlockQuestCompleted) { Reasons.Add(NSLOCTEXT("NUWorldPortal", "QuestRequired", "선행 퀘스트를 완료해야 합니다.")); }
	if (!bEnabledByDefault && UnlockQuestId == 0) { Reasons.Add(NSLOCTEXT("NUWorldPortal", "Disabled", "아직 이용할 수 없는 출입구입니다.")); }
	if (PlayerLevel < RequiredLevel) { Reasons.Add(FText::Format(NSLOCTEXT("NUWorldPortal", "LevelRequired", "레벨 {0} 이상이어야 합니다."), FText::AsNumber(RequiredLevel))); }
	if (RequiredItemId > 0 && RequiredItemCount <= 0) { Reasons.Add(NSLOCTEXT("NUWorldPortal", "ItemRequired", "입장에 필요한 아이템을 보유해야 합니다.")); }
	return FText::Join(FText::FromString(TEXT("\n")), Reasons);
}

bool FNUWorldPortalDefinition::AreRequirementsMet(int32 PlayerLevel, bool bUnlockQuestCompleted, int32 RequiredItemCount) const
{
	const bool bQuestMet = UnlockQuestId == 0 || bUnlockQuestCompleted;
	const bool bEnabled = bEnabledByDefault || (UnlockQuestId > 0 && bUnlockQuestCompleted);
	return bEnabled && bQuestMet && PlayerLevel >= RequiredLevel && (RequiredItemId == 0 || RequiredItemCount > 0);
}

bool FNUWorldPortalDefinition::ValidateRow(FName RowName, FString& OutError) const
{
	OutError.Reset();
	if (PortalId <= 0 || RowName.ToString() != LexToString(PortalId))
	{
		OutError = TEXT("RowName은 PortalId와 동일한 양의 정수 문자열이어야 합니다.");
	}
	else if (PortalKey.IsNone() || DisplayName.IsEmptyOrWhitespace() || PortalType.IsNone())
	{
		OutError = TEXT("PortalKey, DisplayName, PortalType이 필요합니다.");
	}
	else if (SourceAreaId <= 0 || SourceLocationId <= 0 || SourceEntryPointId.IsNone() || DestinationAreaId <= 0 || DestinationLocationId <= 0 || DestinationEntryPointId.IsNone())
	{
		OutError = TEXT("양 끝의 지역, 장소, EntryPoint ID가 필요합니다.");
	}
	else if (SourceAreaId == DestinationAreaId && SourceLocationId == DestinationLocationId)
	{
		OutError = TEXT("양 끝의 지역/장소가 같아 플레이어 위치로 이동 방향을 구분할 수 없습니다.");
	}
	else if (RequiredLevel < 0 || UnlockQuestId < 0 || RequiredItemId < 0)
	{
		OutError = TEXT("입장 조건의 값은 0 이상이어야 합니다.");
	}
	else if (ActivationMode != ENUWorldPortalActivation::Interaction && ActivationMode != ENUWorldPortalActivation::Overlap && ActivationMode != ENUWorldPortalActivation::Interact)
	{
		OutError = TEXT("ActivationMode가 유효하지 않습니다.");
	}
	return OutError.IsEmpty();
}
