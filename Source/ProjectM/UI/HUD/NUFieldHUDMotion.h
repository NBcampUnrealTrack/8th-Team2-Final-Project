#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Layout/Geometry.h"
#include "Slate/WidgetTransform.h"

class UWidget;

// Containers gate input/visibility; their children own the motion, avoiding double fades.
enum class ENUFieldHUDMotionRole : uint8
{
	Panel,
	Container,
	Content
};

// One reversible timeline for all persistent field HUD pieces. No world timer or strong widget refs.
class FNUFieldHUDMotion
{
public:
	void SetVisible(bool bVisible, const FGeometry& InScreenGeometry = FGeometry());
	void Present(UWidget* Widget, ESlateVisibility NormalVisibility,
		ENUFieldHUDMotionRole Role = ENUFieldHUDMotionRole::Panel);
	void Forget(UWidget* Widget);
	bool Advance(float DeltaTime);
	void Finish();
	void Reset();

private:
	struct FEntry
	{
		TWeakObjectPtr<UWidget> Widget;
		FWidgetTransform RestTransform;
		float RestOpacity = 1.0f;
		float Direction = -1.0f;
		ESlateVisibility NormalVisibility = ESlateVisibility::SelfHitTestInvisible;
		ENUFieldHUDMotionRole Role = ENUFieldHUDMotionRole::Panel;
	};

	void Apply(FEntry& Entry);
	void ApplyAll();
	void MeasureDirection(FEntry& Entry) const;
	static void Restore(const FEntry& Entry);

	TArray<FEntry> Entries;
	FGeometry ScreenGeometry;
	float HiddenProgress = 0.0f;
	bool bTargetVisible = true;
};
