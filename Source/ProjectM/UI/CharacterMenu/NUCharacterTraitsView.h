#pragma once

#include "CoreMinimal.h"

// Coordinates are local to the clipped graph, independent of viewport DPI.
struct FNUCharacterTraitsView
{
	static constexpr float MinZoom = 1.f;
	static constexpr float MaxZoom = 8.f;
	float Zoom = MinZoom;
	FVector2D Offset = FVector2D::ZeroVector;

	void ZoomAt(float RequestedZoom, const FVector2D& Anchor)
	{
		const float NewZoom = FMath::Clamp(RequestedZoom, MinZoom, MaxZoom);
		Offset = Anchor - (Anchor - Offset) * (NewZoom / Zoom);
		Zoom = NewZoom;
	}

	void Constrain(const FVector2D& ViewSize, const FBox2D& ContentBounds)
	{
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const double ContentSize = ContentBounds.GetSize()[Axis] * Zoom;
			Offset[Axis] = ContentSize <= ViewSize[Axis]
				? (ViewSize[Axis] - ContentSize) * .5 - ContentBounds.Min[Axis] * Zoom
				: FMath::Clamp(Offset[Axis], ViewSize[Axis] - ContentBounds.Max[Axis] * Zoom,
					-ContentBounds.Min[Axis] * Zoom);
		}
	}
};
