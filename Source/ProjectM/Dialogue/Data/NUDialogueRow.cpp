#include "Dialogue/Data/NUDialogueRow.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FNUDialogueRow::IsSceneDialogue() const
{
	return !DialogueId.IsNone() && EventId.IsNone() && NpcId == 0 && QuestId == 0 && ObjectiveId == 0 && Phase == ENUQuestInteractionPhase::Idle;
}

bool FNUDialogueRow::Validate(FString& OutError) const
{
	OutError.Reset();

	if (DialogueKey.IsNone() || DialogueText.IsEmpty() || SpeakerNpcId < 0 || NpcId < 0 || QuestId < 0 || ObjectiveId < 0)
	{
		OutError = TEXT("대사 키, 본문과 0 이상의 NPC·퀘스트·목표 ID가 필요합니다.");
		return false;
	}

	// NPC 대사는 이름이 있어야 하고, 나레이션·독백은 화자 칸을 비워 이름을 재생 시점에 정한다.
	if (Speaker == ENUDialogueSpeaker::Npc ? SpeakerName.IsEmpty() : (SpeakerNpcId != 0 || !SpeakerName.IsEmpty()))
	{
		OutError = TEXT("NPC 대사(Npc)는 화자 이름이 필요하고, 나레이션(Narration)·독백(Player)은 화자 ID와 이름을 비웁니다.");
		return false;
	}

	// 아직 장면에 연결되지 않은 문장도 원문을 잃지 않고 보관한다.
	if (DialogueId.IsNone()) { return true; }
	if (LineIndex <= 0) { OutError = TEXT("재생 문장 순서는 1 이상이어야 합니다."); return false; }

	// 언제 재생할지는 연출 표가 정한다.
	if (IsSceneDialogue()) { return true; }

	if (!EventId.IsNone())
	{
		if (QuestId != 0 || ObjectiveId != 0 || Phase != ENUQuestInteractionPhase::Idle)
		{
			OutError = TEXT("직접 호출하는 대화에는 퀘스트 단계를 중복 지정하지 않습니다.");
			return false;
		}
	}
	else if (NpcId == 0 || QuestId == 0 || Phase == ENUQuestInteractionPhase::Idle || Phase == ENUQuestInteractionPhase::Locked || (ObjectiveId != 0 && Phase != ENUQuestInteractionPhase::Target))
	{
		OutError = TEXT("퀘스트 대화에는 NPC, 퀘스트와 재생 단계가 필요합니다. 목표 ID는 Target 단계에만 지정합니다. 조건 없이 두면 연출 표가 부르는 대화입니다.");
		return false;
	}
	return true;
}

void FNUDialogueRow::OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems)
{
	FString Error;
	if (!Validate(Error))
	{
		OutCollectedImportProblems.Add(FString::Printf(TEXT("대사 행 %s: %s"), *InRowName.ToString(), *Error));
		return;
	}

#if WITH_EDITOR
	// CSV 재임포트 후에도 기존 대사의 지역화 키를 유지한다.
	const FString Key = DialogueKey.ToString();
	SpeakerName = FText::ChangeKey(TEXT("NPCDialogue"), Key + TEXT("_Speaker"), SpeakerName);
	DialogueText = FText::ChangeKey(TEXT("NPCDialogue"), Key, FText::FromString(DialogueText.ToString().Replace(TEXT("**"), TEXT(""))));
#endif
}

#if WITH_EDITOR
EDataValidationResult FNUDialogueRow::IsDataValid(FDataValidationContext& Context) const
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
