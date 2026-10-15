#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Quest/Data/NUQuestTypes.h"
#include "Styling/SlateTypes.h"
#include "NUQuestJournalWidget.generated.h"

class UBorder;
class UListView;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UWidget;
class UNUQuestComponent;
class UNUQuestJournalListItem;
struct FNUQuestJournalMotionState;

UENUM(BlueprintType)
enum class ENUQuestJournalStateFilter : uint8
{
	Available,
	InProgress,
	Completed,
};

/**
 * Q로 여는 퀘스트 목록 WBP의 C++ 베이스.
 * 필터링, 목록 선택, 상세 정보와 추적 요청을 담당하고 레이아웃은 WBP가 담당한다.
 */
UCLASS()
class PROJECTM_API UNUQuestJournalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Journal")
	void OpenJournal();

	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Journal")
	void CloseJournal();

	// Forced gameplay/session transitions bypass the cosmetic closing delay.
	void CloseJournalImmediately();
	bool IsJournalClosing() const;

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Journal")
	bool IsJournalOpen() const;

	// 무효 태그를 넣으면 전체 분류를 표시한다
	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Journal")
	void SetCategoryFilter(UPARAM(meta = (Categories = "NU.Quest.Category")) FGameplayTag NewFilter);

	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Journal")
	void SetStateFilter(ENUQuestJournalStateFilter NewFilter);

	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Journal")
	void TrackSelectedQuest();

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Journal")
	FGameplayTag GetCategoryFilter() const { return CategoryFilter; }

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Journal")
	ENUQuestJournalStateFilter GetStateFilter() const { return StateFilter; }

	void RefreshQuestComponent();
	void RefreshJournal();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 탭 선택 색상과 애니메이션은 WBP가 담당한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Quest|Journal", meta = (DisplayName = "On Journal Filter Changed"))
	void BP_OnJournalFilterChanged(
		UPARAM(meta = (Categories = "NU.Quest.Category")) FGameplayTag NewCategoryFilter,
		ENUQuestJournalStateFilter NewStateFilter
	);

	// When on, the category tabs are restyled here after the WBP graph's filter event (glass journal).
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style")
	bool bUseTabStyles = false;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style", meta = (EditCondition = "bUseTabStyles"))
	FButtonStyle TabStyle;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style", meta = (EditCondition = "bUseTabStyles"))
	FButtonStyle SelectedTabStyle;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style", meta = (EditCondition = "bUseTabStyles"))
	FSlateColor TabLabelColor;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style", meta = (EditCondition = "bUseTabStyles"))
	FSlateColor SelectedTabLabelColor;

	// State headers ("진행 중 1" ...) keep this label color regardless of the graph.
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style", meta = (EditCondition = "bUseTabStyles"))
	FSlateColor StateLabelColor;

private:
	void NotifyFilterChanged();
	void ApplyTabStyles();
	void BindQuestComponent(UNUQuestComponent* NewQuestComponent);
	void SetJournalVisible(bool bVisible);
	void StartJournalMotion(bool bOpening, bool bFromHidden);
	void StopJournalMotion(bool bRestoreVisuals);
	bool TickJournalMotion(float DeltaSeconds);
	void RefreshJournalOutlines();
	void HandleJournalEntryGenerated(UUserWidget& Entry);
	void HandleJournalEntryReleased(UUserWidget& Entry);
	void FinishJournalClose();
	void BindQuestListView(UListView* ListView);
	void UnbindQuestListView(UListView* ListView);
	void UpdateStateSectionVisibility();
	void RefreshDetails();
	void ClearDetails();
	bool MatchesCategoryFilter(const FGameplayTag& InCategoryTag) const;
	FText BuildRequirementText(const FNUQuestRequirementSpec& Requirement) const;
	FText BuildObjectiveText(FName QuestId) const;
	FText BuildRewardText(FName QuestId) const;

	UFUNCTION()
	void HandleQuestProgressChanged();

	UFUNCTION()
	void HandleCloseClicked();

	void HandleQuestSelectionChanged(UObject* SelectedItem);

	// 명명 계약: 카테고리/상태 필터 버튼 8개는 .cpp의 NU_BIND_FILTER_BUTTON 매크로로 바인딩한다.
	// (AddDynamic/RemoveDynamic은 매크로라 컴파일타임 함수 리터럴이 필요해서 함수로 감쌀 수 없다 —
	// 'AddUFunction'/런타임 FName 바인딩은 이 델리게이트 타입들엔 없다.)

	// LegacyText가 있으면(버튼 밖에 텍스트가 따로 있는 레거시 레이아웃) 거기에 쓰고,
	// 없으면(WBP_CommonButton으로 교체된 경우) 그 버튼 자신의 라벨에 반영한다.
	void ApplyStateButtonText(const TCHAR* ButtonWidgetName, UTextBlock* LegacyText, const FText& InText);

	UFUNCTION() void HandleAllCategoryClicked();
	UFUNCTION() void HandleMainCategoryClicked();
	UFUNCTION() void HandleSideCategoryClicked();
	UFUNCTION() void HandleDailyCategoryClicked();
	UFUNCTION() void HandleTutorialCategoryClicked();
	UFUNCTION() void HandleAvailableStateClicked();
	UFUNCTION() void HandleInProgressStateClicked();
	UFUNCTION() void HandleCompletedStateClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UNUQuestComponent> BoundQuestComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUQuestJournalListItem>> JournalItems;

	// 목록 높이는 WBP 항목의 실제 DesiredSize로 계산한다.
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> EntrySizePreview;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> QuestListView;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> AvailableQuestListView;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> CompletedQuestListView;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> InProgressListSizeBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> AvailableListSizeBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> CompletedListSizeBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> DetailPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> StateSectionScrollBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> DetailScrollBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyListText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailMetaText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailDescriptionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailObjectiveText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailRewardText;

	// 명명 계약: WBP에 "TrackButton"이 있으면 고정/고정 해제 동작을 C++가 바인딩한다.
	// 순정 Button, UNU_CloseButtonWidget, UNU_CommonButtonWidget 셋 다 지원하려면 BindWidget으로
	// 타입을 하나로 고정할 수 없어 이름으로 찾아 캐스팅한다 (CloseButton과 동일 패턴).
	void BindTrackButton();
	void UnbindTrackButton();
	// SetIsEnabled는 UWidget 공통 API라 실제 타입과 무관하게 쓸 수 있어 범용 포인터로 캐싱해둔다.
	TWeakObjectPtr<UWidget> TrackButtonWidget;

	// 레거시 레이아웃(버튼 밖에 별도 텍스트)에서만 쓰인다 — WBP_CommonButton으로 바꾸면
	// 라벨은 그 버튼 내부 Txt_Label이 대신하므로 이 위젯은 없어도 된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TrackButtonText;

	// "고정"/"고정 해제" 문구를 TrackButtonText가 있으면 거기에, 없으면(WBP_CommonButton으로
	// 교체된 경우) 버튼 자신의 라벨에 반영한다.
	void SetTrackButtonLabel(const FText& InText);

	// 명명 계약: WBP에 "CloseButton"이 있으면 닫기 동작을 C++가 바인딩한다.
	// 순정 Button(레거시)과 UNU_CloseButtonWidget(WBP_CloseButton, 신규 공용 부품) 둘 다 지원하려면
	// BindWidget으로 타입을 하나로 고정할 수 없어 이름으로 찾아 캐스팅한다 (NUSheetWidgetBase와 동일 패턴).
	void BindCloseButton();
	void UnbindCloseButton();

	// 레거시 레이아웃(버튼 안 자식 텍스트)에서만 쓰인다 — WBP_CommonButton으로 바꾸면
	// 카운트 문구는 ApplyStateButtonText가 그 버튼 자신의 라벨에 대신 반영한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AvailableStateText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InProgressStateText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompletedStateText;

	// 무효 태그 = 전체 분류
	FGameplayTag CategoryFilter;
	ENUQuestJournalStateFilter StateFilter = ENUQuestJournalStateFilter::InProgress;
	FName SelectedQuestId = NAME_None;
	int32 AvailableQuestCount = 0;
	int32 InProgressQuestCount = 0;
	int32 CompletedQuestCount = 0;
	bool bAvailableSectionExpanded = false;
	bool bInProgressSectionExpanded = true;
	bool bCompletedSectionExpanded = true;
	bool bUpdatingSelection = false;

	// Owns only weak widget references; a real-time ticker exists during transitions only.
	TSharedPtr<FNUQuestJournalMotionState> JournalMotion;
};
