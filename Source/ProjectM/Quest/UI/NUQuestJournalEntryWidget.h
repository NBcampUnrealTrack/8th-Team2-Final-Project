#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "NUQuestJournalEntryWidget.generated.h"

class UTextBlock;
class UNUQuestJournalListItem;

/** WBP_QuestJournalEntry가 상속할 퀘스트 목록 항목 베이스. */
UCLASS()
class PROJECTM_API UNUQuestJournalEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;

	// Card layout (optional names): QR_EntryCaption, QR_EntryProgressBar, QR_EntryProgressText, QR_EntrySelectedBar.
	// When QR_EntryCaption exists, selection is styled here after the WBP graph has run.
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style")
	FSlateColor CaptionColor = FSlateColor(FLinearColor(0.402f, 0.456f, 0.520f));

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style")
	FSlateColor SelectedCaptionColor = FSlateColor(FLinearColor(0.672f, 0.515f, 0.262f));

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Journal|Style")
	FLinearColor SelectedBackgroundTint = FLinearColor(1.45f, 1.45f, 1.45f, 1.f);

private:
	UPROPERTY(Transient)
	TObjectPtr<UNUQuestJournalListItem> ListItem;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuestTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuestStatusText;
};
