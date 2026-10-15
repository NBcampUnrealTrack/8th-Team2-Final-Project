#include "CharacterProgression/UI/Stats/NUProgressionDebugWidget.h"

#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

void UNUProgressionDebugWidget::InitializeProgression(UNUProgressionComponent* InProgression)
{
	Progression = InProgression;
}

TSharedRef<SWidget> UNUProgressionDebugWidget::RebuildWidget()
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.025f, 0.9f));
		Panel->SetPadding(FMargin(14.f, 10.f));
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		StatsText = WidgetTree->ConstructWidget<UTextBlock>();
		StatsText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		StatsText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14));
		StatsText->SetAutoWrapText(false);
		StatsText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Panel->SetContent(StatsText);

		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
		PanelSlot->SetPosition(FVector2D(24.f, 80.f));
		PanelSlot->SetAutoSize(true);

		WidgetTree->RootWidget = Root;
	}

	return Super::RebuildWidget();
}

void UNUProgressionDebugWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Progression.IsValid())
	{
		return;
	}

	Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	Progression->OnProgressionChanged.AddDynamic(this, &ThisClass::HandleProgressionChanged);

	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Progression->GetOwner());
	ASC = PlayerState != nullptr ? PlayerState->GetNUASC() : nullptr;

	BindAttributeDelegates();
	RefreshText();
}

void UNUProgressionDebugWidget::NativeDestruct()
{
	if (Progression.IsValid())
	{
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	}

	UnbindAttributeDelegates();
	ASC.Reset();
	bRefreshQueued = false;

	Super::NativeDestruct();
}

void UNUProgressionDebugWidget::HandleProgressionChanged()
{
	RequestRefresh();
}

void UNUProgressionDebugWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	RequestRefresh();
}

void UNUProgressionDebugWidget::RequestRefresh()
{
	if (bRefreshQueued || GetWorld() == nullptr)
	{
		return;
	}

	bRefreshQueued = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bRefreshQueued = false;
		RefreshText();
	}));
}

void UNUProgressionDebugWidget::RefreshText()
{
	if (StatsText == nullptr || !Progression.IsValid())
	{
		return;
	}

	const FNUProgressionState& State = Progression->GetState();
	const FNUPrimaryStats& Primary = State.PrimaryStats;
	const int64 RequiredExperience = Progression->GetRequiredExperienceToNext();

	float Health = 0.f;
	float MaxHealth = 0.f;
	float SP = 0.f;
	float MaxSP = 0.f;
	float AttackPower = 0.f;
	float MagicAttackPower = 0.f;
	float Defense = 0.f;
	float Speed = 0.f;
	float CriticalRate = 0.f;
	float CriticalDamage = 0.f;

	if (ASC.IsValid())
	{
		if (const UNUVitalAttributeSet* Vital = ASC->GetVitalAttributeSet())
		{
			Health = Vital->GetHealth();
			MaxHealth = Vital->GetMaxHealth();
			SP = Vital->GetSP();
			MaxSP = Vital->GetMaxSP();
		}

		if (const UNUCombatAttributeSet* Combat = ASC->GetCombatAttributeSet())
		{
			AttackPower = Combat->GetAttackPower();
			MagicAttackPower = Combat->GetMagicAttackPower();
			Defense = Combat->GetDefense();
			Speed = Combat->GetSpeed();
			CriticalRate = Combat->GetCriticalRate();
			CriticalDamage = Combat->GetCriticalDamage();
		}
	}

	StatsText->SetText(FText::FromString(FString::Printf(
		TEXT(
			"[ Progression Debug ]\n\n"
			"Level: %d\n"
			"EXP: %lld / %lld\n"
			"Progress: %.2f%%\n\n"
			"[ Primary ]\n"
			"STR: %d\n"
			"DEX: %d\n"
			"INT: %d\n"
			"LUK: %d\n"
			"VIT: %d\n\n"
			"[ Secondary ]\n"
			"HP: %.2f / %.2f\n"
			"SP: %.2f / %.2f\n"
			"Attack: %.2f\n"
			"Magic Attack: %.2f\n"
			"Defense: %.2f\n"
			"Speed: %.2f\n"
			"Critical Rate: %.2f%%\n"
			"Critical Damage: %.2f%%"),
		State.Level,
		State.ExperienceIntoLevel,
		RequiredExperience,
		Progression->GetExperienceProgressRatio() * 100.f,
		Primary.Strength,
		Primary.Dexterity,
		Primary.Intelligence,
		Primary.Luck,
		Primary.Vitality,
		Health,
		MaxHealth,
		SP,
		MaxSP,
		AttackPower,
		MagicAttackPower,
		Defense,
		Speed,
		// 어트리뷰트는 0~1 확률과 배율이라 표기할 때 퍼센트로 환산한다
		CriticalRate * 100.f,
		CriticalDamage * 100.f))
	);
}

void UNUProgressionDebugWidget::BindAttributeDelegates()
{
	if (!ASC.IsValid())
	{
		return;
	}

	for (const FGameplayAttribute& Attribute : GetWatchedAttributes())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged);
	}
}

void UNUProgressionDebugWidget::UnbindAttributeDelegates()
{
	if (!ASC.IsValid())
	{
		return;
	}

	for (const FGameplayAttribute& Attribute : GetWatchedAttributes())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
	}
}

TArray<FGameplayAttribute> UNUProgressionDebugWidget::GetWatchedAttributes()
{
	return
	{
		UNUVitalAttributeSet::GetHealthAttribute(),
		UNUVitalAttributeSet::GetMaxHealthAttribute(),
		UNUVitalAttributeSet::GetSPAttribute(),
		UNUVitalAttributeSet::GetMaxSPAttribute(),
		UNUCombatAttributeSet::GetAttackPowerAttribute(),
		UNUCombatAttributeSet::GetMagicAttackPowerAttribute(),
		UNUCombatAttributeSet::GetDefenseAttribute(),
		UNUCombatAttributeSet::GetSpeedAttribute(),
		UNUCombatAttributeSet::GetCriticalRateAttribute(),
		UNUCombatAttributeSet::GetCriticalDamageAttribute()
	};
}
