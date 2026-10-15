#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/UI/NUProgressionWindowBase.h"
#include "UI/NUConfirmDialog.h"
#include "Naia/UI/NUStarterNaiaCardWidget.h"   // TSubclassOf 멤버 --- 여는 쪽 번역 단위에도 완전한 형이 필요
#include "GameplayTagContainer.h"
#include "NUStarterNaiaSelectionWidget.generated.h"

class UBorder;
class UNUFlipbookImage;
class UNUNaiaRosterComponent;
class UNUStarterNaiaSelectionEntryButton;
class UHorizontalBox;
class UImage;
class UPaperFlipbook;
class UTextBlock;
class UTexture2D;
class UWidget;

USTRUCT(BlueprintType)
struct PROJECTM_API FNUStarterNaiaSelectionViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	int32 NaiaId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FText ElementName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FLinearColor ElementColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FText Description;

	// 속성 태그의 끝 이름 (Fire, Water ...) --- 속성 아이콘 표의 키
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FName ElementKey;

	// 타고난 패시브 (PassiveSkill1Id)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FText PassiveName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FText PassiveDescription;

	// 처음 능력치 --- 체력 · SP · 공격 · 마법 공격 · 방어 · 속도
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	TArray<float> Stats;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	FText EvolutionName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	int32 EvolutionLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	TSoftObjectPtr<UPaperFlipbook> Flipbook;
};

// 직업 선택 직후 테이블의 Starter/Stage1 나이아 중 하나를 확정함
UCLASS()
class PROJECTM_API UNUStarterNaiaSelectionWidget : public UNUProgressionWindowBase
{
	GENERATED_BODY()

public:
	void InitializeSelection(UNUNaiaRosterComponent* InRosterComponent, FSimpleDelegate InCloseRequested);

	UFUNCTION(BlueprintCallable, Category = "NU|Naia|UI")
	void SelectNaia(int32 NaiaId);

	UFUNCTION(BlueprintPure, Category = "NU|Naia|UI")
	const TArray<FNUStarterNaiaSelectionViewData>& GetNaiaEntries() const { return NaiaEntries; }

	// 창을 여는 쪽(UNUStarterNaiaSelectionUIController)이 창 옵션에 싣는다
	bool ShouldHideFieldHUD() const { return bHideFieldHUDWhileOpen; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool CanDragWindow() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|UI")
	TArray<FNUStarterNaiaSelectionViewData> NaiaEntries;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> NaiaListPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedNaiaText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	// ---------- 무대형 화면 (선택, 기본 꺼짐 --- 비우면 옛 회색 카드 그대로) ----------

	// 나이아 한 칸. 지정하면 NaiaListPanel에 이 칸을 만든다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	TSubclassOf<UNUStarterNaiaCardWidget> NaiaCardClass;

	// 속성 그림 아이콘 (키 = 속성 태그 끝 이름: Fire · Water · Grass · Earth · Lightning · Wind)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	TMap<FName, TSoftObjectPtr<UTexture2D>> ElementIcons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	bool bSelectFirstOnOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	bool bHideFieldHUDWhileOpen = false;

	// 확인 창 (비우면 프로젝트 세팅 NU UI의 공용 확인 창)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	TSubclassOf<UNUConfirmDialog> ConfirmDialogClass;

	// 확인 버튼 글자 --- {0} = 나이아 이름 + 와/과
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	FText ConfirmLabelFormat = NSLOCTEXT("NUStarterNaia", "ConfirmLabelFormat", "{0} 함께하기");

	// 능력치 막대 중 여섯 나이아에서 가장 높은 값의 색 (나머지는 속성 색)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	FLinearColor BestStatColor = FLinearColor(0.87f, 0.68f, 0.29f, 1.f);

	// 진화 실루엣 색 (나이아 그림에 곱한다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	FLinearColor SilhouetteTint = FLinearColor(0.02f, 0.03f, 0.05f, 0.92f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DetailElementIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AccentBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ElementChip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ElementText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PassiveNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UNUFlipbookImage> EvolutionFigure;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EvolutionLevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EvolutionNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmLabel;

private:
	// WBP 버튼 교체 중에도 순정 Button과 공용 버튼을 모두 지원한다.
	bool BindConfirmButton();
	void UnbindConfirmButton();
	void SetConfirmButtonEnabled(bool bEnabled);
	void RebuildNaiaEntries();
	UWidget* BuildDefaultNaiaCard(const FNUStarterNaiaSelectionViewData& Entry);
	UWidget* BuildStageCard(const FNUStarterNaiaSelectionViewData& Entry);
	bool UsesStage() const { return NaiaCardClass != nullptr; }
	// 아래 판: 속성 · 타고난 힘 · 능력치(StatValue0~5 글자, StatBar0~5 막대 --- 이름으로 찾는다) · 진화 · 확인 버튼 글자
	void RefreshDetail(const FNUStarterNaiaSelectionViewData* Entry);
	void StepSelection(int32 Step);
	UImage* BuildPortraitImage(int32 NaiaId) const;
	void RefreshState();
	void SubmitSelection();
	const FNUStarterNaiaSelectionViewData* FindSelectedEntry() const;
	static FText GetElementDisplayName(const FGameplayTag& ElementTag);
	static FLinearColor GetElementColor(const FGameplayTag& ElementTag);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleSelectionConfirmed();

	UFUNCTION()
	void HandleSelectionCancelled();

	UFUNCTION()
	void HandleRosterChanged();

	TWeakObjectPtr<UNUNaiaRosterComponent> RosterComponent;
	int32 SelectedNaiaId = 0;
	bool bCompletionHandled = false;

	UPROPERTY(Transient)
	TObjectPtr<UNUConfirmDialog> ActiveConfirmDialog;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNUStarterNaiaSelectionEntryButton>> NaiaButtons;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNUStarterNaiaCardWidget>> NaiaCards;
};
