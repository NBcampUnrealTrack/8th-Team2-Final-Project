#include "Field/Presence/NUActorPresenceRow.h"

#include "Quest/Data/NUQuestBattleDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FNUActorPresenceRow::Validate(FString& OutError) const
{
	OutError.Reset();
	if (PresenceId.IsNone() || StepOrder < 0)
	{
		OutError = TEXT("PresenceId와 0 이상의 StepOrder가 필요합니다.");
		return false;
	}
	if (ContractedStarterNaiaId < 0 || QuestActorId < 0)
	{
		OutError = TEXT("나이아와 퀘스트 대상 ID는 0(지정 안 함) 이상이어야 합니다.");
		return false;
	}
	for (const FNUQuestStateCondition& Condition : Conditions)
	{
		if (!Condition.Validate(OutError)) { return false; }
	}
	if (!bOverrideQuestInteraction && (QuestActorId != 0 || ObjectiveType.IsValid() || !Battle.IsNull()))
	{
		OutError = TEXT("퀘스트 역할을 지정하려면 bOverrideQuestInteraction을 켭니다.");
		return false;
	}
	if (!bVisible && (QuestActorId != 0 || !Battle.IsNull() || bHoverAroundPlayer))
	{
		OutError = TEXT("숨는 행에는 퀘스트 대상·전투·맴돌기를 지정할 수 없습니다.");
		return false;
	}
	if ((QuestActorId == 0) != !ObjectiveType.IsValid())
	{
		OutError = TEXT("QuestActorId와 ObjectiveType은 함께 지정하거나 함께 비웁니다.");
		return false;
	}
	if (!Battle.IsNull() && QuestActorId == 0)
	{
		OutError = TEXT("전투에는 QuestActorId가 필요합니다.");
		return false;
	}
	return true;
}

bool FNUActorPresenceRow::IsMet(TFunctionRef<const FNUQuestProgressState*(FName)> FindProgress, TFunctionRef<const UNUQuestDefinition*(FName)> FindDefinition, int32 LocalStarterNaiaId) const
{
	if (ContractedStarterNaiaId > 0 && ContractedStarterNaiaId != LocalStarterNaiaId)
	{
		return false;
	}
	for (const FNUQuestStateCondition& Condition : Conditions)
	{
		if (Condition.IsSet() && !Condition.IsMet(FindProgress, FindDefinition)) { return false; }
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult FNUActorPresenceRow::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return EDataValidationResult::Valid;
}
#endif
