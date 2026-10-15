#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Dialogue/Data/NUDialogueRow.h"
#include "NUDialogueDefinition.generated.h"

class UTexture2D;

/**
 * 대화창에 한 번에 표시되는 한 줄의 대사.
 * 실제 레이아웃과 연출은 WBP가 담당하고, 이 구조체는 기획 데이터만 보관한다.
 */
USTRUCT(BlueprintType)
struct PROJECTM_API FNUDialogueLine
{
	GENERATED_BODY()

	// 지정하면 아래 직접 입력 값 대신 테이블의 문장을 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue", meta = (RowType = "/Script/ProjectM.NUDialogueRow"))
	FDataTableRowHandle DialogueRow;

	// 표시 중인 문장의 연출 연결용 ID. 본문과 번역 문자열은 변경하지 않는다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Dialogue")
	FName SourceRowName;

	bool Resolve(FNUDialogueLine& OutLine, FString& OutError) const;
	static bool ResolveLines(const TArray<FNUDialogueLine>& SourceLines, TArray<FNUDialogueLine>& OutLines, FString& OutError);

	// 이름 칸에 표시할 화자 종류. 독백(Player)은 대화 시작 시 플레이어 캐릭터 이름으로 채운다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue")
	ENUDialogueSpeaker Speaker = ENUDialogueSpeaker::Npc;

	// 테이블 발화자 정보를 대화 시작 시 NPC 정의에 연결한다. 0은 내레이션 등 직접 입력이다.
	UPROPERTY(BlueprintReadOnly, Category = "NU|Dialogue")
	int32 SpeakerNpcId = 0;

	// 대사를 말하는 인물 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue")
	FText SpeakerName;

	// 화면에 표시할 대사
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue", meta = (MultiLine = "true"))
	FText DialogueText;

	// 줄마다 초상화를 바꿔야 할 때 사용. 지정하지 않으면 초상화 영역을 숨긴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue")
	TSoftObjectPtr<UTexture2D> Portrait;
};

/**
 * 순서대로 재생되는 선형 NPC 대화 데이터.
 * 분기와 조건은 실제 콘텐츠 요구가 생겼을 때 별도 노드 타입으로 확장한다.
 */
UCLASS(BlueprintType)
class PROJECTM_API UNUDialogueDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// 대사 정의의 필수 데이터가 유효한지 검증한다.
	bool ValidateDefinition(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// 로그와 기획 데이터 식별에 사용하는 ID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Identity")
	FName DialogueId = NAME_None;

	// 표시 순서대로 대사 행을 지정한다. 직접 입력은 기존 에셋과 런타임 안내문에 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Content", meta = (TitleProperty = "{RowName} {DialogueText}"))
	TArray<FNUDialogueLine> Lines;
};
