#include "CharacterProgression/UI/Job/NUJobSelectionCardWidget.h"

#include "CharacterSelect/UI/NUCharacterSlotWidget.h"
#include "UI/NUFlipbookImage.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "PaperFlipbook.h"

void UNUJobSelectionCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CardButton != nullptr)
	{
		CardButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
		CardButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}
}

void UNUJobSelectionCardWidget::NativeDestruct()
{
	if (CardButton != nullptr)
	{
		CardButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}
	Super::NativeDestruct();
}

void UNUJobSelectionCardWidget::Setup(int32 InJobId, const FText& InName, UTexture2D* InEmblem, const FLinearColor& InAccent,
	const FNUCharacterSummary& InAppearance)
{
	JobId = InJobId;
	Accent = InAccent;
	Appearance = InAppearance;
	Appearance.JobId = InJobId;
	bFigureBuilt = false;
	if (NameText != nullptr)
	{
		NameText->SetText(InName);
	}
	if (EmblemImage != nullptr)
	{
		EmblemImage->SetVisibility(InEmblem != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (InEmblem != nullptr)
		{
			EmblemImage->SetBrushResourceObject(InEmblem);   // 크기는 WBP 브러시 그대로
		}
	}
	const FLinearColor Beam(Accent.R, Accent.G, Accent.B, BeamOpacity);
	const FLinearColor Ground(Accent.R, Accent.G, Accent.B, GroundOpacity);
	if (LightBeam != nullptr)
	{
		LightBeam->SetColorAndOpacity(Beam);
	}
	if (LightGround != nullptr)
	{
		LightGround->SetColorAndOpacity(Ground);
	}
	SetSelected(bSelected);
}

UWidget* UNUJobSelectionCardWidget::GetFocusTarget() const
{
	return CardButton;
}

void UNUJobSelectionCardWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (!bFigureBuilt || bFigureSelected != bSelected)
	{
		RebuildFigure();
	}
	if (NameText != nullptr)
	{
		NameText->SetColorAndOpacity(bSelected ? SelectedNameColor : NameColor);
	}
	if (EmblemImage != nullptr)
	{
		EmblemImage->SetColorAndOpacity(bSelected ? SelectedEmblemTint : EmblemTint);
	}
	if (SelectedLight != nullptr)
	{
		SelectedLight->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UNUJobSelectionCardWidget::RebuildFigure()
{
	bFigureBuilt = true;
	bFigureSelected = bSelected;
	if (FigureOverlay == nullptr || JobId <= 0)
	{
		return;
	}

	// 모두 정면 대기 (캐릭터의 성별 그대로 --- 그 성별 직업 몸이 아직 없는 직업은 노비스 옷으로 보인다)
	UNUCharacterSlotWidget::BuildPortrait(this, FigureOverlay, Appearance, false, 0.f, FigurePixelScale, false, nullptr, nullptr);
	// 고른 칸은 재생 (머리·얼굴은 BuildPortrait가 몸 그림의 흔들림 표대로 따라가게 해 둔다), 고르지 않은 칸은 첫 그림에 세워 어둡게
	if (bSelected)
	{
		return;
	}
	for (int32 Index = 0; Index < FigureOverlay->GetChildrenCount(); ++Index)
	{
		if (UNUFlipbookImage* Layer = Cast<UNUFlipbookImage>(FigureOverlay->GetChildAt(Index)))
		{
			Layer->StopAtKeyFrame(0);
			Layer->SetColorAndOpacity(DimmedFigureTint);
		}
	}
}

void UNUJobSelectionCardWidget::HandleClicked()
{
	OnCardClicked.ExecuteIfBound(JobId);
}
