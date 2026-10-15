#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUQuestDataSettings.generated.h"

class UDataTable;
class UNUDialogueDefinition;

// 전투 튜토리얼(연습 전투) 안내 문구 한 벌. 지금 쓰는 장치(키보드·패드)에 맞춰 바뀌는 자리:
// {Key} 반응 키(키 설정의 Parry), {Confirm} 전투 명령 확인 키, {LeftRight} 좌우 이동 키
USTRUCT(BlueprintType)
struct FNUQuestPracticeGuideText
{
	GENERATED_BODY()

	FNUQuestPracticeGuideText() = default;
	FNUQuestPracticeGuideText(const FText& InEyebrow, const FText& InTitle, const TArray<FText>& InSteps, const FText& InCountLabel, const FText& InSummary)
		: Eyebrow(InEyebrow), Title(InTitle), Steps(InSteps), CountLabel(InCountLabel), Summary(InSummary)
	{
	}

	// 제목 위의 작은 말 (예: 전투 튜토리얼 ②)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Practice Guide")
	FText Eyebrow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Practice Guide")
	FText Title;

	// 하는 법. 줄마다 번호가 붙는다. 카드 한 줄에 들어가게 짧게, 세 줄 안쪽으로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Practice Guide")
	TArray<FText> Steps;

	// 횟수 앞에 붙는 말 (예: 패링 성공)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Practice Guide")
	FText CountLabel;

	// 카드 아래 요약. {Chance}는 같은 크기의 금색 확률로 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Practice Guide")
	FText Summary;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Quest Data"))
class PROJECTM_API UNUQuestDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Data")
	TSoftObjectPtr<UDataTable> QuestDefinitionTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectM.NUQuestObjectiveRow"))
	TSoftObjectPtr<UDataTable> QuestObjectiveTable;


	// 언제·어디서·어떻게 대사와 시네마틱을 보여줄지 정하는 연출 표. 클라이언트 연출 전용이다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Presentation", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectM.NUQuestSceneRow"))
	TSoftObjectPtr<UDataTable> QuestSceneTable;

	// 로컬 액터의 등장(표시·배치)과 선택적인 퀘스트 역할. 일반 상호작용은 액터에 남는다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Presentation", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectM.NUActorPresenceRow"))
	TSoftObjectPtr<UDataTable> ActorPresenceTable;

	// 온라인 진정 전투를 클라이언트 판정으로 돌리고 결과만 서버에 보고한다(QuestCalmStart 105 → QuestCalmResult 106).
	// 끄면 서버 전투(BattleStart 40, 승리 = 성공)를 쓴다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Battle")
	bool bUseClientCalmJudgement = false;

	// 전투 튜토리얼 ①②③(Q3 기본 공격, Q5 패링·회피) 동안 화면에 띄우는 안내. 횟수는 서버(온라인)·로컬 규칙(오프라인)이 센 값을 그대로 보여 준다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Battle|Practice Guide")
	FNUQuestPracticeGuideText BasicAttackGuide = FNUQuestPracticeGuideText(
		NSLOCTEXT("NUQuestPracticeGuide", "BasicAttackEyebrow", "전투 연습"),
		NSLOCTEXT("NUQuestPracticeGuide", "BasicAttackTitle", "기본 공격"),
		TArray<FText>{
			NSLOCTEXT("NUQuestPracticeGuide", "BasicAttackStep1", "공격 선택 {Confirm}"),
			NSLOCTEXT("NUQuestPracticeGuide", "BasicAttackStep2", "{LeftRight} SP 배분"),
			NSLOCTEXT("NUQuestPracticeGuide", "BasicAttackStep3", "{Confirm} 공격 실행")
		},
		NSLOCTEXT("NUQuestPracticeGuide", "BasicAttackCount", "기본 공격"),
		NSLOCTEXT("NUQuestPracticeGuide", "BasicAttackSummary", "SP 사용 시 연속 공격 가능"));

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Battle|Practice Guide")
	FNUQuestPracticeGuideText ParryGuide = FNUQuestPracticeGuideText(
		NSLOCTEXT("NUQuestPracticeGuide", "ParryEyebrow", "전투 연습"),
		NSLOCTEXT("NUQuestPracticeGuide", "ParryTitle", "패링"),
		TArray<FText>{
			NSLOCTEXT("NUQuestPracticeGuide", "ParryStep1", "적의 공격 시 게이지 확인"),
			NSLOCTEXT("NUQuestPracticeGuide", "ParryStep2", "주황색 구간에서 {Key}"),
			NSLOCTEXT("NUQuestPracticeGuide", "ParryStep3", "성공 시 피해 감소")
		},
		NSLOCTEXT("NUQuestPracticeGuide", "ParryCount", "패링 성공"),
		NSLOCTEXT("NUQuestPracticeGuide", "ParrySummary", "시도마다 SP 소모"));

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Battle|Practice Guide")
	FNUQuestPracticeGuideText DodgeGuide = FNUQuestPracticeGuideText(
		NSLOCTEXT("NUQuestPracticeGuide", "DodgeEyebrow", "전투 연습"),
		NSLOCTEXT("NUQuestPracticeGuide", "DodgeTitle", "회피"),
		TArray<FText>{
			NSLOCTEXT("NUQuestPracticeGuide", "DodgeStep1", "회피 안내 확인"),
			NSLOCTEXT("NUQuestPracticeGuide", "DodgeStep2", "{Key} 회피 시도"),
			NSLOCTEXT("NUQuestPracticeGuide", "DodgeStep3", "성공 시 피해 완전 회피")
		},
		NSLOCTEXT("NUQuestPracticeGuide", "DodgeCount", "회피 성공"),
		NSLOCTEXT("NUQuestPracticeGuide", "DodgeSummary", "성공 확률 {Chance}"));

	// 기존 콘텐츠 호환용. 새 대사는 Dialogue Data의 StarterNaiaIntroduction 연결을 사용한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Quest Presentation")
	TSoftObjectPtr<UNUDialogueDefinition> StarterNaiaSelectionDialogue;
};
