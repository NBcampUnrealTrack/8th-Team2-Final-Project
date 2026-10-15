#pragma once

#include "CoreMinimal.h"

class UObject;
class UTextBlock;
class UUserWidget;
class UWidget;

// Presentation-only motion for the unified character menu. Drives render opacity, translation,
// scale and text colour from a Slate pre-tick (runs while paused, no UMG animation tracks:
// RenderTransform tracks do not repaint at runtime). Never touches layout, styles or input.
// Every tween ends on the widget's rest values, and Stop() restores them immediately.
namespace NUCharacterMenuMotion
{
	// Shell entrance: rail slides in from the left, heading/gold/close fade in.
	void OpenShell(UUserWidget* Menu);
	// Page entrance: the page's top-level panels fade in and slide, staggered. Direction is the
	// rail movement (+1 = to a lower tab: content enters from below, -1 from above, 0 = small rise).
	void EnterPage(UUserWidget* Menu, UWidget* Page, int32 Direction = 0);
	// Heading swap that follows the same direction; also hands the direction to the next EnterPage.
	void SwapHeading(UUserWidget* Menu, UWidget* Heading, int32 Direction);
	// Moves Widget's render translation to Target (instantly when Duration <= 0); Target becomes its rest.
	void SlideTo(UUserWidget* Owner, UWidget* Widget, const FVector2D& Target, float Duration);
	// True: the exit plays and owns Completion. False: caller closes now.
	bool DeferClose(UUserWidget* Menu, FSimpleDelegate Completion);
	// Ends every tween owned by Menu and restores rest values.
	void Stop(UUserWidget* Menu);

	// Feedback
	void Pulse(UWidget* Widget, float FromScale = 1.12f, float Duration = 0.18f);
	void FlashText(UTextBlock* Text, const FLinearColor& Color, float Duration = 0.7f);
	// Lifts Widget (the drag visual): scales up and tilts slightly.
	void Lift(UWidget* Widget);
	// While set, the target under the cursor is raised; OnHot reports (previous, new) hot targets so the
	// owner can add its own highlight. Cleared with an empty array.
	void TrackDropTargets(UWidget* Owner, TArray<TWeakObjectPtr<UWidget>> Targets,
		TFunction<void(UWidget* Previous, UWidget* Current)> OnHot = nullptr);

	// Change tracking for feedback without page members. First sighting is not a change.
	// Returns the difference from the previous value of Key on Owner (0 the first time).
	float TrackValue(const UObject* Owner, FName Key, float Value);
	// True when Key on Owner held a different name before (false the first time).
	bool TrackName(const UObject* Owner, FName Key, FName Value);
	// Flashes Text green/red when Value moved since the last call for the same Owner/Key.
	void FlashOnChange(const UObject* Owner, UTextBlock* Text, float Value);

	// Gain/loss colours used by stat feedback.
	FLinearColor GainColor();
	FLinearColor LossColor();
}
