#include "UI/NUKeyBindingGroupWidget.h"

#include "Components/TextBlock.h"

void UNUKeyBindingGroupWidget::SetTitle(const FText& InTitle)
{
	GroupText->SetText(InTitle);
}
