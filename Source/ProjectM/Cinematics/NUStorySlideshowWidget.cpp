#include "Cinematics/NUStorySlideshowWidget.h"

#include "Dialogue/UI/NUDialogueWidget.h"
#include "UI/NUUISettings.h"
#include "Cinematics/NUStorySequenceCue.h"
#include "Cinematics/NUStorySlideshowData.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"

namespace NUStoryMotion
{
	constexpr int32 MoteCount = 64;

	// 나이아 색을 닮은 빛: 금빛, 민트, 분홍, 하늘, 흰빛
	static const FLinearColor MoteColors[] = {
		FLinearColor(1.00f, 0.85f, 0.45f), FLinearColor(0.62f, 1.00f, 0.80f), FLinearColor(1.00f, 0.70f, 0.86f),
		FLinearColor(0.66f, 0.86f, 1.00f), FLinearColor(1.00f, 0.98f, 0.92f),
	};
}

namespace NUStorySkip
{
	// 설정·HUD 와 같은 팔레트(sRGB): 남색 #16293D, 샴페인 금 #C8B691, 아이보리 #F1EDE6
	static FLinearColor Navy() { return FLinearColor(FColor(0x16, 0x29, 0x3D)); }
	static FLinearColor Gold() { return FLinearColor(FColor(0xC8, 0xB6, 0x91)); }
	static FLinearColor Ivory() { return FLinearColor(FColor(0xF1, 0xED, 0xE6)); }
	static FLinearColor WithAlpha(FLinearColor Color, float Alpha) { Color.A = Alpha; return Color; }
	// 자막처럼 밝은 그림 위에서도 읽히는 어두운 외곽선
	static FLinearColor Outline() { return FLinearColor(0.02f, 0.03f, 0.05f, 0.75f); }
	constexpr float IdleOpacity = 0.62f;
	// 시작 검은 화면에서는 숨겨 두었다가 첫 그림과 함께 떠오른다.
	constexpr float AppearStart = 0.8f;
	constexpr float AppearEnd = 2.2f;
}

void UNUStorySlideshowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor::Black);
	UCanvasPanelSlot* BackgroundSlot = Canvas->AddChildToCanvas(Backdrop);
	BackgroundSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	BackgroundSlot->SetOffsets(FMargin(0));

	PictureScale = WidgetTree->ConstructWidget<UScaleBox>();
	PictureScale->SetStretch(EStretch::ScaleToFit);
	UCanvasPanelSlot* PictureSlot = Canvas->AddChildToCanvas(PictureScale);
	PictureSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PictureSlot->SetOffsets(FMargin(0));
	Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Picture"));
	PictureScale->AddChild(Picture);

	// 크로스페이드: 이전 사진을 새 사진 위에 얹어 녹여 낸다.
	PrevPictureScale = WidgetTree->ConstructWidget<UScaleBox>();
	PrevPictureScale->SetStretch(EStretch::ScaleToFit);
	UCanvasPanelSlot* PrevSlot = Canvas->AddChildToCanvas(PrevPictureScale);
	PrevSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PrevSlot->SetOffsets(FMargin(0));
	PrevPicture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PrevPicture"));
	PrevPicture->SetRenderOpacity(0.0f);
	PrevPictureScale->AddChild(PrevPicture);
	PrevPictureScale->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 도트 빛 입자 층
	MoteLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
	UCanvasPanelSlot* MoteSlot = Canvas->AddChildToCanvas(MoteLayer);
	MoteSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	MoteSlot->SetOffsets(FMargin(0));
	MoteLayer->SetVisibility(ESlateVisibility::Collapsed);
	Motes.SetNum(NUStoryMotion::MoteCount);
	for (int32 Index = 0; Index < NUStoryMotion::MoteCount; ++Index)
	{
		// 입자 하나 = 가로 막대 + 세로 막대. 반짝일 때 팔이 뻗어 '+' 모양 도트 반짝이가 된다.
		for (int32 Arm = 0; Arm < 2; ++Arm)
		{
			UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
			Box->SetBrushColor(NUStoryMotion::MoteColors[Index % UE_ARRAY_COUNT(NUStoryMotion::MoteColors)]);
			Box->SetRenderOpacity(0.0f);
			MoteLayer->AddChildToCanvas(Box);
			MoteBoxes.Add(Box);
		}
		ResetMote(Index, true);
	}

	// 타이틀은 사진 위, 대사 아래에 둔다. 크기와 위치는 화면 크기를 보고 매 프레임 맞춘다.
	Title = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StoryTitle"));
	Title->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* TitleSlot = Canvas->AddChildToCanvas(Title);
	TitleSlot->SetAnchors(FAnchors(0, 0));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));

	// 장면 전환용 빛과 시네마 레터박스. 자막은 그 위에 그린다.
	Flash = WidgetTree->ConstructWidget<UBorder>();
	UCanvasPanelSlot* FlashSlot = Canvas->AddChildToCanvas(Flash);
	FlashSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	FlashSlot->SetOffsets(FMargin(0));
	Flash->SetVisibility(ESlateVisibility::Collapsed);

	LetterboxTop = WidgetTree->ConstructWidget<UBorder>();
	LetterboxTop->SetBrushColor(FLinearColor::Black);
	UCanvasPanelSlot* TopSlot = Canvas->AddChildToCanvas(LetterboxTop);
	TopSlot->SetAnchors(FAnchors(0, 0, 1, 0));
	TopSlot->SetOffsets(FMargin(0, 0, 0, 0));
	LetterboxTop->SetVisibility(ESlateVisibility::Collapsed);

	LetterboxBottom = WidgetTree->ConstructWidget<UBorder>();
	LetterboxBottom->SetBrushColor(FLinearColor::Black);
	UCanvasPanelSlot* BottomSlot = Canvas->AddChildToCanvas(LetterboxBottom);
	BottomSlot->SetAnchors(FAnchors(0, 1, 1, 1));
	BottomSlot->SetAlignment(FVector2D(0, 1));
	BottomSlot->SetOffsets(FMargin(0, 0, 0, 0));
	LetterboxBottom->SetVisibility(ESlateVisibility::Collapsed);

	// 기존 NPC 대사창을 재사용하며 DialogueText 위젯 이름으로 본문을 찾는다.
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUDialogueWidget> WindowClass = Settings->DialogueWidgetClass.LoadSynchronous();
	if (WindowClass)
	{
		DialogueWindow = CreateWidget<UNUDialogueWidget>(GetOwningPlayer(), WindowClass);
		UCanvasPanelSlot* WindowSlot = Canvas->AddChildToCanvas(DialogueWindow);
		WindowSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		WindowSlot->SetOffsets(FMargin(0));
		Dialogue = Cast<UTextBlock>(DialogueWindow->GetWidgetFromName(TEXT("DialogueText")));
	}

	Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StorySubtitle"));
	Subtitle->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Subtitle->SetShadowColorAndOpacity(FLinearColor::Black);
	Subtitle->SetShadowOffset(FVector2D(1, 2));
	Subtitle->SetJustification(ETextJustify::Center);
	Subtitle->SetAutoWrapText(true);
	Subtitle->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* SubtitleSlot = Canvas->AddChildToCanvas(Subtitle);
	SubtitleSlot->SetAnchors(FAnchors(0.12f, 1, 0.88f, 1));
	SubtitleSlot->SetAlignment(FVector2D(0, 1));
	SubtitleSlot->SetOffsets(FMargin(0, -60, 0, 0));
	SubtitleSlot->SetAutoSize(true);

	SkipButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SkipButton"));
	SkipButton->SetBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
	UCanvasPanelSlot* SkipSlot = Canvas->AddChildToCanvas(SkipButton);
	SkipSlot->SetAnchors(FAnchors(1, 1));
	SkipSlot->SetAlignment(FVector2D(1, 1));
	SkipSlot->SetPosition(FVector2D(-32, -24));
	SkipSlot->SetSize(FVector2D(112, 42));
	UOverlay* SkipContent = WidgetTree->ConstructWidget<UOverlay>();
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

	BuildHoldSkipContent();

	SkipButton->OnClicked.AddDynamic(this, &ThisClass::RequestSkip);
	SkipButton->OnPressed.AddDynamic(this, &ThisClass::HandleSkipPressed);
	SkipButton->OnReleased.AddDynamic(this, &ThisClass::HandleSkipReleased);
}

bool UNUStorySlideshowWidget::StartSequencePresentation(UNUStorySlideshowData* InData, ANUStorySequenceCue* InCue)
{
	if (!InData || !IsValid(InCue) || !Backdrop || !Picture || !Subtitle)
	{
		return false;
	}

	Dialogue = InData->bUseSubtitles ? Subtitle.Get()
		: DialogueWindow != nullptr ? Cast<UTextBlock>(DialogueWindow->GetWidgetFromName(TEXT("DialogueText"))) : nullptr;
	if (Dialogue == nullptr)
	{
		return false;
	}

	Data = InData;
	SequenceCue = InCue;
	SkipButton->SetVisibility(Data->bAllowSkip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SlideIndex = INDEX_NONE;
	LineIndex = INDEX_NONE;
	DisplayedSlideIndex = INDEX_NONE;
	Picture->SetRenderOpacity(0.0f);
	PictureScale->SetStretch(Data->bPixelPerfectPhotos ? EStretch::UserSpecified : EStretch::ScaleToFit);
	PictureScale->SetRenderTransform(FWidgetTransform());
	PrevPictureScale->SetStretch(Data->bPixelPerfectPhotos ? EStretch::UserSpecified : EStretch::ScaleToFit);
	PrevPicture->SetRenderOpacity(0.0f);
	PrevFade = 0.0f;
	LastPhotoOpacity = 0.0f;
	Flash->SetBrushColor(Data->FlashColor);
	for (int32 Index = 0; Index < Motes.Num(); ++Index)
	{
		ResetMote(Index, true);
	}

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
	SkipShownSeconds = 0.0f;
	SkipEmphasis = 0.0f;
	const bool bHold = Data->bAllowSkip && Data->bHoldToSkip;
	SkipProgress->SetPercent(0.0f);
	if (bHold)
	{
		ApplyHoldSkipStyle();
		SetUserFocus(GetOwningPlayer());
		SetKeyboardFocus();
	}
	Dialogue->SetText(FText::GetEmpty());
	if (Data->bUseSubtitles)
	{
		if (DialogueWindow != nullptr)
		{
			DialogueWindow->SetVisibility(ESlateVisibility::Collapsed);
		}
		Subtitle->SetFont(Data->SubtitleFont.FontObject != nullptr ? Data->SubtitleFont : FCoreStyle::GetDefaultFontStyle("Regular", 26));
		Subtitle->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Subtitle->SetVisibility(ESlateVisibility::Collapsed);
		PrepareDialogueWindow();
	}
	bIsPlaying = true;
	return true;
}

void UNUStorySlideshowWidget::PrepareDialogueWindow()
{
	// 기존 WBP는 NativeConstruct에서 숨겨지므로 뷰포트에 붙인 뒤 연출용으로 표시한다.
	DialogueWindow->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWidget* Panel = DialogueWindow->GetWidgetFromName(TEXT("DialoguePanel")))
	{
		Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	for (const FName Name : { FName(TEXT("SpeakerNameText")), FName(TEXT("PortraitImage")), FName(TEXT("NextButton")), FName(TEXT("CloseButton")), FName(TEXT("InteractionOptionPanel")) })
	{
		if (UWidget* Widget = DialogueWindow->GetWidgetFromName(Name))
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	Dialogue->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNUStorySlideshowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bIsPlaying || !IsValid(SequenceCue) || !Data)
	{
		return;
	}
	UpdateHoldToSkip(InDeltaTime);
	UpdateHoldSkipLook(InDeltaTime);
	if (!bIsPlaying || !IsValid(SequenceCue) || !Data)
	{
		// 스킵이 확정되어 재생이 끝났다.
		return;
	}
	Backdrop->SetRenderOpacity(FMath::Clamp(SequenceCue->BackdropOpacity, 0.0f, 1.0f));
	UpdateOverlays(MyGeometry);
	UpdateMotes(InDeltaTime, MyGeometry);
	UpdateCrossfade(InDeltaTime, MyGeometry);
	ApplyTitleLayout(MyGeometry);
	if (Data->SubtitleBottomFraction > 0.0f)
	{
		if (UCanvasPanelSlot* SubtitleSlot = Cast<UCanvasPanelSlot>(Subtitle->Slot))
		{
			SubtitleSlot->SetOffsets(FMargin(0, -MyGeometry.GetLocalSize().Y * Data->SubtitleBottomFraction, 0, 0));
		}
	}
	if (!Data->Slides.IsValidIndex(SequenceCue->SlideIndex))
	{
		Picture->SetRenderOpacity(0.0f);
		Dialogue->SetText(FText::GetEmpty());
		SlideIndex = INDEX_NONE;
		LineIndex = INDEX_NONE;
		return;
	}

	// 시퀀스의 현재 상태를 읽으므로 일시정지와 시간 이동이 화면에도 반영된다.
	const bool bCaptionChanged = SlideIndex != SequenceCue->SlideIndex || LineIndex != SequenceCue->LineIndex;
	SlideIndex = SequenceCue->SlideIndex;
	LineIndex = SequenceCue->LineIndex;
	const FNUStorySlide& Slide = Data->Slides[SlideIndex];
	if (DisplayedSlideIndex != SlideIndex)
	{
		// 앞 사진이 보이던 중이면 그 사진을 위층에 남겨 녹여 낸다.
		const UTexture2D* PreviousImage = Data->Slides.IsValidIndex(DisplayedSlideIndex) ? Data->Slides[DisplayedSlideIndex].Image.Get() : nullptr;
		if (Data->bCrossfadePhotos && PreviousImage != nullptr && LastPhotoOpacity > 0.0f && Data->PhotoTransitionSeconds > 0.0f)
		{
			PrevTexture = Data->Slides[DisplayedSlideIndex].Image;
			PrevPicture->SetBrushFromTexture(PrevTexture, true);
			PrevPanX = LastPanX;
			PrevPanY = LastPanY;
			PrevFade = LastPhotoOpacity;
		}
		Picture->SetBrushFromTexture(Slide.Image, true);
		DisplayedSlideIndex = SlideIndex;
	}
	// 사진이 없는 장면은 대사만 띄운다(암전, 레벨 화면 위).
	LastPhotoOpacity = Slide.Image != nullptr ? FMath::Clamp(SequenceCue->PhotoOpacity, 0.0f, 1.0f) : 0.0f;
	Picture->SetRenderOpacity(LastPhotoOpacity);
	if (Slide.Image != nullptr)
	{
		ApplyPhotoLayout(MyGeometry);
		LastPanX = SequenceCue->PhotoPanX;
		LastPanY = SequenceCue->PhotoPanY;
	}
	if (!Slide.Lines.IsValidIndex(LineIndex))
	{
		Dialogue->SetText(FText::GetEmpty());
		return;
	}
	// 대사가 바뀔 때만 글자 경계를 계산하고, 표시할 글자 수는 매 프레임 확인한다.
	if (bCaptionChanged)
	{
		TextReveal.SetText(Slide.Lines[LineIndex]);
	}
	TextReveal.ApplyTo(*Dialogue, FMath::FloorToInt(SequenceCue->RevealedCharacters));
	const float CaptionOpacity = FMath::Clamp(SequenceCue->CaptionOpacity, 0.0f, 1.0f);
	Dialogue->SetRenderOpacity(CaptionOpacity);
	if (Data->bUseSubtitles)
	{
		// 나타날 때 살짝 떠오르고 사라질 때 살짝 가라앉는다.
		Dialogue->SetRenderTranslation(FVector2D(0.0f, (1.0f - CaptionOpacity) * 10.0f));
	}
}

void UNUStorySlideshowWidget::ApplyPhotoLayout(const FGeometry& MyGeometry)
{
	const UTexture2D* Texture = Data->Slides.IsValidIndex(SlideIndex) ? Data->Slides[SlideIndex].Image.Get() : nullptr;
	LayoutPhoto(PictureScale, Texture, SequenceCue->PhotoPanX, SequenceCue->PhotoPanY, MyGeometry);
}

int32 UNUStorySlideshowWidget::GetPixelsPerTexel(const UTexture2D* Texture, const FGeometry& MyGeometry) const
{
	const FVector2D PixelSize = MyGeometry.GetAbsoluteSize();
	const float TextureWidth = Texture != nullptr ? Texture->GetSurfaceWidth() : 0.0f;
	const float TextureHeight = Texture != nullptr ? Texture->GetSurfaceHeight() : 0.0f;
	if (TextureWidth <= 0.0f || TextureHeight <= 0.0f)
	{
		return FMath::Max(1, FMath::RoundToInt(PixelSize.Y / 360.0));
	}
	return FMath::Max(1, FMath::CeilToInt(FMath::Max(PixelSize.X / TextureWidth, PixelSize.Y / TextureHeight) - 0.001));
}

void UNUStorySlideshowWidget::LayoutPhoto(UScaleBox* Box, const UTexture2D* Texture, float InPanX, float InPanY, const FGeometry& MyGeometry) const
{
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FVector2D PixelSize = MyGeometry.GetAbsoluteSize();
	if (Box == nullptr || LocalSize.X <= 0.0 || LocalSize.Y <= 0.0 || PixelSize.X <= 0.0)
	{
		return;
	}
	const float PanX = FMath::Clamp(InPanX, -1.0f, 1.0f);
	const float PanY = FMath::Clamp(InPanY, -1.0f, 1.0f);

	if (!Data->bPixelPerfectPhotos)
	{
		// 일반 사진: 확대한 만큼 생긴 여백 안에서 옮긴다.
		const float Scale = FMath::Max(SequenceCue->PhotoScale, 0.01f);
		const FVector2D Room = LocalSize * 0.5 * FMath::Max(Scale - 1.0f, 0.0f);
		Box->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Box->SetRenderTransform(FWidgetTransform(FVector2D(PanX * Room.X, PanY * Room.Y), FVector2D(Scale, Scale), FVector2D::ZeroVector, 0.0f));
		return;
	}

	const float TextureWidth = Texture != nullptr ? Texture->GetSurfaceWidth() : 0.0f;
	const float TextureHeight = Texture != nullptr ? Texture->GetSurfaceHeight() : 0.0f;
	if (TextureWidth <= 0.0f || TextureHeight <= 0.0f)
	{
		return;
	}

	// 화면을 덮는 가장 작은 정수 배율. 그림 한 칸이 화면 픽셀 N개가 되어 도트가 고르게 보인다.
	const double DpiScale = PixelSize.X / LocalSize.X;
	const int32 PixelsPerTexel = GetPixelsPerTexel(Texture, MyGeometry);
	Box->SetUserSpecifiedScale(static_cast<float>(PixelsPerTexel / DpiScale));

	// 화면 밖으로 남는 여백 안에서 그림 한 칸 단위로 옮긴다.
	const double RoomX = FMath::Max(0.0, (TextureWidth * PixelsPerTexel - PixelSize.X) * 0.5);
	const double RoomY = FMath::Max(0.0, (TextureHeight * PixelsPerTexel - PixelSize.Y) * 0.5);
	const double OffsetX = FMath::RoundToDouble(PanX * RoomX / PixelsPerTexel) * PixelsPerTexel;
	const double OffsetY = FMath::RoundToDouble(PanY * RoomY / PixelsPerTexel) * PixelsPerTexel;
	Box->SetRenderTransform(FWidgetTransform(FVector2D(OffsetX / DpiScale, OffsetY / DpiScale), FVector2D(1.0f, 1.0f), FVector2D::ZeroVector, 0.0f));
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
	Title->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Title->SetRenderScale(FVector2D(FMath::Max(SequenceCue->TitleScale, 0.01f)));
	Title->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNUStorySlideshowWidget::UpdateCrossfade(float DeltaSeconds, const FGeometry& MyGeometry)
{
	if (PrevFade <= 0.0f)
	{
		PrevPicture->SetRenderOpacity(0.0f);
		return;
	}
	PrevFade = FMath::Max(0.0f, PrevFade - DeltaSeconds / FMath::Max(Data->PhotoTransitionSeconds, 0.05f));
	// 이전 사진은 사라지는 동안 마지막 위치에 멈춰 있다.
	LayoutPhoto(PrevPictureScale, PrevTexture, PrevPanX, PrevPanY, MyGeometry);
	PrevPicture->SetRenderOpacity(FMath::SmoothStep(0.0f, 1.0f, PrevFade));
}

void UNUStorySlideshowWidget::UpdateOverlays(const FGeometry& MyGeometry)
{
	const float FlashAmount = FMath::Clamp(SequenceCue->FlashOpacity, 0.0f, 1.0f);
	Flash->SetVisibility(FlashAmount > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Flash->SetRenderOpacity(FlashAmount);

	const float Letterbox = FMath::Clamp(SequenceCue->LetterboxAmount, 0.0f, 1.0f);
	const float Height = MyGeometry.GetLocalSize().Y * Data->LetterboxFraction * Letterbox;
	for (UBorder* Bar : { LetterboxTop.Get(), LetterboxBottom.Get() })
	{
		Bar->SetVisibility(Height > 0.5f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* BarSlot = Cast<UCanvasPanelSlot>(Bar->Slot))
		{
			BarSlot->SetOffsets(FMargin(0, 0, 0, Height));
		}
	}
}

void UNUStorySlideshowWidget::ResetMote(int32 Index, bool bAnywhere)
{
	FMote& Mote = Motes[Index];
	Mote.Position = FVector2D(FMath::FRand(), bAnywhere ? FMath::FRandRange(0.05f, 1.0f) : FMath::FRandRange(0.8f, 1.05f));
	Mote.Speed = FMath::FRandRange(0.012f, 0.04f);
	Mote.Sway = FMath::FRandRange(0.003f, 0.012f);
	Mote.Phase = FMath::FRandRange(0.0f, UE_TWO_PI);
	Mote.Life = FMath::FRandRange(3.5f, 7.5f);
	Mote.Age = bAnywhere ? FMath::FRandRange(0.0f, Mote.Life) : 0.0f;
	Mote.SizeTexels = FMath::FRand() < 0.3f ? 2 : 1;
}

void UNUStorySlideshowWidget::UpdateMotes(float DeltaSeconds, const FGeometry& MyGeometry)
{
	const float Intensity = FMath::Clamp(SequenceCue->MoteIntensity, 0.0f, 1.0f);
	if (Intensity <= 0.001f)
	{
		MoteLayer->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	MoteLayer->SetVisibility(ESlateVisibility::HitTestInvisible);

	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FVector2D PixelSize = MyGeometry.GetAbsoluteSize();
	if (LocalSize.X <= 0.0 || PixelSize.X <= 0.0)
	{
		return;
	}
	// 그림과 같은 도트 크기로 그리고, 위치도 그림 한 칸 격자에 맞춘다.
	const UTexture2D* Texture = Data->Slides.IsValidIndex(SlideIndex) ? Data->Slides[SlideIndex].Image.Get() : nullptr;
	if (Texture == nullptr && Data->Slides.Num() > 0)
	{
		Texture = Data->Slides[0].Image.Get();
	}
	const double DpiScale = PixelSize.X / LocalSize.X;
	const double Texel = GetPixelsPerTexel(Texture, MyGeometry) / DpiScale;
	for (int32 Index = 0; Index < Motes.Num(); ++Index)
	{
		FMote& Mote = Motes[Index];
		Mote.Age += DeltaSeconds;
		if (Mote.Age >= Mote.Life || Mote.Position.Y < -0.05f)
		{
			ResetMote(Index, false);
		}
		Mote.Position.Y -= Mote.Speed * DeltaSeconds;
		const float LifeT = Mote.Age / Mote.Life;
		const float LifeFade = FMath::SmoothStep(0.0f, 0.2f, LifeT) * (1.0f - FMath::SmoothStep(0.7f, 1.0f, LifeT));
		const float Twinkle = 0.5f + 0.5f * FMath::Sin(Mote.Phase * 3.0f + Mote.Age * 4.0f);
		const double X = (Mote.Position.X + FMath::Sin(Mote.Phase + Mote.Age * 1.3f) * Mote.Sway) * LocalSize.X;
		const double Y = Mote.Position.Y * LocalSize.Y;
		// 격자에 맞춘 중심. 반짝일수록 팔이 한 칸씩 뻗는다(작은 것 1~3칸, 큰 것 2~6칸).
		const double Core = Texel * Mote.SizeTexels;
		const int32 ArmTexels = Twinkle > 0.75f ? 2 : (Twinkle > 0.4f ? 1 : 0);
		const double Arm = Core * ArmTexels;
		const double CenterX = FMath::RoundToDouble(X / Texel) * Texel;
		const double CenterY = FMath::RoundToDouble(Y / Texel) * Texel;
		const float Opacity = Intensity * LifeFade * (0.7f + 0.3f * Twinkle);
		for (int32 Part = 0; Part < 2; ++Part)
		{
			UBorder* Box = MoteBoxes[Index * 2 + Part];
			const bool bHorizontal = Part == 0;
			if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(Box->Slot))
			{
				BoxSlot->SetPosition(bHorizontal ? FVector2D(CenterX - Arm, CenterY) : FVector2D(CenterX, CenterY - Arm));
				BoxSlot->SetSize(bHorizontal ? FVector2D(Core + Arm * 2.0, Core) : FVector2D(Core, Core + Arm * 2.0));
			}
			// 팔 끝은 가운데보다 조금 옅게 보이도록 세로 막대를 살짝 낮춘다.
			Box->SetRenderOpacity(Opacity * (bHorizontal ? 1.0f : 0.85f));
		}
	}
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
	HoldSkipBar->SetPercent(Progress);
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
	const bool bGamepad = InKeyEvent.GetKey().IsGamepadKey();
	if (bGamepad != bSkipUsingGamepad)
	{
		bSkipUsingGamepad = bGamepad;
		SkipKeyLabel->SetText(bGamepad ? NSLOCTEXT("NUStory", "SkipKeyPad", "B") : NSLOCTEXT("NUStory", "SkipKeyEsc", "Esc"));
	}
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

FReply UNUStorySlideshowWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bSkipUsingGamepad && !InMouseEvent.GetCursorDelta().IsNearlyZero())
	{
		bSkipUsingGamepad = false;
		SkipKeyLabel->SetText(NSLOCTEXT("NUStory", "SkipKeyEsc", "Esc"));
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UNUStorySlideshowWidget::BuildHoldSkipContent()
{
	using namespace NUStorySkip;
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HoldSkip"));
	HoldSkipContent = Column;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Row);

	// 키 칩: 금빛 둥근 칸 안에 남색 둥근 칸. 외곽선 브러시는 렌더 불투명도를 따르지 않아 칸 두 개로 테두리를 그린다.
	SkipKeyRim = WidgetTree->ConstructWidget<UBorder>();
	SkipKeyRim->SetPadding(FMargin(1.0f));
	SkipKeyFill = WidgetTree->ConstructWidget<UBorder>();
	SkipKeyFill->SetPadding(FMargin(9.0f, 1.0f, 9.0f, 2.0f));
	SkipKeyFill->SetHorizontalAlignment(HAlign_Center);
	SkipKeyFill->SetVerticalAlignment(VAlign_Center);
	SkipKeyRim->SetContent(SkipKeyFill);
	SkipKeyLabel = WidgetTree->ConstructWidget<UTextBlock>();
	SkipKeyLabel->SetText(NSLOCTEXT("NUStory", "SkipKeyEsc", "Esc"));
	SkipKeyLabel->SetColorAndOpacity(FSlateColor(Gold()));
	SkipKeyFill->SetContent(SkipKeyLabel);
	if (UHorizontalBoxSlot* ChipSlot = Row->AddChildToHorizontalBox(SkipKeyRim))
	{
		ChipSlot->SetVerticalAlignment(VAlign_Center);
	}

	HoldSkipLabel = WidgetTree->ConstructWidget<UTextBlock>();
	HoldSkipLabel->SetText(NSLOCTEXT("NUStory", "HoldToSkip", "길게 눌러 건너뛰기"));
	HoldSkipLabel->SetColorAndOpacity(FSlateColor(Ivory()));
	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(HoldSkipLabel))
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
	}

	// 누르고 있는 동안 왼쪽부터 차오르는 금빛 줄. 평소에는 옅은 줄만 보여 '누르고 있는' 조작임을 알린다.
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
	BarBox->SetHeightOverride(2.0f);
	HoldSkipBar = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(FSlateColorBrush(WithAlpha(Gold(), 0.30f)));
	BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
	BarStyle.SetMarqueeImage(FSlateNoResource());
	HoldSkipBar->SetWidgetStyle(BarStyle);
	HoldSkipBar->SetFillColorAndOpacity(Gold());
	HoldSkipBar->SetPercent(0.0f);
	BarBox->AddChild(HoldSkipBar);
	if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(BarBox))
	{
		BarSlot->SetPadding(FMargin(0.0f, 7.0f, 0.0f, 0.0f));
	}
}

bool UNUStorySlideshowWidget::IsHoldSkipStyle() const
{
	return SkipButton != nullptr && HoldSkipContent != nullptr && SkipButton->GetContent() == HoldSkipContent;
}

void UNUStorySlideshowWidget::ApplyHoldSkipStyle()
{
	using namespace NUStorySkip;
	// 엔진 기본 회색 버튼 대신 배경 없는 버튼: 보이는 것은 키 칩과 글자, 금빛 줄뿐이다.
	FButtonStyle Style;
	Style.SetNormal(FSlateNoResource());
	Style.SetHovered(FSlateNoResource());
	Style.SetPressed(FSlateNoResource());
	Style.SetDisabled(FSlateNoResource());
	Style.SetNormalPadding(FMargin(0.0f));
	Style.SetPressedPadding(FMargin(0.0f));
	SkipButton->SetStyle(Style);
	SkipButton->SetBackgroundColor(FLinearColor::White);
	SkipButton->SetContent(HoldSkipContent);

	SkipKeyRim->SetBrush(FSlateRoundedBoxBrush(WithAlpha(Gold(), 0.85f), 6.0f));
	SkipKeyFill->SetBrush(FSlateRoundedBoxBrush(WithAlpha(Navy(), 0.92f), 5.0f));

	// 자막과 같은 글꼴(마루부리)에 같은 어두운 외곽선
	FSlateFontInfo LabelFont = Data->SubtitleFont.FontObject != nullptr ? Data->SubtitleFont : FCoreStyle::GetDefaultFontStyle("Regular", 16);
	LabelFont.Size = 17;
	LabelFont.OutlineSettings.OutlineSize = 2;
	LabelFont.OutlineSettings.OutlineColor = Outline();
	HoldSkipLabel->SetFont(LabelFont);
	FSlateFontInfo KeyFont = LabelFont;
	KeyFont.Size = 13;
	KeyFont.OutlineSettings.OutlineSize = 0;
	SkipKeyLabel->SetFont(KeyFont);
	SkipKeyLabel->SetText(bSkipUsingGamepad ? NSLOCTEXT("NUStory", "SkipKeyPad", "B") : NSLOCTEXT("NUStory", "SkipKeyEsc", "Esc"));
	HoldSkipLabel->SetColorAndOpacity(FSlateColor(Ivory()));
	HoldSkipBar->SetPercent(0.0f);

	if (UCanvasPanelSlot* SkipSlot = Cast<UCanvasPanelSlot>(SkipButton->Slot))
	{
		SkipSlot->SetAutoSize(true);
		SkipSlot->SetPosition(FVector2D(-44.0f, -40.0f));
	}
	SkipButton->SetRenderOpacity(0.0f);
}

void UNUStorySlideshowWidget::UpdateHoldSkipLook(float DeltaSeconds)
{
	using namespace NUStorySkip;
	if (!IsHoldSkipStyle())
	{
		return;
	}
	SkipShownSeconds += DeltaSeconds;
	const float Appear = FMath::SmoothStep(AppearStart, AppearEnd, SkipShownSeconds);
	const bool bActive = bSkipButtonHeld || bSkipKeyHeld || SkipButton->IsHovered();
	SkipEmphasis = FMath::FInterpTo(SkipEmphasis, bActive ? 1.0f : 0.0f, DeltaSeconds, 10.0f);
	SkipButton->SetRenderOpacity(Appear * FMath::Lerp(IdleOpacity, 1.0f, SkipEmphasis));
	// 누르는 동안 글자도 금빛으로 물든다.
	const float Hold = FMath::Clamp(SkipHoldSeconds / FMath::Max(Data->HoldToSkipSeconds, 0.1f), 0.0f, 1.0f);
	HoldSkipLabel->SetColorAndOpacity(FSlateColor(FMath::Lerp(Ivory(), Gold(), FMath::Min(1.0f, Hold * 1.5f))));
}

void UNUStorySlideshowWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	// 키를 누른 채 포커스를 잃으면 떼는 입력이 오지 않는다. 그대로 두면 저절로 스킵된다.
	bSkipKeyHeld = false;
	Super::NativeOnFocusLost(InFocusEvent);
}

void UNUStorySlideshowWidget::FinishSlideshow()
{
	if (!bIsPlaying)
	{
		return;
	}

	bIsPlaying = false;
	RemoveFromParent();
	OnFinished.Broadcast();
}

void UNUStorySlideshowWidget::RequestSkip()
{
	// 길게 눌러야 하는 연출은 클릭 한 번으로 넘기지 않는다.
	if (bIsPlaying && Data != nullptr && Data->bAllowSkip && !Data->bHoldToSkip)
	{
		OnSkipRequested.Broadcast();
	}
}

void UNUStorySlideshowWidget::NativeDestruct()
{
	const bool bWasPlaying = bIsPlaying;
	bIsPlaying = false;
	Super::NativeDestruct();

	// 외부 창 종료도 재생 소유자에게 알려 시퀀스만 남지 않게 한다.
	if (bWasPlaying)
	{
		OnFinished.Broadcast();
	}
}
