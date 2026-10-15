#include "UI/CharacterMenu/NUCharacterTraitsPage.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr float ZoomFactor = 1.25f;
}

void UNUCharacterTraitsPage::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	if (GraphZoomInButton) GraphZoomInButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ZoomIn);
	if (GraphZoomOutButton) GraphZoomOutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ZoomOut);
	if (GraphResetButton) GraphResetButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ResetView);
	MeasureContentBounds();
	ApplyView();
}

void UNUCharacterTraitsPage::NativeDestruct()
{
	if (HasMouseCapture() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
	bPanning = false;
	if (GraphZoomInButton) GraphZoomInButton->OnClicked.RemoveDynamic(this, &ThisClass::ZoomIn);
	if (GraphZoomOutButton) GraphZoomOutButton->OnClicked.RemoveDynamic(this, &ThisClass::ZoomOut);
	if (GraphResetButton) GraphResetButton->OnClicked.RemoveDynamic(this, &ThisClass::ResetView);
	Super::NativeDestruct();
}

void UNUCharacterTraitsPage::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!GetViewSize().Equals(LastViewSize)) ApplyView();
}

FVector2D UNUCharacterTraitsPage::GetViewSize() const
{
	if (!GraphClip) return FVector2D::ZeroVector;
	const FVector2D Size = GraphClip->GetCachedGeometry().GetLocalSize();
	if (Size.X > 0.f && Size.Y > 0.f) return Size;
	const UCanvasPanelSlot* ClipSlot = Cast<UCanvasPanelSlot>(GraphClip->Slot);
	return ClipSlot ? ClipSlot->GetSize() : FVector2D::ZeroVector;
}

bool UNUCharacterTraitsPage::IsOverGraph(const FVector2D& ScreenPosition) const
{
	return GraphClip && GraphClip->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

void UNUCharacterTraitsPage::MeasureContentBounds()
{
	if (!GraphViewport) return;
	const UCanvasPanelSlot* GraphSlot = Cast<UCanvasPanelSlot>(GraphViewport->Slot);
	if (!GraphSlot) return;
	const FVector2D Center = GraphSlot->GetSize() * .5f;
	FVector2D Extent = FVector2D::ZeroVector;
	for (UWidget* Child : GraphViewport->GetAllChildren())
	{
		// Bounds of the existing nodes, not the large empty margins of the canvas.
		if (!Child->IsA<UButton>()) continue;
		if (const UCanvasPanelSlot* NodeSlot = Cast<UCanvasPanelSlot>(Child->Slot))
		{
			const FVector2D Min = NodeSlot->GetPosition() - Center;
			const FVector2D Max = Min + NodeSlot->GetSize();
			Extent.X = FMath::Max(Extent.X, FMath::Max(FMath::Abs(Min.X), FMath::Abs(Max.X)));
			Extent.Y = FMath::Max(Extent.Y, FMath::Max(FMath::Abs(Min.Y), FMath::Abs(Max.Y)));
		}
	}
	if (Extent.X > 0.f && Extent.Y > 0.f)
	{
		Extent += FVector2D(12.f);
		ContentBounds = FBox2D(Center - Extent, Center + Extent);
	}
	else ContentBounds = FBox2D(FVector2D::ZeroVector, GraphSlot->GetSize());
}

void UNUCharacterTraitsPage::ApplyView()
{
	if (!GraphViewport) return;
	const FVector2D Size = GetViewSize();
	if (Size.X <= 0.f || Size.Y <= 0.f) return;
	View.Constrain(Size, ContentBounds);
	GraphViewport->SetRenderTransformPivot(FVector2D::ZeroVector);
	GraphViewport->SetRenderScale(FVector2D(View.Zoom));
	GraphViewport->SetRenderTranslation(View.Offset);
	LastViewSize = Size;
	if (GraphZoomText)
	{
		GraphZoomText->SetText(FText::Format(NSLOCTEXT("NUCharacterMenu", "TraitZoom", "{0}%"),
			FText::AsNumber(FMath::RoundToInt(View.Zoom * 100.f))));
	}
	if (GraphZoomInButton) GraphZoomInButton->SetIsEnabled(View.Zoom < FNUCharacterTraitsView::MaxZoom);
	if (GraphZoomOutButton) GraphZoomOutButton->SetIsEnabled(View.Zoom > FNUCharacterTraitsView::MinZoom);
}

void UNUCharacterTraitsPage::ChangeZoom(float NewZoom, const FVector2D& Anchor)
{
	View.ZoomAt(NewZoom, Anchor);
	ApplyView();
}

void UNUCharacterTraitsPage::ZoomIn() { ChangeZoom(View.Zoom * ZoomFactor, GetViewSize() * .5f); }
void UNUCharacterTraitsPage::ZoomOut() { ChangeZoom(View.Zoom / ZoomFactor, GetViewSize() * .5f); }
void UNUCharacterTraitsPage::ResetView()
{
	View = FNUCharacterTraitsView();
	ApplyView();
}

FReply UNUCharacterTraitsPage::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!IsOverGraph(Event.GetScreenSpacePosition())) return Super::NativeOnMouseWheel(Geometry, Event);
	const FVector2D Anchor = GraphClip->GetCachedGeometry().AbsoluteToLocal(Event.GetScreenSpacePosition());
	ChangeZoom(View.Zoom * FMath::Pow(ZoomFactor, Event.GetWheelDelta()), Anchor);
	return FReply::Handled();
}

FReply UNUCharacterTraitsPage::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FKey Button = Event.GetEffectingButton();
	if (!IsOverGraph(Event.GetScreenSpacePosition())
		|| (Button != EKeys::LeftMouseButton && Button != EKeys::RightMouseButton && Button != EKeys::MiddleMouseButton))
	{
		return Super::NativeOnMouseButtonDown(Geometry, Event);
	}
	// Left-clicks consumed by node buttons remain available for future node selection.
	bPanning = true;
	PanButton = Button;
	return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

FReply UNUCharacterTraitsPage::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bPanning || Event.GetEffectingButton() != PanButton) return Super::NativeOnMouseButtonUp(Geometry, Event);
	bPanning = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UNUCharacterTraitsPage::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bPanning) return Super::NativeOnMouseMove(Geometry, Event);
	if (!Event.IsMouseButtonDown(PanButton) || !GraphClip)
	{
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	const FGeometry& ClipGeometry = GraphClip->GetCachedGeometry();
	View.Offset += ClipGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition())
		- ClipGeometry.AbsoluteToLocal(Event.GetLastScreenSpacePosition());
	ApplyView();
	return FReply::Handled();
}

void UNUCharacterTraitsPage::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	bPanning = false;
	Super::NativeOnMouseCaptureLost(Event);
}
