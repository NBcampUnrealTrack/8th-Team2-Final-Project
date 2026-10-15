#include "Shop/UI/NUShopPageWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Field/NPC/NUNPCBase.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUUIManager.h"

void UNUShopPageWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!MerchantActor.IsValid())
	{
		const ANUPlayerController* PC = GetOwningPlayer<ANUPlayerController>();
		SetMerchantActor(PC ? PC->GetLastDialogueSource() : nullptr);
	}
	else
	{
		SetMerchantActor(MerchantActor.Get());
	}
	// The purchase popup lives inside this page instead of opening a separate window.
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->SetPopupButtonHover(GetWidgetFromName(TEXT("PurchaseDialogPanel")), true);
	}
}

void UNUShopPageWidget::NativeDestruct()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->SetPopupButtonHover(GetWidgetFromName(TEXT("PurchaseDialogPanel")), false);
	}
	ClearMerchantPortrait();
	MerchantActor.Reset();
	MerchantSprite.Reset();
	Super::NativeDestruct();
}

void UNUShopPageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshMerchantPortrait();
}

void UNUShopPageWidget::SetMerchantActor(AActor* SourceActor)
{
	ClearMerchantPortrait();
	MerchantActor = IsValid(SourceActor) && SourceActor->GetWorld() == GetWorld() ? SourceActor : nullptr;
	MerchantSprite = MerchantActor.IsValid()
		? MerchantActor->FindComponentByClass<UPaperFlipbookComponent>() : nullptr;
	if (MerchantName)
	{
		const ANUNPCBase* NPC = Cast<ANUNPCBase>(MerchantActor.Get());
		MerchantName->SetText(NPC ? NPC->GetNPCDisplayName() : FText::GetEmpty());
	}
	RefreshMerchantPortrait();
}

void UNUShopPageWidget::RefreshMerchantPortrait()
{
	UPaperFlipbookComponent* Sprite = MerchantSprite.Get();
	UPaperFlipbook* Flipbook = MerchantActor.IsValid() && IsValid(Sprite) ? Sprite->GetFlipbook() : nullptr;
	UPaperSprite* Frame = Flipbook ? Flipbook->GetSpriteAtTime(Sprite->GetPlaybackPosition(), true) : nullptr;
	if (!Frame)
	{
		ClearMerchantPortrait();
		return;
	}
	if (!MerchantPortrait || !MerchantPortraitClip || DisplayedFrame.Get() == Frame) return;

	UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(MerchantPortrait->Slot);
	const UCanvasPanelSlot* ClipSlot = Cast<UCanvasPanelSlot>(MerchantPortraitClip->Slot);
	const FVector2D SourceSize = Frame->GetSlateAtlasData().GetSourceDimensions();
	if (!ImageSlot || !ClipSlot || SourceSize.X <= 0.0 || SourceSize.Y <= 0.0) return;

	// Paper2D's atlas interface works with cooked sprites too. Keep the real animation,
	// alpha and colors without spawning a second NPC or changing its world pose.
	TScriptInterface<ISlateTextureAtlasInterface> Atlas(Frame);
	MerchantPortrait->SetBrushFromAtlasInterface(Atlas);
	const FVector2D ClipSize = ClipSlot->GetSize();
	const double VisibleFraction = FMath::Clamp(PortraitVisibleFraction, 0.3f, 1.0f);
	// Fit both the full width and the upper-body height. Anchor the crop at the
	// counter so wide characters keep their shoulders without revealing their legs.
	const double Scale = FMath::Min(ClipSize.X / SourceSize.X, ClipSize.Y / (SourceSize.Y * VisibleFraction));
	const double Height = SourceSize.Y * Scale;
	const double Width = SourceSize.X * Scale;
	ImageSlot->SetSize(FVector2D(Width, Height));
	ImageSlot->SetPosition(FVector2D((ClipSize.X - Width) * 0.5, ClipSize.Y - Height * VisibleFraction));
	MerchantPortrait->SetColorAndOpacity(Sprite->GetSpriteColor());
	MerchantPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (MerchantName)
	{
		const ANUNPCBase* NPC = Cast<ANUNPCBase>(MerchantActor.Get());
		MerchantName->SetText(NPC ? NPC->GetNPCDisplayName() : FText::GetEmpty());
	}
	DisplayedFrame = Frame;
}

void UNUShopPageWidget::ClearMerchantPortrait()
{
	if (MerchantPortrait)
	{
		MerchantPortrait->SetBrushFromTexture(nullptr);
		MerchantPortrait->SetVisibility(ESlateVisibility::Hidden);
	}
	if (MerchantName) MerchantName->SetText(FText::GetEmpty());
	DisplayedFrame.Reset();
}
