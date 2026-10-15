#include "Quest/UI/NUQuestPracticeGuideWidget.h"

#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Data/NUQuestDataSettings.h"
#include "Quest/UI/SNUQuestPracticeGuideArt.h"
#include "UI/NUKeyBindingsWindow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NativeWidgetHost.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Styling/CoreStyle.h"

namespace NUQuestPracticeGuide
{
	// 설정 창·공용 팝업과 같은 팔레트
	FLinearColor Gold() { return FLinearColor::FromSRGBColor(FColor(238, 202, 133, 255)); }
	FLinearColor Ivory() { return FLinearColor::FromSRGBColor(FColor(237, 239, 240, 255)); }
	FLinearColor Muted() { return FLinearColor::FromSRGBColor(FColor(184, 194, 205, 255)); }

	constexpr float PulseSeconds = 0.3f;
	constexpr float DesignWidth = 300.f;
	constexpr float DesignHeight = DesignWidth * 620.f / 505.f;

	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Radius, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), OutlineWidth);
		return Brush;
	}

	FSlateFontInfo BodyFont(int32 Size, bool bBold, int32 LetterSpacing = 0)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FSlateFontInfo& Font, const FLinearColor& Color, bool bWrap)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(bWrap);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	}

	// 최종 카드 크기에서 배치·글꼴을 계산한다. 카드 전체를 ScaleBox로 다시 축소하지 않는다.
	struct FCardLayout
	{
		explicit FCardLayout(float Width) : Scale(Width / DesignWidth) {}
		float Scale;
		int32 FontSize(int32 Size) const { return FMath::Max(1, FMath::RoundToInt(Size * Scale)); }
		void Place(UCanvasPanel* Canvas, UWidget* Widget, float X, float Y, float Width, float Height) const
		{
			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
			Slot->SetPosition(FVector2D(X, Y) * Scale);
			Slot->SetSize(FVector2D(Width, Height) * Scale);
		}
	};

	UScaleBox* FitLine(UWidgetTree* Tree, UWidget* Content, EHorizontalAlignment Alignment = HAlign_Left)
	{
		UScaleBox* Fit = Tree->ConstructWidget<UScaleBox>();
		Fit->SetStretch(EStretch::ScaleToFit);
		Fit->SetStretchDirection(EStretchDirection::DownOnly);
		Fit->SetContent(Content);
		UScaleBoxSlot* Slot = CastChecked<UScaleBoxSlot>(Content->Slot);
		Slot->SetHorizontalAlignment(Alignment);
		Slot->SetVerticalAlignment(VAlign_Center);
		return Fit;
	}

	UWidget* Keycap(UWidgetTree* Tree, const FString& Key, const FSlateFontInfo& SymbolFont, const FCardLayout& Layout)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetMinDesiredWidth(25.f * Layout.Scale);
		Size->SetHeightOverride(27.f * Layout.Scale);
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrush(MakeRoundedBrush(FLinearColor::FromSRGBColor(FColor(8, 23, 31)), Gold(), 3.f * Layout.Scale, 1.6f * Layout.Scale));
		Border->SetPadding(FMargin(7.f * Layout.Scale, 0.f));
		Border->SetHorizontalAlignment(HAlign_Center);
		Border->SetVerticalAlignment(VAlign_Center);
		// 기본 본문 글꼴의 방향 화살표가 비는 환경에서도 키 표시를 유지한다.
		const bool bArrow = Key.Contains(TEXT("←")) || Key.Contains(TEXT("→"));
		UTextBlock* Label = MakeText(Tree, bArrow ? SymbolFont : BodyFont(Layout.FontSize(14), false), Ivory(), false);
		Label->SetText(FText::FromString(Key));
		Border->SetContent(Label);
		Size->SetContent(Border);
		return Size;
	}

	// 번역된 문장 속 [키]만 키캡으로 만든다. 장치를 바꿔도 문장 순서가 유지된다.
	UHorizontalBox* KeyLine(UWidgetTree* Tree, const FText& Text, const FSlateFontInfo& SymbolFont, const FCardLayout& Layout)
	{
		UHorizontalBox* Box = Tree->ConstructWidget<UHorizontalBox>();
		auto Add = [Box, &Layout](UWidget* Widget)
		{
			UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Widget);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(Box->GetChildrenCount() > 1 ? 7.f * Layout.Scale : 0.f, 0.f, 0.f, 0.f));
		};
		auto Plain = [Tree, &Add, &Layout](const FString& Part)
		{
			const FString Trimmed = Part.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				UTextBlock* Label = MakeText(Tree, BodyFont(Layout.FontSize(14), false), Ivory(), false);
				Label->SetText(FText::FromString(Trimmed));
				Add(Label);
			}
		};
		FString Remaining = Text.ToString();
		int32 Open = INDEX_NONE;
		while (Remaining.FindChar(TEXT('['), Open))
		{
			const int32 Close = Remaining.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
			if (Close == INDEX_NONE) { break; }
			Plain(Remaining.Left(Open));
			Add(Keycap(Tree, Remaining.Mid(Open + 1, Close - Open - 1), SymbolFont, Layout));
			Remaining = Remaining.Mid(Close + 1);
		}
		Plain(Remaining);
		return Box;
	}
}

TSharedRef<SWidget> UNUQuestPracticeGuideWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	if (CardArtHost)
	{
		CardArtHost->SetContent(SAssignNew(CardArt, SNUQuestPracticeGuideArt));
		CardArt->SetPractice(Kind, Count, Required);
	}
	return Super::RebuildWidget();
}

void UNUQuestPracticeGuideWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	CardArt.Reset();
}

FSlateFontInfo UNUQuestPracticeGuideWidget::GetTitleFont(int32 Size) const
{
	if (const UObject* Font = TitleFont.LoadSynchronous())
	{
		return FSlateFontInfo(Font, Size);
	}
	return NUQuestPracticeGuide::BodyFont(Size, true);
}

void UNUQuestPracticeGuideWidget::BuildWidgetTree()
{
	using namespace NUQuestPracticeGuide;
	const FCardLayout Layout(CardWidth);

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;
	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardRoot"));
	CardSize->SetWidthOverride(CardWidth);
	CardSize->SetHeightOverride(CardWidth * DesignHeight / DesignWidth);
	CardSize->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanel* Face = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CardFace"));
	CardSize->SetContent(Face);
	CardArtHost = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("CardArt"));
	CardArtHost->SetVisibility(ESlateVisibility::HitTestInvisible);
	Layout.Place(Face, CardArtHost, 0.f, 0.f, DesignWidth, DesignHeight);

	CardEyebrow = MakeText(WidgetTree, BodyFont(Layout.FontSize(11), false), Muted(), false);
	Layout.Place(Face, FitLine(WidgetTree, CardEyebrow), 65.f, 16.f, 150.f, 22.f);
	CardIndex = MakeText(WidgetTree, BodyFont(Layout.FontSize(11), false), Muted(), false);
	Layout.Place(Face, FitLine(WidgetTree, CardIndex, HAlign_Right), 226.f, 16.f, 57.f, 22.f);
	CardTitle = MakeText(WidgetTree, GetTitleFont(Layout.FontSize(24)), FLinearColor::FromSRGBColor(FColor(255, 233, 196)), false);
	Layout.Place(Face, FitLine(WidgetTree, CardTitle), 28.f, 45.f, 254.f, 42.f);

	// 세 카드가 같은 좌표를 공유한다. 문구 길이는 행 안에서만 맞춘다.
	CardSteps = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CardSteps"));
	Layout.Place(Face, CardSteps, 18.f, 113.f, 264.f, 129.f);
	CardSummary = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CardSummary"));
	Layout.Place(Face, CardSummary, 17.f, 245.f, 266.f, 62.f);

	UHorizontalBox* CountRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CountRow"));
	CardCountLabel = MakeText(WidgetTree, BodyFont(Layout.FontSize(12), false), Muted(), false);
	CountRow->AddChildToHorizontalBox(CardCountLabel)->SetVerticalAlignment(VAlign_Center);
	CardCount = MakeText(WidgetTree, BodyFont(Layout.FontSize(16), false), Ivory(), false);
	CardCount->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	UHorizontalBoxSlot* CountSlot = CountRow->AddChildToHorizontalBox(CardCount);
	CountSlot->SetPadding(FMargin(9.f * Layout.Scale, 0.f, 0.f, 0.f));
	CountSlot->SetVerticalAlignment(VAlign_Center);
	Layout.Place(Face, FitLine(WidgetTree, CountRow), 20.f, 327.f, 172.f, 30.f);

	UCanvasPanelSlot* CardSlot = RootCanvas->AddChildToCanvas(CardSize);
	CardSlot->SetPosition(CardOffset);
	CardSlot->SetAutoSize(true);
	CardRoot = CardSize;
}

void UNUQuestPracticeGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bGamepadMode = IsGamepadMode();
	RefreshTexts();
}

void UNUQuestPracticeGuideWidget::SetPractice(ENUQuestBattleKind InKind, int32 InCount, int32 InRequired)
{
	const bool bKindChanged = !bKindSet || Kind != InKind;
	const bool bIncreased = !bKindChanged && InCount > Count;
	Kind = InKind;
	bKindSet = true;
	Count = FMath::Max(0, InCount);
	Required = FMath::Max(0, InRequired);
	if (bKindChanged)
	{
		RefreshTexts();
	}
	if (bIncreased)
	{
		CountPulse = NUQuestPracticeGuide::PulseSeconds;
	}
	RefreshCount();
}

void UNUQuestPracticeGuideWidget::Dismiss()
{
	bDismissing = true;
}

const FNUQuestPracticeGuideText* UNUQuestPracticeGuideWidget::FindGuideText() const
{
	const UNUQuestDataSettings* Settings = GetDefault<UNUQuestDataSettings>();
	if (Settings == nullptr || !bKindSet) { return nullptr; }
	switch (Kind)
	{
	case ENUQuestBattleKind::BasicAttack: return &Settings->BasicAttackGuide;
	case ENUQuestBattleKind::Parry: return &Settings->ParryGuide;
	case ENUQuestBattleKind::Dodge: return &Settings->DodgeGuide;
	default: return nullptr;
	}
}

FText UNUQuestPracticeGuideWidget::FormatGuideText(const FText& Source) const
{
	const bool bPad = IsGamepadMode();
	const FText KeyCap = NSLOCTEXT("NUQuestPracticeGuide", "KeyCap", "[{0}]");

	// 반응 키는 키 설정 창과 같은 값 --- 패드 모드면 패드 키, 아니면 키보드 키
	FText ReactionKey = NUKeyBindings::GetMappedKeyText(GetOwningPlayer(), TEXT("Parry"));
	ReactionKey = ReactionKey.IsEmpty()
		? NSLOCTEXT("NUQuestPracticeGuide", "ReactionKeyFallback", "반응 키")
		: FText::Format(KeyCap, ReactionKey);

	// 확인 키는 키 설정에 없어 전투 입력에 묶인 키를 그대로 읽는다 (전투 입력이 켜진 뒤에만 나온다)
	FText ConfirmKey = NSLOCTEXT("NUQuestPracticeGuide", "ConfirmKeyFallback", "확인 키");
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Input = Player != nullptr ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UInputAction* Confirm = ConfirmAction.LoadSynchronous();
	if (Input != nullptr && Confirm != nullptr)
	{
		for (const FKey& Key : Input->QueryKeysMappedToAction(Confirm))
		{
			if (Key.IsValid() && !Key.IsMouseButton() && Key.IsGamepadKey() == bPad)
			{
				ConfirmKey = FText::Format(KeyCap, NUKeyBindings::GetKeyText(Key));
				break;
			}
		}
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), ReactionKey);
	Args.Add(TEXT("Confirm"), ConfirmKey);
	Args.Add(TEXT("LeftRight"), bPad
		? NSLOCTEXT("NUQuestPracticeGuide", "LeftRightPad", "[십자키 ←] [→]")
		: NSLOCTEXT("NUQuestPracticeGuide", "LeftRightKeyboard", "[←] [→]"));
	return FText::Format(Source, Args);
}

void UNUQuestPracticeGuideWidget::RebuildSteps(const FNUQuestPracticeGuideText& Guide)
{
	using namespace NUQuestPracticeGuide;
	if (CardSteps == nullptr) { return; }
	const FCardLayout Layout(CardWidth);
	CardSteps->ClearChildren();
	const int32 StepCount = FMath::Min(Guide.Steps.Num(), 3);
	for (int32 Index = 0; Index < StepCount; ++Index)
	{
		const float Y = Index * 40.f;
		UTextBlock* Number = MakeText(WidgetTree, BodyFont(Layout.FontSize(12), false), Gold(), false);
		Number->SetText(FText::FromString(FString::Printf(TEXT("%02d"), Index + 1)));
		Layout.Place(CardSteps, FitLine(WidgetTree, Number), 0.f, Y, 20.f, 30.f);
		Layout.Place(CardSteps, FitLine(WidgetTree, KeyLine(WidgetTree, FormatGuideText(Guide.Steps[Index]), GetTitleFont(Layout.FontSize(14)), Layout)), 38.f, Y, 226.f, 32.f);
	}
}

void UNUQuestPracticeGuideWidget::RebuildSummary(const FNUQuestPracticeGuideText& Guide)
{
	using namespace NUQuestPracticeGuide;
	if (CardSummary == nullptr) { return; }
	const FCardLayout Layout(CardWidth);
	CardSummary->ClearChildren();
	if (Kind == ENUQuestBattleKind::BasicAttack)
	{
		UTextBlock* Summary = MakeText(WidgetTree, BodyFont(Layout.FontSize(13), false), Ivory(), false);
		Summary->SetText(Guide.Summary);
		Layout.Place(CardSummary, FitLine(WidgetTree, Summary), 13.f, 14.f, 205.f, 32.f);
		return;
	}
	if (Kind == ENUQuestBattleKind::Parry)
	{
		UTextBlock* Summary = MakeText(WidgetTree, BodyFont(Layout.FontSize(12), false), Muted(), false);
		Summary->SetText(Guide.Summary);
		Layout.Place(CardSummary, FitLine(WidgetTree, Summary, HAlign_Center), 10.f, 32.f, 246.f, 26.f);
		return;
	}
	UTextBlock* ExampleText = MakeText(WidgetTree, BodyFont(Layout.FontSize(13), false), Ivory(), false);
	ExampleText->SetText(NSLOCTEXT("NUQuestPracticeGuide", "DodgeExample", "SP 6 소모하여 회피"));
	Layout.Place(CardSummary, FitLine(WidgetTree, ExampleText, HAlign_Center), 12.f, 5.f, 242.f, 25.f);

	UHorizontalBox* Caption = WidgetTree->ConstructWidget<UHorizontalBox>();
	const FString Source = Guide.Summary.ToString();
	const int32 Marker = Source.Find(TEXT("{Chance}"));
	auto AddCaption = [this, Caption, &Layout](const FString& Text, FLinearColor Color)
	{
		if (Text.IsEmpty()) { return; }
		UTextBlock* Label = NUQuestPracticeGuide::MakeText(WidgetTree, NUQuestPracticeGuide::BodyFont(Layout.FontSize(12), false), Color, false);
		Label->SetText(FText::FromString(Text));
		Caption->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
	};
	if (Marker != INDEX_NONE)
	{
		AddCaption(Source.Left(Marker), Muted());
		AddCaption(FText::AsPercent(0.4).ToString(), Gold());
		AddCaption(Source.Mid(Marker + 8), Muted());
	}
	else { AddCaption(Source, Muted()); }
	Layout.Place(CardSummary, FitLine(WidgetTree, Caption, HAlign_Center), 10.f, 32.f, 246.f, 26.f);
}

void UNUQuestPracticeGuideWidget::RefreshTexts()
{
	const FNUQuestPracticeGuideText* Guide = FindGuideText();
	if (Guide == nullptr || CardTitle == nullptr)
	{
		return;
	}
	CardEyebrow->SetText(Guide->Eyebrow);
	CardTitle->SetText(Guide->Title);
	const int32 Index = Kind == ENUQuestBattleKind::BasicAttack ? 1 : Kind == ENUQuestBattleKind::Parry ? 2 : 3;
	CardIndex->SetText(FText::FromString(FString::Printf(TEXT("%02d / 03"), Index)));
	RebuildSteps(*Guide);
	RebuildSummary(*Guide);
	RefreshCount();
}

void UNUQuestPracticeGuideWidget::RefreshCount()
{
	using namespace NUQuestPracticeGuide;
	const FNUQuestPracticeGuideText* Guide = FindGuideText();
	if (Guide == nullptr || CardCount == nullptr)
	{
		return;
	}

	const bool bDone = Required > 0 && Count >= Required;
	CardCountLabel->SetText(bDone
		? FText::Format(NSLOCTEXT("NUQuestPracticeGuide", "CountLabelDone", "{0} 완료"), Guide->CountLabel)
		: Guide->CountLabel);
	CardCount->SetText(Required > 0
		? FText::Format(NSLOCTEXT("NUQuestPracticeGuide", "CountOfRequired", "{0} / {1}"), FText::AsNumber(Count), FText::AsNumber(Required))
		: FText::AsNumber(Count));
	CardCount->SetColorAndOpacity(FSlateColor(bDone ? Gold() : Ivory()));
	if (CardArt.IsValid())
	{
		CardArt->SetPractice(Kind, Count, Required);
	}
}

bool UNUQuestPracticeGuideWidget::IsBattleStageVisible() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ANUPlayerState* PS = PC != nullptr ? Cast<ANUPlayerState>(PC->PlayerState) : nullptr;
	const UNUAbilitySystemComponent* ASC = PS != nullptr ? PS->GetNUASC() : nullptr;
	if (ASC != nullptr && ASC->GetBattleEncounter() != nullptr)
	{
		return true;
	}
	// 서버 전투는 넷 전투 세트가 준비된 동안. IsTransitionActive는 세트가 떠 있는 내내 참이라 쓰지 않는다
	const UWorld* World = GetWorld();
	const UNUBattleNetStageSubsystem* Stage = World != nullptr ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;
	return Stage != nullptr && Stage->IsNetStageReady();
}

bool UNUQuestPracticeGuideWidget::IsGamepadMode() const
{
	const ANUPlayerController* PC = Cast<ANUPlayerController>(GetOwningPlayer());
	return PC != nullptr && PC->IsGamepadMode();
}

void UNUQuestPracticeGuideWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace NUQuestPracticeGuide;
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 키보드와 패드를 바꾸거나 전투 무대가 막 떴으면(전투 입력이 켜져 확인 키를 읽을 수 있다) 안내의 키 이름을 다시 쓴다
	const bool bPad = IsGamepadMode();
	const bool bStageVisible = IsBattleStageVisible();
	if (bPad != bGamepadMode || (bStageVisible && !bWasStageVisible))
	{
		bGamepadMode = bPad;
		RefreshTexts();
	}
	bWasStageVisible = bStageVisible;

	CardAlpha = FMath::FInterpConstantTo(CardAlpha, bStageVisible && bKindSet ? 1.f : 0.f, InDeltaTime, 4.f);
	if (bDismissing)
	{
		DismissAlpha = FMath::FInterpConstantTo(DismissAlpha, 0.f, InDeltaTime, 3.f);
	}
	if (CardRoot != nullptr)
	{
		const float Alpha = CardAlpha * DismissAlpha;
		CardRoot->SetRenderOpacity(Alpha);
		const ESlateVisibility Wanted = Alpha <= 0.f ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
		if (CardRoot->GetVisibility() != Wanted) { CardRoot->SetVisibility(Wanted); }
	}

	if (CardCount != nullptr && CountPulse > 0.f)
	{
		// 횟수가 오른 순간 숫자를 한 번 튀게 한다
		CountPulse = FMath::Max(0.f, CountPulse - InDeltaTime);
		CardCount->SetRenderScale(FVector2D(1.f + 0.3f * (CountPulse / PulseSeconds)));
	}

	if (bDismissing && DismissAlpha <= 0.f)
	{
		RemoveFromParent();
	}
}
