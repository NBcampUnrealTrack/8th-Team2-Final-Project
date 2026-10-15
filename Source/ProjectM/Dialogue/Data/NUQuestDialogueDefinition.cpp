#include "Dialogue/Data/NUQuestDialogueDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	bool ValidateLines(const TCHAR* PhaseName, const TArray<FNUDialogueLine>& Lines, FString& OutError)
	{
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FNUDialogueLine Resolved;
			if (!Lines[LineIndex].Resolve(Resolved, OutError))
			{
				OutError = FString::Printf(TEXT("%s[%d]: %s"), PhaseName, LineIndex, *OutError);
				return false;
			}
		}

		return true;
	}
}

const TArray<FNUDialogueLine>* UNUQuestDialogueDefinition::FindLines(ENUQuestInteractionPhase Phase) const
{
	switch (Phase)
	{
	case ENUQuestInteractionPhase::Offer:
		return Offer.IsEmpty() ? nullptr : &Offer;

	case ENUQuestInteractionPhase::InProgress:
		return InProgress.IsEmpty() ? nullptr : &InProgress;

	case ENUQuestInteractionPhase::TurnIn:
		return TurnIn.IsEmpty() ? nullptr : &TurnIn;

	case ENUQuestInteractionPhase::Completed:
		return Completed.IsEmpty() ? nullptr : &Completed;

	case ENUQuestInteractionPhase::Target:
		return Target.IsEmpty() ? nullptr : &Target;

	default:
		return nullptr;
	}
}

FName UNUQuestDialogueDefinition::FindDialogue(ENUQuestInteractionPhase Phase) const
{
	const FName* Dialogue = PhaseDialogues.Find(Phase);
	return Dialogue != nullptr ? *Dialogue : NAME_None;
}

bool UNUQuestDialogueDefinition::HasPhase(ENUQuestInteractionPhase Phase) const
{
	return !FindDialogue(Phase).IsNone() || FindLines(Phase) != nullptr;
}

bool UNUQuestDialogueDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();

	if (QuestId.IsNone())
	{
		OutError = TEXT("QuestId가 비어 있습니다.");
		return false;
	}

	for (const auto& Pair : PhaseDialogues)
	{
		if (Pair.Value.IsNone() || Pair.Key == ENUQuestInteractionPhase::Idle || Pair.Key == ENUQuestInteractionPhase::Locked)
		{
			OutError = TEXT("단계별 연결 ID 또는 단계가 유효하지 않습니다.");
			return false;
		}
	}

	if (PhaseDialogues.IsEmpty() && Offer.IsEmpty() && InProgress.IsEmpty() && TurnIn.IsEmpty() && Completed.IsEmpty() && Target.IsEmpty())
	{
		OutError = TEXT("단계별 대사가 하나도 없습니다.");
		return false;
	}

	return ValidateLines(TEXT("Offer"), Offer, OutError)
		&& ValidateLines(TEXT("InProgress"), InProgress, OutError)
		&& ValidateLines(TEXT("TurnIn"), TurnIn, OutError)
		&& ValidateLines(TEXT("Completed"), Completed, OutError)
		&& ValidateLines(TEXT("Target"), Target, OutError);
}

#if WITH_EDITOR
EDataValidationResult UNUQuestDialogueDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	FString ValidationError;
	if (!ValidateDefinition(ValidationError))
	{
		Context.AddError(FText::FromString(ValidationError));
		Result = EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif
