#include "CharacterProgression/UI/Job/NUAdvancementSelectionWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUJobTypes.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterProgression/UI/Job/NUJobSelectionEntryButton.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Styling/CoreStyle.h"

namespace
{
	UTextBlock* MakeAdvancementText(UWidgetTree* WidgetTree, const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>();
		Result->SetText(Text);
		Result->SetFont(FCoreStyle::GetDefaultFontStyle(Size >= 24 ? "Bold" : "Regular", Size));
		Result->SetColorAndOpacity(FSlateColor(Color));
		return Result;
	}
}

void UNUAdvancementSelectionWidget::InitializeSelection(
	int32 InNodeId,
	UNUJobComponent* InJobComponent,
	UNUProgressionComponent* InProgression,
	FSimpleDelegate InCloseRequested)
{
	NodeId = InNodeId;
	JobComponent = InJobComponent;
	Progression = InProgression;
	SetCloseRequested(MoveTemp(InCloseRequested));
}

void UNUAdvancementSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCanvasPanelSlot* WindowSlot = WindowFrame != nullptr
		? Cast<UCanvasPanelSlot>(WindowFrame->Slot)
		: nullptr)
	{
		WindowSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		WindowSlot->SetOffsets(FMargin(0.f));
		WindowSlot->SetAlignment(FVector2D::ZeroVector);
	}
	TitleText->SetText(NSLOCTEXT("NUAdvancement", "SelectionTitle", "2차 전직을 선택하세요"));
	LevelStatusText->SetVisibility(ESlateVisibility::Collapsed);
	KeyHintText->SetVisibility(ESlateVisibility::Collapsed);
	ConfirmButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	ConfirmButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
	if (JobComponent.IsValid())
	{
		JobComponent->OnJobChanged.RemoveDynamic(this, &ThisClass::HandleJobChanged);
		JobComponent->OnJobChanged.AddDynamic(this, &ThisClass::HandleJobChanged);
	}
	if (Progression.IsValid())
	{
		Progression->OnProgressionRequestStateChanged.RemoveDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
		Progression->OnProgressionRequestStateChanged.AddDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
		Progression->OnProgressionRequestFailed.RemoveDynamic(this, &ThisClass::HandleProgressionRequestFailed);
		Progression->OnProgressionRequestFailed.AddDynamic(this, &ThisClass::HandleProgressionRequestFailed);
	}

	RebuildEntries();
	RefreshState();
}

void UNUAdvancementSelectionWidget::NativeDestruct()
{
	ConfirmButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	if (JobComponent.IsValid())
	{
		JobComponent->OnJobChanged.RemoveDynamic(this, &ThisClass::HandleJobChanged);
	}
	if (Progression.IsValid())
	{
		Progression->OnProgressionRequestStateChanged.RemoveDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
		Progression->OnProgressionRequestFailed.RemoveDynamic(this, &ThisClass::HandleProgressionRequestFailed);
	}
	AdvancementButtons.Reset();
	AdvancementEntries.Reset();
	SelectedAdvancementId = 0;

	Super::NativeDestruct();
}

bool UNUAdvancementSelectionWidget::CanDragWindow() const
{
	return false;
}

void UNUAdvancementSelectionWidget::RebuildEntries()
{
	AdvancementEntries.Reset();
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Data == nullptr)
	{
		BP_OnAdvancementEntriesChanged(AdvancementEntries);
		return;
	}

	for (const FNUProgressionNodeRewardRow* Reward : Data->GetRewardsByNode(NodeId))
	{
		if (Reward == nullptr || Reward->RewardType != ENUProgressionNodeRewardType::Advancement)
		{
			continue;
		}
		const FNUJobAdvancementDefinitionRow* Advancement = Data->FindAdvancement(Reward->ChoiceAdvancementId);
		if (Advancement == nullptr || AdvancementEntries.ContainsByPredicate([Advancement](const FNUAdvancementSelectionViewData& Entry)
		{
			return Entry.AdvancementId == Advancement->AdvancementId;
		}))
		{
			continue;
		}

		FNUAdvancementSelectionViewData& Entry = AdvancementEntries.AddDefaulted_GetRef();
		Entry.AdvancementId = Advancement->AdvancementId;
		Entry.DisplayName = Advancement->DisplayName;
		Entry.Description = BuildChoiceDescription(Advancement->AdvancementId, Advancement->Description);
		Entry.AccentColor = GetAdvancementAccent(Advancement->AdvancementId);
	}

	BP_OnAdvancementEntriesChanged(AdvancementEntries);
}

void UNUAdvancementSelectionWidget::BP_OnAdvancementEntriesChanged_Implementation(
	const TArray<FNUAdvancementSelectionViewData>& InEntries)
{
	JobListPanel->ClearChildren();
	AdvancementButtons.Reset();
	for (const FNUAdvancementSelectionViewData& Entry : InEntries)
	{
		UWidget* Card = BuildDefaultCard(Entry);
		if (UHorizontalBoxSlot* BoxSlot = JobListPanel->AddChildToHorizontalBox(Card))
		{
			BoxSlot->SetPadding(FMargin(14.f));
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

UWidget* UNUAdvancementSelectionWidget::BuildDefaultCard(const FNUAdvancementSelectionViewData& Entry)
{
	UNUJobSelectionEntryButton* Button = WidgetTree->ConstructWidget<UNUJobSelectionEntryButton>();
	Button->SetJobId(Entry.AdvancementId);
	Button->OnEntryClicked.BindUObject(this, &ThisClass::SelectAdvancement);
	Button->SetBackgroundColor(FLinearColor(0.12f, 0.135f, 0.16f, 1.f));
	AdvancementButtons.Add(Entry.AdvancementId, Button);

	UTextBlock* NameText = MakeAdvancementText(WidgetTree, Entry.DisplayName, 26, FLinearColor(0.94f, 0.94f, 0.96f));
	NameText->SetJustification(ETextJustify::Center);
	Button->AddChild(NameText);
	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(NameText->Slot))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
	CardSize->SetWidthOverride(360.f);
	CardSize->SetHeightOverride(360.f);
	CardSize->AddChild(Button);
	return CardSize;
}

void UNUAdvancementSelectionWidget::SelectAdvancement(int32 AdvancementId)
{
	if (!JobComponent.IsValid() || JobComponent->GetAdvancementId() > 0)
	{
		return;
	}
	SelectedAdvancementId = AdvancementId;
	RefreshState();
}

void UNUAdvancementSelectionWidget::RefreshState()
{
	const FNUAdvancementSelectionViewData* SelectedEntry = FindSelectedEntry();
	SelectedJobText->SetText(SelectedEntry != nullptr
		? FText::Format(NSLOCTEXT("NUAdvancement", "SelectedAdvancement", "선택: {0}"), SelectedEntry->DisplayName)
		: NSLOCTEXT("NUAdvancement", "SelectPrompt", "2차 전직 갈래를 선택하세요"));
	DescriptionText->SetText(SelectedEntry != nullptr
		? SelectedEntry->Description
		: NSLOCTEXT("NUAdvancement", "SelectionGuide", "두 전직 갈래 중 하나를 선택하세요. 선택한 전직은 변경할 수 없습니다."));
	ConfirmButton->SetIsEnabled(
		SelectedEntry != nullptr
		&& JobComponent.IsValid()
		&& JobComponent->GetAdvancementId() <= 0
		&& Progression.IsValid()
		&& Progression->HasServerProgressionSnapshot()
		&& !Progression->IsServerProgressionRequestPending()
		&& Progression->GetNodeState(NodeId) == ENUProgressionNodeState::Available);

	for (const TPair<int32, TObjectPtr<UNUJobSelectionEntryButton>>& Pair : AdvancementButtons)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->SetBackgroundColor(Pair.Key == SelectedAdvancementId
				? GetAdvancementAccent(Pair.Key) * 0.65f
				: FLinearColor(0.12f, 0.135f, 0.16f, 1.f));
		}
	}
}

void UNUAdvancementSelectionWidget::HandleConfirmClicked()
{
	const FNUAdvancementSelectionViewData* SelectedEntry = FindSelectedEntry();
	if (SelectedEntry == nullptr
		|| !Progression.IsValid()
		|| Progression->GetNodeState(NodeId) != ENUProgressionNodeState::Available)
	{
		return;
	}
	if (!Progression->RequestNodeAcquire(NodeId, SelectedEntry->AdvancementId))
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
		{
			UI->ShowNotice(NSLOCTEXT("NUAdvancement", "RequestUnavailable", "현재 전직을 요청할 수 없습니다."));
		}
	}
	RefreshState();
}

void UNUAdvancementSelectionWidget::HandleProgressionRequestStateChanged()
{
	RefreshState();
}

void UNUAdvancementSelectionWidget::HandleProgressionRequestFailed(int32 ResultCode, const FString& Message)
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(FText::FromString(Message));
	}
	RefreshState();
}

void UNUAdvancementSelectionWidget::HandleJobChanged()
{
	if (!JobComponent.IsValid() || JobComponent->GetAdvancementId() != SelectedAdvancementId)
	{
		RefreshState();
		return;
	}
	const FNUAdvancementSelectionViewData* SelectedEntry = FindSelectedEntry();
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()); UI != nullptr && SelectedEntry != nullptr)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Field::JobChange);
		UI->ShowNotice(FText::Format(
			NSLOCTEXT("NUAdvancement", "SelectionComplete", "{0} 전직 완료 · 전직 스킬 3종 획득"),
			SelectedEntry->DisplayName));
	}
	RequestClose();
}

const FNUAdvancementSelectionViewData* UNUAdvancementSelectionWidget::FindSelectedEntry() const
{
	return AdvancementEntries.FindByPredicate([this](const FNUAdvancementSelectionViewData& Entry)
	{
		return Entry.AdvancementId == SelectedAdvancementId;
	});
}

FText UNUAdvancementSelectionWidget::BuildChoiceDescription(
	int32 AdvancementId,
	const FText& AdvancementDescription) const
{
	TArray<FText> Lines;
	if (!AdvancementDescription.IsEmpty())
	{
		Lines.Add(AdvancementDescription);
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* ProgressionData = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	const UNUBattleDataSubsystem* BattleData = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	if (ProgressionData == nullptr || BattleData == nullptr)
	{
		return FText::Join(FText::FromString(TEXT("\n\n")), Lines);
	}

	for (const FNUProgressionNodeRewardRow* Reward : ProgressionData->GetRewardsByNode(NodeId))
	{
		if (Reward == nullptr
			|| Reward->ChoiceAdvancementId != AdvancementId
			|| (Reward->RewardType != ENUProgressionNodeRewardType::ActiveSkill
				&& Reward->RewardType != ENUProgressionNodeRewardType::PassiveSkill))
		{
			continue;
		}
		const FNUJobSkillDefinition* Skill = BattleData->FindJobSkillDefinition(Reward->RewardId);
		if (Skill == nullptr)
		{
			continue;
		}
		const FText SkillName = Skill->Name_KR.IsEmpty() ? FText::FromName(Reward->RewardKey) : Skill->Name_KR;
		const FText SkillType = Skill->SkillType.IsNone() ? FText::GetEmpty() : FText::FromName(Skill->SkillType);
		const FText SPCost = Skill->SP_Cost > 0
			? FText::Format(NSLOCTEXT("NUAdvancement", "SkillSPCost", " · SP {0}"), FText::AsNumber(Skill->SP_Cost))
			: FText::GetEmpty();
		Lines.Add(FText::Format(
			NSLOCTEXT("NUAdvancement", "SkillDescription", "[{0}] · {1}{2}\n{3}"),
			SkillName,
			SkillType,
			SPCost,
			Skill->Description));
	}
	return FText::Join(FText::FromString(TEXT("\n\n")), Lines);
}

FLinearColor UNUAdvancementSelectionWidget::GetAdvancementAccent(int32 AdvancementId)
{
	return AdvancementId > 0 && AdvancementId % 2 == 1
		? FLinearColor(0.92f, 0.36f, 0.18f, 1.f)
		: FLinearColor(0.24f, 0.7f, 0.92f, 1.f);
}
