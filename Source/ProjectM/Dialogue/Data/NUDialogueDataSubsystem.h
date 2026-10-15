#pragma once

#include "CoreMinimal.h"
#include "Dialogue/Data/NUDialogueRow.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUDialogueDataSubsystem.generated.h"

// NPC 대사와 연출 표가 부르는 대사의 정적 데이터 조회를 담당한다. 언제 재생할지는 연출 표(QuestSceneTable)가 정한다.
UCLASS()
class PROJECTM_API UNUDialogueDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 상호작용 조건 또는 직접 호출 ID로 재생할 대화를 찾는다.
	FName FindQuestDialogue(int32 NpcId, const FNUQuestInteractionOption& Option) const;
	FName FindEventDialogue(FName EventId) const;
	TArray<FName> GetInProgressQuestIds(int32 NpcId) const;

	UFUNCTION(BlueprintCallable, Category = "NU|Dialogue")
	bool GetLines(FName DialogueId, TArray<FNUDialogueLine>& OutLines) const;

	// 검증을 통과한 대화가 있는지. 연출 표의 참조를 확인할 때 쓴다.
	bool HasDialogue(FName DialogueId) const;

	// 이전 에셋 호출도 테이블을 우선 사용한다.
	bool GetDefinitionLines(const UNUDialogueDefinition* Definition, TArray<FNUDialogueLine>& OutLines) const;

private:
	// 재임포트하면 조건과 문장 순서를 함께 갱신한다.
	struct FDialogue
	{
		int32 NpcId = 0;
		FName QuestId;
		FName ObjectiveId;
		ENUQuestInteractionPhase Phase = ENUQuestInteractionPhase::Idle;
		FName EventId;
		TMap<int32, FDataTableRowHandle> Lines;
		bool bValid = true;

		// NPC 조건도 호출 ID도 없어 연출 표만 부르는 대화다.
		bool IsSceneDialogue() const;
	};

	void RebuildIndex();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDataTable>> Tables;

	TMap<FName, FDialogue> Dialogues;
	TMap<int32, TArray<FName>> NpcDialogues;
	TMap<FName, FName> EventDialogues;
};
