// C++가 직접 만드는 UI(전체 메뉴 항목, WBP 없는 폴백 창)의 공용 룩.
// 엔진 기본 버튼(밝은 회색, 둥근 모서리, 눌림 오프셋)은 어두운 창 위에서 붕 떠서, 납작한 단색 버튼으로 통일한다.
// WBP가 있는 창은 여기를 보지 않는다 --- 모양은 WBP 소유.
#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"

namespace NUWidgetStyle
{
	// 밝기만 바꾼다 (알파 유지) --- FLinearColor * float는 알파까지 곱해 버린다
	inline FLinearColor Scaled(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, Color.A);
	}

	// 납작한 버튼 --- 보통/호버/눌림 색을 직접 준다
	inline FButtonStyle FlatButton(const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed, float Radius = 4.0f)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateRoundedBoxBrush(Normal, Radius));
		Style.SetHovered(FSlateRoundedBoxBrush(Hovered, Radius));
		Style.SetPressed(FSlateRoundedBoxBrush(Pressed, Radius));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f)); // 눌릴 때 글자가 내려앉지 않게
		return Style;
	}

	// 바탕색 하나로 --- 호버는 밝게, 눌림은 어둡게
	inline FButtonStyle FlatButton(const FLinearColor& Base, float Radius = 4.0f)
	{
		return FlatButton(Base, Scaled(Base, 1.3f), Scaled(Base, 0.75f), Radius);
	}
}
