#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUQuestPracticeGuideWidget.generated.h"

class UCanvasPanel;
class UInputAction;
class UNativeWidgetHost;
class UTextBlock;
class SNUQuestPracticeGuideArt;
struct FNUQuestPracticeGuideText;
enum class ENUQuestBattleKind : uint8;

// 전투 튜토리얼 (기본 공격·패링·회피 연습) 안내 카드. 연습 전투 동안 필드 퀘스트 추적 창 자리에 하는 법과 횟수를 보여 준다.
// 전투는 멈추지 않는다(온라인 전투는 서버가 진행한다). 횟수를 세지는 않고 UNUQuestEncounterSubsystem이 넘겨 준 값만 그린다.
// 문구는 프로젝트 설정 NU Quest Data의 Practice Guide 칸. WBP 없이 스스로 위젯 트리를 만든다(패링 타이밍 막대와 같은 방식).
UCLASS()
class PROJECTM_API UNUQuestPracticeGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|Quest|PracticeGuide")
	void SetPractice(ENUQuestBattleKind InKind, int32 InCount, int32 InRequired);

	// 연습이 끝나면 부른다. 흐려진 뒤 화면에서 빠진다.
	void Dismiss();
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void RefreshTexts();
	void RefreshCount();
	void RebuildSteps(const FNUQuestPracticeGuideText& Guide);
	void RebuildSummary(const FNUQuestPracticeGuideText& Guide);
	const FNUQuestPracticeGuideText* FindGuideText() const;
	FText FormatGuideText(const FText& Source) const;
	FSlateFontInfo GetTitleFont(int32 Size) const;
	// 전투 무대가 화면에 떠 있는지. 로컬 전투는 전투 객체가 있는 동안, 서버 전투는 넷 전투 세트가 준비된 동안이다.
	bool IsBattleStageVisible() const;
	bool IsGamepadMode() const;

	// 왼쪽 TURN 표시 아래. 세 연습 모두 승인 시안의 505:620 비율로 동일하게 표시한다.
	UPROPERTY(EditAnywhere, Category = "NU|Quest|PracticeGuide")
	FVector2D CardOffset = FVector2D(24.f, 240.f);

	UPROPERTY(EditAnywhere, Category = "NU|Quest|PracticeGuide", meta = (ClampMin = "200"))
	float CardWidth = 300.f;

	// 제목 글꼴. 퀘스트 의뢰 창 제목과 같은 마루부리
	UPROPERTY(EditAnywhere, Category = "NU|Quest|PracticeGuide")
	TSoftObjectPtr<UObject> TitleFont = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Art/Font/MaruBuri-Bold_Font.MaruBuri-Bold_Font")));

	// 문구의 {Confirm}에 넣을 전투 명령 확인 입력 (지금 장치에 묶인 키를 읽는다)
	UPROPERTY(EditAnywhere, Category = "NU|Quest|PracticeGuide")
	TSoftObjectPtr<UInputAction> ConfirmAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/UI/IA_UI_Confirm.IA_UI_Confirm")));

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> CardRoot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CardEyebrow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CardTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CardIndex;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CardSteps;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CardSummary;

	// 장식은 Slate로 직접 그리고 문구·입력 키는 UMG 텍스트로 유지한다.
	UPROPERTY(Transient)
	TObjectPtr<UNativeWidgetHost> CardArtHost;

	TSharedPtr<SNUQuestPracticeGuideArt> CardArt;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CardCountLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CardCount;

	ENUQuestBattleKind Kind{};
	int32 Count = 0;
	int32 Required = 0;
	bool bKindSet = false;
	bool bGamepadMode = false;
	bool bWasStageVisible = false;
	float CardAlpha = 0.f;
	float CountPulse = 0.f;
	bool bDismissing = false;
	float DismissAlpha = 1.f;
};
