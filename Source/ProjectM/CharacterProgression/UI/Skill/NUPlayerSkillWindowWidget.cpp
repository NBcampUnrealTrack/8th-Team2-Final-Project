#include "CharacterProgression/UI/Skill/NUPlayerSkillWindowWidget.h"

#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

void UNUPlayerSkillWindowWidget::InitializeSkillWindow(
	UNUPlayerSkillComponent* InSkillComponent,
	FSimpleDelegate InCloseRequested)
{
	SkillComponent = InSkillComponent;
	SetCloseRequested(MoveTemp(InCloseRequested));
}

void UNUPlayerSkillWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillsChanged.RemoveDynamic(this, &ThisClass::HandleSkillsChanged);
		SkillComponent->OnSkillsChanged.AddDynamic(this, &ThisClass::HandleSkillsChanged);
	}

	RefreshSkillEntries();
}

void UNUPlayerSkillWindowWidget::NativeDestruct()
{
	if (SkillComponent.IsValid())
	{
		SkillComponent->OnSkillsChanged.RemoveDynamic(this, &ThisClass::HandleSkillsChanged);
	}
	SkillComponent.Reset();

	Super::NativeDestruct();
}

void UNUPlayerSkillWindowWidget::RefreshSkillEntries()
{
	SkillEntries.Reset();
	if (!SkillComponent.IsValid())
	{
		BP_OnSkillEntriesChanged(SkillEntries);
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* BattleData = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	TSet<int32> AddedSkillIds;
	const auto AddOwnedSkill = [this, BattleData, &AddedSkillIds](
		const auto& OwnedSkill,
		const FText& FallbackSkillTypeText)
	{
		if (OwnedSkill.SkillId <= 0 || AddedSkillIds.Contains(OwnedSkill.SkillId))
		{
			return;
		}
		AddedSkillIds.Add(OwnedSkill.SkillId);

		FNUPlayerSkillViewData& ViewData = SkillEntries.AddDefaulted_GetRef();
		ViewData.SkillId = OwnedSkill.SkillId;
		ViewData.SkillTypeText = FallbackSkillTypeText;

		const FNUJobSkillDefinition* Definition = BattleData != nullptr
			? BattleData->FindJobSkillDefinition(OwnedSkill.SkillId)
			: nullptr;
		if (Definition != nullptr)
		{
			ViewData.SkillKey = Definition->Skill_Key;
			ViewData.SkillTypeText = !Definition->SkillType.IsNone()
				? FText::FromName(Definition->SkillType)
				: FallbackSkillTypeText;
			ViewData.DisplayName = !Definition->Name_KR.IsEmpty()
				? Definition->Name_KR
				: FText::FromName(Definition->Skill_Key);
			ViewData.Description = !Definition->Description.IsEmpty()
				? Definition->Description
				: NSLOCTEXT("NUPlayerSkill", "EmptyJobSkillDescription", "등록된 스킬 설명이 없습니다.");
			ViewData.SPCost = Definition->SP_Cost;
		}
		else
		{
			ViewData.DisplayName = FText::Format(
				NSLOCTEXT("NUPlayerSkill", "OwnedSkillId", "스킬 {0}"),
				FText::AsNumber(ViewData.SkillId));
			ViewData.Description = NSLOCTEXT(
				"NUPlayerSkill",
				"MissingJobSkillDefinition",
				"직업 스킬 표시 정의가 아직 없습니다.");
		}
	};

	for (const FNUOwnedActiveSkill& OwnedSkill : SkillComponent->GetState().OwnedActiveSkills)
	{
		AddOwnedSkill(
			OwnedSkill,
			NSLOCTEXT("NUPlayerSkill", "ActiveSkillLabel", "액티브"));
	}
	for (const FNUOwnedPassiveSkill& OwnedSkill : SkillComponent->GetState().OwnedPassiveSkills)
	{
		AddOwnedSkill(
			OwnedSkill,
			NSLOCTEXT("NUPlayerSkill", "PassiveSkillLabel", "패시브"));
	}

	SkillEntries.Sort([](const FNUPlayerSkillViewData& A, const FNUPlayerSkillViewData& B)
	{
		return A.SkillId < B.SkillId;
	});

	BP_OnSkillEntriesChanged(SkillEntries);
}

void UNUPlayerSkillWindowWidget::HandleSkillsChanged()
{
	RefreshSkillEntries();
}

void UNUPlayerSkillWindowWidget::BP_OnSkillEntriesChanged_Implementation(const TArray<FNUPlayerSkillViewData>& InSkillEntries)
{
	if (SkillListPanel == nullptr)
	{
		return;
	}

	SkillListPanel->ClearChildren();
	SkillListPanel->SetVisibility(ESlateVisibility::Visible);
	if (EmptyStateText != nullptr)
	{
		EmptyStateText->SetVisibility(InSkillEntries.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (InSkillEntries.IsEmpty())
	{
		if (EmptyStateText != nullptr)
		{
			SkillListPanel->AddChildToVerticalBox(EmptyStateText)->SetPadding(FMargin(12.0f, 48.0f));
		}
		return;
	}

	for (const FNUPlayerSkillViewData& Entry : InSkillEntries)
	{
		UBorder* SkillRow = WidgetTree != nullptr
			? WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass())
			: nullptr;
		UTextBlock* SkillText = WidgetTree != nullptr
			? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
			: nullptr;
		if (SkillRow == nullptr || SkillText == nullptr)
		{
			continue;
		}

		const FText SkillName = Entry.DisplayName.IsEmpty()
			? NSLOCTEXT("NUPlayerSkill", "FallbackSkillName", "미정")
			: Entry.DisplayName;
		SkillText->SetText(FText::Format(
			NSLOCTEXT("NUPlayerSkill", "SkillTypeAndName", "[{0}]  {1}"),
			Entry.SkillTypeText,
			SkillName));
		SkillText->SetAutoWrapText(true);
		SkillText->SetColorAndOpacity(FSlateColor(FLinearColor(0.86f, 0.91f, 1.0f, 1.0f)));
		SkillText->SetFontSize(18.0f);

		SkillRow->SetBrushColor(FLinearColor(0.04f, 0.06f, 0.1f, 0.72f));
		SkillRow->SetPadding(FMargin(12.0f, 10.0f));
		SkillRow->AddChild(SkillText);
		SkillRow->SetToolTip(BuildSkillTooltip(Entry));
		SkillListPanel->AddChildToVerticalBox(SkillRow)->SetPadding(FMargin(16.0f, 5.0f));
	}
}

UWidget* UNUPlayerSkillWindowWidget::BuildSkillTooltip(const FNUPlayerSkillViewData& Entry) const
{
	if (WidgetTree == nullptr)
	{
		return nullptr;
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.97f));
	Frame->SetPadding(FMargin(12.0f, 10.0f));

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Frame->AddChild(Box);

	const auto AddLine = [this, Box](
		const FText& Text,
		const FLinearColor& Color,
		int32 FontSize,
		const char* Typeface,
		float TopPadding,
		bool bWrap)
	{
		if (Text.IsEmpty())
		{
			return;
		}

		UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
		Line->SetText(Text);
		Line->SetColorAndOpacity(FSlateColor(Color));
		Line->SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, FontSize));
		if (bWrap)
		{
			Line->SetAutoWrapText(true);
			Line->SetWrapTextAt(360.0f);
		}
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Box->AddChild(Line)))
		{
			Slot->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
		}
	};

	AddLine(
		Entry.DisplayName,
		FLinearColor(1.0f, 0.85f, 0.4f, 1.0f),
		15,
		"Bold",
		0.0f,
		false);
	AddLine(
		Entry.SkillTypeText,
		FLinearColor(0.62f, 0.62f, 0.7f, 1.0f),
		11,
		"Regular",
		2.0f,
		false);
	if (Entry.SPCost > 0)
	{
		AddLine(
			FText::Format(
				NSLOCTEXT("NUPlayerSkill", "TooltipSPCost", "SP 소모 {0}"),
				FText::AsNumber(Entry.SPCost)),
			FLinearColor(0.55f, 0.8f, 1.0f, 1.0f),
			11,
			"Regular",
			4.0f,
			false);
	}
	AddLine(
		Entry.Description,
		FLinearColor(0.85f, 0.85f, 0.9f, 1.0f),
		12,
		"Regular",
		8.0f,
		true);

	return Frame;
}
