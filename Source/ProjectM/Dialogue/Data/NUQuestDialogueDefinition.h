#pragma once

#include "CoreMinimal.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Engine/DataAsset.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestDialogueDefinition.generated.h"

/**
 * 퀘스트 하나의 선형 대사를 관리
 * Complete, Target 상태는 선택적으로 추가 가능
 */
UCLASS(BlueprintType)
class PROJECTM_API UNUQuestDialogueDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FNUDialogueLine>* FindLines(ENUQuestInteractionPhase Phase) const;
	bool HasPhase(ENUQuestInteractionPhase Phase) const;
	FName FindDialogue(ENUQuestInteractionPhase Phase) const;
	bool ValidateDefinition(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Identity")
	FName QuestId = NAME_None;

	// 같은 QuestId에서 특정 NPC, 오브젝트만 사용하는 대사 변형 식별자
	// 비어 있으면 퀘스트 공용 대사만 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Identity")
	FName VariantId = NAME_None;

	// 이전 Blueprint의 에셋 참조만 유지하며, 단계별 문장과 순서는 테이블에서 읽는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Phase")
	TMap<ENUQuestInteractionPhase, FName> PhaseDialogues;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Phase", meta = (TitleProperty = "{RowName} {DialogueText}"))
	TArray<FNUDialogueLine> Offer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Phase", meta = (TitleProperty = "{RowName} {DialogueText}"))
	TArray<FNUDialogueLine> InProgress;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Phase", meta = (TitleProperty = "{RowName} {DialogueText}"))
	TArray<FNUDialogueLine> TurnIn;

	// 현재 완료한 일회성 퀘스트는 선택지에서 숨긴다. 정책 변경에 대비한 선택 데이터다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Optional", meta = (TitleProperty = "{RowName} {DialogueText}"))
	TArray<FNUDialogueLine> Completed;

	// 진행 중 목표 액터와 상호작용할 때 사용한다. 런타임 Option이 QuestId를 보존해야 선택된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Quest|Optional", meta = (TitleProperty = "{RowName} {DialogueText}"))
	TArray<FNUDialogueLine> Target;
};
