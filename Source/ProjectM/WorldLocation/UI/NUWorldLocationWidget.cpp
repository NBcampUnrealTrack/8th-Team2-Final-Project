#include "WorldLocation/UI/NUWorldLocationWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/NUUISettings.h"

TSharedRef<SWidget> UNUWorldLocationWidget::RebuildWidget()
{
	// WBP가 있으면 디자이너 배치를 그대로 사용한다.
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		const UNUUISettings* Settings = GetDefault<UNUUISettings>();
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetBrushColor(Settings->PanelColor);
		Panel->SetPadding(FMargin(14.0f, 10.0f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetPosition(FVector2D(16.0, 16.0));
		PanelSlot->SetAutoSize(true);

		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
		Width->SetWidthOverride(240.0f);
		Panel->SetContent(Width);
		UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>();
		Width->SetContent(Names);

		AreaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AreaText"));
		FSlateFontInfo AreaFont = AreaText->GetFont();
		AreaFont.Size = 14;
		AreaText->SetFont(AreaFont);
		AreaText->SetColorAndOpacity(Settings->SubTextColor);
		AreaText->SetAutoWrapText(true);
		Names->AddChildToVerticalBox(AreaText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		LocationText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LocationText"));
		FSlateFontInfo LocationFont = LocationText->GetFont();
		LocationFont.Size = 20;
		LocationText->SetFont(LocationFont);
		LocationText->SetColorAndOpacity(Settings->TextColor);
		LocationText->SetAutoWrapText(true);
		Names->AddChildToVerticalBox(LocationText);
	}
	return Super::RebuildWidget();
}

void UNUWorldLocationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshText();
	// 화면에 붙기 전에 이름이 먼저 와서 배너가 이미 시작됐으면 그대로 둔다
	if (!BannerMotion.IsRunning() && BannerHoldLeft <= 0.f)
	{
		HideBannerNow();
	}
}

void UNUWorldLocationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BattleFold.Tick(this, InDeltaTime);
	const bool bMoving = BannerMotion.Advance(InDeltaTime);
	if (BannerRoot == nullptr || BannerRoot->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	if (bBannerLeaving)
	{
		if (!bMoving)
		{
			HideBannerNow();
		}
		return;
	}
	if (!bMoving)
	{
		BannerHoldLeft -= InDeltaTime;
		if (BannerHoldLeft <= 0.f)
		{
			bBannerLeaving = true;
			BannerMotion.AddOut(BannerRoot, 0.f, 0.7f, FVector2D(0.f, -6.f));
			BannerMotion.Start();
		}
	}
}

void UNUWorldLocationWidget::ShowBanner()
{
	if (BannerRoot == nullptr || LocationName.IsEmpty())
	{
		return;
	}
	UNUSFXSubsystem::PlayCommon(this, NU::SFX::Field::AreaEnter);
	if (BannerAreaText != nullptr)
	{
		BannerAreaText->SetText(AreaName);
	}
	if (BannerLocationText != nullptr)
	{
		BannerLocationText->SetText(LocationName);
	}
	BannerMotion.Finish();
	BannerRoot->SetRenderOpacity(1.f);
	BannerRoot->SetRenderTransform(FWidgetTransform());
	BannerRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	// 이름이 먼저 내려앉고, 금색 줄이 가운데에서 펴지고, 지역 이름이 뒤따른다
	BannerMotion.Add(BannerRoot, 0.f, 0.55f, FVector2D(0.f, 10.f));
	BannerMotion.Add(BannerRuleLeft, 0.15f, 0.7f, FVector2D(40.f, 0.f), 0.f, 0.2f);
	BannerMotion.Add(BannerRuleRight, 0.15f, 0.7f, FVector2D(-40.f, 0.f), 0.f, 0.2f);
	BannerMotion.Add(BannerAreaText, 0.25f, 0.5f, FVector2D(0.f, 6.f));
	BannerMotion.Start();
	BannerHoldLeft = BannerHoldSeconds;
	bBannerLeaving = false;
}

bool UNUWorldLocationWidget::IsBannerShowing() const
{
	return BannerRoot != nullptr && BannerRoot->GetVisibility() != ESlateVisibility::Collapsed && IsVisible();
}

void UNUWorldLocationWidget::HideBannerNow()
{
	BannerMotion.Finish();
	bBannerLeaving = false;
	if (BannerRoot != nullptr)
	{
		BannerRoot->SetVisibility(ESlateVisibility::Collapsed);
		BannerRoot->SetRenderOpacity(1.f);
		BannerRoot->SetRenderTransform(FWidgetTransform());
	}
}

void UNUWorldLocationWidget::SetLocationNames(const FText& InAreaName, const FText& InLocationName)
{
	AreaName = InAreaName;
	LocationName = InLocationName;
	RefreshText();

	// 같은 장소를 다시 알려 주면(구역 경계를 오가는 등) 배너를 다시 띄우지 않는다
	const FText Key = FText::Format(INVTEXT("{0}|{1}"), AreaName, LocationName);
	if (!Key.EqualTo(LastBannerKey))
	{
		LastBannerKey = Key;
		ShowBanner();
	}
}

void UNUWorldLocationWidget::RefreshText()
{
	if (AreaText != nullptr)
	{
		AreaText->SetText(AreaName.IsEmpty() ? NSLOCTEXT("NUWorldLocation", "UnknownArea", "지역 미확인") : AreaName);
	}
	if (LocationText != nullptr)
	{
		LocationText->SetText(LocationName.IsEmpty() ? NSLOCTEXT("NUWorldLocation", "NoLocation", "—") : LocationName);
	}

	// 한 줄 배치(새 HUD: 지역 · 장소)는 장소가 없으면 점과 대시를 숨겨 지역 이름만 남긴다 --- 잘린 줄처럼 보이지 않게
	if (UWidget* Dot = GetWidgetFromName(TEXT("LocDot")))
	{
		const ESlateVisibility Shown = LocationName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
		Dot->SetVisibility(Shown);
		if (LocationText != nullptr)
		{
			LocationText->SetVisibility(Shown);
		}
	}
}
