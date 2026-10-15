#include "UI/NUUIGeometry.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Widgets/SWidget.h"

bool NUUIGeometry::GetViewportRect(const UObject* WorldContextObject, const SWidget& Widget, FVector2D& OutPosition, FVector2D& OutSize)
{
	const FGeometry& Geometry = Widget.GetCachedGeometry();
	const FVector2D AbsoluteSize = Geometry.GetAbsoluteSize();
	if (WorldContextObject == nullptr || AbsoluteSize.X <= 0.0f || AbsoluteSize.Y <= 0.0f)
	{
		return false;
	}

	// 데스크톱 좌표 => 뷰포트 좌표. 크기는 DPI 배율만 나누면 된다
	FVector2D PixelPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(WorldContextObject, Geometry.GetAbsolutePosition(), PixelPosition, OutPosition);
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(WorldContextObject), UE_KINDA_SMALL_NUMBER);
	OutSize = AbsoluteSize / Scale;
	return true;
}
