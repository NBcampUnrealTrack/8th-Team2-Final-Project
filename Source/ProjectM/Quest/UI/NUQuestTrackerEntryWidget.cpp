#include "Quest/UI/NUQuestTrackerEntryWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "PlayerController/NUPlayerController.h"

void UNUQuestTrackerEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(UntrackButton))
	{
		UntrackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::UntrackQuest);
	}
}

void UNUQuestTrackerEntryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Motion.Advance(InDeltaTime);
}

void UNUQuestTrackerEntryWidget::PlayIntro()
{
	Motion.Finish();
	Motion.Add(this, 0.f, 0.45f, FVector2D(32.f, 0.f), 0.f);
	if (IsValid(EntryFlash))
	{
		Motion.AddPulse(EntryFlash, 0.18f, 1.1f, 1.f);
	}
	Motion.Start();
}

void UNUQuestTrackerEntryWidget::PlayReadyFlash()
{
	Motion.Finish();
	if (IsValid(EntryFlash))
	{
		Motion.AddPulse(EntryFlash, 0.f, 1.2f, 1.f);
	}
	if (IsValid(EntryTurnInBadge))
	{
		Motion.Add(EntryTurnInBadge, 0.1f, 0.4f, FVector2D::ZeroVector, 0.f, 1.6f);
	}
	Motion.Start();
}

void UNUQuestTrackerEntryWidget::ApplyReadyStyle()
{
	if (IsValid(EntryTurnInBadge))
	{
		EntryTurnInBadge->SetVisibility(bIsReadyToTurnIn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(EntryReadyBack))
	{
		EntryReadyBack->SetVisibility(bIsReadyToTurnIn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bApplyReadyStyle)
	{
		return;
	}
	const FSlateColor& Color = bIsReadyToTurnIn ? ReadyTitleColor : NormalTitleColor;
	if (IsValid(EntryTitleText))
	{
		EntryTitleText->SetColorAndOpacity(Color);
	}
	if (UImage* Diamond = Cast<UImage>(EntryDiamond))
	{
		Diamond->SetColorAndOpacity(bIsReadyToTurnIn ? ReadyTitleColor.GetSpecifiedColor() : FLinearColor::White);
	}
}

void UNUQuestTrackerEntryWidget::NativeDestruct()
{
	if (IsValid(UntrackButton))
	{
		UntrackButton->OnClicked.RemoveDynamic(this, &ThisClass::UntrackQuest);
	}
	Super::NativeDestruct();
}

void UNUQuestTrackerEntryWidget::UntrackQuest()
{
	if (ANUPlayerController* Controller = Cast<ANUPlayerController>(GetOwningPlayer()); Controller != nullptr && !QuestId.IsNone())
	{
		Controller->UntrackQuest(QuestId);
	}
}

void UNUQuestTrackerEntryWidget::SetQuestDisplay(FName InQuestId, const FText& Title, const FText& Objective, bool bReadyToTurnIn)
{
	if (QuestId == InQuestId && QuestTitle.EqualTo(Title) && ObjectiveText.EqualTo(Objective) && bIsReadyToTurnIn == bReadyToTurnIn)
	{
		return;
	}
	QuestId = InQuestId;
	QuestTitle = Title;
	ObjectiveText = Objective;
	bIsReadyToTurnIn = bReadyToTurnIn;

	if (IsValid(EntryTitleText))
	{
		EntryTitleText->SetText(Title);
	}

	if (IsValid(EntryObjectiveText))
	{
		EntryObjectiveText->SetText(Objective);
	}

	ApplyReadyStyle();
	BP_OnTrackerEntryUpdated(bReadyToTurnIn);
}
