#include "UI/Common/NUPopupButtonHover.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "UI/NUUIManager.h"
#include "Widgets/SWidget.h"

namespace
{
	UWidget* ParentOf(UWidget* Widget)
	{
		if (UWidget* Parent = Widget->GetParent()) { return Parent; }
		const UWidgetTree* Tree = Cast<UWidgetTree>(Widget->GetOuter());
		return Tree ? Cast<UUserWidget>(Tree->GetOuter()) : nullptr;
	}

	bool IsAvailable(UWidget* Widget)
	{
		for (UWidget* Current = Widget; Current; Current = ParentOf(Current))
		{
			if (!Current->IsVisible() || !Current->GetIsEnabled()) { return false; }
		}
		return true;
	}

	float ScaleAt(float Progress)
	{
		return FMath::Lerp(1.0f, FNUPopupButtonHover::HoverScale, FMath::SmoothStep(0.0f, 1.0f, Progress));
	}
}

void FNUPopupButtonHover::RegisterRoot(UWidget* Root)
{
	if (!IsValid(Root)) { return; }
	UNUUIManager::ForEachWidgetDeep(Root, [this, Root](UWidget* Widget)
	{
		UButton* Button = Cast<UButton>(Widget);
		if (!Button || Entries.ContainsByPredicate([Button](const FEntry& Entry) { return Entry.Button == Button; })) { return; }
		FEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Root = Root;
		Entry.Button = Button;
		Entry.RestScale = Button->GetRenderTransform().Scale;
	});
}

void FNUPopupButtonHover::Restore(const FEntry& Entry)
{
	if (UButton* Button = Entry.Button.Get()) { Button->SetRenderScale(Entry.RestScale); }
}

void FNUPopupButtonHover::UnregisterRoot(UWidget* Root)
{
	Entries.RemoveAll([Root](const FEntry& Entry)
	{
		if (Entry.Root.Get() != Root && Entry.Root.IsValid()) { return false; }
		Restore(Entry);
		return true;
	});
}

void FNUPopupButtonHover::Reset()
{
	for (const FEntry& Entry : Entries) { Restore(Entry); }
	Entries.Reset();
}

bool FNUPopupButtonHover::HoldAtOriginalEdge(const FEntry& Entry)
{
	// Shrinking must not repeatedly fire enter/leave under a stationary cursor at the rim.
	// Hold only inside the original bounds and while the same popup is actually under it.
	if (!Entry.bHovered || !FSlateApplication::IsInitialized()) { return false; }
	FSlateApplication& App = FSlateApplication::Get();
	if (!App.IsActive()) { return false; }
	const TSharedPtr<SWidget> Button = Entry.Button->GetCachedWidget();
	const TSharedPtr<SWidget> Root = Entry.Root->GetCachedWidget();
	if (!Button || !Root) { return false; }
	const FGeometry& Geometry = Button->GetPaintSpaceGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0) { return false; }
	const FVector2D Pivot = Entry.Button->GetRenderTransformPivot() * Size;
	const FVector2D Point = Pivot + (Geometry.AbsoluteToLocal(App.GetCursorPos()) - Pivot) * ScaleAt(Entry.Progress);
	if (Point.X < 0.0 || Point.Y < 0.0 || Point.X > Size.X || Point.Y > Size.Y) { return false; }
	const FWidgetPath Path = App.LocateWindowUnderMouse(App.GetCursorPos(), App.GetInteractiveTopLevelWindows());
	return Path.ContainsWidget(Root.Get());
}

void FNUPopupButtonHover::Advance(float DeltaTime)
{
	Advance(DeltaTime, [](UButton& Button) { return Button.IsHovered(); });
}

void FNUPopupButtonHover::Advance(float DeltaTime, TFunctionRef<bool(UButton&)> HoverQuery)
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		FEntry& Entry = Entries[Index];
		UWidget* Root = Entry.Root.Get();
		UButton* Button = Entry.Button.Get();
		UUserWidget* Owner = Root ? Cast<UUserWidget>(Root) : nullptr;
		if (Root && !Owner) { Owner = Root->GetTypedOuter<UUserWidget>(); }
		if (!Root || !Button || (Owner && !Owner->IsInViewport()))
		{
			Restore(Entry);
			Entries.RemoveAtSwap(Index);
			continue;
		}
		if (!IsAvailable(Root) || !IsAvailable(Button))
		{
			Restore(Entry);
			Entry.Progress = 0.0f;
			Entry.bHovered = false;
			continue;
		}
		Entry.bHovered = HoverQuery(*Button) || HoldAtOriginalEdge(Entry);
		const float Step = FMath::Max(0.0f, DeltaTime) / Duration;
		const float Next = FMath::Clamp(Entry.Progress + (Entry.bHovered ? Step : -Step), 0.0f, 1.0f);
		if (Next != Entry.Progress)
		{
			Entry.Progress = Next;
			Button->SetRenderScale(Entry.RestScale * ScaleAt(Next));
		}
	}
}
