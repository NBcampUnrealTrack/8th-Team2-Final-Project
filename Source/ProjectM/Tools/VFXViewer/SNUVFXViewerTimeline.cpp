// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/VFXViewer/SNUVFXViewerTimeline.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Tools/VFXViewer/NUVFXViewerDirector.h"

namespace
{
	// 막대 배치 (Slate 단위), 위에서부터 눈금 → 막대 → 단계 이름
	constexpr float BarMargin = 8.f;
	constexpr float TickTop = 5.f;
	constexpr float TrackTop = 14.f;
	constexpr float TrackHeight = 8.f;
	constexpr float LabelTop = 25.f;
	constexpr float WidgetHeight = 40.f;

	const FLinearColor TrackColor(0.10f, 0.11f, 0.14f);
	const FLinearColor TelegraphColor(0.55f, 0.55f, 0.62f);
	const FLinearColor ApproachColor(0.33f, 0.55f, 0.95f);
	const FLinearColor ActingColor(1.f, 0.62f, 0.22f);
	const FLinearColor ReturnColor(0.33f, 0.78f, 0.70f);
	const FLinearColor TailColor(0.55f, 0.45f, 0.75f);
	const FLinearColor CastTickColor(0.35f, 0.95f, 1.f);
	const FLinearColor HitTickColor(1.f, 0.32f, 0.28f);
	const FLinearColor PlayheadColor(1.f, 0.85f, 0.45f);
	const FLinearColor LabelColor(0.66f, 0.69f, 0.76f);
	const FLinearColor IconColor(0.92f, 0.93f, 0.96f);

	// 아직 안 지난 구간은 흐리게
	constexpr float FutureOpacity = 0.35f;

	void DrawBox(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry,
		float X, float Y, float Width, float Height, const FLinearColor& Color)
	{
		if (Width <= 0.f || Height <= 0.f)
		{
			return;
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(FVector2f(Width, Height), FSlateLayoutTransform(FVector2f(X, Y))),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			Color);
	}

	void DrawTriangle(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry,
		const FVector2f& A, const FVector2f& B, const FVector2f& C, const FLinearColor& Color)
	{
		const FSlateResourceHandle Handle =
			FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();
		const FColor PackedColor = Color.ToFColor(true);

		TArray<FSlateVertex> Vertices;
		Vertices.Reserve(3);
		for (const FVector2f& Point : { A, B, C })
		{
			Vertices.Add(FSlateVertex::Make(Transform, Point, FVector2f(0.5f, 0.5f), PackedColor));
		}

		const TArray<SlateIndex> Indices = { 0, 1, 2 };
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Vertices, Indices, nullptr, 0, 0);
	}
}

/////////////////////////////////////////////////////////////////
// 진행바
/////////////////////////////////////////////////////////////////

void SNUVFXViewerTimeline::Construct(const FArguments& InArgs)
{
	Director = InArgs._Director;

	SetToolTipText(FText::FromString(
		TEXT("눌러서 · 끌어서 재생 위치를 옮김\n")
		TEXT("막대 색: 예고 · 접근 · 행동 · 복귀 · 잔상(행동이 끝난 뒤 이펙트가 남은 시간)\n")
		TEXT("청록 눈금 = 시전(투사체 출발) · 빨강 눈금 = 타격")));
}

FVector2D SNUVFXViewerTimeline::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(200.f, WidgetHeight);
}

float SNUVFXViewerTimeline::TimeAtLocalX(float LocalX, float Width) const
{
	const ANUVFXViewerDirector* Viewer = GetDirector();
	if (!Viewer)
	{
		return 0.f;
	}

	const float Usable = FMath::Max(Width - BarMargin * 2.f, 1.f);
	return FMath::Clamp((LocalX - BarMargin) / Usable, 0.f, 1.f) * Viewer->GetTimelineLength();
}

float SNUVFXViewerTimeline::LocalXAtTime(float Time, float Width) const
{
	const ANUVFXViewerDirector* Viewer = GetDirector();
	const float Length = Viewer ? Viewer->GetTimelineLength() : 1.f;
	const float Usable = FMath::Max(Width - BarMargin * 2.f, 1.f);
	return BarMargin + FMath::Clamp(Time / FMath::Max(Length, KINDA_SMALL_NUMBER), 0.f, 1.f) * Usable;
}

int32 SNUVFXViewerTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const float Width = AllottedGeometry.GetLocalSize().X;
	DrawBox(OutDrawElements, LayerId, AllottedGeometry, BarMargin, TrackTop, Width - BarMargin * 2.f, TrackHeight, TrackColor);

	const ANUVFXViewerDirector* Viewer = GetDirector();
	if (!Viewer || !Viewer->GetReport().bValid)
	{
		return LayerId + 1;
	}

	const FNUVFXViewerTimeline& Timeline = Viewer->GetTimeline();
	const float Length = Viewer->GetTimelineLength();
	const float NowX = LocalXAtTime(FMath::Clamp(Viewer->GetPlaybackTime(), 0.f, Length), Width);

	struct FSegment
	{
		float Start;
		float End;
		FLinearColor Color;
		const TCHAR* Label;
	};

	const FSegment Segments[] =
	{
		{ 0.f, Timeline.TelegraphEnd, TelegraphColor, TEXT("예고") },
		{ Timeline.TelegraphEnd, Timeline.ApproachEnd, ApproachColor, TEXT("접근") },
		{ Timeline.ApproachEnd, Timeline.ActingEnd, ActingColor, TEXT("행동") },
		{ Timeline.ActingEnd, Timeline.ReturnEnd, ReturnColor, TEXT("복귀") },
		{ Timeline.ReturnEnd, Length, TailColor, TEXT("잔상") }
	};

	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8);
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	for (const FSegment& Segment : Segments)
	{
		const float StartX = LocalXAtTime(Segment.Start, Width);
		const float EndX = LocalXAtTime(Segment.End, Width);
		if (EndX - StartX < 0.5f)
		{
			continue;
		}

		// 지난 만큼만 진하게 칠해 진행 정도를 보임
		DrawBox(OutDrawElements, LayerId + 1, AllottedGeometry, StartX, TrackTop, EndX - StartX, TrackHeight,
			Segment.Color.CopyWithNewOpacity(FutureOpacity));
		DrawBox(OutDrawElements, LayerId + 1, AllottedGeometry, StartX, TrackTop, FMath::Min(EndX, NowX) - StartX, TrackHeight,
			Segment.Color);

		// 구간 이름은 들어갈 때만
		const FVector2D LabelSize = FontMeasure->Measure(Segment.Label, LabelFont);
		if (LabelSize.X + 4.f <= EndX - StartX)
		{
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(LabelSize), FSlateLayoutTransform(FVector2f((StartX + EndX - LabelSize.X) * 0.5f, LabelTop))),
				FString(Segment.Label),
				LabelFont,
				ESlateDrawEffect::None,
				LabelColor);
		}
	}

	for (const TPair<const TArray<float>*, FLinearColor>& Ticks : {
		TPair<const TArray<float>*, FLinearColor>(&Timeline.CastTimes, CastTickColor),
		TPair<const TArray<float>*, FLinearColor>(&Timeline.HitTimes, HitTickColor) })
	{
		for (const float Time : *Ticks.Key)
		{
			const float X = LocalXAtTime(Time, Width);
			DrawBox(OutDrawElements, LayerId + 2, AllottedGeometry, X - 1.f, TickTop, 2.f, TrackTop + TrackHeight - TickTop, Ticks.Value);
		}
	}

	// 마우스를 올린 곳
	if (HoverTime >= 0.f && !bDragging)
	{
		DrawBox(OutDrawElements, LayerId + 2, AllottedGeometry, LocalXAtTime(HoverTime, Width) - 0.5f, TickTop - 2.f,
			1.f, TrackTop + TrackHeight - TickTop + 4.f, FLinearColor(1.f, 1.f, 1.f, 0.45f));
	}

	// 재생 위치
	DrawBox(OutDrawElements, LayerId + 3, AllottedGeometry, NowX - 1.f, 1.f, 2.f, LabelTop - 1.f, PlayheadColor);
	DrawBox(OutDrawElements, LayerId + 3, AllottedGeometry, NowX - 5.f, TrackTop - 3.f, 10.f, TrackHeight + 6.f, PlayheadColor);

	return LayerId + 4;
}

void SNUVFXViewerTimeline::SeekToMouse(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ANUVFXViewerDirector* Viewer = GetDirector();
	if (!Viewer)
	{
		return;
	}

	// 고정 60fps 프레임에 맞춰 옮김, 프레임 넘기기와 같은 칸에 섬
	const float LocalX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	const float Time = TimeAtLocalX(LocalX, MyGeometry.GetLocalSize().X);
	Viewer->SeekTo(FMath::RoundToFloat(Time / ANUVFXViewerDirector::FrameStep) * ANUVFXViewerDirector::FrameStep);
}

FReply SNUVFXViewerTimeline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ANUVFXViewerDirector* Viewer = GetDirector();
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !Viewer)
	{
		return FReply::Unhandled();
	}

	bDragging = true;
	Viewer->BeginScrub();
	SeekToMouse(MyGeometry, MouseEvent);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SNUVFXViewerTimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bDragging)
	{
		return FReply::Unhandled();
	}

	bDragging = false;
	if (ANUVFXViewerDirector* Viewer = GetDirector())
	{
		Viewer->EndScrub();
	}

	return FReply::Handled().ReleaseMouseCapture();
}

FReply SNUVFXViewerTimeline::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float LocalX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	HoverTime = TimeAtLocalX(LocalX, MyGeometry.GetLocalSize().X);

	if (bDragging && HasMouseCapture())
	{
		SeekToMouse(MyGeometry, MouseEvent);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SNUVFXViewerTimeline::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseLeave(MouseEvent);
	HoverTime = -1.f;
}

void SNUVFXViewerTimeline::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// 창 밖에서 놓는 등 버튼을 놓는 이벤트를 못 받은 경우
	if (bDragging)
	{
		bDragging = false;
		if (ANUVFXViewerDirector* Viewer = GetDirector())
		{
			Viewer->EndScrub();
		}
	}
}

FCursorReply SNUVFXViewerTimeline::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(EMouseCursor::Hand);
}

/////////////////////////////////////////////////////////////////
// 재생 조작 버튼 그림 (14 x 14 칸 기준)
/////////////////////////////////////////////////////////////////

void SNUVFXViewerTransportIcon::Construct(const FArguments& InArgs)
{
	Icon = InArgs._Icon;
}

FVector2D SNUVFXViewerTransportIcon::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(14.f, 14.f);
}

int32 SNUVFXViewerTransportIcon::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const float Unit = FMath::Min(Size.X, Size.Y) / 14.f;
	const FVector2f Origin((Size.X - 14.f * Unit) * 0.5f, (Size.Y - 14.f * Unit) * 0.5f);
	const FLinearColor Color = IconColor * InWidgetStyle.GetColorAndOpacityTint();

	const auto Point = [&Origin, Unit](float X, float Y)
	{
		return Origin + FVector2f(X, Y) * Unit;
	};

	const auto Triangle = [&](const FVector2f& A, const FVector2f& B, const FVector2f& C)
	{
		DrawTriangle(OutDrawElements, LayerId, AllottedGeometry, Point(A.X, A.Y), Point(B.X, B.Y), Point(C.X, C.Y), Color);
	};

	const auto Bar = [&](float X, float Y, float Width, float Height)
	{
		DrawBox(OutDrawElements, LayerId, AllottedGeometry, Origin.X + X * Unit, Origin.Y + Y * Unit, Width * Unit, Height * Unit, Color);
	};

	switch (Icon.Get())
	{
	case ENUVFXViewerTransportIcon::ToStart:
		Bar(1.5f, 2.f, 2.f, 10.f);
		Triangle(FVector2f(8.5f, 2.f), FVector2f(3.5f, 7.f), FVector2f(8.5f, 12.f));
		Triangle(FVector2f(13.5f, 2.f), FVector2f(8.5f, 7.f), FVector2f(13.5f, 12.f));
		break;

	case ENUVFXViewerTransportIcon::BackSecond:
		Triangle(FVector2f(7.f, 2.f), FVector2f(1.f, 7.f), FVector2f(7.f, 12.f));
		Triangle(FVector2f(13.f, 2.f), FVector2f(7.f, 7.f), FVector2f(13.f, 12.f));
		break;

	case ENUVFXViewerTransportIcon::BackFrame:
		Bar(2.f, 2.f, 2.f, 10.f);
		Triangle(FVector2f(12.f, 2.f), FVector2f(5.f, 7.f), FVector2f(12.f, 12.f));
		break;

	case ENUVFXViewerTransportIcon::Play:
		Triangle(FVector2f(3.f, 1.5f), FVector2f(12.5f, 7.f), FVector2f(3.f, 12.5f));
		break;

	case ENUVFXViewerTransportIcon::Pause:
		Bar(3.f, 2.f, 3.f, 10.f);
		Bar(8.f, 2.f, 3.f, 10.f);
		break;

	case ENUVFXViewerTransportIcon::ForwardFrame:
		Triangle(FVector2f(2.f, 2.f), FVector2f(9.f, 7.f), FVector2f(2.f, 12.f));
		Bar(10.f, 2.f, 2.f, 10.f);
		break;

	case ENUVFXViewerTransportIcon::ForwardSecond:
		Triangle(FVector2f(1.f, 2.f), FVector2f(7.f, 7.f), FVector2f(1.f, 12.f));
		Triangle(FVector2f(7.f, 2.f), FVector2f(13.f, 7.f), FVector2f(7.f, 12.f));
		break;
	}

	return LayerId + 1;
}
