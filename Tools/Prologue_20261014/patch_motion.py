# 일회성 패치: 그림책 위젯에 크로스페이드·도트 입자·플래시·레터박스·자막 상승·타이틀 확대를 넣는다.
import sys

ROOT = sys.argv[1]
C = ROOT + '/Source/ProjectM/Cinematics/'


def patch(name, pairs):
    p = C + name
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (name, a[:70])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)


patch('NUStorySequenceCue.h', [(
'''	// 데이터의 TitleImage 불투명도''',
'''	// 그림 위를 떠다니는 도트 빛 입자의 양(0~1). 0이면 그리지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float MoteIntensity = 0.0f;

	// 위아래 시네마 레터박스(0~1). 데이터의 LetterboxFraction 만큼 덮는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float LetterboxAmount = 0.0f;

	// 화면 전체를 덮는 빛(데이터의 FlashColor). 장면 전환을 빛으로 덮을 때 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float FlashOpacity = 0.0f;

	// 타이틀 크기 배율(가운데 기준)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float TitleScale = 1.0f;

	// 데이터의 TitleImage 불투명도''')])

patch('NUStorySlideshowData.h', [(
'''	// 시퀀스의 TitleOpacity로 띄우는 타이틀 이미지(로고). 화면 가운데에 그린다.''',
'''	// 켜면 사진이 바뀔 때 이전 사진이 PhotoTransitionSeconds 동안 새 사진 위로 녹아 사라진다(검은 화면을 거치지 않음).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	bool bCrossfadePhotos = false;

	// 시퀀스의 FlashOpacity 로 덮는 빛의 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	FLinearColor FlashColor = FLinearColor(1.0f, 0.93f, 0.78f, 1.0f);

	// 레터박스 한 줄의 높이(화면 높이 비율)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float LetterboxFraction = 0.11f;

	// 시퀀스의 TitleOpacity로 띄우는 타이틀 이미지(로고). 화면 가운데에 그린다.''')])

patch('NUStorySlideshowWidget.h', [
('''class UBorder;
class UButton;''', '''class UBorder;
class UButton;
class UCanvasPanel;
class UTexture2D;'''),
('''	UPROPERTY(Transient)
	TObjectPtr<UImage> Title;
''', '''	UPROPERTY(Transient)
	TObjectPtr<UImage> Title;

	// 크로스페이드용 이전 사진. 새 사진 위에 얹혀 녹아 사라진다.
	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> PrevPictureScale;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PrevPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PrevTexture;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MoteLayer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MoteBoxes;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Flash;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LetterboxTop;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LetterboxBottom;
'''),
('''	void ApplyPhotoLayout(const FGeometry& MyGeometry);''', '''	void ApplyPhotoLayout(const FGeometry& MyGeometry);
	void LayoutPhoto(UScaleBox* Box, const UTexture2D* Texture, float PanX, float PanY, const FGeometry& MyGeometry) const;
	int32 GetPixelsPerTexel(const UTexture2D* Texture, const FGeometry& MyGeometry) const;
	void UpdateCrossfade(float DeltaSeconds, const FGeometry& MyGeometry);
	void UpdateOverlays(const FGeometry& MyGeometry);
	void UpdateMotes(float DeltaSeconds, const FGeometry& MyGeometry);

	// 도트 빛 입자 하나. 위치는 화면 비율, 그릴 때 그림 한 칸 격자에 맞춘다.
	struct FMote
	{
		FVector2D Position = FVector2D::ZeroVector;
		float Speed = 0.0f;
		float Sway = 0.0f;
		float Phase = 0.0f;
		float Age = 0.0f;
		float Life = 1.0f;
		int32 SizeTexels = 1;
	};
	void ResetMote(int32 Index, bool bAnywhere);
	TArray<FMote> Motes;
	float PrevFade = 0.0f;
	float PrevPanX = 0.0f;
	float PrevPanY = 0.0f;
	float LastPanX = 0.0f;
	float LastPanY = 0.0f;
	float LastPhotoOpacity = 0.0f;'''),
])

patch('NUStorySlideshowWidget.cpp', [
('''#include "Styling/CoreStyle.h"
''', '''#include "Styling/CoreStyle.h"

namespace NUStoryMotion
{
	constexpr int32 MoteCount = 56;

	// 나이아 색을 닮은 빛: 금빛, 민트, 분홍, 하늘, 흰빛
	static const FLinearColor MoteColors[] = {
		FLinearColor(1.00f, 0.85f, 0.45f), FLinearColor(0.62f, 1.00f, 0.80f), FLinearColor(1.00f, 0.70f, 0.86f),
		FLinearColor(0.66f, 0.86f, 1.00f), FLinearColor(1.00f, 0.98f, 0.92f),
	};
}
'''),
('''	Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Picture"));
	PictureScale->AddChild(Picture);
''', '''	Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Picture"));
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
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
		Box->SetBrushColor(NUStoryMotion::MoteColors[Index % UE_ARRAY_COUNT(NUStoryMotion::MoteColors)]);
		Box->SetRenderOpacity(0.0f);
		MoteLayer->AddChildToCanvas(Box);
		MoteBoxes.Add(Box);
		ResetMote(Index, true);
	}
'''),
('''	TitleSlot->SetAnchors(FAnchors(0, 0));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
''', '''	TitleSlot->SetAnchors(FAnchors(0, 0));
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
'''),
('''	PictureScale->SetStretch(Data->bPixelPerfectPhotos ? EStretch::UserSpecified : EStretch::ScaleToFit);
	PictureScale->SetRenderTransform(FWidgetTransform());
''', '''	PictureScale->SetStretch(Data->bPixelPerfectPhotos ? EStretch::UserSpecified : EStretch::ScaleToFit);
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
'''),
('''	Backdrop->SetRenderOpacity(FMath::Clamp(SequenceCue->BackdropOpacity, 0.0f, 1.0f));
	ApplyTitleLayout(MyGeometry);''', '''	Backdrop->SetRenderOpacity(FMath::Clamp(SequenceCue->BackdropOpacity, 0.0f, 1.0f));
	UpdateOverlays(MyGeometry);
	UpdateMotes(InDeltaTime, MyGeometry);
	UpdateCrossfade(InDeltaTime, MyGeometry);
	ApplyTitleLayout(MyGeometry);'''),
('''	if (DisplayedSlideIndex != SlideIndex)
	{
		Picture->SetBrushFromTexture(Slide.Image, true);
		DisplayedSlideIndex = SlideIndex;
	}
	// 사진이 없는 장면은 대사만 띄운다(암전, 레벨 화면 위).
	Picture->SetRenderOpacity(Slide.Image != nullptr ? FMath::Clamp(SequenceCue->PhotoOpacity, 0.0f, 1.0f) : 0.0f);
	if (Slide.Image != nullptr)
	{
		ApplyPhotoLayout(MyGeometry);
	}''', '''	if (DisplayedSlideIndex != SlideIndex)
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
	}'''),
('''	Dialogue->SetRenderOpacity(FMath::Clamp(SequenceCue->CaptionOpacity, 0.0f, 1.0f));
}''', '''	const float CaptionOpacity = FMath::Clamp(SequenceCue->CaptionOpacity, 0.0f, 1.0f);
	Dialogue->SetRenderOpacity(CaptionOpacity);
	if (Data->bUseSubtitles)
	{
		// 나타날 때 살짝 떠오르고 사라질 때 살짝 가라앉는다.
		Dialogue->SetRenderTranslation(FVector2D(0.0f, (1.0f - CaptionOpacity) * 10.0f));
	}
}'''),
# ApplyPhotoLayout -> LayoutPhoto
('''void UNUStorySlideshowWidget::ApplyPhotoLayout(const FGeometry& MyGeometry)
{
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FVector2D PixelSize = MyGeometry.GetAbsoluteSize();
	if (LocalSize.X <= 0.0 || LocalSize.Y <= 0.0 || PixelSize.X <= 0.0)
	{
		return;
	}
	const float PanX = FMath::Clamp(SequenceCue->PhotoPanX, -1.0f, 1.0f);
	const float PanY = FMath::Clamp(SequenceCue->PhotoPanY, -1.0f, 1.0f);
''', '''void UNUStorySlideshowWidget::ApplyPhotoLayout(const FGeometry& MyGeometry)
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
'''),
('''	const UTexture2D* Texture = Data->Slides.IsValidIndex(SlideIndex) ? Data->Slides[SlideIndex].Image.Get() : nullptr;
	const float TextureWidth = Texture != nullptr ? Texture->GetSurfaceWidth() : 0.0f;''', '''	const float TextureWidth = Texture != nullptr ? Texture->GetSurfaceWidth() : 0.0f;'''),
('''	const int32 PixelsPerTexel = FMath::Max(1, FMath::CeilToInt(FMath::Max(PixelSize.X / TextureWidth, PixelSize.Y / TextureHeight) - 0.001));
	PictureScale->SetUserSpecifiedScale(static_cast<float>(PixelsPerTexel / DpiScale));''', '''	const int32 PixelsPerTexel = GetPixelsPerTexel(Texture, MyGeometry);
	PictureScale->SetUserSpecifiedScale(static_cast<float>(PixelsPerTexel / DpiScale));'''),
# title scale
('''	Title->SetRenderOpacity(Opacity);
	Title->SetVisibility(ESlateVisibility::HitTestInvisible);
}''', '''	Title->SetRenderOpacity(Opacity);
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
	Mote.SizeTexels = FMath::FRand() < 0.25f ? 2 : 1;
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
		const float Twinkle = 0.6f + 0.4f * FMath::Sin(Mote.Phase * 3.0f + Mote.Age * 4.5f);
		const double X = (Mote.Position.X + FMath::Sin(Mote.Phase + Mote.Age * 1.3f) * Mote.Sway) * LocalSize.X;
		const double Y = Mote.Position.Y * LocalSize.Y;
		UBorder* Box = MoteBoxes[Index];
		if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(Box->Slot))
		{
			BoxSlot->SetPosition(FVector2D(FMath::RoundToDouble(X / Texel) * Texel, FMath::RoundToDouble(Y / Texel) * Texel));
			BoxSlot->SetSize(FVector2D(Texel * Mote.SizeTexels, Texel * Mote.SizeTexels));
		}
		Box->SetRenderOpacity(Intensity * LifeFade * Twinkle);
	}
}'''),
])
# LayoutPhoto 본문은 멤버 PictureScale 대신 인자 Box 를 쓴다.
p = C + 'NUStorySlideshowWidget.cpp'
s = open(p, encoding='utf-8').read()
start = s.index('void UNUStorySlideshowWidget::LayoutPhoto(')
end = s.index('void UNUStorySlideshowWidget::ApplyTitleLayout(')
body = s[start:end].replace('PictureScale->', 'Box->')
s = s[:start] + body + s[end:]
open(p, 'w', encoding='utf-8').write(s)
print('patched')
