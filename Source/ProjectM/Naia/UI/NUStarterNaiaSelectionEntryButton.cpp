#include "Naia/UI/NUStarterNaiaSelectionEntryButton.h"

UNUStarterNaiaSelectionEntryButton::UNUStarterNaiaSelectionEntryButton()
{
	OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
}

void UNUStarterNaiaSelectionEntryButton::HandleClicked()
{
	OnEntryClicked.ExecuteIfBound(NaiaId);
}
