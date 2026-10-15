#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

enum class ENUQuestBattleKind : uint8;

// 작은 카드에서도 장식의 선이 끊기지 않도록 현재 화면 해상도로 그린다.
class SNUQuestPracticeGuideArt : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SNUQuestPracticeGuideArt) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetPractice(ENUQuestBattleKind InKind, int32 InCount, int32 InRequired);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	ENUQuestBattleKind Kind{};
	int32 Count = 0;
	int32 Required = 0;
};
