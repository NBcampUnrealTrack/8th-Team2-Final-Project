#include "Naia/UI/NUStarterNaiaCardWidget.h"

#include "UI/NUFlipbookImage.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "PaperFlipbook.h"

void UNUStarterNaiaCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CardButton != nullptr)
	{
		CardButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
		CardButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}
}

void UNUStarterNaiaCardWidget::NativeDestruct()
{
	if (CardButton != nullptr)
	{
		CardButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}
	Super::NativeDestruct();
}

void UNUStarterNaiaCardWidget::Setup(int32 InNaiaId, const FText& InName, UPaperFlipbook* InFlipbook, UTexture2D* InElementIcon,
	const FLinearColor& InElementColor)
{
	NaiaId = InNaiaId;
	Flipbook = InFlipbook;
	if (NameText != nullptr)
	{
		NameText->SetText(InName);
	}
	if (ElementIcon != nullptr)
	{
		ElementIcon->SetVisibility(InElementIcon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (InElementIcon != nullptr)
		{
			ElementIcon->SetBrushResourceObject(InElementIcon);
		}
	}
	if (LightBeam != nullptr)
	{
		LightBeam->SetColorAndOpacity(FLinearColor(InElementColor.R, InElementColor.G, InElementColor.B, BeamOpacity));
	}
	if (LightGround != nullptr)
	{
		LightGround->SetColorAndOpacity(FLinearColor(InElementColor.R, InElementColor.G, InElementColor.B, GroundOpacity));
	}
	SetSelected(bSelected);
}

UWidget* UNUStarterNaiaCardWidget::GetFocusTarget() const
{
	return CardButton;
}

void UNUStarterNaiaCardWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (FigureImage != nullptr)
	{
		// 나이아는 한 장짜리 그림이라 고르지 않은 칸도 대기 재생 (어둡게만)
		FigureImage->SetFlipbookAtScale(Flipbook, bSelected ? SelectedPixelScale : FigurePixelScale);
		FigureImage->SetColorAndOpacity(bSelected ? FLinearColor::White : DimmedFigureTint);
	}
	if (NameText != nullptr)
	{
		NameText->SetColorAndOpacity(bSelected ? SelectedNameColor : NameColor);
	}
	if (ElementIcon != nullptr)
	{
		ElementIcon->SetRenderOpacity(bSelected ? 1.f : 0.7f);
		ElementIcon->SetRenderScale(bSelected ? FVector2D(1.25f, 1.25f) : FVector2D(1.f, 1.f));
	}
	if (SelectedLight != nullptr)
	{
		SelectedLight->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UNUStarterNaiaCardWidget::HandleClicked()
{
	OnCardClicked.ExecuteIfBound(NaiaId);
}
