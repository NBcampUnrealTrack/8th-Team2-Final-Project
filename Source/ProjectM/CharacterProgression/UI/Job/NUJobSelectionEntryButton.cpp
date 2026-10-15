#include "CharacterProgression/UI/Job/NUJobSelectionEntryButton.h"

UNUJobSelectionEntryButton::UNUJobSelectionEntryButton()
{
	OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
}

void UNUJobSelectionEntryButton::HandleClicked()
{
	OnEntryClicked.ExecuteIfBound(JobId);
}
