#include "Quest/UI/NUQuestJournalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/IUserListEntry.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "UI/NUUIManager.h"

namespace NUQuestJournalMotion
{
	constexpr float OpenDuration = 0.38f;
	constexpr float CloseDuration = 0.20f;
	constexpr float ReopenDuration = 0.20f;
	constexpr float OpenTravel = 24.0f;
	constexpr float CloseTravel = 15.0f;

	float EaseOut(float Time, float Start, float End)
	{
		const float T = FMath::Clamp((Time - Start) / (End - Start), 0.0f, 1.0f);
		return 1.0f - FMath::Pow(1.0f - T, 3.0f);
	}

	enum class ERole : uint8 { Surface, Content, TranslationOnly, Divider, Compass };

	struct FTarget
	{
		TWeakObjectPtr<UWidget> Widget;
		ERole Role = ERole::Content;
		float Side = 0.0f;
		float Start = 0.0f;
		float End = 0.0f;
		float BaseOpacity = 1.0f;
		float FromOpacity = 1.0f;
		FWidgetTransform BaseTransform;
		FWidgetTransform FromTransform;
		FLinearColor BaseColor = FLinearColor::White;
		FLinearColor FromColor = FLinearColor::White;
	};

	// RoundedBox outlines with independent transparency bypass Slate's inherited
	// render opacity. Keep their authored alpha (including transparent-fill trims)
	// and apply the same fade as their owning widget hierarchy.
	void VisitBrushes(UWidget* Widget, TFunctionRef<bool(FSlateBrush&, int32)> Visit)
	{
		if (UBorder* Border = Cast<UBorder>(Widget))
		{
			FSlateBrush Brush = Border->Background;
			if (Visit(Brush, 0)) Border->SetBrush(Brush);
		}
		else if (UImage* Image = Cast<UImage>(Widget))
		{
			FSlateBrush Brush = Image->GetBrush();
			if (Visit(Brush, 0)) Image->SetBrush(Brush);
		}
		else if (UButton* Button = Cast<UButton>(Widget))
		{
			FButtonStyle Style = Button->GetStyle();
			bool bChanged = Visit(Style.Normal, 0);
			bChanged |= Visit(Style.Hovered, 1);
			bChanged |= Visit(Style.Pressed, 2);
			bChanged |= Visit(Style.Disabled, 3);
			if (bChanged) Button->SetStyle(Style);
		}
		else if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget))
		{
			FCheckBoxStyle Style = CheckBox->GetWidgetStyle();
			FSlateBrush* Brushes[] = {
				&Style.UncheckedImage, &Style.UncheckedHoveredImage, &Style.UncheckedPressedImage,
				&Style.CheckedImage, &Style.CheckedHoveredImage, &Style.CheckedPressedImage,
				&Style.UndeterminedImage, &Style.UndeterminedHoveredImage, &Style.UndeterminedPressedImage,
				&Style.BackgroundImage, &Style.BackgroundHoveredImage, &Style.BackgroundPressedImage
			};
			bool bChanged = false;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Brushes); ++Index)
				bChanged |= Visit(*Brushes[Index], Index);
			if (bChanged) CheckBox->SetWidgetStyle(Style);
		}
	}

	float GetInheritedOpacity(const UWidget* Widget, TConstArrayView<FTarget> RestTargets = {})
	{
		float Opacity = 1.0f;
		while (Widget)
		{
			// A row can first appear while its animated parent is fully transparent.
			// Its baseline must use resting opacities, not that partially faded frame.
			const FTarget* Rest = RestTargets.FindByPredicate(
				[Widget](const FTarget& Target) { return Target.Widget.Get() == Widget; });
			Opacity *= Rest ? Rest->BaseOpacity : Widget->GetRenderOpacity();
			if (const UPanelWidget* Parent = Widget->GetParent()) Widget = Parent;
			else if (const IUserListEntry* Entry = Cast<IUserListEntry>(Widget))
				Widget = Entry->GetOwningListView();
			else
			{
				// A nested UserWidget's root has no panel parent.
				const UWidgetTree* Tree = Cast<UWidgetTree>(Widget->GetOuter());
				Widget = Tree ? Cast<UUserWidget>(Tree->GetOuter()) : nullptr;
			}
		}
		return Opacity;
	}

	struct FOutlineTarget
	{
		TWeakObjectPtr<UWidget> Widget;
		FSlateColor BaseColors[12];
		bool bFadeBrush[12] = {};
		float BaseOpacity = 1.0f;

		bool Capture(UWidget* InWidget, TConstArrayView<FTarget> RestTargets)
		{
			Widget = InWidget;
			BaseOpacity = GetInheritedOpacity(InWidget, RestTargets);
			bool bHasOutline = false;
			VisitBrushes(InWidget, [this, &bHasOutline](FSlateBrush& Brush, int32 Index)
			{
				if (Brush.DrawAs == ESlateBrushDrawType::RoundedBox
					&& !Brush.OutlineSettings.bUseBrushTransparency && Brush.OutlineSettings.Width > 0.0f)
				{
					BaseColors[Index] = Brush.OutlineSettings.Color;
					bFadeBrush[Index] = true;
					bHasOutline = true;
				}
				return false;
			});
			return bHasOutline;
		}

		void Apply(bool bRestore = false) const
		{
			UWidget* Target = Widget.Get();
			if (!Target) return;
			const float Fade = bRestore ? 1.0f : (BaseOpacity > UE_SMALL_NUMBER
				? FMath::Clamp(GetInheritedOpacity(Target) / BaseOpacity, 0.0f, 1.0f) : 0.0f);
			VisitBrushes(Target, [this, Fade](FSlateBrush& Brush, int32 Index)
			{
				if (!bFadeBrush[Index]) return false;
				const FSlateColor Color = Fade == 1.0f ? BaseColors[Index]
					: FSlateColor(BaseColors[Index].GetSpecifiedColor().CopyWithNewOpacity(
						BaseColors[Index].GetSpecifiedColor().A * Fade));
				if (Brush.OutlineSettings.Color == Color) return false;
				Brush.OutlineSettings.Color = Color;
				return true;
			});
		}
	};
}

struct FNUQuestJournalMotionState
{
	TArray<NUQuestJournalMotion::FTarget> Targets;
	TMap<TWeakObjectPtr<UWidget>, NUQuestJournalMotion::FOutlineTarget> Outlines;
	TWeakObjectPtr<UBackgroundBlur> Blur;
	TWeakObjectPtr<UWidget> Dim;
	TWeakObjectPtr<UWidget> InputShield;
	FTSTicker::FDelegateHandle Ticker;
	float BaseBlur = 4.5f;
	float BaseDimOpacity = 1.0f;
	float FromBlur = 0.0f;
	float FromDimOpacity = 0.0f;
	double StartSeconds = 0.0;
	bool bInitialFrame = true;
	bool bActive = false;
	bool bOpening = false;
	bool bFreshOpen = false;
};

bool UNUQuestJournalWidget::IsJournalClosing() const
{
	return JournalMotion && JournalMotion->bActive && !JournalMotion->bOpening;
}

void UNUQuestJournalWidget::StartJournalMotion(bool bOpening, bool bFromHidden)
{
	using namespace NUQuestJournalMotion;
	const bool bCaptureOutlines = !JournalMotion || !JournalMotion->bActive;
	StopJournalMotion(false);
	if (!JournalMotion)
	{
		JournalMotion = MakeShared<FNUQuestJournalMotionState>();
		FNUQuestJournalMotionState& M = *JournalMotion;
		M.Blur = Cast<UBackgroundBlur>(GetWidgetFromName(TEXT("QM_WorldBlur")));
		M.Dim = GetWidgetFromName(TEXT("QM_WorldTint"));
		M.InputShield = GetWidgetFromName(TEXT("QMotion_InputShield"));
		if (M.Blur.IsValid()) M.BaseBlur = M.Blur->GetBlurStrength();
		if (M.Dim.IsValid()) M.BaseDimOpacity = M.Dim->GetRenderOpacity();

		auto Add = [this, &M](const TCHAR* Name, ERole Role, float Side, float Start, float End)
		{
			if (UWidget* Widget = GetWidgetFromName(Name))
			{
				FTarget& Target = M.Targets.AddDefaulted_GetRef();
				Target.Widget = Widget;
				Target.Role = Role;
				Target.Side = Side;
				Target.Start = Start;
				Target.End = End;
				Target.BaseOpacity = Widget->GetRenderOpacity();
				Target.BaseTransform = Widget->GetRenderTransform();
				if (const UImage* Image = Cast<UImage>(Widget)) Target.BaseColor = Image->GetColorAndOpacity();
			}
		};

		// Open_Surface: small opposing translations; panel scale always stays at 1.
		Add(TEXT("QM_IndexMist"), ERole::Surface, -1, .04f, .25f);
		Add(TEXT("QM_ReadingMist"), ERole::Surface, 1, .04f, .25f);
		Add(TEXT("QL_RailDivider"), ERole::Surface, -1, .04f, .25f);
		Add(TEXT("QMotion_GoldDivider"), ERole::Divider, -1, .08f, .28f);
		Add(TEXT("DetailPanel"), ERole::TranslationOnly, 1, .04f, .25f);

		// Open_Content: grouped stagger keeps information readable without a long cascade.
		for (const TCHAR* Name : { TEXT("JournalHeaderCard"), TEXT("QP_HeaderRule"), TEXT("QP_HeaderDiamond") })
			Add(Name, ERole::Content, -1, .13f, .25f);
		for (const TCHAR* Name : { TEXT("QJ_IndexSize"), TEXT("QL_AllTile"), TEXT("QL_MainTile"), TEXT("QL_SideTile"), TEXT("QL_DailyTile"), TEXT("QL_TutorialTile") })
			Add(Name, ERole::Content, -1, .16f, .29f);
		for (const TCHAR* Name : { TEXT("QV4_CategorySize"), TEXT("QP_CategoryRule") })
			Add(Name, ERole::Content, 1, .19f, .31f);
		for (const TCHAR* Name : { TEXT("DetailTitleText"), TEXT("DetailMetaText"), TEXT("QR_TitleBadgeSize"), TEXT("QR_LocationRow") })
			Add(Name, ERole::Content, 0, .21f, .33f);
		for (const TCHAR* Name : { TEXT("QP_DescriptionRow"), TEXT("DetailDescriptionText"), TEXT("QP_ObjectiveRow"), TEXT("DetailObjectiveText"), TEXT("QP_RewardRow"), TEXT("DetailRewardText") })
			Add(Name, ERole::Content, 0, .24f, .35f);
		Add(TEXT("ActionButtonRow"), ERole::Content, 0, .26f, .36f);
		Add(TEXT("QM_CompassWatermark"), ERole::Content, 1, .20f, .35f);
		Add(TEXT("QV4_FeatherSize"), ERole::Content, 1, .20f, .35f);
		Add(TEXT("CloseButtonSizeBox"), ERole::Content, 0, .13f, .25f);
		Add(TEXT("NF_JournalCrest"), ERole::Compass, 0, .04f, .38f);
	}

	FNUQuestJournalMotionState& M = *JournalMotion;
	if (bCaptureOutlines)
	{
		// Refresh idle styles between transitions, but never capture a partially
		// faded alpha when a close/open reverses an in-flight animation.
		M.Outlines.Reset();
	}
	M.bOpening = bOpening;
	M.bFreshOpen = bOpening && bFromHidden;
	M.bActive = true;
	M.StartSeconds = FPlatformTime::Seconds();
	M.bInitialFrame = true;
	M.FromBlur = M.Blur.IsValid() ? M.Blur->GetBlurStrength() : 0.0f;
	M.FromDimOpacity = M.Dim.IsValid() ? M.Dim->GetRenderOpacity() : 0.0f;
	if (M.bFreshOpen) { M.FromBlur = 0; M.FromDimOpacity = 0; }
	for (FTarget& Target : M.Targets)
	{
		if (UWidget* Widget = Target.Widget.Get())
		{
			Target.FromOpacity = Widget->GetRenderOpacity();
			Target.FromTransform = Widget->GetRenderTransform();
			if (const UImage* Image = Cast<UImage>(Widget)) Target.FromColor = Image->GetColorAndOpacity();
			if (M.bFreshOpen)
			{
				Target.FromTransform = Target.BaseTransform;
				Target.FromOpacity = Target.BaseOpacity;
				Target.FromColor = Target.BaseColor;
				if (Target.Role != ERole::TranslationOnly && Target.Role != ERole::Compass) Target.FromOpacity = 0;
				Target.FromTransform.Translation.X += Target.Side * OpenTravel;
				if (Target.Role == ERole::Content) Target.FromTransform.Translation.Y += 6;
				if (Target.Role == ERole::Divider) Target.FromTransform.Scale.Y = 0;
				if (Target.Role == ERole::Compass) Target.FromTransform.Angle -= 6;
			}
		}
	}
	if (M.InputShield.IsValid()) M.InputShield->SetVisibility(ESlateVisibility::Visible);
	// Apply time zero synchronously: no one-frame flash of the finished panel.
	TickJournalMotion(0);
	M.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
		[this](float DeltaSeconds) { return TickJournalMotion(DeltaSeconds); }));
}

bool UNUQuestJournalWidget::TickJournalMotion(float /*DeltaSeconds*/)
{
	using namespace NUQuestJournalMotion;
	if (!JournalMotion || !JournalMotion->bActive) return false;
	FNUQuestJournalMotionState& M = *JournalMotion;
	// Wall time avoids slow exits after a hitch or frame-rate clamp; time zero stays exact.
	const float Time = M.bInitialFrame ? 0.0f : static_cast<float>(FPlatformTime::Seconds() - M.StartSeconds);
	M.bInitialFrame = false;
	const float Duration = M.bOpening ? (M.bFreshOpen ? OpenDuration : ReopenDuration) : CloseDuration;

	// Open_Backdrop / Close_Backdrop. Core ticker uses real time, including while the world is paused.
	const float BackdropAlpha = EaseOut(Time, 0, M.bOpening ? .12f : .20f);
	if (M.Blur.IsValid()) M.Blur->SetBlurStrength(FMath::Lerp(M.FromBlur, M.bOpening ? M.BaseBlur : 0.0f, BackdropAlpha));
	if (M.Dim.IsValid()) M.Dim->SetRenderOpacity(FMath::Lerp(M.FromDimOpacity, M.bOpening ? M.BaseDimOpacity : 0.0f, BackdropAlpha));

	for (const FTarget& Target : M.Targets)
	{
		UWidget* Widget = Target.Widget.Get();
		if (!Widget) continue;
		FWidgetTransform Transform = Target.FromTransform;
		float Opacity = Target.FromOpacity;
		if (M.bOpening)
		{
			const float Alpha = EaseOut(Time, M.bFreshOpen ? Target.Start : 0.0f, M.bFreshOpen ? Target.End : .18f);
			const float Slide = EaseOut(Time, M.bFreshOpen ? .04f : 0.0f, M.bFreshOpen ? .25f : .18f);
			Transform.Translation.X = FMath::Lerp(Target.FromTransform.Translation.X, Target.BaseTransform.Translation.X, Slide);
			Transform.Translation.Y = FMath::Lerp(Target.FromTransform.Translation.Y, Target.BaseTransform.Translation.Y, Alpha);
			Opacity = FMath::Lerp(Target.FromOpacity, Target.BaseOpacity, Alpha);
			if (Target.Role == ERole::Divider)
			{
				Transform.Scale.Y = FMath::Lerp(Target.FromTransform.Scale.Y, Target.BaseTransform.Scale.Y, Alpha);
				if (M.bFreshOpen) Opacity = Target.BaseOpacity * Slide;
			}
			if (Target.Role == ERole::Compass)
			{
				if (M.bFreshOpen)
				{
					Transform.Angle = Target.BaseTransform.Angle + (Time < .23f
						? FMath::Lerp(-6.0f, 1.5f, EaseOut(Time, .04f, .23f))
						: FMath::Lerp(1.5f, 0.0f, EaseOut(Time, .23f, .38f)));
				}
				else Transform.Angle = FMath::Lerp(Target.FromTransform.Angle, Target.BaseTransform.Angle, Alpha);
				if (UImage* Compass = Cast<UImage>(Widget))
				{
					FLinearColor Color = FMath::Lerp(Target.FromColor, Target.BaseColor, Alpha);
					const float Pulse = M.bFreshOpen ? .10f * FMath::Sin(PI * FMath::Clamp((Time - .10f) / .28f, 0.0f, 1.0f)) : 0.0f;
					Color.R += Pulse; Color.G += Pulse * .8f; Color.B += Pulse * .4f;
					Compass->SetColorAndOpacity(Color);
				}
			}
		}
		else
		{
			// Close is independent of Open: capture the displayed state, then move outward and fade.
			// No divider rewind or compass flourish, including when closing during an opening.
			const bool bContent = Target.Role == ERole::Content;
			const float Fade = EaseOut(Time, bContent ? 0.0f : .025f, bContent ? .08f : .16f);
			if (Target.Role != ERole::Compass && Target.Role != ERole::TranslationOnly)
				Opacity = FMath::Lerp(Target.FromOpacity, 0.0f, Fade);
			Transform.Translation.X += Target.Side * CloseTravel * EaseOut(Time, .025f, .16f);
		}
		Widget->SetRenderTransform(Transform);
		Widget->SetRenderOpacity(Opacity);
	}
	// All parent opacities must be updated before their outlines are evaluated.
	RefreshJournalOutlines();
	if (M.bOpening && Time >= (M.bFreshOpen ? .25f : .18f) && M.InputShield.IsValid())
		M.InputShield->SetVisibility(ESlateVisibility::Collapsed);
	if (Time < Duration) return true;
	M.bActive = false;
	M.Ticker.Reset();
	if (!M.bOpening) FinishJournalClose();
	return false;
}

void UNUQuestJournalWidget::RefreshJournalOutlines()
{
	using namespace NUQuestJournalMotion;
	if (!JournalMotion || !JournalMotion->bActive || !WidgetTree) return;
	FNUQuestJournalMotionState& M = *JournalMotion;
	TFunction<void(UWidget*)> Collect = [&M, &Collect](UWidget* Widget)
	{
		const TWeakObjectPtr<UWidget> Key(Widget);
		if (!M.Outlines.Contains(Key))
		{
			FOutlineTarget Outline;
			if (Outline.Capture(Widget, M.Targets)) M.Outlines.Add(Key, MoveTemp(Outline));
		}
		// Virtualized rows live in the list's pool, outside its UMG WidgetTree.
		if (const UListViewBase* List = Cast<UListViewBase>(Widget))
			for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
				if (Entry && Entry->WidgetTree) Entry->WidgetTree->ForEachWidgetAndDescendants(Collect);
	};
	WidgetTree->ForEachWidgetAndDescendants(Collect);
	for (auto It = M.Outlines.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
		else It.Value().Apply();
	}
}

void UNUQuestJournalWidget::HandleJournalEntryGenerated(UUserWidget& /*Entry*/)
{
	RefreshJournalOutlines();
}

void UNUQuestJournalWidget::HandleJournalEntryReleased(UUserWidget& Entry)
{
	if (!JournalMotion || !Entry.WidgetTree) return;
	Entry.WidgetTree->ForEachWidgetAndDescendants([this](UWidget* Widget)
	{
		const TWeakObjectPtr<UWidget> Key(Widget);
		if (const NUQuestJournalMotion::FOutlineTarget* Outline = JournalMotion->Outlines.Find(Key))
		{
			// Pooled rows must not carry a faded brush into their next assignment.
			Outline->Apply(true);
			JournalMotion->Outlines.Remove(Key);
		}
	});
}

void UNUQuestJournalWidget::StopJournalMotion(bool bRestoreVisuals)
{
	if (!JournalMotion) return;
	FNUQuestJournalMotionState& M = *JournalMotion;
	if (M.Ticker.IsValid()) { FTSTicker::RemoveTicker(M.Ticker); M.Ticker.Reset(); }
	M.bActive = false;
	if (!bRestoreVisuals) return;
	for (const NUQuestJournalMotion::FTarget& Target : M.Targets)
	{
		if (UWidget* Widget = Target.Widget.Get())
		{
			Widget->SetRenderOpacity(Target.BaseOpacity);
			Widget->SetRenderTransform(Target.BaseTransform);
			if (Target.Role == NUQuestJournalMotion::ERole::Compass)
				if (UImage* Compass = Cast<UImage>(Widget)) Compass->SetColorAndOpacity(Target.BaseColor);
		}
	}
	if (M.Blur.IsValid()) M.Blur->SetBlurStrength(M.BaseBlur);
	if (M.Dim.IsValid()) M.Dim->SetRenderOpacity(M.BaseDimOpacity);
	for (const auto& Pair : M.Outlines) Pair.Value.Apply(true);
	if (M.InputShield.IsValid()) M.InputShield->SetVisibility(ESlateVisibility::Collapsed);
}

void UNUQuestJournalWidget::FinishJournalClose()
{
	SetJournalVisible(false);
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer())) UI->CloseWindow(this);
	StopJournalMotion(true);
}

void UNUQuestJournalWidget::CloseJournalImmediately()
{
	StopJournalMotion(true);
	FinishJournalClose();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUQuestJournalOutlineFadeTest,
	"ProjectM.UI.QuestJournal.OutlineFade.LateCheckboxAndReversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUQuestJournalOutlineFadeTest::RunTest(const FString& /*Parameters*/)
{
	using namespace NUQuestJournalMotion;
	TStrongObjectPtr<UBorder> Section(NewObject<UBorder>());
	UCheckBox* CheckBox = NewObject<UCheckBox>(Section.Get());
	Section->SetContent(CheckBox);
	CheckBox->SetRenderOpacity(0.6f);
	VisitBrushes(CheckBox, [](FSlateBrush& Brush, int32 Index)
	{
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			FLinearColor(0.3f, 0.5f, 0.7f, (Index + 1) / 12.0f), 1.8f);
		return true;
	});
	TArray<FTarget> Targets;
	FTarget& SectionTarget = Targets.AddDefaulted_GetRef();
	SectionTarget.Widget = Section.Get();
	SectionTarget.BaseOpacity = 0.8f;
	Section->SetRenderOpacity(0.0f);
	FOutlineTarget Outline;
	TestTrue(TEXT("A late checkbox registers its outline styles"), Outline.Capture(CheckBox, Targets));
	TestEqual(TEXT("Late registration uses the resting parent opacity"), Outline.BaseOpacity, 0.48f);

	// Opening, closing halfway, reopening and closing again must not accumulate
	// alpha loss. Transparent fills must not erase the independent outlines.
	for (float Fade : { 0.0f, 0.25f, 0.75f, 0.2f, 0.6f, 1.0f, 0.0f })
	{
		Section->SetRenderOpacity(0.8f * Fade);
		Outline.Apply();
		int32 Visited = 0;
		VisitBrushes(CheckBox, [this, Fade, &Visited](FSlateBrush& Brush, int32 Index)
		{
			++Visited;
			const FLinearColor Color = Brush.OutlineSettings.Color.GetSpecifiedColor();
			TestEqual(FString::Printf(TEXT("Checkbox style %d follows fade %.2f"), Index, Fade),
				Color.A, (Index + 1) / 12.0f * Fade);
			TestEqual(TEXT("Outline color is preserved"), Color.R, 0.3f);
			TestEqual(TEXT("Transparent fill is preserved"), Brush.TintColor.GetSpecifiedColor().A, 0.0f);
			return false;
		});
		TestEqual(TEXT("All checkbox states and backgrounds are covered"), Visited, 12);
	}
	Outline.Apply(true);
	VisitBrushes(CheckBox, [this](FSlateBrush& Brush, int32 Index)
	{
		TestEqual(TEXT("Releasing or closing restores authored alpha"),
			Brush.OutlineSettings.Color.GetSpecifiedColor().A, (Index + 1) / 12.0f);
		return false;
	});
	return true;
}
#endif
