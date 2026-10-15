#include "CharacterProgression/UI/Job/NUJobSelectionWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUJobTypes.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "CharacterProgression/UI/Job/NUJobSelectionCardWidget.h"
#include "CharacterProgression/UI/Job/NUJobSelectionEntryButton.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "UI/NUKoreanText.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
	UTextBlock* MakeJobText(UWidgetTree* WidgetTree, const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>();
		Result->SetText(Text);
		Result->SetFont(FCoreStyle::GetDefaultFontStyle(Size >= 24 ? "Bold" : "Regular", Size));
		Result->SetColorAndOpacity(FSlateColor(Color));
		return Result;
	}
}

void UNUJobSelectionWidget::InitializeSelection(UNUJobComponent* InJobComponent, UNUProgressionComponent* InProgression, FSimpleDelegate InCloseRequested, FSimpleDelegate InSelectionCompleted)
{
	JobComponent = InJobComponent;
	Progression = InProgression;
	SetCloseRequested(MoveTemp(InCloseRequested));
	SelectionCompleted = MoveTemp(InSelectionCompleted);
}

void UNUJobSelectionWidget::NativeConstruct()
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
	TitleText->SetText(NSLOCTEXT("NUJob", "SelectionTitle", "어떤 힘의 방식을 쓸 것인가 (직업)"));
	LevelStatusText->SetVisibility(ESlateVisibility::Collapsed);
	KeyHintText->SetVisibility(ESlateVisibility::Collapsed);
	BindConfirmButton();
	// 위젯이 화면에 있는 동안 직업 확정에 맞춰 표시를 갱신
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
		if (!Progression->HasServerProgressionSnapshot() && !Progression->IsServerProgressionRequestPending())
		{
			Progression->RequestProgressionState();
		}
	}

	RebuildJobEntries();
	if (bSelectFirstOnOpen && SelectedJobId == 0 && !JobEntries.IsEmpty())
	{
		SelectedJobId = JobEntries[0].JobId;
	}
	RefreshState();
}

void UNUJobSelectionWidget::NativeDestruct()
{
	UnbindConfirmButton();
	if (JobComponent.IsValid())
	{
		JobComponent->OnJobChanged.RemoveDynamic(this, &ThisClass::HandleJobChanged);
	}
	if (Progression.IsValid())
	{
		Progression->OnProgressionRequestStateChanged.RemoveDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
		Progression->OnProgressionRequestFailed.RemoveDynamic(this, &ThisClass::HandleProgressionRequestFailed);
	}
	if (ActiveConfirmDialog != nullptr)
	{
		ActiveConfirmDialog->Dismiss();
		ActiveConfirmDialog = nullptr;
	}
	JobButtons.Reset();
	JobCards.Reset();
	JobEntries.Reset();

	Super::NativeDestruct();
}

bool UNUJobSelectionWidget::CanDragWindow() const
{
	return false;
}

bool UNUJobSelectionWidget::BindConfirmButton()
{
	UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton"));
	if (UButton* NativeButton = Cast<UButton>(ConfirmWidget))
	{
		NativeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
		NativeButton->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
		return true;
	}
	if (UNU_CommonButtonWidget* CommonButton = Cast<UNU_CommonButtonWidget>(ConfirmWidget))
	{
		CommonButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
		CommonButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("직업 선택 WBP에 지원되는 ConfirmButton이 없습니다."));
	return false;
}

void UNUJobSelectionWidget::UnbindConfirmButton()
{
	UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton"));
	if (UButton* NativeButton = Cast<UButton>(ConfirmWidget))
	{
		NativeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	else if (UNU_CommonButtonWidget* CommonButton = Cast<UNU_CommonButtonWidget>(ConfirmWidget))
	{
		CommonButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
}

void UNUJobSelectionWidget::SetConfirmButtonEnabled(bool bEnabled)
{
	if (UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton")))
	{
		ConfirmWidget->SetIsEnabled(bEnabled);
	}
}

void UNUJobSelectionWidget::RebuildJobEntries()
{
	JobEntries.Reset();
	// 서브시스템에서 검증과 정렬이 끝난 직업 정의만 화면 목록으로 변환
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Data == nullptr)
	{
		BP_OnJobEntriesChanged(JobEntries);
		return;
	}

	for (const FNUJobDefinitionRow* Job : Data->GetAllJobs())
	{
		if (Job == nullptr)
		{
			continue;
		}

		FNUJobSelectionViewData& Entry = JobEntries.AddDefaulted_GetRef();
		Entry.JobId = Job->JobId;
		Entry.DisplayName = Job->DisplayName;
		Entry.Description = Job->Description;
		Entry.AccentColor = ResolveAccent(Job->JobId);
	}

	BP_OnJobEntriesChanged(JobEntries);
}

void UNUJobSelectionWidget::BP_OnJobEntriesChanged_Implementation(const TArray<FNUJobSelectionViewData>& InJobEntries)
{
	JobListPanel->ClearChildren();
	JobButtons.Reset();
	JobCards.Reset();
	for (const FNUJobSelectionViewData& Entry : InJobEntries)
	{
		UWidget* Card = UsesStage() ? BuildStageCard(Entry) : BuildDefaultJobCard(Entry);
		if (UHorizontalBoxSlot* BoxSlot = Card != nullptr ? JobListPanel->AddChildToHorizontalBox(Card) : nullptr)
		{
			BoxSlot->SetPadding(UsesStage() ? FMargin(0.f) : FMargin(7.f));
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

UWidget* UNUJobSelectionWidget::BuildDefaultJobCard(const FNUJobSelectionViewData& Entry)
{
	UNUJobSelectionEntryButton* Button = WidgetTree->ConstructWidget<UNUJobSelectionEntryButton>();
	Button->SetJobId(Entry.JobId);
	Button->OnEntryClicked.BindUObject(this, &ThisClass::SelectJob);
	Button->SetBackgroundColor(FLinearColor(0.12f, 0.135f, 0.16f, 1.f));
	JobButtons.Add(Entry.JobId, Button);

	UTextBlock* NameText = MakeJobText(WidgetTree, Entry.DisplayName, 22, FLinearColor(0.94f, 0.94f, 0.96f));
	NameText->SetJustification(ETextJustify::Center);
	Button->AddChild(NameText);
	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(NameText->Slot))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
	CardSize->SetWidthOverride(270.f);
	CardSize->SetHeightOverride(290.f);
	CardSize->AddChild(Button);
	return CardSize;
}

UWidget* UNUJobSelectionWidget::BuildStageCard(const FNUJobSelectionViewData& Entry)
{
	UNUJobSelectionCardWidget* Card = CreateWidget<UNUJobSelectionCardWidget>(GetOwningPlayer(), JobCardClass);
	if (Card == nullptr)
	{
		return nullptr;
	}
	// 내 캐릭터의 외형 (고른 캐릭터가 없는 시험 플레이면 기본 노비스)
	FNUCharacterSummary Appearance;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UNUCharacterManager* Manager = GameInstance->GetSubsystem<UNUCharacterManager>())
		{
			Manager->GetSelectedOrTestCharacter(Appearance);
		}
	}
	Card->Setup(Entry.JobId, Entry.DisplayName, ResolveEmblem(Entry.JobId), Entry.AccentColor, Appearance);
	Card->OnCardClicked.BindUObject(this, &ThisClass::SelectJob);
	JobCards.Add(Entry.JobId, Card);
	return Card;
}

void UNUJobSelectionWidget::SelectJob(int32 JobId)
{
	if (JobComponent.IsValid() && JobComponent->HasJob())
	{
		return;
	}
	SelectedJobId = JobId;
	RefreshState();
}

void UNUJobSelectionWidget::RefreshState()
{
	const FNUJobSelectionViewData* SelectedEntry = FindSelectedEntry();
	if (UsesStage())
	{
		// 무대형: 판에는 이름과 직업 문장(1인칭 대사)을 따옴표로
		SelectedJobText->SetText(SelectedEntry != nullptr ? SelectedEntry->DisplayName : NSLOCTEXT("NUJob", "SelectPromptShort", "직업을 고르세요"));
		DescriptionText->SetText(SelectedEntry != nullptr && !SelectedEntry->Description.IsEmpty()
			? FText::Format(NSLOCTEXT("NUJob", "MottoQuote", "“{0}”"), SelectedEntry->Description)
			: FText::GetEmpty());
		for (const TPair<int32, TObjectPtr<UNUJobSelectionCardWidget>>& Pair : JobCards)
		{
			if (Pair.Value != nullptr)
			{
				Pair.Value->SetSelected(Pair.Key == SelectedJobId);
			}
		}
		RefreshDetail(SelectedEntry);
	}
	else
	{
		SelectedJobText->SetText(SelectedEntry != nullptr
			? FText::Format(NSLOCTEXT("NUJob", "SelectedJob", "선택: {0}"), SelectedEntry->DisplayName)
			: NSLOCTEXT("NUJob", "SelectPrompt", "직업을 선택하세요"));
		DescriptionText->SetText(SelectedEntry != nullptr
			? SelectedEntry->Description
			: NSLOCTEXT("NUJob", "SelectionGuide", "다섯 가지 힘의 방식 중 하나를 선택하세요."));
	}
	SetConfirmButtonEnabled(
		SelectedEntry != nullptr
		&& JobComponent.IsValid()
		&& !JobComponent->HasJob()
		&& Progression.IsValid()
		&& Progression->HasServerProgressionSnapshot()
		&& !Progression->IsServerProgressionRequestPending());

	for (const TPair<int32, TObjectPtr<UNUJobSelectionEntryButton>>& Pair : JobButtons)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->SetBackgroundColor(Pair.Key == SelectedJobId
				? GetJobAccent(Pair.Key) * 0.65f
				: FLinearColor(0.12f, 0.135f, 0.16f, 1.f));
		}
	}
}

void UNUJobSelectionWidget::HandleConfirmClicked()
{
	const FNUJobSelectionViewData* Entry = FindSelectedEntry();
	if (Entry == nullptr || !JobComponent.IsValid())
	{
		return;
	}
	// 최초 직업은 되돌릴 수 없으므로 서버 요청 전에 한 번 더 사용자 확인을 받는다.
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUConfirmDialog> DialogClass = Settings != nullptr
		? Settings->ConfirmDialogClass.LoadSynchronous()
		: nullptr;
	ActiveConfirmDialog = UNUConfirmDialog::Show(
		GetOwningPlayer(),
		ConfirmDialogClass != nullptr ? ConfirmDialogClass : DialogClass,
		FText::Format(
			NSLOCTEXT(
				"NUJob",
				"ConfirmSelection",
				"{0} 직업을 선택하시겠습니까?\n직업 스킬 3종을 함께 획득하며, 선택 후 변경할 수 없습니다."),
			Entry->DisplayName),
		NSLOCTEXT("NUJob", "Confirm", "선택"),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSelectionConfirmed),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSelectionCancelled),
		NSLOCTEXT("NUJob", "Cancel", "취소"));
	if (ActiveConfirmDialog == nullptr)
	{
		SubmitJobSelection();
	}
}

void UNUJobSelectionWidget::HandleSelectionConfirmed()
{
	ActiveConfirmDialog = nullptr;
	SubmitJobSelection();
}

void UNUJobSelectionWidget::HandleSelectionCancelled()
{
	ActiveConfirmDialog = nullptr;
}

void UNUJobSelectionWidget::SubmitJobSelection()
{
	if (SelectedJobId > 0 && JobComponent.IsValid())
	{
		if (!JobComponent->RequestInitialJobSelection(SelectedJobId))
		{
			if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
			{
				UI->ShowNotice(NSLOCTEXT("NUJob", "SelectionRequestUnavailable", "현재 직업 선택을 요청할 수 없습니다."));
			}
		}
		RefreshState();
	}
}

void UNUJobSelectionWidget::HandleProgressionRequestStateChanged()
{
	RefreshState();
}

void UNUJobSelectionWidget::HandleProgressionRequestFailed(int32 ResultCode, const FString& Message)
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(FText::FromString(Message));
	}
	RefreshState();
}

void UNUJobSelectionWidget::HandleJobChanged()
{
	if (!JobComponent.IsValid() || !JobComponent->HasJob())
	{
		return;
	}
	const FNUJobSelectionViewData* SelectedEntry = FindSelectedEntry();
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()); UI != nullptr && SelectedEntry != nullptr)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Field::JobChange);
		UI->ShowNotice(FText::Format(
			NSLOCTEXT("NUJob", "SelectionComplete", "{0} 전직 완료 · 직업 스킬 3종 획득"),
			SelectedEntry->DisplayName));
	}
	// 복제된 직업 상태가 실제로 바뀐 뒤에만 선택 창을 닫고 다음 선택 단계로 넘어간다.
	const FSimpleDelegate Completion = SelectionCompleted;
	RequestClose();
	Completion.ExecuteIfBound();
}

const FNUJobSelectionViewData* UNUJobSelectionWidget::FindSelectedEntry() const
{
	return JobEntries.FindByPredicate([this](const FNUJobSelectionViewData& Entry)
	{
		return Entry.JobId == SelectedJobId;
	});
}

FLinearColor UNUJobSelectionWidget::GetJobAccent(int32 JobId)
{
	switch (JobId)
	{
	case 1001: return FLinearColor(0.92f, 0.42f, 0.18f, 1.f);
	case 1002: return FLinearColor(0.82f, 0.78f, 0.22f, 1.f);
	case 1003: return FLinearColor(0.2f, 0.72f, 0.95f, 1.f);
	case 1004: return FLinearColor(0.7f, 0.34f, 0.9f, 1.f);
	case 1005: return FLinearColor(0.3f, 0.78f, 0.45f, 1.f);
	default: return FLinearColor(0.55f, 0.6f, 0.68f, 1.f);
	}
}

FLinearColor UNUJobSelectionWidget::ResolveAccent(int32 JobId) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUJobDefinitionRow* Job = Data != nullptr ? Data->FindJob(JobId) : nullptr;
	if (const FLinearColor* Color = Job != nullptr ? JobAccents.Find(Job->JobKey) : nullptr)
	{
		return *Color;
	}
	return GetJobAccent(JobId);
}

UTexture2D* UNUJobSelectionWidget::ResolveEmblem(int32 JobId) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUJobDefinitionRow* Job = Data != nullptr ? Data->FindJob(JobId) : nullptr;
	if (Job == nullptr)
	{
		return nullptr;
	}
	if (const TSoftObjectPtr<UTexture2D>* Emblem = JobEmblems.Find(Job->JobKey))
	{
		return Emblem->LoadSynchronous();
	}
	return Job->Icon.LoadSynchronous();
}

void UNUJobSelectionWidget::RefreshDetail(const FNUJobSelectionViewData* Entry)
{
	const int32 JobId = Entry != nullptr ? Entry->JobId : 0;
	const FLinearColor Accent = Entry != nullptr ? Entry->AccentColor : FLinearColor::White;
	if (DetailEmblem != nullptr)
	{
		UTexture2D* Emblem = Entry != nullptr ? ResolveEmblem(JobId) : nullptr;
		DetailEmblem->SetVisibility(Emblem != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (Emblem != nullptr)
		{
			DetailEmblem->SetBrushResourceObject(Emblem);
			DetailEmblem->SetColorAndOpacity(Accent);
		}
	}
	if (AccentBar != nullptr)
	{
		AccentBar->SetColorAndOpacity(Accent);
	}
	if (ConfirmLabel != nullptr)
	{
		ConfirmLabel->SetText(Entry != nullptr
			? FText::Format(ConfirmLabelFormat, FText::FromString(NUKoreanText::Ro(Entry->DisplayName.ToString())))
			: NSLOCTEXT("NUJob", "ConfirmLabelEmpty", "직업을 고르세요"));
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUJobDefinitionRow* Job = Data != nullptr && JobId > 0 ? Data->FindJob(JobId) : nullptr;

	// 시작 스킬 --- 직업 스킬 표에서 이 직업(Class = JobKey)의 시작스킬 줄을 패시브 => 액티브 => 시너지 순으로
	if (SkillList != nullptr && SkillRowClass != nullptr)
	{
		SkillList->ClearChildren();
		const UNUBattleDataSettings* BattleSettings = GetDefault<UNUBattleDataSettings>();
		const UDataTable* Table = Job != nullptr && BattleSettings != nullptr ? BattleSettings->JobSkillDefinitionTable.LoadSynchronous() : nullptr;
		TArray<const FNUJobSkillDefinition*> Skills;
		if (Table != nullptr && Table->GetRowStruct() == FNUJobSkillDefinition::StaticStruct())
		{
			for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
			{
				const FNUJobSkillDefinition* Skill = reinterpret_cast<const FNUJobSkillDefinition*>(Row.Value);
				if (Skill != nullptr && Skill->Class == Job->JobKey && Skill->NodeType == FName(TEXT("시작스킬")))
				{
					Skills.Add(Skill);
				}
			}
		}
		const auto KindOrder = [](const FName Kind)
		{
			return Kind == FName(TEXT("패시브")) ? 0 : Kind == FName(TEXT("액티브")) ? 1 : 2;
		};
		Skills.Sort([&KindOrder](const FNUJobSkillDefinition& A, const FNUJobSkillDefinition& B)
		{
			const int32 KA = KindOrder(A.SkillType);
			const int32 KB = KindOrder(B.SkillType);
			return KA != KB ? KA < KB : A.Skill_ID < B.Skill_ID;
		});
		for (const FNUJobSkillDefinition* Skill : Skills)
		{
			UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), SkillRowClass);
			if (Row == nullptr)
			{
				continue;
			}
			// 종류 칩 색: 패시브 = 흐린 글자색, 액티브 = 직업 색, 시너지 = 금
			const int32 Kind = KindOrder(Skill->SkillType);
			const FLinearColor KindColor = Kind == 0 ? FLinearColor(0.40f, 0.47f, 0.52f) : Kind == 1 ? Accent : FLinearColor(0.87f, 0.68f, 0.29f);
			if (UTextBlock* KindText = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("SkillKind"))))
			{
				KindText->SetText(FText::FromName(Skill->SkillType));
				KindText->SetColorAndOpacity(FSlateColor(KindColor));
			}
			if (UBorder* Chip = Cast<UBorder>(Row->GetWidgetFromName(TEXT("SkillKindChip"))))
			{
				Chip->SetBrushColor(KindColor);
			}
			if (UTextBlock* NameText = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("SkillName"))))
			{
				NameText->SetText(Skill->Name_KR);
			}
			if (UTextBlock* DescText = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("SkillDesc"))))
			{
				DescText->SetText(Skill->Description);
				Row->SetToolTipText(Skill->Description);   // 한 줄에 다 안 들어가는 설명은 마우스를 올려 본다
			}
			SkillList->AddChild(Row);
		}
	}

	// 전직 갈래 (2차)
	if (AdvancementList != nullptr && AdvancementChipClass != nullptr)
	{
		AdvancementList->ClearChildren();
		if (Data != nullptr && JobId > 0)
		{
			for (const FNUJobAdvancementDefinitionRow* Advancement : Data->GetAdvancementsByJob(JobId))
			{
				if (Advancement == nullptr || Advancement->ParentAdvancementId != 0)
				{
					continue;
				}
				UUserWidget* Chip = CreateWidget<UUserWidget>(GetOwningPlayer(), AdvancementChipClass);
				if (UTextBlock* ChipText = Chip != nullptr ? Cast<UTextBlock>(Chip->GetWidgetFromName(TEXT("ChipText"))) : nullptr)
				{
					ChipText->SetText(Advancement->DisplayName);
					AdvancementList->AddChild(Chip);
				}
			}
		}
	}
}

void UNUJobSelectionWidget::StepSelection(int32 Step)
{
	if (JobEntries.IsEmpty() || (JobComponent.IsValid() && JobComponent->HasJob()))
	{
		return;
	}
	const int32 Current = JobEntries.IndexOfByPredicate([this](const FNUJobSelectionViewData& Entry) { return Entry.JobId == SelectedJobId; });
	const int32 Next = Current == INDEX_NONE ? 0 : FMath::Clamp(Current + Step, 0, JobEntries.Num() - 1);
	SelectJob(JobEntries[Next].JobId);
	if (const TObjectPtr<UNUJobSelectionCardWidget>* Card = JobCards.Find(SelectedJobId))
	{
		if (*Card != nullptr && (*Card)->GetFocusTarget() != nullptr)
		{
			(*Card)->GetFocusTarget()->SetFocus();   // 패드 포커스도 고른 칸을 따라간다
		}
	}
}

FReply UNUJobSelectionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (UsesStage() && ActiveConfirmDialog == nullptr)
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::A)
		{
			StepSelection(-1);
			return FReply::Handled();
		}
		if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::D)
		{
			StepSelection(1);
			return FReply::Handled();
		}
		if (Key == EKeys::F || Key == EKeys::Gamepad_FaceButton_Left)
		{
			const UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton"));
			if (ConfirmWidget != nullptr && ConfirmWidget->GetIsEnabled())
			{
				HandleConfirmClicked();
			}
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
