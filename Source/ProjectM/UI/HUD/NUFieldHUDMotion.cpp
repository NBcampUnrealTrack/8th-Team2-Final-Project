#include "UI/HUD/NUFieldHUDMotion.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/Widget.h"

namespace
{
	constexpr float TransitionSeconds = 0.24f;
	constexpr float SlideDistance = 32.0f; // Slate units; follows the existing viewport DPI scale.
}

void FNUFieldHUDMotion::SetVisible(bool bVisible, const FGeometry& InScreenGeometry)
{
	if (bTargetVisible == bVisible)
	{
		return;
	}
	ScreenGeometry = InScreenGeometry;
	if (HiddenProgress == 0.0f)
	{
		for (FEntry& Entry : Entries)
		{
			if (UWidget* Widget = Entry.Widget.Get())
			{
				// Capture resting changes made by another presentation owner while idle.
				Entry.RestTransform = Widget->GetRenderTransform();
				Entry.RestOpacity = Widget->GetRenderOpacity();
				MeasureDirection(Entry);
			}
		}
	}
	// Reversing only changes the target: neither position nor opacity jumps.
	bTargetVisible = bVisible;
	ApplyAll();
}

void FNUFieldHUDMotion::Present(UWidget* Widget, ESlateVisibility NormalVisibility, ENUFieldHUDMotionRole Role)
{
	if (!IsValid(Widget))
	{
		return;
	}
	Entries.RemoveAll([](const FEntry& Entry) { return !Entry.Widget.IsValid(); });
	FEntry* Entry = Entries.FindByPredicate([Widget](const FEntry& Item) { return Item.Widget == Widget; });
	if (!Entry)
	{
		Entry = &Entries.AddDefaulted_GetRef();
		Entry->Widget = Widget;
		Entry->RestTransform = Widget->GetRenderTransform();
		Entry->RestOpacity = Widget->GetRenderOpacity();
		Entry->Role = Role;
		MeasureDirection(*Entry);
	}
	Entry->NormalVisibility = NormalVisibility;
	Apply(*Entry);
}

void FNUFieldHUDMotion::MeasureDirection(FEntry& Entry) const
{
	const UWidget* Widget = Entry.Widget.Get();
	if (!Widget)
	{
		return;
	}
	// Slots also work before the first layout and while the HUD is collapsed.
	Entry.Direction = -1.0f;
	if (const UOverlaySlot* Slot = Cast<UOverlaySlot>(Widget->Slot))
	{
		Entry.Direction = Slot->GetHorizontalAlignment() == HAlign_Right ? 1.0f : -1.0f;
	}
	else if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		const FAnchors Anchors = CanvasSlot->GetAnchors();
		Entry.Direction = (Anchors.Minimum.X + Anchors.Maximum.X) * 0.5f > 0.5f ? 1.0f : -1.0f;
	}
	const FGeometry& Geometry = Widget->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D ScreenSize = ScreenGeometry.GetLocalSize();
	if (Size.X > 0.0 && Size.Y > 0.0 && ScreenSize.X > 0.0)
	{
		const FVector2D Left = ScreenGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D Right = ScreenGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Size));
		// Full-screen canvas wrappers have no meaningful side; use their slot fallback.
		if (FMath::Abs(Right.X - Left.X) < ScreenSize.X * 0.9)
		{
			Entry.Direction = (Left.X + Right.X) * 0.5 > ScreenSize.X * 0.5 ? 1.0f : -1.0f;
		}
	}
}

bool FNUFieldHUDMotion::Advance(float DeltaTime)
{
	const float Target = bTargetVisible ? 0.0f : 1.0f;
	HiddenProgress = FMath::FInterpConstantTo(HiddenProgress, Target, FMath::Max(DeltaTime, 0.0f), 1.0f / TransitionSeconds);
	ApplyAll();
	return HiddenProgress != Target;
}

void FNUFieldHUDMotion::Apply(FEntry& Entry)
{
	UWidget* Widget = Entry.Widget.Get();
	if (!Widget)
	{
		return;
	}
	if (Entry.Role != ENUFieldHUDMotionRole::Container)
	{
		const float Ease = HiddenProgress * HiddenProgress * (3.0f - 2.0f * HiddenProgress);
		FWidgetTransform Transform = Entry.RestTransform;
		Transform.Translation.X += Entry.Direction * SlideDistance * Ease;
		Widget->SetRenderTransform(Transform);
		Widget->SetRenderOpacity(Entry.RestOpacity * (1.0f - Ease));
	}
	if (Entry.Role != ENUFieldHUDMotionRole::Content)
	{
		ESlateVisibility Visibility = Entry.NormalVisibility;
		if (Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden)
		{
			// Hide input immediately, but keep drawing until the outgoing motion is complete.
			Visibility = bTargetVisible ? Visibility
				: HiddenProgress >= 1.0f ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
		}
		Widget->SetVisibility(Visibility);
	}
}

void FNUFieldHUDMotion::ApplyAll()
{
	Entries.RemoveAll([](const FEntry& Entry) { return !Entry.Widget.IsValid(); });
	for (FEntry& Entry : Entries)
	{
		Apply(Entry);
	}
}

void FNUFieldHUDMotion::Restore(const FEntry& Entry)
{
	if (UWidget* Widget = Entry.Widget.Get())
	{
		if (Entry.Role != ENUFieldHUDMotionRole::Container)
		{
			Widget->SetRenderTransform(Entry.RestTransform);
			Widget->SetRenderOpacity(Entry.RestOpacity);
		}
		if (Entry.Role != ENUFieldHUDMotionRole::Content)
		{
			Widget->SetVisibility(Entry.NormalVisibility);
		}
	}
}

void FNUFieldHUDMotion::Forget(UWidget* Widget)
{
	Entries.RemoveAll([Widget](const FEntry& Entry)
	{
		if (Entry.Widget == Widget)
		{
			Restore(Entry);
			return true;
		}
		return !Entry.Widget.IsValid();
	});
}

void FNUFieldHUDMotion::Finish()
{
	HiddenProgress = bTargetVisible ? 0.0f : 1.0f;
	ApplyAll();
}

void FNUFieldHUDMotion::Reset()
{
	for (const FEntry& Entry : Entries)
	{
		Restore(Entry);
	}
	Entries.Reset();
	HiddenProgress = bTargetVisible ? 0.0f : 1.0f;
}
