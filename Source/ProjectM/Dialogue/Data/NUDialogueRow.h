#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUDialogueRow.generated.h"

class UTexture2D;

// 대화창 이름 칸에 누구를 표시할지. 재생 시점은 Phase·EventId·StoryTrigger가 따로 정한다.
// 연출 대사는 NPC가 자동 재생 조건으로 말하는 줄이라 Npc를 쓴다.
UENUM(BlueprintType)
enum class ENUDialogueSpeaker : uint8
{
	Npc,
	Narration,
	Player
};

// NPCDialogue.csv의 한 문장. 같은 DialogueId의 행을 LineIndex 순서대로 재생한다.
// NPC에게 말을 걸 때의 조건(NpcId·QuestId·Phase)이나 코드 호출 ID(EventId)를 함께 두고,
// 조건이 없는 대화는 연출 표(QuestSceneTable)가 DialogueId로 불러 재생한다. DialogueId가 없으면 미연결 원문으로 보존한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	// 재생할 대화의 ID와 문장 순서. 행 이름은 개별 문장의 고정 ID다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	FName DialogueId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	int32 LineIndex = 0;

	// 같은 DialogueId의 모든 행은 동일한 재생 조건을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	int32 NpcId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	int32 QuestId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	int32 ObjectiveId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	ENUQuestInteractionPhase Phase = ENUQuestInteractionPhase::Idle;

	// 일반·회복 안내처럼 코드에서 직접 요청하는 대화의 ID.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	FName EventId;

	// 지역화 키와 발화자. NPC 이름·기본 초상화는 기존 NPC 테이블을 우선 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	FName DialogueKey;

	// 나레이션은 이름 칸을 비우고, 독백(Player)은 재생할 때 플레이어 캐릭터 이름을 넣는다. 둘 다 화자 칸을 비워 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	ENUDialogueSpeaker Speaker = ENUDialogueSpeaker::Npc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue", meta = (ClampMin = "0"))
	int32 SpeakerNpcId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue", meta = (MultiLine = "true"))
	FText DialogueText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	TSoftObjectPtr<UTexture2D> Portrait;

	// NPC 상호작용 조건도 코드 호출 ID도 없는, 연출 표가 불러 재생하는 대화인지.
	bool IsSceneDialogue() const;

	bool Validate(FString& OutError) const;

	virtual void OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
