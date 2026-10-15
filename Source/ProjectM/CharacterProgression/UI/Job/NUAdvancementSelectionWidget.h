	#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/UI/NUProgressionWindowBase.h"
#include "NUAdvancementSelectionWidget.generated.h"

class UNUJobComponent;
class UNUJobSelectionEntryButton;
class UNUProgressionComponent;
class UNU_CommonButtonWidget;
class UHorizontalBox;
class UTextBlock;
class UWidget;

USTRUCT(BlueprintType)
struct PROJECTM_API FNUAdvancementSelectionViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Advancement|UI")
	int32 AdvancementId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Advancement|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Advancement|UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Advancement|UI")
	FLinearColor AccentColor = FLinearColor::White;
};

UCLASS(Abstract)
class PROJECTM_API UNUAdvancementSelectionWidget : public UNUProgressionWindowBase
{
	GENERATED_BODY()

public:
	void InitializeSelection(
		int32 InNodeId,
		UNUJobComponent* InJobComponent,
		UNUProgressionComponent* InProgression,
		FSimpleDelegate InCloseRequested);

	UFUNCTION(BlueprintCallable, Category = "NU|Advancement|UI")
	void SelectAdvancement(int32 AdvancementId);

	UFUNCTION(BlueprintPure, Category = "NU|Advancement|UI")
	const TArray<FNUAdvancementSelectionViewData>& GetAdvancementEntries() const { return AdvancementEntries; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool CanDragWindow() const override;

	UFUNCTION(BlueprintNativeEvent, Category = "NU|Advancement|UI", meta = (DisplayName = "2차 전직 선택 목록 갱신"))
	void BP_OnAdvancementEntriesChanged(const TArray<FNUAdvancementSelectionViewData>& InEntries);
	virtual void BP_OnAdvancementEntriesChanged_Implementation(const TArray<FNUAdvancementSelectionViewData>& InEntries);

	UPROPERTY(BlueprintReadOnly, Category = "NU|Advancement|UI")
	TArray<FNUAdvancementSelectionViewData> AdvancementEntries;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> JobListPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SelectedJobText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> KeyHintText;

private:
	void RebuildEntries();
	UWidget* BuildDefaultCard(const FNUAdvancementSelectionViewData& Entry);
	void RefreshState();
	const FNUAdvancementSelectionViewData* FindSelectedEntry() const;
	FText BuildChoiceDescription(int32 AdvancementId, const FText& AdvancementDescription) const;
	static FLinearColor GetAdvancementAccent(int32 AdvancementId);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleJobChanged();
	UFUNCTION()
	void HandleProgressionRequestStateChanged();
	UFUNCTION()
	void HandleProgressionRequestFailed(int32 ResultCode, const FString& Message);

	TWeakObjectPtr<UNUJobComponent> JobComponent;
	TWeakObjectPtr<UNUProgressionComponent> Progression;
	int32 NodeId = 0;
	int32 SelectedAdvancementId = 0;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNUJobSelectionEntryButton>> AdvancementButtons;
};
