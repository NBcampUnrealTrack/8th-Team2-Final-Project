#pragma once

#include "CoreMinimal.h"

class UButton;
class UWidget;

// Popup-local presentation only; leaves button styles, delegates and layout slots intact.
class FNUPopupButtonHover
{
public:
	static constexpr float HoverScale = 0.97f;
	static constexpr float Duration = 0.12f;

	void RegisterRoot(UWidget* Root);
	void UnregisterRoot(UWidget* Root);
	void Reset();
	bool HasTargets() const { return !Entries.IsEmpty(); }
	void Advance(float DeltaTime);
	// Also allows deterministic verification without moving the desktop cursor.
	void Advance(float DeltaTime, TFunctionRef<bool(UButton&)> HoverQuery);

private:
	struct FEntry
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<UButton> Button;
		FVector2D RestScale = FVector2D::UnitVector;
		float Progress = 0.0f;
		bool bHovered = false;
	};

	static void Restore(const FEntry& Entry);
	static bool HoldAtOriginalEdge(const FEntry& Entry);
	TArray<FEntry> Entries;
};
