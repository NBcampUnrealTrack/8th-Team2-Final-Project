#include "Quest/UI/NUQuestJournalWidget.h"

#include "CharacterState/NUPlayerState.h"
#include "Inventory/NUInventoryComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/UI/NUQuestJournalListItem.h"
#include "Quest/UI/NUQuestJournalPresentation.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ProgressBar.h"
#include "Components/ListView.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

// 카테고리/상태 필터 버튼 8개 전용 — 순정 Button(레거시), UNU_CloseButtonWidget, UNU_CommonButtonWidget
// (WBP_CommonButton, 신규 공용 부품) 셋 다 지원한다. AddDynamic/RemoveDynamic은 매크로라 컴파일타임
// 함수 리터럴이 필요해서 함수로 감쌀 수 없으므로 여기서만 매크로로 반복한다.
#define NU_BIND_FILTER_BUTTON(WidgetName, Handler) \
	do \
	{ \
		UWidget* FilterWidget_##Handler = GetWidgetFromName(TEXT(WidgetName)); \
		if (UButton* NativeButton_##Handler = Cast<UButton>(FilterWidget_##Handler)) \
		{ \
			NativeButton_##Handler->OnClicked.RemoveDynamic(this, &ThisClass::Handler); \
			NativeButton_##Handler->OnClicked.AddDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CloseButtonWidget* CloseStyleButton_##Handler = Cast<UNU_CloseButtonWidget>(FilterWidget_##Handler)) \
		{ \
			CloseStyleButton_##Handler->OnCloseClicked.RemoveDynamic(this, &ThisClass::Handler); \
			CloseStyleButton_##Handler->OnCloseClicked.AddDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CommonButtonWidget* CommonButton_##Handler = Cast<UNU_CommonButtonWidget>(FilterWidget_##Handler)) \
		{ \
			CommonButton_##Handler->OnCommonClicked.RemoveDynamic(this, &ThisClass::Handler); \
			CommonButton_##Handler->OnCommonClicked.AddDynamic(this, &ThisClass::Handler); \
		} \
	} while (0)

#define NU_UNBIND_FILTER_BUTTON(WidgetName, Handler) \
	do \
	{ \
		UWidget* FilterWidget_##Handler = GetWidgetFromName(TEXT(WidgetName)); \
		if (UButton* NativeButton_##Handler = Cast<UButton>(FilterWidget_##Handler)) \
		{ \
			NativeButton_##Handler->OnClicked.RemoveDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CloseButtonWidget* CloseStyleButton_##Handler = Cast<UNU_CloseButtonWidget>(FilterWidget_##Handler)) \
		{ \
			CloseStyleButton_##Handler->OnCloseClicked.RemoveDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CommonButtonWidget* CommonButton_##Handler = Cast<UNU_CommonButtonWidget>(FilterWidget_##Handler)) \
		{ \
			CommonButton_##Handler->OnCommonClicked.RemoveDynamic(this, &ThisClass::Handler); \
		} \
	} while (0)

namespace NUQuestJournal
{
	// Optional row templates keep their appearance editable in WBP. Older journal
	// layouts continue using the native multiline TextBlocks when templates are absent.
	bool PopulateObjectiveRows(UUserWidget& Owner, const UNUQuestDefinition& Definition,
		const FNUQuestProgressState& Progress)
	{
		UVerticalBox* Rows = Cast<UVerticalBox>(Owner.GetWidgetFromName(TEXT("QR_ObjectiveRows")));
		UUserWidget* Template = Cast<UUserWidget>(Owner.GetWidgetFromName(TEXT("QR_ObjectiveTemplate")));
		if (!Rows || !Template) { return false; }
		Rows->ClearChildren();
		Rows->SetVisibility(ESlateVisibility::Collapsed);
		if (Progress.QuestState == ENUQuestState::Locked) { return false; }
		for (const FNUQuestObjectiveSpec& Objective : Definition.Objectives)
		{
			if (!Progress.IsObjectiveOpenForDisplay(Definition, Objective)) { continue; }
			UUserWidget* Row = CreateWidget<UUserWidget>(Owner.GetOwningPlayer(), Template->GetClass());
			UTextBlock* Label = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("ObjectiveLabel"))) : nullptr;
			UTextBlock* Counter = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("ObjectiveCount"))) : nullptr;
			UCheckBox* Check = Row ? Cast<UCheckBox>(Row->GetWidgetFromName(TEXT("ObjectiveCheck"))) : nullptr;
			if (!Label || !Counter || !Check) { Rows->ClearChildren(); return false; }
			const FNUQuestObjectiveProgress* Current = Progress.FindObjectiveProgress(Objective.ObjectiveId);
			const int32 Count = FMath::Clamp(Current ? Current->CurrentCount : 0, 0, Objective.RequiredCount);
			FText Description = Objective.Description.IsEmpty()
				? NSLOCTEXT("NUQuest", "UnnamedObjective", "목표 확인하기") : Objective.Description;
			if (Objective.bOptional)
			{
				Description = FText::Format(NSLOCTEXT("NUQuestJournal", "OptionalObjectiveRow", "{0} · 선택"), Description);
			}
			Label->SetText(Description);
			Counter->SetText(FText::Format(NSLOCTEXT("NUQuestJournal", "ObjectiveCounter", "{0} / {1}"),
				FText::AsNumber(Count), FText::AsNumber(Objective.RequiredCount)));
			Check->SetIsChecked(Current && Current->IsComplete(Objective.RequiredCount));
			if (UProgressBar* Bar = Cast<UProgressBar>(Row->GetWidgetFromName(TEXT("ObjectiveProgress"))))
			{
				Bar->SetPercent(Objective.RequiredCount > 0 ? static_cast<float>(Count) / Objective.RequiredCount : 0.f);
			}
			Check->SetVisibility(ESlateVisibility::HitTestInvisible);
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
			Rows->AddChildToVerticalBox(Row);
		}
		const bool bHasRows = Rows->GetChildrenCount() > 0;
		Rows->SetVisibility(bHasRows ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		return bHasRows;
	}

	bool PopulateRewardRows(UUserWidget& Owner, const UNUQuestDefinition& Definition)
	{
		UWrapBox* Rows = Cast<UWrapBox>(Owner.GetWidgetFromName(TEXT("QR_RewardRows")));
		UUserWidget* Template = Cast<UUserWidget>(Owner.GetWidgetFromName(TEXT("QR_RewardTemplate")));
		if (!Rows || !Template) { return false; }
		Rows->ClearChildren();
		Rows->SetVisibility(ESlateVisibility::Collapsed);
		for (const FNUQuestRewardSpec& Reward : Definition.Rewards)
		{
			UUserWidget* Row = CreateWidget<UUserWidget>(Owner.GetOwningPlayer(), Template->GetClass());
			UTextBlock* Label = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("RewardLabel"))) : nullptr;
			UTextBlock* Amount = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("RewardAmount"))) : nullptr;
			UWidgetSwitcher* Icon = Row ? Cast<UWidgetSwitcher>(Row->GetWidgetFromName(TEXT("RewardIconSwitcher"))) : nullptr;
			if (!Label || !Amount || !Icon) { Rows->ClearChildren(); return false; }
			const bool bExperience = Reward.RewardType == NU::Quest::Reward::Experience;
			const bool bGold = Reward.RewardType == NU::Quest::Reward::Gold;
			const bool bItem = Reward.RewardType == NU::Quest::Reward::Item;
			Label->SetText(bExperience ? NSLOCTEXT("NUQuestJournal", "ExperienceLabel", "경험치")
				: bGold ? NSLOCTEXT("NUQuestJournal", "GoldLabel", "골드")
				: bItem ? (Reward.DisplayName.IsEmpty() ? NSLOCTEXT("NUQuest", "ItemRewardFallback", "아이템") : Reward.DisplayName)
				: Reward.GetDisplayText());
			Amount->SetText(bExperience || bGold || bItem ? FText::AsNumber(Reward.Amount) : FText::GetEmpty());
			Amount->SetVisibility(bExperience || bGold || bItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Icon->SetActiveWidgetIndex(bExperience ? 0 : bGold ? 1 : 2);
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
			Rows->AddChildToWrapBox(Row);
		}
		const bool bHasRows = Rows->GetChildrenCount() > 0;
		Rows->SetVisibility(bHasRows ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		return bHasRows;
	}

	const FName CursorReason(TEXT("QuestJournal"));
	const FName InputBlockReason(TEXT("QuestJournal"));


	void HideEmbeddedListScrollbar(UListView* ListView)
	{
		if (IsValid(ListView))
		{
			ListView->SetScrollbarVisibility(ESlateVisibility::Collapsed);
			ListView->SetAllowOverScroll(false);
		}
	}

	const UNUQuestDefinition* FindDefinition(const UWorld* World, FName QuestId)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		const UNUQuestDataSubsystem* DataSubsystem = GameInstance != nullptr
			? GameInstance->GetSubsystem<UNUQuestDataSubsystem>()
			: nullptr;

		return DataSubsystem != nullptr ? DataSubsystem->FindDefinition(QuestId) : nullptr;
	}

	FText GetCategoryText(FGameplayTag CategoryTag)
	{
		static const TMap<FGameplayTag, FText> CategoryTexts = {
			{ NU::Quest::Category::Main,	NSLOCTEXT("NUQuestJournal", "CategoryMain", "메인 퀘스트") },
			{ NU::Quest::Category::Side,	NSLOCTEXT("NUQuestJournal", "CategorySide", "서브 퀘스트") },
			{ NU::Quest::Category::Daily,	NSLOCTEXT("NUQuestJournal", "CategoryDaily", "일일 퀘스트") },
			{ NU::Quest::Category::Tutorial,	NSLOCTEXT("NUQuestJournal", "CategoryTutorial", "튜토리얼 퀘스트") },
			{ NU::Quest::Category::Job,		NSLOCTEXT("NUQuestJournal", "CategoryJob", "직업 퀘스트") },
			{ NU::Quest::Category::Craft,	NSLOCTEXT("NUQuestJournal", "CategoryCraft", "생활 퀘스트") },
			{ NU::Quest::Category::Hidden,	NSLOCTEXT("NUQuestJournal", "CategoryHidden", "히든 퀘스트") },
		};

		// 세분화한 분류(NU.Quest.Category.Side.Daily)는 상위 분류의 이름 사용
		for (FGameplayTag Current = CategoryTag; Current.IsValid(); Current = Current.RequestDirectParent())
		{
			if (const FText* Found = CategoryTexts.Find(Current))
			{
				return *Found;
			}
		}

		ensureMsgf(false, TEXT("Quest: 표시 이름이 없는 분류 태그 [%s]"), *CategoryTag.ToString());

		return FText::FromString(CategoryTag.ToString());
	}

}

void UNUQuestJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	StopJournalMotion(true);
	JournalMotion.Reset();

	BindQuestListView(QuestListView);
	BindQuestListView(AvailableQuestListView);
	BindQuestListView(CompletedQuestListView);
	NUQuestJournal::HideEmbeddedListScrollbar(QuestListView);
	NUQuestJournal::HideEmbeddedListScrollbar(AvailableQuestListView);
	NUQuestJournal::HideEmbeddedListScrollbar(CompletedQuestListView);

	BindTrackButton();
	BindCloseButton();
	NU_BIND_FILTER_BUTTON("AllCategoryButton", HandleAllCategoryClicked);
	NU_BIND_FILTER_BUTTON("MainCategoryButton", HandleMainCategoryClicked);
	NU_BIND_FILTER_BUTTON("SideCategoryButton", HandleSideCategoryClicked);
	NU_BIND_FILTER_BUTTON("DailyCategoryButton", HandleDailyCategoryClicked);
	NU_BIND_FILTER_BUTTON("TutorialCategoryButton", HandleTutorialCategoryClicked);
	NU_BIND_FILTER_BUTTON("AvailableStateButton", HandleAvailableStateClicked);
	NU_BIND_FILTER_BUTTON("InProgressStateButton", HandleInProgressStateClicked);
	NU_BIND_FILTER_BUTTON("CompletedStateButton", HandleCompletedStateClicked);

	RefreshQuestComponent();
	NotifyFilterChanged();
	SetJournalVisible(false);
}

void UNUQuestJournalWidget::NativeDestruct()
{
	CloseJournalImmediately();
	JournalMotion.Reset();
	UnbindQuestListView(QuestListView);
	UnbindQuestListView(AvailableQuestListView);
	UnbindQuestListView(CompletedQuestListView);

	UnbindTrackButton();
	UnbindCloseButton();
	NU_UNBIND_FILTER_BUTTON("AllCategoryButton", HandleAllCategoryClicked);
	NU_UNBIND_FILTER_BUTTON("MainCategoryButton", HandleMainCategoryClicked);
	NU_UNBIND_FILTER_BUTTON("SideCategoryButton", HandleSideCategoryClicked);
	NU_UNBIND_FILTER_BUTTON("DailyCategoryButton", HandleDailyCategoryClicked);
	NU_UNBIND_FILTER_BUTTON("TutorialCategoryButton", HandleTutorialCategoryClicked);
	NU_UNBIND_FILTER_BUTTON("AvailableStateButton", HandleAvailableStateClicked);
	NU_UNBIND_FILTER_BUTTON("InProgressStateButton", HandleInProgressStateClicked);
	NU_UNBIND_FILTER_BUTTON("CompletedStateButton", HandleCompletedStateClicked);

	BindQuestComponent(nullptr);
	SetJournalVisible(false);

	Super::NativeDestruct();
}

void UNUQuestJournalWidget::OpenJournal()
{
	if (IsJournalOpen() && !IsJournalClosing())
	{
		return;
	}
	const bool bFromHidden = !IsJournalOpen();
	RefreshQuestComponent();
	RefreshJournal();
	StartJournalMotion(true, bFromHidden);
	SetJournalVisible(true);
}

void UNUQuestJournalWidget::CloseJournal()
{
	if (IsJournalOpen() && !IsJournalClosing())
	{
		StartJournalMotion(false, false);
	}
}

bool UNUQuestJournalWidget::IsJournalOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed
		&& GetVisibility() != ESlateVisibility::Hidden;
}

void UNUQuestJournalWidget::RefreshQuestComponent()
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	ANUPlayerState* PlayerState = OwningPlayer != nullptr
		? OwningPlayer->GetPlayerState<ANUPlayerState>()
		: nullptr;

	BindQuestComponent(PlayerState != nullptr ? PlayerState->GetQuestComponent() : nullptr);
	RefreshJournal();
}

void UNUQuestJournalWidget::BindQuestComponent(UNUQuestComponent* NewQuestComponent)
{
	if (BoundQuestComponent == NewQuestComponent)
	{
		return;
	}

	if (IsValid(BoundQuestComponent))
	{
		BoundQuestComponent->OnQuestProgressChanged.RemoveDynamic(
			this,
			&ThisClass::HandleQuestProgressChanged
		);
	}

	BoundQuestComponent = NewQuestComponent;

	if (IsValid(BoundQuestComponent))
	{
		BoundQuestComponent->OnQuestProgressChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleQuestProgressChanged
		);
	}
}

void UNUQuestJournalWidget::SetJournalVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayer());
	if (IsValid(PlayerController))
	{
		PlayerController->RequestCursor(NUQuestJournal::CursorReason, bVisible, this);
		PlayerController->RequestGameInputBlock(NUQuestJournal::InputBlockReason, bVisible, this);
	}
}

void UNUQuestJournalWidget::BindQuestListView(UListView* ListView)
{
	if (IsValid(ListView))
	{
		ListView->OnItemSelectionChanged().AddUObject(
			this,
			&ThisClass::HandleQuestSelectionChanged
		);
		ListView->OnEntryWidgetGenerated().AddUObject(this, &ThisClass::HandleJournalEntryGenerated);
		ListView->OnEntryWidgetReleased().AddUObject(this, &ThisClass::HandleJournalEntryReleased);
	}
}

void UNUQuestJournalWidget::UnbindQuestListView(UListView* ListView)
{
	if (IsValid(ListView))
	{
		ListView->OnItemSelectionChanged().RemoveAll(this);
		ListView->OnEntryWidgetGenerated().RemoveAll(this);
		ListView->OnEntryWidgetReleased().RemoveAll(this);
	}
}

void UNUQuestJournalWidget::RefreshJournal()
{
	if (!IsValid(QuestListView)
		|| !IsValid(AvailableQuestListView)
		|| !IsValid(CompletedQuestListView))
	{
		return;
	}

	struct FJournalSource
	{
		const FNUQuestProgressState* Progress = nullptr;
		const UNUQuestDefinition* Definition = nullptr;
	};

	TArray<FJournalSource> AvailableSources;
	TArray<FJournalSource> InProgressSources;
	TArray<FJournalSource> CompletedSources;

	if (IsValid(BoundQuestComponent))
	{
		for (const FNUQuestProgressState& Progress : BoundQuestComponent->GetQuestProgressList())
		{
			const UNUQuestDefinition* Definition = NUQuestJournal::FindDefinition(GetWorld(), Progress.QuestId);
			if (Definition == nullptr || !MatchesCategoryFilter(Definition->CategoryTag))
			{
				continue;
			}

			switch (Progress.QuestState)
			{
			case ENUQuestState::Locked:
			case ENUQuestState::Available:
				AvailableSources.Add({ &Progress, Definition });
				break;

			case ENUQuestState::Active:
				InProgressSources.Add({ &Progress, Definition });
				break;

			case ENUQuestState::ReadyToTurnIn:
			case ENUQuestState::Completed:
				CompletedSources.Add({ &Progress, Definition });
				break;

			default:
				continue;
			}
		}
	}

	const auto SortSources = [](TArray<FJournalSource>& Sources)
	{
		Sources.Sort([](const FJournalSource& A, const FJournalSource& B)
		{
			if (A.Definition->SortOrder != B.Definition->SortOrder)
			{
				return A.Definition->SortOrder < B.Definition->SortOrder;
			}

			return A.Progress->QuestId.ToString() < B.Progress->QuestId.ToString();
		});
	};
	SortSources(InProgressSources);
	SortSources(AvailableSources);
	SortSources(CompletedSources);

	AvailableQuestCount = AvailableSources.Num();
	InProgressQuestCount = InProgressSources.Num();
	CompletedQuestCount = CompletedSources.Num();

	ApplyStateButtonText(TEXT("AvailableStateButton"), AvailableStateText, FText::Format(
		NSLOCTEXT("NUQuestJournal", "AvailableCount", "진행 가능    {0}    {1}"),
		FText::AsNumber(AvailableQuestCount),
		bAvailableSectionExpanded ? FText::FromString(TEXT("▲")) : FText::FromString(TEXT("▼"))
	));

	ApplyStateButtonText(TEXT("InProgressStateButton"), InProgressStateText, FText::Format(
		NSLOCTEXT("NUQuestJournal", "InProgressCount", "진행 중    {0}    {1}"),
		FText::AsNumber(InProgressQuestCount),
		bInProgressSectionExpanded ? FText::FromString(TEXT("▲")) : FText::FromString(TEXT("▼"))
	));

	ApplyStateButtonText(TEXT("CompletedStateButton"), CompletedStateText, FText::Format(
		NSLOCTEXT("NUQuestJournal", "CompletedCount", "진행 완료    {0}    {1}"),
		FText::AsNumber(CompletedQuestCount),
		bCompletedSectionExpanded ? FText::FromString(TEXT("▲")) : FText::FromString(TEXT("▼"))
	));

	const FName PreviousSelection = SelectedQuestId;
	QuestListView->ClearListItems();
	AvailableQuestListView->ClearListItems();
	CompletedQuestListView->ClearListItems();
	JournalItems.Reset();

	UNUQuestJournalListItem* ItemToSelect = nullptr;
	const auto PopulateList = [this, PreviousSelection, &ItemToSelect](
		UListView* ListView,
		const TArray<FJournalSource>& Sources)
	{
		for (const FJournalSource& Source : Sources)
		{
			UNUQuestJournalListItem* Item = NewObject<UNUQuestJournalListItem>(this);
			Item->Initialize(
				Source.Progress->QuestId,
				Source.Definition->Title.IsEmpty()
					? FText::FromName(Source.Progress->QuestId)
					: Source.Definition->Title,
				Source.Definition->CategoryTag,
				Source.Progress->QuestState,
				Source.Definition->SortOrder
			);
			// List cards show required-objective progress; a completed quest reads as full.
			for (const FNUQuestObjectiveSpec& Objective : Source.Definition->Objectives)
			{
				if (Objective.bOptional || Objective.RequiredCount <= 0)
				{
					continue;
				}
				const FNUQuestObjectiveProgress* Current = Source.Progress->FindObjectiveProgress(Objective.ObjectiveId);
				Item->ProgressRequired += Objective.RequiredCount;
				Item->ProgressCount += FMath::Clamp(Current ? Current->CurrentCount : 0, 0, Objective.RequiredCount);
			}
			if (Source.Progress->QuestState == ENUQuestState::Completed)
			{
				Item->ProgressCount = Item->ProgressRequired;
			}

			JournalItems.Add(Item);
			ListView->AddItem(Item);

			if (Item->QuestId == PreviousSelection)
			{
				ItemToSelect = Item;
			}
		}
	};
	PopulateList(QuestListView, InProgressSources);
	PopulateList(AvailableQuestListView, AvailableSources);
	PopulateList(CompletedQuestListView, CompletedSources);
	UpdateStateSectionVisibility();

	if (ItemToSelect == nullptr && !JournalItems.IsEmpty())
	{
		ItemToSelect = JournalItems[0];
	}

	if (IsValid(ItemToSelect))
	{
		if (QuestListView->GetListItems().Contains(ItemToSelect))
		{
			QuestListView->SetSelectedItem(ItemToSelect);
		}
		else if (AvailableQuestListView->GetListItems().Contains(ItemToSelect))
		{
			AvailableQuestListView->SetSelectedItem(ItemToSelect);
		}
		else
		{
			CompletedQuestListView->SetSelectedItem(ItemToSelect);
		}
		HandleQuestSelectionChanged(ItemToSelect);
	}
	else
	{
		SelectedQuestId = NAME_None;
		ClearDetails();
	}

	if (IsValid(EmptyListText))
	{
		EmptyListText->SetVisibility(
			JournalItems.IsEmpty()
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed
		);
	}
}

void UNUQuestJournalWidget::RefreshDetails()
{
	const UNUQuestDefinition* Definition = NUQuestJournal::FindDefinition(GetWorld(), SelectedQuestId);
	const FNUQuestProgressState* Progress = IsValid(BoundQuestComponent)
		? BoundQuestComponent->FindQuestProgress(SelectedQuestId)
		: nullptr;

	if (Definition == nullptr || Progress == nullptr)
	{
		ClearDetails();
		return;
	}

	if (IsValid(DetailPanel))
	{
		DetailPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (IsValid(DetailTitleText))
	{
		DetailTitleText->SetText(
			Definition->Title.IsEmpty() ? FText::FromName(SelectedQuestId) : Definition->Title
		);
	}

	if (IsValid(DetailMetaText))
	{
		DetailMetaText->SetText(FText::Format(
			NSLOCTEXT("NUQuestJournal", "QuestMeta", "{0}    ·    {1}    ·    권장 레벨 {2}"),
			NUQuestJournal::GetCategoryText(Definition->CategoryTag),
			NUQuest::GetStateDisplayText(Progress->QuestState),
			FText::AsNumber(Definition->RecommendedLevel)
		));
	}

	if (IsValid(DetailDescriptionText))
	{
		DetailDescriptionText->SetText(
			!Definition->Description.IsEmpty() ? Definition->Description : Definition->Summary
		);
	}

	if (UTextBlock* LocationText = Cast<UTextBlock>(GetWidgetFromName(TEXT("QR_LocationText"))))
	{
		const FText Location = NUQuestJournalPresentation::GetLocationText(GetWorld(), Definition);
		LocationText->SetText(Location);
		if (UWidget* LocationRow = GetWidgetFromName(TEXT("QR_LocationRow")))
		{
			LocationRow->SetVisibility(Location.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}

	if (IsValid(DetailObjectiveText))
	{
		// 잠긴 퀘스트는 목표 대신 부족한 수락 조건을 보여준다
		const FNUQuestRequirementSpec* Unmet = Progress->QuestState == ENUQuestState::Locked
			? BoundQuestComponent->FindUnmetRequirement(Definition)
			: nullptr;

		DetailObjectiveText->SetText(
			Unmet != nullptr
			? BuildRequirementText(*Unmet)
			: BuildObjectiveText(SelectedQuestId)
		);
	}

	if (IsValid(DetailRewardText))
	{
		DetailRewardText->SetText(BuildRewardText(SelectedQuestId));
	}

	const bool bObjectiveRows = NUQuestJournal::PopulateObjectiveRows(*this, *Definition, *Progress);
	if (IsValid(DetailObjectiveText))
	{
		const FText TurnIn = Progress->QuestState == ENUQuestState::ReadyToTurnIn
			? GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>()->GetTurnInDisplayText(SelectedQuestId)
			: FText::GetEmpty();
		const bool bShowTurnIn = bObjectiveRows && !TurnIn.IsEmpty();
		if (bShowTurnIn)
		{
			DetailObjectiveText->SetText(TurnIn);
		}
		DetailObjectiveText->SetVisibility(!bObjectiveRows || bShowTurnIn
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const bool bRewardRows = NUQuestJournal::PopulateRewardRows(*this, *Definition);
	if (IsValid(DetailRewardText))
	{
		DetailRewardText->SetVisibility(bRewardRows ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	const bool bCanTrack = Progress->QuestState == ENUQuestState::Active
		|| Progress->QuestState == ENUQuestState::ReadyToTurnIn;
	if (TrackButtonWidget.IsValid())
	{
		TrackButtonWidget->SetIsEnabled(bCanTrack);
	}

	{
		const ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayer());
		const bool bIsTracked = IsValid(PlayerController)
			&& PlayerController->IsQuestTracked(SelectedQuestId);

		SetTrackButtonLabel(
			bIsTracked
				? NSLOCTEXT("NUQuestJournal", "Untrack", "목표 숨기기")
				: NSLOCTEXT("NUQuestJournal", "Track", "목표 표시")
		);
	}
}

void UNUQuestJournalWidget::UpdateStateSectionVisibility()
{
	float EntryHeight = 30.0f;
	if (EntrySizePreview == nullptr && IsValid(QuestListView) && QuestListView->GetEntryWidgetClass() != nullptr)
	{
		EntrySizePreview = CreateWidget<UUserWidget>(this, QuestListView->GetEntryWidgetClass());
	}
	if (IsValid(EntrySizePreview))
	{
		const TSharedRef<SWidget> PreviewSlate = EntrySizePreview->TakeWidget();
		EntrySizePreview->ForceLayoutPrepass();
		EntryHeight = FMath::Max(1.0f, EntrySizePreview->GetDesiredSize().Y);
	}
	const auto UpdateListSize = [EntryHeight](USizeBox* SizeBox, bool bExpanded, int32 ItemCount)
	{
		if (!IsValid(SizeBox))
		{
			return;
		}

		const bool bShowList = bExpanded && ItemCount > 0;
		SizeBox->SetVisibility(bShowList ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bShowList)
		{
			SizeBox->SetHeightOverride(static_cast<float>(ItemCount) * EntryHeight);
		}
	};

	UpdateListSize(InProgressListSizeBox, bInProgressSectionExpanded, InProgressQuestCount);
	UpdateListSize(AvailableListSizeBox, bAvailableSectionExpanded, AvailableQuestCount);
	UpdateListSize(CompletedListSizeBox, bCompletedSectionExpanded, CompletedQuestCount);
}

void UNUQuestJournalWidget::ClearDetails()
{
	if (IsValid(DetailPanel))
	{
		DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (TrackButtonWidget.IsValid())
	{
		TrackButtonWidget->SetIsEnabled(false);
	}
}

void UNUQuestJournalWidget::SetCategoryFilter(FGameplayTag NewFilter)
{
	if (CategoryFilter == NewFilter)
	{
		return;
	}

	CategoryFilter = NewFilter;
	NotifyFilterChanged();
	RefreshJournal();
}

void UNUQuestJournalWidget::NotifyFilterChanged()
{
	BP_OnJournalFilterChanged(CategoryFilter, StateFilter);
	ApplyTabStyles();
}

void UNUQuestJournalWidget::ApplyTabStyles()
{
	if (!bUseTabStyles)
	{
		return;
	}
	const FGameplayTag AllTag = FGameplayTag::RequestGameplayTag(TEXT("NU.Quest.Category"));
	const struct { const TCHAR* Name; FGameplayTag Tag; } Tabs[] = {
		{ TEXT("AllCategoryButton"), AllTag },
		{ TEXT("MainCategoryButton"), NU::Quest::Category::Main },
		{ TEXT("SideCategoryButton"), NU::Quest::Category::Side },
		{ TEXT("DailyCategoryButton"), NU::Quest::Category::Daily },
		{ TEXT("TutorialCategoryButton"), NU::Quest::Category::Tutorial },
	};
	for (const auto& Tab : Tabs)
	{
		UButton* Button = Cast<UButton>(GetWidgetFromName(Tab.Name));
		if (Button == nullptr)
		{
			continue;
		}
		const bool bSelected = Tab.Tag == AllTag ? (!CategoryFilter.IsValid() || CategoryFilter == AllTag) : CategoryFilter == Tab.Tag;
		Button->SetStyle(bSelected ? SelectedTabStyle : TabStyle);
		// The graph tints the old navy tabs: background and the button's content color (which darkens the label).
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetColorAndOpacity(FLinearColor::White);
		if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("%s_Label"), Tab.Name))))
		{
			Label->SetColorAndOpacity(bSelected ? SelectedTabLabelColor : TabLabelColor);
		}
	}
	for (const TCHAR* Name : { TEXT("InProgressStateButton"), TEXT("AvailableStateButton"), TEXT("CompletedStateButton") })
	{
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(Name)))
		{
			Button->SetBackgroundColor(FLinearColor::White);
			Button->SetColorAndOpacity(FLinearColor::White);
		}
		if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("%s_Label"), Name))))
		{
			Label->SetColorAndOpacity(StateLabelColor);
		}
	}
}

void UNUQuestJournalWidget::SetStateFilter(ENUQuestJournalStateFilter NewFilter)
{
	StateFilter = NewFilter;
	switch (NewFilter)
	{
	case ENUQuestJournalStateFilter::Available:
		bAvailableSectionExpanded = !bAvailableSectionExpanded;
		break;

	case ENUQuestJournalStateFilter::InProgress:
		bInProgressSectionExpanded = !bInProgressSectionExpanded;
		break;

	case ENUQuestJournalStateFilter::Completed:
		bCompletedSectionExpanded = !bCompletedSectionExpanded;
		break;

	default:
		break;
	}

	NotifyFilterChanged();
	RefreshJournal();
}

bool UNUQuestJournalWidget::MatchesCategoryFilter(const FGameplayTag& InCategoryTag) const
{
	return !CategoryFilter.IsValid() || InCategoryTag.MatchesTag(CategoryFilter);
}

FText UNUQuestJournalWidget::BuildRequirementText(const FNUQuestRequirementSpec& Requirement) const
{
	if (Requirement.RequirementType == NU::Quest::Requirement::PlayerLevel)
	{
		const int32 CurrentLevel = IsValid(BoundQuestComponent) ? BoundQuestComponent->GetTestPlayerLevel() : 0;

		return FText::Format(
			NSLOCTEXT("NUQuestJournal", "ReqPlayerLevel", "[수락 조건]\n레벨 {0} 이상 (현재 {1})"),
			FText::AsNumber(Requirement.RequiredValue),
			FText::AsNumber(CurrentLevel)
		);
	}

	if (Requirement.RequirementType == NU::Quest::Requirement::QuestCompleted)
	{
		const UNUQuestDefinition* Prerequisite = NUQuestJournal::FindDefinition(GetWorld(), Requirement.TargetId);
		const FText PrerequisiteTitle =
			Prerequisite != nullptr && !Prerequisite->Title.IsEmpty()
			? Prerequisite->Title
			: FText::FromName(Requirement.TargetId);

		return FText::Format(
			NSLOCTEXT("NUQuestJournal", "ReqQuestCompleted", "[수락 조건]\n[{0}] 완료"),
			PrerequisiteTitle
		);
	}

	if (Requirement.RequirementType == NU::Quest::Requirement::QuestGroupCompletedCount)
	{
		return FText::Format(
			NSLOCTEXT("NUQuestJournal", "ReqQuestGroup", "[수락 조건]\n[{0}] 퀘스트 {1}개 이상 완료"),
			FText::FromName(Requirement.TargetId),
			FText::AsNumber(Requirement.RequiredValue)
		);
	}

	return NSLOCTEXT("NUQuestJournal", "ReqOther", "[수락 조건]\n아직 조건이 갖춰지지 않음");
}

FText UNUQuestJournalWidget::BuildObjectiveText(FName QuestId) const
{
	const UNUQuestDefinition* Definition = NUQuestJournal::FindDefinition(GetWorld(), QuestId);
	const FNUQuestProgressState* Progress = IsValid(BoundQuestComponent) ? BoundQuestComponent->FindQuestProgress(QuestId) : nullptr;
	if (Definition == nullptr) { return FText::GetEmpty(); }
	FNUQuestProgressState Preview;
	if (Progress == nullptr) { Preview.Initialize(*Definition); Progress = &Preview; }
	const FText Objectives = Progress->GetObjectiveDisplayText(*Definition);
	const FText TurnIn = Progress->QuestState == ENUQuestState::ReadyToTurnIn
		? GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>()->GetTurnInDisplayText(QuestId)
		: FText::GetEmpty();
	return TurnIn.IsEmpty() ? Objectives : FText::Format(NSLOCTEXT("NUQuest", "ObjectivesWithTurnIn", "{0}\n{1}"), Objectives, TurnIn);
}

FText UNUQuestJournalWidget::BuildRewardText(FName QuestId) const
{
	const UNUQuestDefinition* Definition = NUQuestJournal::FindDefinition(GetWorld(), QuestId);
	if (Definition == nullptr || Definition->Rewards.IsEmpty())
	{
		return NSLOCTEXT("NUQuestJournal", "NoReward", "없음");
	}

	// 아이템 보상은 테이블에 이름이 없어 여기서 아이템 정의의 이름으로 채운다.
	// 인벤토리는 폰이 아니라 플레이어 스테이트에 붙어 있다.
	const ANUPlayerState* OwningPlayerState = GetOwningPlayerState<ANUPlayerState>();
	const UNUInventoryComponent* Inventory = OwningPlayerState != nullptr
		? OwningPlayerState->GetInventoryComponent()
		: nullptr;

	TArray<FString> RewardLines;
	for (const FNUQuestRewardSpec& Reward : Definition->Rewards)
	{
		FNUQuestRewardSpec Line = Reward;
		if (Line.RewardType == NU::Quest::Reward::Item && Line.DisplayName.IsEmpty() && Inventory != nullptr)
		{
			int32 NumericId = 0;
			if (LexTryParseString(NumericId, *Line.TargetId.ToString()))
			{
				if (const FNUItemDefRow* ItemDef = Inventory->FindItemDefByNumericId(NumericId))
				{
					Line.DisplayName = ItemDef->DisplayName;
				}
			}
		}
		RewardLines.Add(Line.GetDisplayText().ToString());
	}

	return RewardLines.IsEmpty()
		? NSLOCTEXT("NUQuestJournal", "UnsupportedReward", "없음")
		: FText::FromString(FString::Printf(
			TEXT("%s"),
			*FString::Join(RewardLines, TEXT("\n"))
		));
}

void UNUQuestJournalWidget::HandleQuestProgressChanged()
{
	RefreshJournal();
}

void UNUQuestJournalWidget::HandleQuestSelectionChanged(UObject* SelectedItem)
{
	const UNUQuestJournalListItem* JournalItem = Cast<UNUQuestJournalListItem>(SelectedItem);
	if (!IsValid(JournalItem) || bUpdatingSelection)
	{
		return;
	}

	TGuardValue<bool> SelectionGuard(bUpdatingSelection, true);
	UListView* ListViews[] = { QuestListView, AvailableQuestListView, CompletedQuestListView };
	for (UListView* ListView : ListViews)
	{
		if (IsValid(ListView) && ListView->GetSelectedItem() != SelectedItem)
		{
			ListView->ClearSelection();
		}
	}

	SelectedQuestId = JournalItem->QuestId;
	RefreshDetails();
}

void UNUQuestJournalWidget::TrackSelectedQuest()
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayer());
	if (IsValid(PlayerController) && !SelectedQuestId.IsNone())
	{
		// 이미 고정된 퀘스트를 다시 누르면 고정을 푼다 (버튼이 토글)
		PlayerController->ToggleTrackedQuest(SelectedQuestId);
		RefreshDetails();
	}
}

void UNUQuestJournalWidget::HandleCloseClicked()
{
	CloseJournal();
}

void UNUQuestJournalWidget::BindCloseButton()
{
	UWidget* CloseWidget = GetWidgetFromName(TEXT("CloseButton"));
	if (UButton* CloseButtonNative = Cast<UButton>(CloseWidget))
	{
		CloseButtonNative->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
		CloseButtonNative->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
		CloseButtonWidget->OnCloseClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(CloseWidget))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
		CommonButtonWidget->OnCommonClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UNUQuestJournalWidget::UnbindCloseButton()
{
	UWidget* CloseWidget = GetWidgetFromName(TEXT("CloseButton"));
	if (UButton* CloseButtonNative = Cast<UButton>(CloseWidget))
	{
		CloseButtonNative->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(CloseWidget))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UNUQuestJournalWidget::BindTrackButton()
{
	UWidget* TrackWidget = GetWidgetFromName(TEXT("TrackButton"));
	TrackButtonWidget = TrackWidget;
	if (UButton* TrackButtonNative = Cast<UButton>(TrackWidget))
	{
		TrackButtonNative->OnClicked.RemoveDynamic(this, &ThisClass::TrackSelectedQuest);
		TrackButtonNative->OnClicked.AddDynamic(this, &ThisClass::TrackSelectedQuest);
	}
	else if (UNU_CloseButtonWidget* CloseStyleButton = Cast<UNU_CloseButtonWidget>(TrackWidget))
	{
		CloseStyleButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::TrackSelectedQuest);
		CloseStyleButton->OnCloseClicked.AddDynamic(this, &ThisClass::TrackSelectedQuest);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(TrackWidget))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &ThisClass::TrackSelectedQuest);
		CommonButtonWidget->OnCommonClicked.AddDynamic(this, &ThisClass::TrackSelectedQuest);
	}
}

void UNUQuestJournalWidget::UnbindTrackButton()
{
	UWidget* TrackWidget = GetWidgetFromName(TEXT("TrackButton"));
	if (UButton* TrackButtonNative = Cast<UButton>(TrackWidget))
	{
		TrackButtonNative->OnClicked.RemoveDynamic(this, &ThisClass::TrackSelectedQuest);
	}
	else if (UNU_CloseButtonWidget* CloseStyleButton = Cast<UNU_CloseButtonWidget>(TrackWidget))
	{
		CloseStyleButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::TrackSelectedQuest);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(TrackWidget))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &ThisClass::TrackSelectedQuest);
	}
}

void UNUQuestJournalWidget::SetTrackButtonLabel(const FText& InText)
{
	if (IsValid(TrackButtonText))
	{
		TrackButtonText->SetText(InText);
		return;
	}

	if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(TrackButtonWidget.Get()))
	{
		CommonButtonWidget->SetLabelText(InText);
	}
	else if (UButton* Button = Cast<UButton>(TrackButtonWidget.Get()))
	{
		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
		{
			Label->SetText(InText);
		}
	}
}

void UNUQuestJournalWidget::ApplyStateButtonText(const TCHAR* ButtonWidgetName, UTextBlock* LegacyText, const FText& InText)
{
	if (IsValid(LegacyText))
	{
		LegacyText->SetText(InText);
		return;
	}

	if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(GetWidgetFromName(ButtonWidgetName)))
	{
		CommonButtonWidget->SetLabelText(InText);
	}
	else if (UButton* Button = Cast<UButton>(GetWidgetFromName(ButtonWidgetName)))
	{
		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
		{
			Label->SetText(InText);
		}
	}
}

void UNUQuestJournalWidget::HandleAllCategoryClicked()
{
	SetCategoryFilter(FGameplayTag::RequestGameplayTag(TEXT("NU.Quest.Category")));
}

void UNUQuestJournalWidget::HandleMainCategoryClicked()
{
	SetCategoryFilter(FGameplayTag::RequestGameplayTag(TEXT("NU.Quest.Category.Main")));
}

void UNUQuestJournalWidget::HandleSideCategoryClicked()
{
	SetCategoryFilter(FGameplayTag::RequestGameplayTag(TEXT("NU.Quest.Category.Side")));
}

void UNUQuestJournalWidget::HandleDailyCategoryClicked()
{
	SetCategoryFilter(FGameplayTag::RequestGameplayTag(TEXT("NU.Quest.Category.Daily")));
}

void UNUQuestJournalWidget::HandleTutorialCategoryClicked()
{
	SetCategoryFilter(FGameplayTag::RequestGameplayTag(TEXT("NU.Quest.Category.Tutorial")));
}

void UNUQuestJournalWidget::HandleAvailableStateClicked()
{
	SetStateFilter(ENUQuestJournalStateFilter::Available);
}

void UNUQuestJournalWidget::HandleInProgressStateClicked()
{
	SetStateFilter(ENUQuestJournalStateFilter::InProgress);
}

void UNUQuestJournalWidget::HandleCompletedStateClicked()
{
	SetStateFilter(ENUQuestJournalStateFilter::Completed);
}

#undef NU_BIND_FILTER_BUTTON
#undef NU_UNBIND_FILTER_BUTTON
