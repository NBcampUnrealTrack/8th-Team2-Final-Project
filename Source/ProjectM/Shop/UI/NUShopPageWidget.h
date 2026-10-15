#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUShopPageWidget.generated.h"

class UCanvasPanel;
class UImage;
class UPaperFlipbookComponent;
class UPaperSprite;
class UTextBlock;

/** Presentation for the shop design page. Commerce remains owned by the shop system. */
UCLASS(Abstract)
class PROJECTM_API UNUShopPageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Use the actual interaction source, never a nearby NPC chosen by distance. */
	UFUNCTION(BlueprintCallable, Category = "NU|Shop|Portrait")
	void SetMerchantActor(AActor* SourceActor);

	UFUNCTION(BlueprintPure, Category = "NU|Shop|Portrait")
	AActor* GetMerchantActor() const { return MerchantActor.Get(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> MerchantPortraitClip;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MerchantPortrait;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MerchantName;

	/** Fraction of the full sprite visible from its top, preserving its aspect ratio. */
	UPROPERTY(EditDefaultsOnly, Category = "NU|Shop|Portrait", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float PortraitVisibleFraction = 0.6f;

private:
	void RefreshMerchantPortrait();
	void ClearMerchantPortrait();

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> MerchantActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPaperFlipbookComponent> MerchantSprite;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPaperSprite> DisplayedFrame;
};
