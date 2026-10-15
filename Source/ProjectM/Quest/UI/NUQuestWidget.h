#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/NU_HudBattleFold.h"
#include "UI/HUD/NU_QuestFx.h"
#include "UI/NURewardPopup.h"
#include "NUQuestWidget.generated.h"

class UBorder;
class UButton;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UNUQuestComponent;
class UNUQuestTrackerEntryWidget;
class UNURewardPopup;
struct FNURewardEntry;

/**
* 퀘스트 진행 표시와 수락/거절을 연결하는 WBP 베이스.
*
*/
UCLASS()
class PROJECTM_API UNUQuestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// PlayerState 복제 시점이 BeginPlay보다 늦을 수 있어 Controller가 다시 호출
	void RefreshQuestComponent();

	// 전투 진입 등 외부 상태 전환에서 열려 있는 퀘스트 제안을 닫기
	void CloseQuestOffer();

	// 제안 패널이 떠 있는지 --- PlayerController가 패드 B로 닫을 때 본다 (9/15 패드)
	bool IsQuestOfferOpen() const { return !OfferedQuestId.IsNone(); }

	// 고정 대상이 바뀌었을 때 PlayerController가 호출
	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Tracker")
	void RefreshTracker();

	UFUNCTION(BlueprintPure, Category = "NU|Quest|Tracker")
	UNUQuestTrackerEntryWidget* FindTrackerEntry(FName QuestId) const;

	// 개발용 (nu.HUD.QuestFx): 0 수락 / 1 목표 달성 / 2 완료 띠를 띄우고 첫 추적 줄에 같은 연출을 준다
	void DebugPlayQuestFx(int32 Kind, const FText& Title);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// 전투 동안 추적 창을 접는다 (필드 HUD와 같은 판정)
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 새 퀘스트가 붙었을 때만 호출한다. 이 시점에는 표시 데이터와 슬롯이 준비돼 있다.
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Quest|Tracker", meta = (DisplayName = "On Tracker Entry Added"))
	void BP_OnTrackerEntryAdded(UNUQuestTrackerEntryWidget* Entry);

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Quest|Tracker", meta = (DisplayName = "On Tracker Entry Removed"))
	void BP_OnTrackerEntryRemoved(FName QuestId);

	// 추가, 제거, 순서 변경 후 동적 슬롯의 배치를 WBP에서 조정한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Quest|Tracker", meta = (DisplayName = "On Tracker Entries Changed"))
	void BP_OnTrackerEntriesChanged();

private:
	void BindQuestComponent(UNUQuestComponent* NewQuestComponent);
	void ShowQuestOffer(FName QuestId);
	void SetOfferVisible(bool bVisible);

	// 고정된 퀘스트 중 표시 가능한 것만 모은다. 보고 가능이 위로 온다.
	// 표시할 수 없게 된 고정은 이 과정에서 해제
	void CollectTrackedQuestIds(TArray<FName>& OutQuestIds);

	void RebuildTrackerEntries(const TArray<FName>& QuestIds);
	void ApplyTrackerPresentationState();

	FText BuildTrackerTitleText(FName QuestId) const;
	FText BuildTrackerObjectiveText(FName QuestId) const;
	FText BuildObjectiveText(FName QuestId) const;
	FText BuildRewardText(FName QuestId) const;

	UFUNCTION()
	void HandleQuestProgressChanged();

	UFUNCTION()
	void HandleQuestAccepted(FName QuestId);

	UFUNCTION()
	void HandleQuestCompleted(FName QuestId);

	// 같은 퀘스트의 후속 연출이 끝난 뒤에도 띄울 수 있도록 보상 창 표시를 분리한다.
	// 완료 띠(QuestFxRoot)가 있으면 띠를 먼저 띄우고 RewardPopupDelay 뒤에 보상 창을 띄운다
	void ShowCompletionReward(FText Title, TArray<FNURewardEntry> Entries);

	// 완료 띠 아랫줄: 받은 보상 몇 가지 ("경험치 120 · 골드 50")
	static FText BuildRewardSummary(const TArray<FNURewardEntry>& Entries);

	UFUNCTION()
	void HandleQuestOfferRequested(FName QuestId);

	UFUNCTION()
	void HandleAcceptClicked();

	UFUNCTION()
	void HandleDeclineClicked();

	UFUNCTION()
	void HandleTrackerCollapseClicked();

	UFUNCTION()
	void HandleTrackerCloseClicked();

	UFUNCTION()
	void HandleTrackerReopenClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UNUQuestComponent> BoundQuestComponent;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> TrackerPanel;

	// 고정 퀘스트 항목이 쌓이는 컨테이너
	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest|Tracker", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> TrackerEntryBox;

	// 스크롤 컨테이너까지 함께 접는다. 기존 WBP는 이 위젯 없이도 동작한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TrackerContent;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TrackerTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> TrackerCollapseButton;

	// 명명 계약: WBP에 "TrackerCloseButton"이 있으면 닫기 동작을 C++가 바인딩한다.
	// 순정 Button(레거시)과 UNU_CloseButtonWidget(WBP_CloseButton, 신규 공용 부품) 둘 다 지원하려면
	// BindWidget으로 타입을 하나로 고정할 수 없어 이름으로 찾아 캐스팅한다 (NUSheetWidgetBase와 동일 패턴).
	void BindTrackerCloseButton();
	void UnbindTrackerCloseButton();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> TrackerReopenButton;

	// WBP_QuestTrackerEntry 클래스
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|UI")
	TSubclassOf<UNUQuestTrackerEntryWidget> TrackerEntryWidgetClass;

	// 항목 내부 여백은 WBP_QuestTrackerEntry에서, 항목 사이 간격은 여기서 조절한다.
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|UI|Layout")
	FMargin TrackerEntryPadding = FMargin(0.0f, 0.0f, 0.0f, 6.0f);

	// 퀘스트 완료 시 띄울 공용 보상 창. 비우면 창을 띄우지 않는다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|UI")
	TSubclassOf<UNURewardPopup> RewardPopupClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Quest|Tracker", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UNUQuestTrackerEntryWidget>> TrackerEntries;

	// 완료 띠가 뜬 뒤 보상 창까지의 간격 (초). 띠 조각이 없는 WBP는 기다리지 않는다. 0보다 작으면 바로
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|UI")
	float RewardPopupDelay = 1.3f;

	// 의뢰 창의 아이템 보상 칸에 마우스를 올리면 뜨는 말풍선 (글자 TipName · TipKind · TipDesc). 비우면 말풍선 없음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|UI")
	TSubclassOf<UUserWidget> RewardTooltipClass;

	// 의뢰 창이 떠 있는 동안 필드 HUD를 접는다 (대화창·상점과 같은 방식). 옛 WBP는 끈 채로 둔다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|UI")
	bool bHideFieldHUDDuringOffer = false;

	// 수락·목표 달성·완료 띠 (조각은 WBP에 이름으로)
	FNUQuestFx QuestFx;

	// 방금 수락한 퀘스트 --- 추적 창에 줄이 생기면 들어오는 연출을 한 번 준다
	FName PendingIntroQuestId = NAME_None;

	struct FPendingReward
	{
		int32 FxId = 0;          // 이 완료 띠가 뜬 뒤부터 Delay 를 센다
		float Delay = 0.f;
		FText Title;
		TArray<FNURewardEntry> Entries;
	};
	TArray<FPendingReward> PendingRewards;

	bool bRefreshingTracker = false;
	bool bTrackerRefreshQueued = false;

	bool bTrackerExpanded = true;
	bool bTrackerClosed = false;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> OfferPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OfferTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OfferSummaryText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OfferObjectiveText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OfferRewardText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> AcceptButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> DeclineButton;

	FName OfferedQuestId = NAME_None;
	FNUHudBattleFold BattleFold;
};

