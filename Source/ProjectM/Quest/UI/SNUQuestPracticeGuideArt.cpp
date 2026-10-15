#include "Quest/UI/SNUQuestPracticeGuideArt.h"

#include "Quest/Data/NUQuestBattleDefinition.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace NUQuestPracticeArt
{
	constexpr float Width = 300.f;
	constexpr float Height = Width * 620.f / 505.f;

	FLinearColor Color(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	TArray<FVector2f> Chamfer(float X, float Y, float W, float H, float Cut)
	{
		return {{X + Cut, Y}, {X + W - Cut, Y}, {X + W, Y + Cut},
			{X + W, Y + H - Cut}, {X + W - Cut, Y + H}, {X + Cut, Y + H},
			{X, Y + H - Cut}, {X, Y + Cut}};
	}
}

void SNUQuestPracticeGuideArt::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
}

void SNUQuestPracticeGuideArt::SetPractice(ENUQuestBattleKind InKind, int32 InCount, int32 InRequired)
{
	if (Kind != InKind || Count != InCount || Required != InRequired)
	{
		Kind = InKind;
		Count = InCount;
		Required = InRequired;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FVector2D SNUQuestPracticeGuideArt::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(NUQuestPracticeArt::Width, NUQuestPracticeArt::Height);
}

int32 SNUQuestPracticeGuideArt::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& Elements,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using namespace NUQuestPracticeArt;
	const FVector2f LocalScale = FVector2f(Geometry.GetLocalSize()) / FVector2f(Width, Height);
	const float PixelScale = Geometry.GetAccumulatedLayoutTransform().GetScale() * LocalScale.X;
	const FLinearColor Tint = Style.GetColorAndOpacityTint();
	const FLinearColor Gold = Color(238, 202, 133);
	const FLinearColor Navy = Color(6, 28, 41);
	const FLinearColor Edge = Color(72, 94, 105);
	const FSlateResourceHandle White = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
	int32 Layer = LayerId;

	// 모두 볼록한 도형이다. 텍스처 확대/축소 없이 화면 좌표에서 채우고 선을 얹는다.
	auto Fill = [&](const TArray<FVector2f>& Points, const FLinearColor& Top, const FLinearColor& Bottom)
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		Vertices.Reserve(Points.Num());
		for (const FVector2f& Point : Points)
		{
			const FLinearColor VertexColor = FMath::Lerp(Top, Bottom, FMath::Clamp(Point.Y / Height, 0.f, 1.f)) * Tint;
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				Geometry.GetAccumulatedRenderTransform(), Point * LocalScale, FVector2f::ZeroVector, VertexColor.ToFColor(true)));
		}
		for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
		{
			Indices.Append({0, static_cast<SlateIndex>(Index), static_cast<SlateIndex>(Index + 1)});
		}
		FSlateDrawElement::MakeCustomVerts(Elements, Layer++, White, Vertices, Indices, nullptr, 0, 0);
	};
	auto Line = [&](TArray<FVector2f> Points, const FLinearColor& Ink, float Thickness = 1.f, bool bClosed = false)
	{
		if (bClosed)
		{
			const FVector2f FirstPoint = Points[0];
			Points.Add(FirstPoint);
		}
		for (FVector2f& Point : Points) { Point *= LocalScale; }
		// Slate 선 두께는 화면 픽셀 단위다. 낮은 DPI에서도 최소 한 픽셀을 남긴다.
		FSlateDrawElement::MakeLines(Elements, Layer++, Geometry.ToPaintGeometry(), MoveTemp(Points),
			ESlateDrawEffect::None, Ink * Tint, true, FMath::Max(1.f, Thickness * PixelScale));
	};
	auto Solid = [&](const TArray<FVector2f>& Points, const FLinearColor& Ink) { Fill(Points, Ink, Ink); };

	const TArray<FVector2f> Panel = Chamfer(1.f, 1.f, Width - 2.f, Height - 2.f, 10.f);
	Fill(Panel, Color(9, 37, 52), Color(5, 23, 35));
	Line(Panel, Gold, 1.4f, true);

	// 머리말의 나침반과 제목 아래 다이아몬드.
	const FVector2f Center(34.f, 31.f);
	Line({Center + FVector2f(-18, 0), Center + FVector2f(18, 0)}, Gold);
	Line({Center + FVector2f(0, -18), Center + FVector2f(0, 18)}, Gold);
	Line({{34, 14}, {38, 27}, {51, 31}, {38, 35}, {34, 48}, {30, 35}, {17, 31}, {30, 27}}, Gold, 1.f, true);
	Line({{25, 22}, {43, 40}}, Gold.CopyWithNewOpacity(0.6f));
	Line({{25, 40}, {43, 22}}, Gold.CopyWithNewOpacity(0.6f));
	Solid({{34, 27}, {38, 31}, {34, 35}, {30, 31}}, Gold);
	Line({{24, 94}, {145, 94}}, Gold);
	Line({{155, 94}, {276, 94}}, Gold);
	Line({{150, 90}, {153, 94}, {150, 98}, {147, 94}}, Gold, 1.f, true);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float Y = 120.f + Row * 40.f;
		Line({{45, Y}, {45, Y + 16}}, Gold.CopyWithNewOpacity(0.7f));
	}

	Line(Chamfer(17, 245, 266, 62, 6), Edge, 1.f, true);
	if (Kind == ENUQuestBattleKind::BasicAttack)
	{
		for (int32 Stripe = 0; Stripe < 3; ++Stripe)
		{
			const float X = 239.f + Stripe * 11.f;
			const float H = 10.f + Stripe * 7.f;
			Solid({{X, 288}, {X + 7, 281}, {X + 7, 281 - H}, {X, 288 - H}}, Stripe == 2 ? Gold : Color(45, 157, 213));
		}
	}
	else if (Kind == ENUQuestBattleKind::Parry)
	{
		Solid(Chamfer(38, 255, 224, 13, 2), Navy);
		Solid({{121, 255}, {179, 255}, {179, 268}, {121, 268}}, Color(237, 148, 53));
		Line(Chamfer(38, 255, 224, 13, 2), Edge, 1.f, true);
		for (const float X : {80.f, 121.f, 179.f, 220.f})
		{
			Line({{X, 259}, {X, 264}}, Edge);
		}
		Line({{150, 252}, {150, 271}}, Color(250, 244, 224), 2.f);
	}
	else if (Kind == ENUQuestBattleKind::Dodge)
	{
		const TArray<FVector2f> Plaque = Chamfer(29, 250, 242, 25, 7);
		Solid(Plaque, Color(16, 28, 29));
		Line(Plaque, Gold, 1.f, true);
		Line(Chamfer(32, 253, 236, 19, 5), Gold.CopyWithNewOpacity(0.7f), 1.f, true);
	}
	Line({{18, 317}, {282, 317}}, Gold);

	// 필요 횟수가 많으면 숫자만 표시한다. 열 칸까지 같은 영역 안에 맞춘다.
	if (Required > 0 && Required <= 10)
	{
		const float Pitch = FMath::Min(18.f, 90.f / Required);
		const float Radius = FMath::Min(5.f, Pitch * 0.35f);
		for (int32 Pip = 0; Pip < Required; ++Pip)
		{
			TArray<FVector2f> Circle;
			for (int32 Segment = 0; Segment < 24; ++Segment)
			{
				const float Angle = UE_TWO_PI * Segment / 24.f;
				Circle.Emplace(277.f - (Required - 1 - Pip) * Pitch + Radius * FMath::Cos(Angle), 342.f + Radius * FMath::Sin(Angle));
			}
			if (Pip < Count) { Solid(Circle, Gold); }
			Line(Circle, Gold, 1.f, true);
		}
	}
	return Layer;
}
