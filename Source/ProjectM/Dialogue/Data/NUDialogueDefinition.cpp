#include "Dialogue/Data/NUDialogueDefinition.h"

#include "Dialogue/Data/NUDialogueRow.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FNUDialogueLine::Resolve(FNUDialogueLine& OutLine, FString& OutError) const
{
	OutError.Reset();
	FNUDialogueLine Resolved;
	if (DialogueRow.IsNull())
	{
		Resolved = *this;
	}
	else
	{
		const UDataTable* Table = DialogueRow.DataTable;
		if (!IsValid(Table) || DialogueRow.RowName.IsNone() || Table->GetRowStruct() != FNUDialogueRow::StaticStruct())
		{
			OutError = FString::Printf(TEXT("대사 테이블 또는 행 형식이 잘못되었습니다. Table=%s Row=%s"), *GetPathNameSafe(Table), *DialogueRow.RowName.ToString());
			return false;
		}
		const FNUDialogueRow* Row = Table->FindRow<FNUDialogueRow>(DialogueRow.RowName, TEXT("Dialogue"), false);
		if (Row == nullptr)
		{
			OutError = FString::Printf(TEXT("대사 행이 없습니다. Table=%s Row=%s"), *Table->GetPathName(), *DialogueRow.RowName.ToString());
			return false;
		}
		Resolved.SourceRowName = DialogueRow.RowName;
		Resolved.Speaker = Row->Speaker;
		Resolved.SpeakerNpcId = Row->SpeakerNpcId;
		Resolved.SpeakerName = Row->SpeakerName;
		Resolved.DialogueText = Row->DialogueText;
		Resolved.Portrait = Row->Portrait;
	}
	if (Resolved.DialogueText.IsEmpty())
	{
		OutError = TEXT("대사 본문이 비어 있습니다.");
		return false;
	}

	// 재임포트 시 무효화될 수 있는 행 포인터를 보관하지 않는다.
	OutLine = MoveTemp(Resolved);
	return true;
}

bool FNUDialogueLine::ResolveLines(const TArray<FNUDialogueLine>& SourceLines, TArray<FNUDialogueLine>& OutLines, FString& OutError)
{
	OutError.Reset();
	TArray<FNUDialogueLine> ResolvedLines;
	ResolvedLines.Reserve(SourceLines.Num());
	for (int32 Index = 0; Index < SourceLines.Num(); ++Index)
	{
		FNUDialogueLine Resolved;
		if (!SourceLines[Index].Resolve(Resolved, OutError))
		{
			OutError = FString::Printf(TEXT("Lines[%d]: %s"), Index, *OutError);
			return false;
		}
		ResolvedLines.Add(MoveTemp(Resolved));
	}
	OutLines = MoveTemp(ResolvedLines);
	return true;
}

bool UNUDialogueDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();

	if (DialogueId.IsNone())
	{
		OutError = TEXT("DialogueId가 비어 있습니다.");
		return false;
	}

	// 본문이 비어 있으면 DialogueId로 테이블을 조회하는 기존 Blueprint용 참조다.
	if (Lines.IsEmpty()) { return true; }

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		FNUDialogueLine Resolved;
		if (!Lines[LineIndex].Resolve(Resolved, OutError))
		{
			OutError = FString::Printf(TEXT("Lines[%d]: %s"), LineIndex, *OutError);
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
EDataValidationResult UNUDialogueDefinition::IsDataValid(FDataValidationContext& Context) const
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
