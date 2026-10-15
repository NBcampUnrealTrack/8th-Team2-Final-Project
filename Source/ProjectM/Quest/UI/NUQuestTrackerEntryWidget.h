#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NUScreenMotion.h"
#include "NUQuestTrackerEntryWidget.generated.h"

class UTextBlock;
class UButton;
class UWidget;

/**
* 트래커에 한 줄로 표시되는 고정 퀘스트 항목의 WBP 베이스
* 여러 퀘스트를 동시에 표시하기 위해 UNUQuestWidget이 이 위젯을 필요한 개수만큼 만든다.
*/
UCLASS()
class PROJECTM_API UNUQuestTrackerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 트래커가 갱신될 때마다 호출
	void SetQuestDisplay(FName InQuestId, const FText& Title, const FText& Objective, bool bReadyToTurnIn);

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Tracker")
	FName GetQuestId() const { return QuestId; }

	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Tracker")
	void UntrackQuest();

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Tracker")
	FText GetQuestTitle() const { return QuestTitle; }

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Tracker")
	FText GetObjectiveText() const { return ObjectiveText; }

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Tracker")
	bool IsReadyToTurnIn() const { return bIsReadyToTurnIn; }

	// 방금 수락해 추적 창에 새로 붙은 줄: 오른쪽에서 미끄러져 들어오며 한 번 빛난다 (EntryFlash 가 있으면)
	void PlayIntro();

	// 진행 중 → 보고 가능으로 바뀐 순간: 줄이 한 번 빛나고 "보고" 칩이 튀어나온다
	void PlayReadyFlash();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 보고 가능일 때 제목 색·마름모 색을 바꾼다. 끄면 색은 WBP 그대로 (BP_OnTrackerEntryUpdated 로 직접 처리)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Tracker")
	bool bApplyReadyStyle = false;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Tracker", meta = (EditCondition = "bApplyReadyStyle"))
	FSlateColor NormalTitleColor = FSlateColor(FLinearColor(0.88f, 0.85f, 0.79f, 1.f));

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Tracker", meta = (EditCondition = "bApplyReadyStyle"))
	FSlateColor ReadyTitleColor = FSlateColor(FLinearColor(0.91f, 0.68f, 0.27f, 1.f));

	// 연출은 WBP에서 처리
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Quest|Tracker", meta = (DisplayName = "On Tracker Entry Updated"))
	void BP_OnTrackerEntryUpdated(bool bReadyToTurnIn);

private:
	// 기본 Button 또는 WBP에서 UntrackQuest 호출로 연결한다. 생략해도 동작한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> UntrackButton;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Tracker", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> EntryTitleText;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Tracker", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> EntryObjectiveText;

	// 선택: 보고 가능일 때만 보이는 칩
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EntryTurnInBadge;

	// 선택: 줄 뒤의 금빛 판 (평소 불투명도 0). 수락·보고 가능 순간 한 번 빛난다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EntryFlash;

	// 선택: 보고 가능일 때 금빛 바탕을 계속 깐다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EntryReadyBack;

	// 선택: 제목 앞 마름모 (보고 가능이면 ReadyTitleColor 로 물든다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EntryDiamond;

	void ApplyReadyStyle();

	FNUScreenMotion Motion;

	FName QuestId = NAME_None;
	FText QuestTitle;
	FText ObjectiveText;
	bool bIsReadyToTurnIn = false;
};
