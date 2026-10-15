#include "Quest/UI/NUQuestJournalEntryWidget.h"

#include "Quest/UI/NUQuestJournalListItem.h"
#include "Quest/UI/NUQuestJournalPresentation.h"
#include "Quest/Data/NUQuestDataSubsystem.h"

#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UNUQuestJournalEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	ListItem = Cast<UNUQuestJournalListItem>(ListItemObject);
	NativeOnItemSelectionChanged(IsValid(ListItem) && IsListItemSelected());
	if (!IsValid(ListItem))
	{
		return;
	}

	if (IsValid(QuestTitleText))
	{
		QuestTitleText->SetText(ListItem->Title);
	}

	if (IsValid(QuestStatusText))
	{
		// The new folio entry opts into a location subtitle by including this pin.
		UWidget* LocationPin = GetWidgetFromName(TEXT("QR_EntryLocationPin"));
		const UNUQuestDataSubsystem* Data = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
		const FText Location = NUQuestJournalPresentation::GetLocationText(
			GetWorld(), Data ? Data->FindDefinition(ListItem->QuestId) : nullptr);
		QuestStatusText->SetText(LocationPin && !Location.IsEmpty() ? Location : ListItem->StatusText);
		if (LocationPin)
		{
			LocationPin->SetVisibility(Location.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		}
	}

	if (UTextBlock* Caption = Cast<UTextBlock>(GetWidgetFromName(TEXT("QR_EntryCaption"))))
	{
		const FText Category = NUQuestJournalPresentation::GetCategoryShortText(ListItem->CategoryTag);
		Caption->SetText(Category.IsEmpty() ? ListItem->StatusText
			: FText::Format(NSLOCTEXT("NUQuestJournal", "EntryCaption", "{0} · {1}"), Category, ListItem->StatusText));
	}
	const bool bHasProgress = ListItem->ProgressRequired > 0;
	if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("QR_EntryProgressBar"))))
	{
		Bar->SetPercent(bHasProgress ? static_cast<float>(ListItem->ProgressCount) / ListItem->ProgressRequired : 0.f);
		Bar->SetVisibility(bHasProgress ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (UTextBlock* Count = Cast<UTextBlock>(GetWidgetFromName(TEXT("QR_EntryProgressText"))))
	{
		Count->SetText(FText::Format(NSLOCTEXT("NUQuestJournal", "EntryProgress", "{0} / {1}"),
			FText::AsNumber(ListItem->ProgressCount), FText::AsNumber(ListItem->ProgressRequired)));
		Count->SetVisibility(bHasProgress ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UNUQuestJournalEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected); // the WBP graph tints EntryBackground here
	UTextBlock* Caption = Cast<UTextBlock>(GetWidgetFromName(TEXT("QR_EntryCaption")));
	if (Caption == nullptr)
	{
		return; // older entry layouts keep the graph's colors
	}
	Caption->SetColorAndOpacity(bIsSelected ? SelectedCaptionColor : CaptionColor);
	if (UBorder* Background = Cast<UBorder>(GetWidgetFromName(TEXT("EntryBackground"))))
	{
		Background->SetBrushColor(bIsSelected ? SelectedBackgroundTint : FLinearColor::White);
	}
	if (UWidget* Bar = GetWidgetFromName(TEXT("QR_EntrySelectedBar")))
	{
		Bar->SetVisibility(bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UNUQuestJournalEntryWidget::NativeOnEntryReleased()
{
	NativeOnItemSelectionChanged(false);
	ListItem = nullptr;
	IUserObjectListEntry::NativeOnEntryReleased();
}
