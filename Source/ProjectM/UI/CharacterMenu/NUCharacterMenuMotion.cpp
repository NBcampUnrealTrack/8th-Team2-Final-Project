#include "UI/CharacterMenu/NUCharacterMenuMotion.h"

#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"

namespace NUCharacterMenuMotion
{
namespace
{
	enum class EKind : uint8 { Move, Scale, Color };

	struct FRest
	{
		FVector2D Translation = FVector2D::ZeroVector;
		FVector2D Scale = FVector2D::UnitVector;
		float Opacity = 1.f;
	};

	struct FTween
	{
		TWeakObjectPtr<UWidget> Widget;
		TWeakObjectPtr<UUserWidget> Owner;
		EKind Kind = EKind::Move;
		float Time = 0.f, Delay = 0.f, Duration = 0.2f;
		FVector2D FromT, ToT, FromS, ToS;
		float FromO = 1.f, ToO = 1.f;
		FLinearColor FromC, ToC;
		bool bBack = false;
		bool bAway = false; // ends away from rest: keep the widget marked displaced
	};

	struct FClose
	{
		TWeakObjectPtr<UUserWidget> Menu;
		float Time = 0.f;
		FSimpleDelegate Completion;
	};

	struct FDrop
	{
		TWeakObjectPtr<UWidget> Owner;
		TArray<TWeakObjectPtr<UWidget>> Targets;
		TWeakObjectPtr<UWidget> Hot;
		TFunction<void(UWidget*, UWidget*)> OnHot;
	};

	constexpr float CloseDuration = 0.2f;
	constexpr float DropScale = 1.08f;
	constexpr float LiftScale = 1.12f;
	constexpr float LiftAngle = -4.f;

	TArray<FTween> Tweens;
	TArray<FClose> Closes;
	TArray<FDrop> Drops;
	TMap<TWeakObjectPtr<UWidget>, FRest> Rests;
	TSet<TWeakObjectPtr<UWidget>> Displaced;
	TMap<TWeakObjectPtr<UTextBlock>, FSlateColor> TextRests;
	TMap<TWeakObjectPtr<const UObject>, TMap<FName, float>> TrackedValues;
	TMap<TWeakObjectPtr<const UObject>, TMap<FName, FName>> TrackedNames;

	template <typename TValue>
	TMap<FName, TValue>& TrackedFor(TMap<TWeakObjectPtr<const UObject>, TMap<FName, TValue>>& Map, const UObject* Owner)
	{
		if (TMap<FName, TValue>* Found = Map.Find(Owner))
		{
			return *Found;
		}
		for (auto It = Map.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid()) It.RemoveCurrent();
		}
		return Map.Add(Owner);
	}
	FDelegateHandle TickHandle;

	float EaseOut(float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		return 1.f - FMath::Pow(1.f - X, 3.f);
	}

	// Slight overshoot for things that "land" (selection pill, lifted items).
	float EaseOutBack(float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		constexpr float C1 = 1.4f, C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(X - 1.f, 3.f) + C1 * FMath::Pow(X - 1.f, 2.f);
	}

	TMap<TWeakObjectPtr<UUserWidget>, int32> PendingDirections;

	int32 FindTween(const UWidget* Widget, EKind Kind)
	{
		return Tweens.IndexOfByPredicate([Widget, Kind](const FTween& T) { return T.Kind == Kind && T.Widget.Get() == Widget; });
	}

	// Rest values are captured when no tween of the kind is running, so a re-trigger mid-flight
	// still settles on the authored values rather than on an intermediate frame.
	const FRest& RestOf(UWidget* Widget, EKind Kind)
	{
		FRest* Existing = Rests.Find(Widget);
		if (Existing && (FindTween(Widget, Kind) != INDEX_NONE || Displaced.Contains(Widget)))
		{
			return *Existing;
		}
		FRest& Rest = Existing ? *Existing : Rests.Add(Widget);
		const FWidgetTransform& Transform = Widget->GetRenderTransform();
		if (Kind == EKind::Move)
		{
			Rest.Translation = Transform.Translation;
			Rest.Opacity = Widget->GetRenderOpacity();
		}
		else if (Kind == EKind::Scale)
		{
			Rest.Scale = Transform.Scale;
		}
		return Rest;
	}

	void Apply(FTween& T, float Alpha)
	{
		UWidget* Widget = T.Widget.Get();
		if (!Widget)
		{
			return;
		}
		switch (T.Kind)
		{
		case EKind::Move:
			Widget->SetRenderTranslation(FMath::Lerp(T.FromT, T.ToT, Alpha));
			Widget->SetRenderOpacity(FMath::Lerp(T.FromO, T.ToO, Alpha));
			break;
		case EKind::Scale:
			Widget->SetRenderScale(FMath::Lerp(T.FromS, T.ToS, Alpha));
			break;
		case EKind::Color:
			if (UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Text->SetColorAndOpacity(FSlateColor(FMath::Lerp(T.FromC, T.ToC, Alpha)));
			}
			break;
		}
	}

	void Tick(float Delta);

	void EnsureTick()
	{
		if (!TickHandle.IsValid() && FSlateApplication::IsInitialized())
		{
			TickHandle = FSlateApplication::Get().OnPreTick().AddStatic(&Tick);
		}
	}

	void StopTickIfIdle()
	{
		if (Tweens.IsEmpty() && Closes.IsEmpty() && Drops.IsEmpty() && TickHandle.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnPreTick().Remove(TickHandle);
			TickHandle.Reset();
		}
	}

	void AddMove(UUserWidget* Owner, UWidget* Widget, const FVector2D& Offset, float Delay, float Duration, bool bFadeIn = true)
	{
		if (!Widget || !Widget->IsVisible())
		{
			return;
		}
		const FRest Rest = RestOf(Widget, EKind::Move);
		if (const int32 Index = FindTween(Widget, EKind::Move); Index != INDEX_NONE)
		{
			Tweens.RemoveAt(Index);
		}
		FTween& T = Tweens.AddDefaulted_GetRef();
		T.Widget = Widget;
		T.Owner = Owner;
		T.Kind = EKind::Move;
		T.Delay = Delay;
		T.Duration = Duration;
		T.FromT = Rest.Translation + Offset;
		T.ToT = Rest.Translation;
		T.FromO = bFadeIn ? 0.f : Rest.Opacity;
		T.ToO = Rest.Opacity;
		Apply(T, 0.f); // hidden at its start offset until its delay elapses
	}

	// bAway: the tween leaves the widget off its rest scale (lift); otherwise it returns to rest.
	void ScaleTo(UWidget* Widget, const FVector2D& From, const FVector2D& To, float Duration, bool bAway = false)
	{
		RestOf(Widget, EKind::Scale); // capture rest before the widget is displaced
		if (const int32 Index = FindTween(Widget, EKind::Scale); Index != INDEX_NONE)
		{
			Tweens.RemoveAt(Index);
		}
		FTween& T = Tweens.AddDefaulted_GetRef();
		T.Widget = Widget;
		T.Kind = EKind::Scale;
		T.Duration = Duration;
		T.FromS = From;
		T.ToS = To;
		T.bAway = bAway;
		Displaced.Add(Widget);
		Apply(T, 0.f);
	}

	// Top-level panels of a page, in reading order (top-to-bottom, then left-to-right).
	TArray<UWidget*> PagePanels(UWidget* Page)
	{
		TArray<UWidget*> Panels;
		UUserWidget* User = Cast<UUserWidget>(Page);
		UPanelWidget* Root = User ? Cast<UPanelWidget>(User->GetRootWidget()) : Cast<UPanelWidget>(Page);
		if (!Root)
		{
			if (Page) Panels.Add(Page);
			return Panels;
		}
		for (UWidget* Child : Root->GetAllChildren())
		{
			if (Child && Child->IsVisible())
			{
				Panels.Add(Child);
			}
		}
		Panels.StableSort([](const UWidget& A, const UWidget& B)
		{
			const UCanvasPanelSlot* SA = Cast<UCanvasPanelSlot>(A.Slot);
			const UCanvasPanelSlot* SB = Cast<UCanvasPanelSlot>(B.Slot);
			if (!SA || !SB) return false;
			const FVector2D PA = SA->GetPosition(), PB = SB->GetPosition();
			return FMath::Abs(PA.Y - PB.Y) > 24.f ? PA.Y < PB.Y : PA.X < PB.X;
		});
		return Panels;
	}

	void TickDrops()
	{
		if (Drops.IsEmpty() || !FSlateApplication::IsInitialized())
		{
			return;
		}
		const FVector2D Cursor = FSlateApplication::Get().GetCursorPos();
		for (int32 Index = Drops.Num() - 1; Index >= 0; --Index)
		{
			FDrop& Drop = Drops[Index];
			UWidget* Owner = Drop.Owner.Get();
			if (!Owner || !Owner->IsVisible())
			{
				if (UWidget* Hot = Drop.Hot.Get())
				{
					Hot->SetRenderScale(RestOf(Hot, EKind::Scale).Scale);
					Displaced.Remove(Hot);
				}
				if (Drop.OnHot) Drop.OnHot(Drop.Hot.Get(), nullptr);
				Drops.RemoveAt(Index);
				continue;
			}
			UWidget* Under = nullptr;
			for (const TWeakObjectPtr<UWidget>& Target : Drop.Targets)
			{
				UWidget* Widget = Target.Get();
				if (Widget && Widget->IsVisible() && Widget->GetCachedGeometry().IsUnderLocation(Cursor))
				{
					Under = Widget;
					break;
				}
			}
			if (Under == Drop.Hot.Get())
			{
				continue;
			}
			if (UWidget* Old = Drop.Hot.Get())
			{
				const FVector2D Rest = RestOf(Old, EKind::Scale).Scale;
				ScaleTo(Old, Old->GetRenderTransform().Scale, Rest, 0.1f);
			}
			if (Under)
			{
				const FVector2D Rest = RestOf(Under, EKind::Scale).Scale;
				ScaleTo(Under, Under->GetRenderTransform().Scale, Rest * DropScale, 0.12f, true);
				Tweens.Last().bBack = true;
			}
			UWidget* Previous = Drop.Hot.Get();
			Drop.Hot = Under;
			if (Drop.OnHot) Drop.OnHot(Previous, Under);
		}
	}

	void Tick(float Delta)
	{
		Delta = FMath::Clamp(Delta, 0.f, 0.1f); // a hitch must not skip an entrance entirely

		for (int32 Index = Tweens.Num() - 1; Index >= 0; --Index)
		{
			FTween& T = Tweens[Index];
			if (!T.Widget.IsValid())
			{
				Tweens.RemoveAt(Index);
				continue;
			}
			T.Time += Delta;
			const float Linear = FMath::Clamp((T.Time - T.Delay) / FMath::Max(T.Duration, KINDA_SMALL_NUMBER), 0.f, 1.f);
			// Colour flashes hold their accent briefly before easing back.
			const float Alpha = T.Kind == EKind::Color ? EaseOut((Linear - 0.25f) / 0.75f) : T.bBack ? EaseOutBack(Linear) : EaseOut(Linear);
			Apply(T, Alpha);
			if (Linear >= 1.f)
			{
				if (T.Kind == EKind::Color)
				{
					TextRests.Remove(Cast<UTextBlock>(T.Widget.Get()));
				}
				if (T.Kind == EKind::Scale && !T.bAway)
				{
					Displaced.Remove(T.Widget); // back on its rest scale
				}
				Tweens.RemoveAt(Index);
			}
		}

		// Completions close the menu, whose destruct calls Stop() and edits these arrays: run them after the loop.
		TArray<FSimpleDelegate, TInlineAllocator<1>> Finished;
		for (int32 Index = Closes.Num() - 1; Index >= 0; --Index)
		{
			FClose& Close = Closes[Index];
			Close.Time += Delta;
			if (Close.Time >= CloseDuration || !Close.Menu.IsValid())
			{
				Finished.Add(MoveTemp(Close.Completion));
				Closes.RemoveAt(Index);
			}
		}
		for (FSimpleDelegate& Completion : Finished)
		{
			Completion.ExecuteIfBound();
		}

		TickDrops();
		StopTickIfIdle();
	}
}

void OpenShell(UUserWidget* Menu)
{
	if (!Menu || !FSlateApplication::IsInitialized())
	{
		return;
	}
	static const TCHAR* Rail[] = { TEXT("RailPanel"), TEXT("RailCaption"), TEXT("NavSlider"), TEXT("NavCharacterButton"), TEXT("NavEquipmentButton"),
		TEXT("NavInventoryButton"), TEXT("NavStatsButton"), TEXT("NavTraitsButton"), TEXT("NavNaiaButton") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rail); ++Index)
	{
		AddMove(Menu, Menu->GetWidgetFromName(Rail[Index]), FVector2D(-24.f, 0.f), Index < 3 ? 0.f : 0.03f + 0.02f * (Index - 3), 0.25f);
	}
	AddMove(Menu, Menu->GetWidgetFromName(TEXT("PageTitleText")), FVector2D(0.f, 10.f), 0.05f, 0.25f);
	for (const TCHAR* Name : { TEXT("HeaderGoldCoin"), TEXT("HeaderGoldText"), TEXT("CloseButton"), TEXT("KeyHintRow") })
	{
		AddMove(Menu, Menu->GetWidgetFromName(Name), FVector2D::ZeroVector, 0.08f, 0.2f);
	}
	EnsureTick();
}

void EnterPage(UUserWidget* Menu, UWidget* Page, int32 Direction)
{
	if (!Page || !FSlateApplication::IsInitialized())
	{
		return;
	}
	if (const int32* Pending = PendingDirections.Find(Menu))
	{
		Direction = *Pending;
		PendingDirections.Remove(Menu);
	}
	const FVector2D Offset(0.f, Direction == 0 ? 8.f : 28.f * FMath::Sign(Direction));
	const TArray<UWidget*> Panels = PagePanels(Page);
	for (int32 Index = 0; Index < Panels.Num(); ++Index)
	{
		// Cap the stagger so long lists (inventory controls) still finish within ~0.4 s.
		AddMove(Menu, Panels[Index], Offset, FMath::Min(0.035f * Index, 0.14f), Direction == 0 ? 0.2f : 0.26f);
	}
	EnsureTick();
}

void SwapHeading(UUserWidget* Menu, UWidget* Heading, int32 Direction)
{
	if (Menu)
	{
		PendingDirections.Add(Menu, Direction); // consumed by the EnterPage that follows
	}
	if (!Heading || !FSlateApplication::IsInitialized())
	{
		return;
	}
	AddMove(Menu, Heading, FVector2D(0.f, 14.f * FMath::Sign(Direction)), 0.f, 0.22f);
	EnsureTick();
}

void SlideTo(UUserWidget* Owner, UWidget* Widget, const FVector2D& Target, float Duration)
{
	if (!Widget)
	{
		return;
	}
	FRest& Rest = Rests.FindOrAdd(Widget);
	Rest.Opacity = Widget->GetRenderOpacity();
	Rest.Translation = Target;
	if (const int32 Index = FindTween(Widget, EKind::Move); Index != INDEX_NONE)
	{
		Tweens.RemoveAt(Index);
	}
	if (Duration <= 0.f || !FSlateApplication::IsInitialized())
	{
		Widget->SetRenderTranslation(Target);
		return;
	}
	FTween& T = Tweens.AddDefaulted_GetRef();
	T.Widget = Widget;
	T.Owner = Owner;
	T.Kind = EKind::Move;
	T.Duration = Duration;
	T.FromT = Widget->GetRenderTransform().Translation;
	T.ToT = Target;
	T.FromO = T.ToO = Rest.Opacity;
	T.bBack = true;
	EnsureTick();
}

bool DeferClose(UUserWidget* Menu, FSimpleDelegate Completion)
{
	if (!Menu || !Menu->IsInViewport() || !FSlateApplication::IsInitialized() || !Menu->GetWorld() || Menu->GetWorld()->bIsTearingDown)
	{
		return false;
	}
	if (Closes.ContainsByPredicate([Menu](const FClose& Close) { return Close.Menu.Get() == Menu; }))
	{
		return true;
	}
	// Finish entrance tweens first so the exit starts from the settled layout.
	for (int32 Index = Tweens.Num() - 1; Index >= 0; --Index)
	{
		if (Tweens[Index].Owner.Get() == Menu)
		{
			Apply(Tweens[Index], 1.f);
			Tweens.RemoveAt(Index);
		}
	}
	// The panels leave the way OpenShell brought them in (rail to the left, page and header down);
	// the dim backdrop stays until the menu is removed, so the exit never dips the whole screen.
	auto Leave = [Menu](const TCHAR* Name, const FVector2D& Offset, float Delay)
	{
		UWidget* Widget = Menu->GetWidgetFromName(Name);
		if (!Widget || !Widget->IsVisible())
		{
			return;
		}
		FTween& T = Tweens.AddDefaulted_GetRef();
		T.Widget = Widget;
		T.Owner = Menu;
		T.Kind = EKind::Move;
		T.Delay = Delay;
		T.Duration = CloseDuration - Delay;
		T.FromT = Widget->GetRenderTransform().Translation;
		T.ToT = T.FromT + Offset;
		T.FromO = Widget->GetRenderOpacity();
		T.ToO = 0.f;
	};
	Leave(TEXT("MajorMenuColumn"), FVector2D(0.f, 12.f), 0.f);
	for (const TCHAR* Name : { TEXT("PageTitleText"), TEXT("HeaderGoldCoin"), TEXT("HeaderGoldText"), TEXT("CloseButton"), TEXT("KeyHintRow") })
	{
		Leave(Name, FVector2D(0.f, 8.f), 0.f);
	}
	static const TCHAR* Rail[] = { TEXT("RailPanel"), TEXT("RailCaption"), TEXT("NavSlider"), TEXT("NavCharacterButton"), TEXT("NavEquipmentButton"),
		TEXT("NavInventoryButton"), TEXT("NavStatsButton"), TEXT("NavTraitsButton"), TEXT("NavNaiaButton") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rail); ++Index)
	{
		Leave(Rail[Index], FVector2D(-24.f, 0.f), Index < 3 ? 0.04f : 0.01f * (Index - 3));
	}
	FClose& Close = Closes.AddDefaulted_GetRef();
	Close.Menu = Menu;
	Close.Completion = MoveTemp(Completion);
	EnsureTick();
	return true;
}

void Stop(UUserWidget* Menu)
{
	for (int32 Index = Tweens.Num() - 1; Index >= 0; --Index)
	{
		FTween& T = Tweens[Index];
		if (T.Owner.Get() != Menu)
		{
			continue; // short self-owned feedback (pulse, flash) settles on its own
		}
		Apply(T, 1.f);
		Tweens.RemoveAt(Index);
	}
	Closes.RemoveAll([Menu](const FClose& Close) { return Close.Menu.Get() == Menu || !Close.Menu.IsValid(); });
	for (auto It = Rests.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	for (auto It = Displaced.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}
	StopTickIfIdle();
}

void Pulse(UWidget* Widget, float FromScale, float Duration)
{
	if (!Widget || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const FVector2D Rest = RestOf(Widget, EKind::Scale).Scale;
	ScaleTo(Widget, Rest * FromScale, Rest, Duration);
	EnsureTick();
}

void FlashText(UTextBlock* Text, const FLinearColor& Color, float Duration)
{
	if (!Text || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const FSlateColor& Base = TextRests.Contains(Text) ? TextRests[Text] : TextRests.Add(Text, Text->GetColorAndOpacity());
	if (const int32 Index = FindTween(Text, EKind::Color); Index != INDEX_NONE)
	{
		Tweens.RemoveAt(Index);
	}
	FTween& T = Tweens.AddDefaulted_GetRef();
	T.Widget = Text;
	T.Kind = EKind::Color;
	T.Duration = Duration;
	T.FromC = Color;
	T.ToC = Base.GetSpecifiedColor();
	Apply(T, 0.f);
	EnsureTick();
}

void Lift(UWidget* Widget)
{
	if (!Widget || !FSlateApplication::IsInitialized())
	{
		return;
	}
	Widget->SetRenderTransformAngle(LiftAngle);
	const FVector2D Rest = RestOf(Widget, EKind::Scale).Scale;
	ScaleTo(Widget, Rest, Rest * LiftScale, 0.14f, true);
	Tweens.Last().bBack = true;
	EnsureTick();
}

void TrackDropTargets(UWidget* Owner, TArray<TWeakObjectPtr<UWidget>> Targets, TFunction<void(UWidget*, UWidget*)> OnHot)
{
	const int32 Existing = Drops.IndexOfByPredicate([Owner](const FDrop& Drop) { return Drop.Owner.Get() == Owner; });
	if (Existing != INDEX_NONE)
	{
		if (UWidget* Hot = Drops[Existing].Hot.Get())
		{
			ScaleTo(Hot, Hot->GetRenderTransform().Scale, RestOf(Hot, EKind::Scale).Scale, 0.1f);
			if (Drops[Existing].OnHot) Drops[Existing].OnHot(Hot, nullptr);
		}
		Drops.RemoveAt(Existing);
	}
	if (Owner && !Targets.IsEmpty() && FSlateApplication::IsInitialized())
	{
		FDrop& Drop = Drops.AddDefaulted_GetRef();
		Drop.Owner = Owner;
		Drop.Targets = MoveTemp(Targets);
		Drop.OnHot = MoveTemp(OnHot);
	}
	EnsureTick();
}

float TrackValue(const UObject* Owner, FName Key, float Value)
{
	if (!Owner) return 0.f;
	TMap<FName, float>& Values = TrackedFor(TrackedValues, Owner);
	const float* Previous = Values.Find(Key);
	const float Delta = Previous ? Value - *Previous : 0.f;
	Values.Add(Key, Value);
	return Delta;
}

bool TrackName(const UObject* Owner, FName Key, FName Value)
{
	if (!Owner) return false;
	TMap<FName, FName>& Names = TrackedFor(TrackedNames, Owner);
	const FName* Previous = Names.Find(Key);
	const bool bChanged = Previous && *Previous != Value;
	Names.Add(Key, Value);
	return bChanged;
}

void FlashOnChange(const UObject* Owner, UTextBlock* Text, float Value)
{
	if (!Text) return;
	const float Delta = TrackValue(Owner, Text->GetFName(), Value);
	if (FMath::Abs(Delta) > KINDA_SMALL_NUMBER)
	{
		FlashText(Text, Delta > 0.f ? GainColor() : LossColor());
	}
}

FLinearColor GainColor() { return FLinearColor::FromSRGBColor(FColor(0x8F, 0xD3, 0xA4)); }
FLinearColor LossColor() { return FLinearColor::FromSRGBColor(FColor(0xF0, 0x9A, 0x8C)); }
}
