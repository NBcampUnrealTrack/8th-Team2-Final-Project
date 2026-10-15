#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Interaction/Data/NUInteractionTypes.h"
#include "UI/Common/NUTextReveal.h"
#include "UI/NUScreenMotion.h"
#include "NUDialogueWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UNUInteractionOptionEntryWidget;
class UNU_CloseButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUDialogueClosed, bool, bCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUDialogueOptionSelected, FNUInteractionOptionHandle, Handle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUDialogueLinePresented, FName);

/**
 * 기존 하단 대화 패널과 그 위쪽 선택지를 함께 표시하는 WBP 베이스
 * 레이아웃은 WBP가 담당하고, 현재 줄과 선택 실행 연결만 이 클래스가 담당함
 */
UCLASS()
class PROJECTM_API UNUDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnNUDialogueLinePresented OnLinePresented;
	bool OpenDialogue(UNUDialogueDefinition* Definition);
	bool OpenDialogueLines(const TArray<FNUDialogueLine>& Lines);
	bool OpenInteractionConversation(UNUDialogueDefinition* OpeningDefinition, const TArray<FNUInteractionOption>& Options);
	bool HandleInputKey(const FKey& Key);

	UFUNCTION(BlueprintCallable, Category = "NU|Dialogue")
	void AdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "NU|Dialogue")
	void CloseDialogue(bool bCompleted = false);

	UFUNCTION(BlueprintPure, Category = "NU|Dialogue")
	bool IsDialogueOpen() const { return bDialogueOpen; }

	UFUNCTION(BlueprintPure, Category = "NU|Dialogue")
	bool IsTextRevealing() const { return TextReveal.IsRevealing(); }

	// 대화창을 여는 쪽(대화 컨트롤러)이 읽는다: 열려 있는 동안 필드 HUD를 접을지
	bool ShouldHideFieldHUD() const { return bHideFieldHUDWhileOpen; }

	UPROPERTY(BlueprintAssignable, Category = "NU|Dialogue")
	FOnNUDialogueClosed OnDialogueClosed;

	UPROPERTY(BlueprintAssignable, Category = "NU|Dialogue")
	FOnNUDialogueOptionSelected OnOptionSelected;

#if WITH_EDITOR
	void SetOptionEntryWidgetClass(TSubclassOf<UNUInteractionOptionEntryWidget> InClass);
	int32 GetActiveOptionCountForTesting() const { return ActiveOptions.Num(); }
	int32 GetFocusedOptionIndexForTesting() const { return FocusedOptionIndex; }
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 패드 왼쪽 스틱 위/아래 = 선택지 이동 (9/15 패드). 스틱은 축이라 키 처리로 안 들어온다
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Interaction")
	TSubclassOf<UNUInteractionOptionEntryWidget> OptionEntryWidgetClass;

	// 초당 표시할 글자 수. 0이면 문장을 즉시 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Text", meta = (ClampMin = "0", UIMin = "0", UIMax = "120"))
	float TextCharactersPerSecond = 30.0f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> DialoguePanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SpeakerNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DialogueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> NextButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNU_CloseButtonWidget> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> InteractionOptionPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> InteractionOptionScroll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> OptionList;

	// ---- 모양 선택 (기본값은 모두 꺼져 있어 기존 WBP는 그대로) ----

	// 대화가 열려 있는 동안 필드 HUD(채팅·메뉴·지도·퀘스트 추적·지역 이름)를 접는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Look")
	bool bHideFieldHUDWhileOpen = false;

	// 열릴 때 대화 줄과 선택지가 아래에서 OpenRise만큼 떠오르며 나타난다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Look")
	bool bPlayOpenMotion = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Look", meta = (EditCondition = "bPlayOpenMotion"))
	float OpenRise = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Look", meta = (EditCondition = "bPlayOpenMotion"))
	float OpenDuration = 0.2f;

	// 대사 없이 선택지만 여는 대화는 대화 줄(DialoguePanel)을 접는다 (빈 자막 띠가 남지 않게)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Look")
	bool bCollapseDialoguePanelWithoutLines = false;

	// 이름이 빈 줄(나레이션)이면 접는 이름 묶음 (이름 글자·무늬·밑줄)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SpeakerNameRow;

	// 다음으로 넘기는 표시 (NextButton 안). 문장이 다 나오면 NextBobAmplitude만큼 천천히 위아래로 움직인다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NextIndicator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Look")
	float NextBobAmplitude = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Dialogue|Look")
	float NextBobPeriod = 1.1f;

private:
	bool OpenDialogueInternal(const TArray<FNUDialogueLine>* Lines, const TArray<FNUInteractionOption>* Options);
	bool RebuildOptionEntries();
	void ClearOptionEntries();
	void RefreshCurrentLine();
	void StartTextReveal();
	void AdvanceTextReveal();
	void FinishTextReveal();
	void StopTextReveal();
	void RefreshNavigation();
	void RefreshSpeakerRow(const FText& SpeakerName);
	void PlayOpenMotion();
	void ResetDialogueState();
	void RequestDialogueFocus();
	void ApplyDialogueFocus();
	FReply HandleDialogueKey(const FKey& Key);
	bool FocusFirstEnabledOption();
	bool MoveOptionFocus(int32 Direction);
	bool FocusOption(int32 Index);
	bool ActivateFocusedOption();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleOptionSelected(FNUInteractionOptionHandle Handle);

	UFUNCTION()
	void HandleOptionFocused(FNUInteractionOptionHandle Handle);

	UPROPERTY(Transient)
	TArray<FNUDialogueLine> ActiveLines;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUInteractionOptionEntryWidget>> OptionEntryWidgets;

	TArray<FNUInteractionOption> ActiveOptions;
	int32 CurrentLineIndex = INDEX_NONE;
	int32 FocusedOptionIndex = INDEX_NONE;

	FNUTextReveal TextReveal;
	FTimerHandle TextRevealTimer;
	int32 RevealedCharacterCount = 0;

	// 스틱 이동 반복 제어 --- 기울인 채 있으면 0.3초마다 한 칸, 가운데로 돌아오면 바로 다시 받는다
	bool bStickCentered = true;
	double LastStickMoveTime = 0.0;
	bool bDialogueOpen = false;
	bool bOptionMode = false;

	FNUScreenMotion OpenMotion;
	ESlateVisibility DialoguePanelRestVisibility = ESlateVisibility::Visible;
	float NextBobTime = 0.f;
};
