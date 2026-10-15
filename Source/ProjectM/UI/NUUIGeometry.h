// UI 좌표 계산 --- "저 위젯 자리에 맞춰 띄우기"에 공용 (패드 포커스 테두리, 포커스된 칸 옆 툴팁 등).
// 모양은 안 다루고 좌표만 돌려준다
#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace NUUIGeometry
{
	// Widget이 마지막으로 그려진 사각형을 뷰포트 좌표로 (위치·크기 모두 DPI 배율을 뺀 값 ---
	// UUserWidget::SetPositionInViewport(Pos, false) / SetDesiredSizeInViewport(Size)에 그대로 넣으면 된다).
	// 아직 한 번도 안 그렸거나 크기가 0이면 false
	PROJECTM_API bool GetViewportRect(const UObject* WorldContextObject, const SWidget& Widget, FVector2D& OutPosition, FVector2D& OutSize);
}
