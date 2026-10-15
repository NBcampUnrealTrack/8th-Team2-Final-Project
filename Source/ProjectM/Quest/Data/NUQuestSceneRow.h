#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Quest/Data/NUQuestStateCondition.h"
#include "NUQuestSceneRow.generated.h"

class UNUStorySlideshowData;
class ULevelSequence;

// 연출이 시작되는 방식. 언제 참인지는 Condition이 정한다.
UENUM(BlueprintType)
enum class ENUQuestSceneTrigger : uint8
{
	// 이번 접속에서 조건이 참이 되는 순간 한 번
	OnCondition,
	// 조건이 참인 동안 NPC 반경이나 접근 트리거 범위에 들어서면 한 번
	OnApproach,
	// 기존 대사 행이 표시되는 순간 카메라 컷만 겹쳐 보여준다.
	OnDialogueLine
};

// QuestSceneTable.csv의 한 줄. 행 이름이 연출 ID다.
// 무엇을 말하는지는 대사 표가, 언제·어디서·어떻게 보여줄지는 이 표가 정한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestSceneRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	ENUQuestSceneTrigger Trigger = ENUQuestSceneTrigger::OnCondition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	FName TriggerDialogueRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	FNUQuestStateCondition Condition;

	// OnApproach에서 들어설 범위. NPC 반경(ApproachNpcId, ApproachRadius)과 배치한 접근 트리거(ApproachActorId) 중 하나를 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene", meta = (ClampMin = "0"))
	int32 ApproachNpcId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene", meta = (ClampMin = "0"))
	float ApproachRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	FName ApproachActorId;

	// 범위 안에 이만큼 머물러야 재생한다. "오래 들여다보면" 같은 연출에 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene", meta = (ClampMin = "0"))
	float DwellSeconds = 0.f;

	// 재생할 것. 대사(DialogueId)와 시네마틱 중 하나를 쓰고, 둘 다 비우면 카메라 컷만 보여준다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	FName DialogueId;

	// 대사 대신 이 NPC에게 말을 건 것처럼 그 NPC의 지금 퀘스트 대사를 연다. 끝까지 읽으면 직접 말을 건 것과 같이 목표를 진행한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene", meta = (ClampMin = "0"))
	int32 TalkNpcId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	TSoftObjectPtr<UNUStorySlideshowData> Cinematic;

	// 기존 대사는 그대로 두고 월드의 인물·소품 행동을 함께 재생한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	TSoftObjectPtr<ULevelSequence> WorldSequence;

	// 대사를 읽는 동안(대사가 없으면 컷의 머무는 시간 동안, 시네마틱이면 재생하는 동안) 보여줄 카메라 컷.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	FName CameraShotId;

	// 시네마틱을 사용자가 스킵하면 함께 건너뛸 연출 ID. 끝까지 보면 적용하지 않는다.
	// 컷 1을 스킵할 때 이어지는 컷 2까지 한 번에 넘기는 데 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	TArray<FName> SkipAlsoSceneIds;

	// 접속했을 때 이미 조건이 참이어도 재생한다. 접속할 때마다 다시 나올 수 있어 진행 중(InProgress) 조건에만 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	bool bPlayIfAlreadyMet = false;

	// 기획서 대조용 메모. 게임에서는 쓰지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Scene")
	FString DesignNote;

	bool Validate(FString& OutError) const;

	virtual void OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems) override;
};
