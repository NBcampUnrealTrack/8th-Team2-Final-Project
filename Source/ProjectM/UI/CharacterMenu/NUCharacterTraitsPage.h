#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CharacterMenu/NUCharacterTraitsView.h"
#include "NUCharacterTraitsPage.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;

// Presentation controls for the authored trait tree; no progression/data mutations.
UCLASS(Abstract)
class PROJECTM_API UNUCharacterTraitsPage : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu|Traits") void ZoomIn();
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu|Traits") void ZoomOut();
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu|Traits") void ResetView();
	UFUNCTION(BlueprintPure, Category = "NU|CharacterMenu|Traits") float GetZoom() const { return View.Zoom; }
	UFUNCTION(BlueprintPure, Category = "NU|CharacterMenu|Traits") FVector2D GetViewOffset() const { return View.Offset; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> GraphClip;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> GraphViewport;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> GraphZoomInButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> GraphZoomOutButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> GraphResetButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GraphZoomText;

private:
	friend class FNUCharacterTraitsInputTest;
	FVector2D GetViewSize() const;
	bool IsOverGraph(const FVector2D& ScreenPosition) const;
	void ChangeZoom(float NewZoom, const FVector2D& Anchor);
	void ApplyView();
	void MeasureContentBounds();

	FNUCharacterTraitsView View;
	FBox2D ContentBounds = FBox2D(FVector2D::ZeroVector, FVector2D(768.f, 420.f));
	FVector2D LastViewSize = FVector2D::ZeroVector;
	FKey PanButton;
	bool bPanning = false;
};
