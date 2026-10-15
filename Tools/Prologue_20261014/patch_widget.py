# 일회성 패치: NUStorySlideshowWidget.cpp 에 도트 사진 배치·타이틀·길게 눌러 스킵을 넣는다.
import sys

p = sys.argv[1]
s = open(p, encoding='utf-8').read()


def rep(a, b):
    global s
    assert s.count(a) == 1, a[:80]
    s = s.replace(a, b)


rep('''#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
''', '''#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
''')

rep('''	UScaleBox* PictureScale = WidgetTree->ConstructWidget<UScaleBox>();
	PictureScale->SetStretch(EStretch::ScaleToFit);''', '''	PictureScale = WidgetTree->ConstructWidget<UScaleBox>();
	PictureScale->SetStretch(EStretch::ScaleToFit);''')

rep('''	Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Picture"));
	PictureScale->AddChild(Picture);
''', '''	Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Picture"));
	PictureScale->AddChild(Picture);

	// 타이틀은 사진 위, 대사 아래에 둔다. 크기와 위치는 화면 크기를 보고 매 프레임 맞춘다.
	Title = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StoryTitle"));
	Title->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* TitleSlot = Canvas->AddChildToCanvas(Title);
	TitleSlot->SetAnchors(FAnchors(0, 0));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
''')

rep('''	UTextBlock* SkipLabel = WidgetTree->ConstructWidget<UTextBlock>();
	SkipLabel->SetText(NSLOCTEXT("NUStory", "Skip", "스킵"));
	SkipLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SkipButton->AddChild(SkipLabel);
	SkipButton->OnClicked.AddDynamic(this, &ThisClass::RequestSkip);
}''', '''	UOverlay* SkipContent = WidgetTree->ConstructWidget<UOverlay>();
	SkipButton->AddChild(SkipContent);
	SkipLabel = WidgetTree->ConstructWidget<UTextBlock>();
	SkipLabel->SetText(NSLOCTEXT("NUStory", "Skip", "스킵"));
	SkipLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UOverlaySlot* LabelSlot = SkipContent->AddChildToOverlay(SkipLabel))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 길게 눌러 스킵할 때만 보이는 진행 막대
	USizeBox* ProgressBox = WidgetTree->ConstructWidget<USizeBox>();
	ProgressBox->SetHeightOverride(3.0f);
	SkipProgress = WidgetTree->ConstructWidget<UProgressBar>();
	SkipProgress->SetPercent(0.0f);
	SkipProgress->SetFillColorAndOpacity(FLinearColor(1.0f, 0.93f, 0.78f, 1.0f));
	ProgressBox->AddChild(SkipProgress);
	ProgressBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* ProgressSlot = SkipContent->AddChildToOverlay(ProgressBox))
	{
		ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
		ProgressSlot->SetVerticalAlignment(VAlign_Bottom);
		ProgressSlot->SetPadding(FMargin(0, 0, 0, -6));
	}

	SkipButton->OnClicked.AddDynamic(this, &ThisClass::RequestSkip);
	SkipButton->OnPressed.AddDynamic(this, &ThisClass::HandleSkipPressed);
	SkipButton->OnReleased.AddDynamic(this, &ThisClass::HandleSkipReleased);
}''')

rep('''	Data = InData;
	SequenceCue = InCue;
	SkipButton->SetVisibility(Data->bAllowSkip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SlideIndex = INDEX_NONE;
	LineIndex = INDEX_NONE;
	DisplayedSlideIndex = INDEX_NONE;
	Picture->SetRenderOpacity(0.0f);''', '''	Data = InData;
	SequenceCue = InCue;
	SkipButton->SetVisibility(Data->bAllowSkip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SlideIndex = INDEX_NONE;
	LineIndex = INDEX_NONE;
	DisplayedSlideIndex = INDEX_NONE;
	Picture->SetRenderOpacity(0.0f);
	PictureScale->SetStretch(Data->bPixelPerfectPhotos ? EStretch::UserSpecified : EStretch::ScaleToFit);
	PictureScale->SetRenderTransform(FWidgetTransform());

	Title->SetRenderOpacity(0.0f);
	Title->SetVisibility(ESlateVisibility::Collapsed);
	if (Data->TitleImage != nullptr)
	{
		Title->SetBrushFromTexture(Data->TitleImage, true);
	}

	// 길게 눌러 스킵하는 연출은 키 입력을 받아야 하므로 포커스를 가져온다. 기존 연출은 그대로 둔다.
	bSkipButtonHeld = false;
	bSkipKeyHeld = false;
	SkipHoldSeconds = 0.0f;
	const bool bHold = Data->bAllowSkip && Data->bHoldToSkip;
	SkipLabel->SetText(bHold ? NSLOCTEXT("NUStory", "HoldToSkip", "길게 눌러 건너뛰기") : NSLOCTEXT("NUStory", "Skip", "스킵"));
	SkipProgress->SetPercent(0.0f);
	if (UWidget* ProgressBox = SkipProgress->GetParent())
	{
		ProgressBox->SetVisibility(bHold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UCanvasPanelSlot* SkipSlot = Cast<UCanvasPanelSlot>(SkipButton->Slot))
	{
		SkipSlot->SetSize(FVector2D(bHold ? 200.0f : 112.0f, 42.0f));
	}
	if (bHold)
	{
		SetUserFocus(GetOwningPlayer());
		SetKeyboardFocus();
	}''')

rep('''	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bIsPlaying || !IsValid(SequenceCue) || !Data)
	{
		return;
	}
	Backdrop->SetRenderOpacity(FMath::Clamp(SequenceCue->BackdropOpacity, 0.0f, 1.0f));''', '''	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bIsPlaying || !IsValid(SequenceCue) || !Data)
	{
		return;
	}
	UpdateHoldToSkip(InDeltaTime);
	if (!bIsPlaying || !IsValid(SequenceCue) || !Data)
	{
		// 스킵이 확정되어 재생이 끝났다.
		return;
	}
	Backdrop->SetRenderOpacity(FMath::Clamp(SequenceCue->BackdropOpacity, 0.0f, 1.0f));
	ApplyTitleLayout(MyGeometry);
	if (Data->SubtitleBottomFraction > 0.0f)
	{
		if (UCanvasPanelSlot* SubtitleSlot = Cast<UCanvasPanelSlot>(Subtitle->Slot))
		{
			SubtitleSlot->SetOffsets(FMargin(0, -MyGeometry.GetLocalSize().Y * Data->SubtitleBottomFraction, 0, 0));
		}
	}''')

rep('''	// 사진이 없는 장면은 대사만 띄운다(암전, 레벨 화면 위).
	Picture->SetRenderOpacity(Slide.Image != nullptr ? FMath::Clamp(SequenceCue->PhotoOpacity, 0.0f, 1.0f) : 0.0f);''', '''	// 사진이 없는 장면은 대사만 띄운다(암전, 레벨 화면 위).
	Picture->SetRenderOpacity(Slide.Image != nullptr ? FMath::Clamp(SequenceCue->PhotoOpacity, 0.0f, 1.0f) : 0.0f);
	if (Slide.Image != nullptr)
	{
		ApplyPhotoLayout(MyGeometry);
	}''')

rep('''void UNUStorySlideshowWidget::FinishSlideshow()''', '''void UNUStorySlideshowWidget::ApplyPhotoLayout(const FGeometry& MyGeometry)
{
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FVector2D PixelSize = MyGeometry.GetAbsoluteSize();
	if (LocalSize.X <= 0.0 || LocalSize.Y <= 0.0 || PixelSize.X <= 0.0)
	{
		return;
	}
	const float PanX = FMath::Clamp(SequenceCue->PhotoPanX, -1.0f, 1.0f);
	const float PanY = FMath::Clamp(SequenceCue->PhotoPanY, -1.0f, 1.0f);

	if (!Data->bPixelPerfectPhotos)
	{
		// 일반 사진: 확대한 만큼 생긴 여백 안에서 옮긴다.
		const float Scale = FMath::Max(SequenceCue->PhotoScale, 0.01f);
		const FVector2D Room = LocalSize * 0.5 * FMath::Max(Scale - 1.0f, 0.0f);
		PictureScale->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		PictureScale->SetRenderTransform(FWidgetTransform(FVector2D(PanX * Room.X, PanY * Room.Y), FVector2D(Scale, Scale), FVector2D::ZeroVector, 0.0f));
		return;
	}

	const UTexture2D* Texture = Data->Slides.IsValidIndex(SlideIndex) ? Data->Slides[SlideIndex].Image.Get() : nullptr;
	const float TextureWidth = Texture != nullptr ? Texture->GetSurfaceWidth() : 0.0f;
	const float TextureHeight = Texture != nullptr ? Texture->GetSurfaceHeight() : 0.0f;
	if (TextureWidth <= 0.0f || TextureHeight <= 0.0f)
	{
		return;
	}

	// 화면을 덮는 가장 작은 정수 배율. 그림 한 칸이 화면 픽셀 N개가 되어 도트가 고르게 보인다.
	const double DpiScale = PixelSize.X / LocalSize.X;
	const int32 PixelsPerTexel = FMath::Max(1, FMath::CeilToInt(FMath::Max(PixelSize.X / TextureWidth, PixelSize.Y / TextureHeight) - 0.001));
	PictureScale->SetUserSpecifiedScale(static_cast<float>(PixelsPerTexel / DpiScale));

	// 화면 밖으로 남는 여백 안에서 그림 한 칸 단위로 옮긴다.
	const double RoomX = FMath::Max(0.0, (TextureWidth * PixelsPerTexel - PixelSize.X) * 0.5);
	const double RoomY = FMath::Max(0.0, (TextureHeight * PixelsPerTexel - PixelSize.Y) * 0.5);
	const double OffsetX = FMath::RoundToDouble(PanX * RoomX / PixelsPerTexel) * PixelsPerTexel;
	const double OffsetY = FMath::RoundToDouble(PanY * RoomY / PixelsPerTexel) * PixelsPerTexel;
	PictureScale->SetRenderTransform(FWidgetTransform(FVector2D(OffsetX / DpiScale, OffsetY / DpiScale), FVector2D(1.0f, 1.0f), FVector2D::ZeroVector, 0.0f));
}

void UNUStorySlideshowWidget::ApplyTitleLayout(const FGeometry& MyGeometry)
{
	const float Opacity = FMath::Clamp(SequenceCue->TitleOpacity, 0.0f, 1.0f);
	if (Data->TitleImage == nullptr || Opacity <= 0.0f)
	{
		Title->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const float TextureWidth = Data->TitleImage->GetSurfaceWidth();
	const float TextureHeight = Data->TitleImage->GetSurfaceHeight();
	if (TextureWidth <= 0.0f || LocalSize.X <= 0.0)
	{
		return;
	}
	const float Width = LocalSize.X * Data->TitleWidthFraction;
	if (UCanvasPanelSlot* TitleSlot = Cast<UCanvasPanelSlot>(Title->Slot))
	{
		TitleSlot->SetPosition(FVector2D(LocalSize.X * 0.5f, LocalSize.Y * Data->TitleCenterHeight));
		TitleSlot->SetSize(FVector2D(Width, Width * TextureHeight / TextureWidth));
	}
	Title->SetRenderOpacity(Opacity);
	Title->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNUStorySlideshowWidget::UpdateHoldToSkip(float DeltaSeconds)
{
	if (!Data->bAllowSkip || !Data->bHoldToSkip)
	{
		return;
	}

	// 손을 떼면 진행이 빠르게 되돌아간다.
	const bool bHeld = bSkipButtonHeld || bSkipKeyHeld;
	SkipHoldSeconds = bHeld ? SkipHoldSeconds + DeltaSeconds : FMath::Max(0.0f, SkipHoldSeconds - DeltaSeconds * 2.0f);
	const float Progress = FMath::Clamp(SkipHoldSeconds / FMath::Max(Data->HoldToSkipSeconds, 0.1f), 0.0f, 1.0f);
	SkipProgress->SetPercent(Progress);
	if (Progress >= 1.0f)
	{
		bSkipButtonHeld = false;
		bSkipKeyHeld = false;
		SkipHoldSeconds = 0.0f;
		OnSkipRequested.Broadcast();
	}
}

void UNUStorySlideshowWidget::HandleSkipPressed()
{
	bSkipButtonHeld = true;
}

void UNUStorySlideshowWidget::HandleSkipReleased()
{
	bSkipButtonHeld = false;
}

bool UNUStorySlideshowWidget::IsSkipKey(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
}

FReply UNUStorySlideshowWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bIsPlaying && Data != nullptr && Data->bAllowSkip && IsSkipKey(InKeyEvent.GetKey()))
	{
		if (Data->bHoldToSkip)
		{
			bSkipKeyHeld = true;
		}
		else if (!InKeyEvent.IsRepeat())
		{
			RequestSkip();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UNUStorySlideshowWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsSkipKey(InKeyEvent.GetKey()))
	{
		bSkipKeyHeld = false;
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UNUStorySlideshowWidget::FinishSlideshow()''')

rep('''void UNUStorySlideshowWidget::RequestSkip()
{
	if (bIsPlaying && Data != nullptr && Data->bAllowSkip)''', '''void UNUStorySlideshowWidget::RequestSkip()
{
	// 길게 눌러야 하는 연출은 클릭 한 번으로 넘기지 않는다.
	if (bIsPlaying && Data != nullptr && Data->bAllowSkip && !Data->bHoldToSkip)''')

open(p, 'w', encoding='utf-8').write(s)
print('ok')
