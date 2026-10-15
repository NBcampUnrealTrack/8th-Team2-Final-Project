#include "Quest/Data/NUQuestSceneRow.h"

#include "Cinematics/NUStorySlideshowData.h"
#include "LevelSequence.h"

bool FNUQuestSceneRow::Validate(FString& OutError) const
{
	OutError.Reset();

	if (!Condition.IsSet() || !Condition.Validate(OutError))
	{
		OutError = OutError.IsEmpty() ? TEXT("연출에는 퀘스트 조건(Condition)이 필요합니다.") : OutError;
		return false;
	}

	// 시네마틱은 자막을 스스로 가진다. 카메라 컷은 레벨 위에 자막만 띄우는 시네마틱에 함께 둘 수 있다.
	const bool bCinematic = !Cinematic.IsNull();
	if (Trigger == ENUQuestSceneTrigger::OnDialogueLine)
	{
		if (TriggerDialogueRow.IsNone() || CameraShotId.IsNone() || bCinematic || !DialogueId.IsNone() || !WorldSequence.IsNull() || TalkNpcId > 0)
		{
			OutError = TEXT("대사 중 연출은 TriggerDialogueRow와 CameraShotId만 지정하며 기존 대화는 그대로 유지합니다.");
			return false;
		}
	}
	else if (!TriggerDialogueRow.IsNone())
	{
		OutError = TEXT("TriggerDialogueRow는 OnDialogueLine에만 지정합니다.");
		return false;
	}
	if ((bCinematic && (!DialogueId.IsNone() || !WorldSequence.IsNull())) || (!bCinematic && DialogueId.IsNone() && TalkNpcId <= 0 && CameraShotId.IsNone() && WorldSequence.IsNull()))
	{
		OutError = TEXT("대사(DialogueId), 카메라 컷(CameraShotId), 월드 동작(WorldSequence)을 조합하거나 시네마틱(Cinematic)을 지정합니다. 시네마틱에는 대사와 월드 동작을 함께 두지 않습니다.");
		return false;
	}

	// 말을 거는 연출은 그 NPC의 퀘스트 대사를 쓰므로 따로 대사·시네마틱·월드 동작을 두지 않는다.
	if (TalkNpcId > 0 && (bCinematic || !DialogueId.IsNone() || !WorldSequence.IsNull()))
	{
		OutError = TEXT("말을 거는 연출(TalkNpcId)에는 대사(DialogueId), 시네마틱, 월드 동작을 함께 두지 않습니다.");
		return false;
	}

	if (Trigger == ENUQuestSceneTrigger::OnApproach)
	{
		const bool bNpc = ApproachNpcId > 0;
		if (bNpc == !ApproachActorId.IsNone() || (bNpc && ApproachRadius <= 0.f))
		{
			OutError = TEXT("가까이 가면 나오는 연출은 NPC 반경(ApproachNpcId, 0보다 큰 ApproachRadius)과 접근 트리거(ApproachActorId) 중 하나만 지정합니다.");
			return false;
		}
	}
	else if (ApproachNpcId > 0 || !ApproachActorId.IsNone() || DwellSeconds > 0.f)
	{
		OutError = TEXT("접근 범위와 머무는 시간은 가까이 가면 나오는 연출(OnApproach)에만 지정합니다.");
		return false;
	}

	// 스킵은 시네마틱의 스킵 버튼에서만 생긴다.
	if (!SkipAlsoSceneIds.IsEmpty() && !bCinematic)
	{
		OutError = TEXT("함께 건너뛸 연출(SkipAlsoSceneIds)은 시네마틱(Cinematic) 연출에만 지정합니다.");
		return false;
	}

	// 수락 후·완료 후처럼 계속 참인 조건이면 접속할 때마다 다시 나온다.
	if (bPlayIfAlreadyMet && (Trigger != ENUQuestSceneTrigger::OnCondition || Condition.State != ENUQuestConditionState::InProgress))
	{
		OutError = TEXT("접속 때 이미 참이어도 재생하는 연출(bPlayIfAlreadyMet)은 OnCondition이고, 조건이 진행 중(InProgress)이어야 합니다.");
		return false;
	}

	return true;
}

void FNUQuestSceneRow::OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems)
{
	FString Error;
	if (!Validate(Error))
	{
		OutCollectedImportProblems.Add(FString::Printf(TEXT("연출 행 %s: %s"), *InRowName.ToString(), *Error));
	}
}
