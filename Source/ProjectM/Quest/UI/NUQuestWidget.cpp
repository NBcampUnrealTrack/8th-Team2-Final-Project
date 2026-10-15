#include "Quest/UI/NUQuestWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterState/NUPlayerState.h"
#include "Field/NPC/Data/NUNPCDataSubsystem.h"
#include "Field/NPC/Data/NUNPCDefinition.h"
#include "Field/NPC/NUNPCBase.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/UI/NUItemUI.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Component/NUQuestSceneComponent.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Encounter/NUQuestEncounterSubsystem.h"

#include "Quest/UI/NUQuestTrackerEntryWidget.h"
#include "Quest/UI/NUQuestJournalPresentation.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NURewardPopup.h"
#include "UI/NUUIManager.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "TimerManager.h"

namespace NUQuestWidget
{
	const FName CursorReason(TEXT("QuestOffer"));
	const FName InputBlockReason(TEXT("QuestOffer"));

	const UNUQuestDefinition* FindDefinition(const UWorld* World, FName QuestId)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		const UNUQuestDataSubsystem* DataSubsystem 
			= GameInstance != nullptr
			? GameInstance->GetSubsystem<UNUQuestDataSubsystem>()
			: nullptr;

		return DataSubsystem != nullptr ? DataSubsystem->FindDefinition(QuestId) : nullptr;
	}

	bool AreEquivalentDisplayTexts(const FText& Left, const FText& Right)
	{
		return !Left.IsEmpty()
			&& !Right.IsEmpty()
			&& Left.ToString().TrimStartAndEnd().Equals(Right.ToString().TrimStartAndEnd(), ESearchCase::CaseSensitive);
	}

	// 의뢰인 얼굴: NPC 정의의 초상이 있으면 그것, 없으면 월드에 선 그 NPC의 지금 스프라이트 윗부분(머리)을 잘라 그린다
	bool ApplyGiverPortrait(UImage* Image, const UWorld* World, int32 NpcId, const FNUNPCDefinition* Definition)
	{
		if (Image == nullptr)
		{
			return false;
		}
		if (UTexture2D* Portrait = Definition != nullptr && !Definition->Portrait.IsNull() ? Definition->Portrait.LoadSynchronous() : nullptr)
		{
			Image->SetBrushResourceObject(Portrait);
			return true;
		}
		// 월드에 선 그 NPC의 지금 그림, 없으면(다른 구역 · 아직 안 불린 NPC) 마을 NPC 대기 플립북 이름 규칙
		const UPaperFlipbook* Book = nullptr;
		for (TActorIterator<ANUNPCBase> It(World); It && Book == nullptr; ++It)
		{
			if (It->GetNpcId() != NpcId)
			{
				continue;
			}
			TInlineComponentArray<UPaperFlipbookComponent*> Components(*It);
			for (UPaperFlipbookComponent* Component : Components)
			{
				if (Component->GetFlipbook() != nullptr && (Book == nullptr || Component->GetFName() == TEXT("Sprite")))
				{
					Book = Component->GetFlipbook();
				}
			}
		}
		if (Book == nullptr)
		{
			Book = LoadObject<UPaperFlipbook>(nullptr, *FString::Printf(
				TEXT("/Game/Art/Sprites/NPC/Libergrove/Flipbooks/FB_NPC_%d_Idle.FB_NPC_%d_Idle"), NpcId, NpcId), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		const UPaperSprite* Sprite = Book != nullptr ? Book->GetSpriteAtFrame(0) : nullptr;
		if (Sprite == nullptr)
		{
			return false;
		}
		const FSlateAtlasData Atlas = Sprite->GetSlateAtlasData();
		const UTexture2D* Texture = Cast<UTexture2D>(Atlas.AtlasTexture);
		if (Texture == nullptr || Atlas.SizeUV.X <= 0.f || Atlas.SizeUV.Y <= 0.f)
		{
			return false;
		}
		// 정사각 머리 칸: 그림 높이의 44%, 가로 가운데, 위에서 2% 아래부터
		const FIntPoint TextureSize = Texture->GetImportedSize();
		const float PixelW = Atlas.SizeUV.X * TextureSize.X;
		const float PixelH = Atlas.SizeUV.Y * TextureSize.Y;
		const float Side = FMath::Min(PixelH * 0.44f, PixelW);
		const FVector2f Start(Atlas.StartUV.X + (PixelW - Side) * 0.5f / TextureSize.X, Atlas.StartUV.Y + PixelH * 0.02f / TextureSize.Y);
		const FVector2f Size(Side / TextureSize.X, Side / TextureSize.Y);
		FSlateBrush Brush = Image->GetBrush();
		Brush.SetResourceObject(const_cast<UTexture2D*>(Texture));
		Brush.SetUVRegion(FBox2f(Start, Start + Size));
		Image->SetBrush(Brush);
		return true;
	}

	// 머리말 "새 의뢰 · 의뢰인"과 의뢰인 얼굴 (QO_GiverMedal · QO_GiverPortrait). 둘 다 없는 옛 WBP는 그대로
	void PopulateOfferGiver(UUserWidget& Owner, const UNUQuestDefinition& Definition)
	{
		UWidget* Medal = Owner.GetWidgetFromName(TEXT("QO_GiverMedal"));
		UTextBlock* Cap = Cast<UTextBlock>(Owner.GetWidgetFromName(TEXT("OfferCap")));
		if (Medal == nullptr)
		{
			return;
		}
		int32 NpcId = 0;
		LexTryParseString(NpcId, *Definition.OfferActorId.ToString());
		const UGameInstance* GameInstance = Owner.GetGameInstance();
		const UNUNPCDataSubsystem* Npcs = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUNPCDataSubsystem>() : nullptr;
		const FNUNPCDefinition* Npc = Npcs != nullptr && NpcId > 0 ? Npcs->FindDefinition(NpcId) : nullptr;
		const bool bPortrait = NpcId > 0 && ApplyGiverPortrait(Cast<UImage>(Owner.GetWidgetFromName(TEXT("QO_GiverPortrait"))), Owner.GetWorld(), NpcId, Npc);
		Medal->SetVisibility(bPortrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Cap != nullptr)
		{
			Cap->SetText(Npc != nullptr && !Npc->DisplayName.IsEmpty()
				? FText::Format(NSLOCTEXT("NUQuest", "OfferCapGiver", "새 의뢰 · {0}"), Npc->DisplayName)
				: NSLOCTEXT("NUQuest", "OfferCap", "새 의뢰"));
		}
	}

	// Optional designer-authored templates keep legacy offer layouts working.
	void PopulateOfferDetails(UUserWidget& Owner, const UNUQuestDefinition& Definition,
		const FNUQuestProgressState* ExistingProgress, TSubclassOf<UUserWidget> RewardTooltipClass)
	{
		if (UTextBlock* Location = Cast<UTextBlock>(Owner.GetWidgetFromName(TEXT("QO_LocationText"))))
		{
			const FText Name = NUQuestJournalPresentation::GetLocationText(Owner.GetWorld(), &Definition);
			Location->SetText(Name);
			if (UWidget* Row = Owner.GetWidgetFromName(TEXT("QO_LocationRow")))
			{
				Row->SetVisibility(Name.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			}
		}

		UVerticalBox* Objectives = Cast<UVerticalBox>(Owner.GetWidgetFromName(TEXT("QO_ObjectiveRows")));
		UUserWidget* ObjectiveTemplate = Cast<UUserWidget>(Owner.GetWidgetFromName(TEXT("QO_ObjectiveTemplate")));
		bool bHasObjectives = false;
		if (Objectives && ObjectiveTemplate)
		{
			Objectives->ClearChildren();
			FNUQuestProgressState Preview;
			Preview.Initialize(Definition, ENUQuestState::Active);
			const FNUQuestProgressState& Progress = ExistingProgress ? *ExistingProgress : Preview;
			for (const FNUQuestObjectiveSpec& Objective : Definition.Objectives)
			{
				if (!Progress.IsObjectiveOpenForDisplay(Definition, Objective)) { continue; }
				UUserWidget* Row = CreateWidget<UUserWidget>(Owner.GetOwningPlayer(), ObjectiveTemplate->GetClass());
				UTextBlock* Label = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("ObjectiveLabel"))) : nullptr;
				UCheckBox* Check = Row ? Cast<UCheckBox>(Row->GetWidgetFromName(TEXT("ObjectiveCheck"))) : nullptr;
				if (!Label || !Check) { Objectives->ClearChildren(); break; }
				FText Description = Objective.Description.IsEmpty()
					? NSLOCTEXT("NUQuest", "UnnamedObjective", "목표 확인하기") : Objective.Description;
				if (Objective.bOptional)
				{
					Description = FText::Format(NSLOCTEXT("NUQuest", "OfferOptionalObjective", "{0} · 선택"), Description);
				}
				Label->SetText(Description);
				const FNUQuestObjectiveProgress* Current = Progress.FindObjectiveProgress(Objective.ObjectiveId);
				Check->SetIsChecked(Current && Current->IsComplete(Objective.RequiredCount));
				if (UWidget* Counter = Row->GetWidgetFromName(TEXT("ObjectiveCount")))
				{
					Counter->SetVisibility(ESlateVisibility::Collapsed);
				}
				Row->SetVisibility(ESlateVisibility::HitTestInvisible);
				Objectives->AddChildToVerticalBox(Row);
			}
			bHasObjectives = Objectives->GetChildrenCount() > 0;
			Objectives->SetVisibility(bHasObjectives ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UWidget* Fallback = Owner.GetWidgetFromName(TEXT("OfferObjectiveText")))
		{
			Fallback->SetVisibility(bHasObjectives ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}

		UWrapBox* Rewards = Cast<UWrapBox>(Owner.GetWidgetFromName(TEXT("QO_RewardRows")));
		UUserWidget* RewardTemplate = Cast<UUserWidget>(Owner.GetWidgetFromName(TEXT("QO_RewardTemplate")));
		bool bHasRewards = false;
		if (Rewards && RewardTemplate)
		{
			Rewards->ClearChildren();
			const ANUPlayerState* PlayerState = Owner.GetOwningPlayerState<ANUPlayerState>();
			const UNUInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
			for (const FNUQuestRewardSpec& Reward : Definition.Rewards)
			{
				UUserWidget* Row = CreateWidget<UUserWidget>(Owner.GetOwningPlayer(), RewardTemplate->GetClass());
				UTextBlock* Label = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("RewardLabel"))) : nullptr;
				UTextBlock* Amount = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("RewardAmount"))) : nullptr;
				UWidgetSwitcher* Icon = Row ? Cast<UWidgetSwitcher>(Row->GetWidgetFromName(TEXT("RewardIconSwitcher"))) : nullptr;
				if (!Label || !Amount || !Icon) { Rewards->ClearChildren(); break; }
				const bool bExperience = Reward.RewardType == NU::Quest::Reward::Experience;
				const bool bGold = Reward.RewardType == NU::Quest::Reward::Gold;
				const bool bItem = Reward.RewardType == NU::Quest::Reward::Item;
				FText ItemName = Reward.DisplayName;
				int32 ItemId = 0;
				const bool bHasItemId = bItem && Inventory && LexTryParseString(ItemId, *Reward.TargetId.ToString());
				const FNUItemDefRow* ItemDef = bHasItemId ? Inventory->FindItemDefByNumericId(ItemId) : nullptr;
				if (bItem && ItemName.IsEmpty() && ItemDef != nullptr) { ItemName = ItemDef->DisplayName; }
				// 아이템 칸은 그 아이템 그림 (가방 기호 대신). 그림이 없는 아이템이면 칸의 기본 그림 그대로
				if (UImage* ItemImage = bHasItemId ? Cast<UImage>(Row->GetWidgetFromName(TEXT("RewardItem"))) : nullptr)
				{
					if (UTexture2D* ItemIcon = NUItemUI::LoadItemIcon(Inventory->FindRowNameByNumericId(ItemId)))
					{
						ItemImage->SetBrushResourceObject(ItemIcon);
					}
				}
				// 아이템 설명 말풍선 --- 말풍선이 뜨려면 칸이 마우스를 받아야 한다
				UUserWidget* Tip = ItemDef != nullptr && RewardTooltipClass != nullptr
					? CreateWidget<UUserWidget>(Owner.GetOwningPlayer(), RewardTooltipClass) : nullptr;
				if (Tip != nullptr)
				{
					if (UTextBlock* TipName = Cast<UTextBlock>(Tip->GetWidgetFromName(TEXT("TipName")))) { TipName->SetText(ItemDef->DisplayName); }
					if (UTextBlock* TipKind = Cast<UTextBlock>(Tip->GetWidgetFromName(TEXT("TipKind")))) { TipKind->SetText(NUItemUI::GetCategoryText(*ItemDef)); }
					if (UTextBlock* TipDesc = Cast<UTextBlock>(Tip->GetWidgetFromName(TEXT("TipDesc")))) { TipDesc->SetText(ItemDef->Description); }
					Row->SetToolTip(Tip);
				}
				Label->SetText(bExperience ? NSLOCTEXT("NUQuestJournal", "ExperienceLabel", "경험치")
					: bGold ? NSLOCTEXT("NUQuestJournal", "GoldLabel", "골드")
					: bItem ? (ItemName.IsEmpty() ? NSLOCTEXT("NUQuest", "ItemRewardFallback", "아이템") : ItemName)
					: Reward.GetDisplayText());
				Amount->SetText(bExperience || bGold || bItem ? FText::AsNumber(Reward.Amount) : FText::GetEmpty());
				Amount->SetVisibility(bExperience || bGold || bItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				Icon->SetActiveWidgetIndex(bExperience ? 0 : bGold ? 1 : 2);
				Row->SetVisibility(Tip != nullptr ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
				Rewards->AddChildToWrapBox(Row);
			}
			bHasRewards = Rewards->GetChildrenCount() > 0;
			Rewards->SetVisibility(bHasRewards ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UWidget* Heading = Owner.GetWidgetFromName(TEXT("QO_RewardHeading")))
		{
			Heading->SetVisibility(bHasRewards ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UWidget* Fallback = Owner.GetWidgetFromName(TEXT("OfferRewardText")))
		{
			Fallback->SetVisibility(bHasRewards ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		if (UScrollBox* Scroll = Cast<UScrollBox>(Owner.GetWidgetFromName(TEXT("OfferBodyScroll"))))
		{
			Scroll->ScrollToStart();
		}
	}
}

void UNUQuestWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BattleFold.Tick(this, InDeltaTime);
	QuestFx.Tick(this, InDeltaTime);

	// 완료 띠 뒤로 미룬 보상 창
	for (int32 Index = 0; Index < PendingRewards.Num();)
	{
		if (!QuestFx.HasStarted(PendingRewards[Index].FxId))
		{
			++Index;
			continue;
		}
		PendingRewards[Index].Delay -= InDeltaTime;
		if (PendingRewards[Index].Delay > 0.f)
		{
			++Index;
			continue;
		}
		const FPendingReward Reward = PendingRewards[Index];
		PendingRewards.RemoveAt(Index);
		UNURewardPopup::Show(GetOwningPlayer(), RewardPopupClass, Reward.Title, Reward.Entries);
	}
}

void UNUQuestWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnFieldHUDVisibilityChanged.RemoveAll(this);
		UI->OnFieldHUDVisibilityChanged.AddUObject(this, &ThisClass::RefreshTracker);
	}

	// 명명 계약: 순정 Button(레거시)과 UNU_CommonButtonWidget(신규 공용 부품) 둘 다 지원해야 해서
	// BindWidget 타입을 UWidget으로 열어두고 여기서 캐스팅해 알맞은 델리게이트에 묶는다
	if (UButton* AcceptAsButton = Cast<UButton>(AcceptButton))
	{
		AcceptAsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAcceptClicked);
	}
	else if (UNU_CommonButtonWidget* AcceptAsCommon = Cast<UNU_CommonButtonWidget>(AcceptButton))
	{
		AcceptAsCommon->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleAcceptClicked);
	}

	if (UButton* DeclineAsButton = Cast<UButton>(DeclineButton))
	{
		DeclineAsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeclineClicked);
	}
	else if (UNU_CommonButtonWidget* DeclineAsCommon = Cast<UNU_CommonButtonWidget>(DeclineButton))
	{
		DeclineAsCommon->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleDeclineClicked);
	}

	if (UButton* CollapseAsButton = Cast<UButton>(TrackerCollapseButton))
	{
		CollapseAsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTrackerCollapseClicked);
	}
	else if (UNU_CommonButtonWidget* CollapseAsCommon = Cast<UNU_CommonButtonWidget>(TrackerCollapseButton))
	{
		CollapseAsCommon->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleTrackerCollapseClicked);
	}

	BindTrackerCloseButton();

	if (UButton* ReopenAsButton = Cast<UButton>(TrackerReopenButton))
	{
		ReopenAsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTrackerReopenClicked);
	}
	else if (UNU_CommonButtonWidget* ReopenAsCommon = Cast<UNU_CommonButtonWidget>(TrackerReopenButton))
	{
		ReopenAsCommon->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleTrackerReopenClicked);
	}

	SetOfferVisible(false);
	if (UNUQuestEncounterSubsystem* Encounters = GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>())
	{
		Encounters->OnPracticeStateChanged.RemoveAll(this);
		Encounters->OnPracticeStateChanged.AddUObject(this, &ThisClass::RefreshTracker);
	}
	RefreshQuestComponent();
}

void UNUQuestWidget::NativeDestruct()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnFieldHUDVisibilityChanged.RemoveAll(this);
		UI->ForgetFieldHUDWidget(TrackerPanel);
		UI->ForgetFieldHUDWidget(TrackerReopenButton);
	}
	if (GetGameInstance() != nullptr)
	{
		if (UNUQuestEncounterSubsystem* Encounters = GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>())
		{
			Encounters->OnPracticeStateChanged.RemoveAll(this);
		}
	}

	if (UButton* AcceptAsButton = Cast<UButton>(AcceptButton))
	{
		AcceptAsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleAcceptClicked);
	}
	else if (UNU_CommonButtonWidget* AcceptAsCommon = Cast<UNU_CommonButtonWidget>(AcceptButton))
	{
		AcceptAsCommon->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleAcceptClicked);
	}

	if (UButton* DeclineAsButton = Cast<UButton>(DeclineButton))
	{
		DeclineAsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDeclineClicked);
	}
	else if (UNU_CommonButtonWidget* DeclineAsCommon = Cast<UNU_CommonButtonWidget>(DeclineButton))
	{
		DeclineAsCommon->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleDeclineClicked);
	}

	if (UButton* CollapseAsButton = Cast<UButton>(TrackerCollapseButton))
	{
		CollapseAsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleTrackerCollapseClicked);
	}
	else if (UNU_CommonButtonWidget* CollapseAsCommon = Cast<UNU_CommonButtonWidget>(TrackerCollapseButton))
	{
		CollapseAsCommon->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleTrackerCollapseClicked);
	}

	UnbindTrackerCloseButton();

	if (UButton* ReopenAsButton = Cast<UButton>(TrackerReopenButton))
	{
		ReopenAsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleTrackerReopenClicked);
	}
	else if (UNU_CommonButtonWidget* ReopenAsCommon = Cast<UNU_CommonButtonWidget>(TrackerReopenButton))
	{
		ReopenAsCommon->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleTrackerReopenClicked);
	}

	BindQuestComponent(nullptr);
	SetOfferVisible(false);

	// 띠를 기다리던 보상 창은 지금 띄운다 (받은 보상을 못 보고 지나가지 않게)
	QuestFx.Reset(this);
	for (const FPendingReward& Reward : PendingRewards)
	{
		UNURewardPopup::Show(GetOwningPlayer(), RewardPopupClass, Reward.Title, Reward.Entries);
	}
	PendingRewards.Reset();

	Super::NativeDestruct();
}

void UNUQuestWidget::RefreshQuestComponent()
{
	APlayerController* OwningPlayer = GetOwningPlayer();
	ANUPlayerState* PlayerState = OwningPlayer != nullptr
		? OwningPlayer->GetPlayerState<ANUPlayerState>()
		: nullptr;

	BindQuestComponent(PlayerState != nullptr ? PlayerState->GetQuestComponent() : nullptr);
	RefreshTracker();
}

void UNUQuestWidget::BindQuestComponent(UNUQuestComponent* NewQuestComponent)
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
		BoundQuestComponent->OnQuestOfferRequested.RemoveDynamic(
			this,
			&ThisClass::HandleQuestOfferRequested
		);
		BoundQuestComponent->OnQuestAccepted.RemoveDynamic(
			this,
			&ThisClass::HandleQuestAccepted
		);
		BoundQuestComponent->OnQuestCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleQuestCompleted
		);
	}

	BoundQuestComponent = NewQuestComponent;

	if (IsValid(BoundQuestComponent))
	{
		BoundQuestComponent->OnQuestProgressChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleQuestProgressChanged
		);
		BoundQuestComponent->OnQuestOfferRequested.AddUniqueDynamic(
			this,
			&ThisClass::HandleQuestOfferRequested
		);
		BoundQuestComponent->OnQuestAccepted.AddUniqueDynamic(
			this,
			&ThisClass::HandleQuestAccepted
		);
		BoundQuestComponent->OnQuestCompleted.AddUniqueDynamic(
			this,
			&ThisClass::HandleQuestCompleted
		);
	}
}

void UNUQuestWidget::HandleQuestCompleted(FName QuestId)
{
	const UNUQuestDefinition* Definition = NUQuestWidget::FindDefinition(GetWorld(), QuestId);
	if (Definition == nullptr)
	{
		return;
	}

	// 플래그와 기능 해금은 내부용이라 표시하지 않는다
	TArray<FNURewardEntry> Entries;
	FNUQuestServerReward CompletionReward;
	const bool bHasCompletionReward = IsValid(BoundQuestComponent)
		&& BoundQuestComponent->TryGetLastCompletionReward(QuestId, CompletionReward);
	if (bHasCompletionReward)
	{
		if (CompletionReward.Experience > 0)
		{
			FNURewardEntry& Entry = Entries.AddDefaulted_GetRef();
			if (CompletionReward.Experience <= MAX_int32)
			{
				Entry.Label = NSLOCTEXT("NUQuest", "RewardExperience", "경험치");
				Entry.Amount = static_cast<int32>(CompletionReward.Experience);
			}
			else
			{
				Entry.Label = FText::Format(NSLOCTEXT("NUQuest", "RewardExperienceLarge", "경험치 {0}"), FText::AsNumber(CompletionReward.Experience));
			}
		}
		if (CompletionReward.Gold > 0)
		{
			FNURewardEntry& Entry = Entries.AddDefaulted_GetRef();
			if (CompletionReward.Gold <= MAX_int32)
			{
				Entry.Label = NSLOCTEXT("NUQuest", "RewardGold", "골드");
				Entry.Amount = static_cast<int32>(CompletionReward.Gold);
			}
			else
			{
				Entry.Label = FText::Format(NSLOCTEXT("NUQuest", "RewardGoldLarge", "골드 {0}"), FText::AsNumber(CompletionReward.Gold));
			}
		}
		if (CompletionReward.bHasGrowth && CompletionReward.Growth.IsLevelUp())
		{
			FNURewardEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Label = FText::Format(
				NSLOCTEXT("NUQuest", "RewardLevelUp", "레벨업 {0} → {1}"),
				FText::AsNumber(CompletionReward.Growth.OldLevel),
				FText::AsNumber(CompletionReward.Growth.NewLevel));
		}
	}
	else
	{
		for (const FNUQuestRewardSpec& Reward : Definition->Rewards)
		{
			if (Reward.RewardType != NU::Quest::Reward::Experience)
			{
				continue;
			}

			FNURewardEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Label = NSLOCTEXT("NUQuest", "RewardExperience", "경험치");
			Entry.Amount = Reward.Amount;
		}
	}

	// 로컬 완료는 정의에 적힌 아이템을 원자적으로 지급한 뒤 알림을 보낸다.
	if (IsValid(BoundQuestComponent) && !BoundQuestComponent->IsServerBackedSession())
	{
		const ANUPlayerState* PlayerState = GetOwningPlayerState<ANUPlayerState>();
		const UNUInventoryComponent* Inventory = PlayerState != nullptr ? PlayerState->GetInventoryComponent() : nullptr;

		for (const FNUQuestRewardSpec& Reward : Definition->Rewards)
		{
			if (Reward.RewardType != NU::Quest::Reward::Item || Reward.Amount <= 0)
			{
				continue;
			}

			int32 ItemId = 0;
			if (Inventory == nullptr || !LexTryParseString(ItemId, *Reward.TargetId.ToString()))
			{
				continue;
			}

			if (const FNUItemDefRow* Item = Inventory->FindItemDefByNumericId(ItemId))
			{
				FNURewardEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.Label = Item->DisplayName;
				Entry.Amount = Reward.Amount;
			}
		}
	}

	const FText Title = Definition->Title.IsEmpty() ? FText::FromName(QuestId) : Definition->Title;

	// 로컬 진행은 대화·전투와 마지막 목표의 후속 연출이 끝난 뒤 보상 창을 띄운다.
	APlayerController* PlayerController = GetOwningPlayer();
	UNUQuestSceneComponent* StoryComponent = PlayerController != nullptr ? PlayerController->FindComponentByClass<UNUQuestSceneComponent>() : nullptr;
	if (StoryComponent != nullptr
		&& StoryComponent->DeferQuestNotice(QuestId, FSimpleDelegate::CreateUObject(this, &ThisClass::ShowCompletionReward, Title, Entries)))
	{
		return;
	}

	ShowCompletionReward(Title, Entries);
}

void UNUQuestWidget::ShowCompletionReward(FText Title, TArray<FNURewardEntry> Entries)
{
	if (GetWidgetFromName(TEXT("QuestFxRoot")) == nullptr || RewardPopupDelay < 0.f)
	{
		UNURewardPopup::Show(GetOwningPlayer(), RewardPopupClass, Title, Entries);
		return;
	}

	const int32 FxId = QuestFx.Queue(FNUQuestFx::EKind::Completed, Title, BuildRewardSummary(Entries));
	if (RewardPopupClass != nullptr && !Entries.IsEmpty())
	{
		PendingRewards.Add({FxId, RewardPopupDelay, Title, MoveTemp(Entries)});
	}
}

void UNUQuestWidget::DebugPlayQuestFx(int32 Kind, const FText& Title)
{
	UNUQuestTrackerEntryWidget* First = TrackerEntries.Num() > 0 ? TrackerEntries[0].Get() : nullptr;
	if (Kind == 2)
	{
		TArray<FNURewardEntry> Entries;
		FNURewardEntry& Exp = Entries.AddDefaulted_GetRef();
		Exp.Label = NSLOCTEXT("NUQuest", "RewardExperience", "경험치");
		Exp.Amount = 120;
		FNURewardEntry& Gold = Entries.AddDefaulted_GetRef();
		Gold.Label = NSLOCTEXT("NUQuest", "RewardGold", "골드");
		Gold.Amount = 50;
		QuestFx.Queue(FNUQuestFx::EKind::Completed, Title, BuildRewardSummary(Entries));
		return;
	}
	if (Kind == 1)
	{
		const FText Detail = NSLOCTEXT("NUQuest", "DebugReadyDetail", "촌장 에델에게 보고하기");
		if (First != nullptr)
		{
			// 보고 가능 모습만 미리 본다 (진행 상태는 그대로 --- 다음 추적 창 갱신 때 원래대로 돌아간다)
			First->SetQuestDisplay(First->GetQuestId(), First->GetQuestTitle(), Detail, true);
			First->PlayReadyFlash();
		}
		QuestFx.Queue(FNUQuestFx::EKind::Ready, Title, Detail);
		return;
	}
	if (First != nullptr)
	{
		First->PlayIntro();
	}
	QuestFx.Queue(FNUQuestFx::EKind::Accepted, Title, NSLOCTEXT("NUQuest", "DebugAcceptDetail", "리베르그로브 아랫마을에 도착하기"));
}

FText UNUQuestWidget::BuildRewardSummary(const TArray<FNURewardEntry>& Entries)
{
	TArray<FText> Parts;
	for (const FNURewardEntry& Entry : Entries)
	{
		if (Parts.Num() == 3)
		{
			break;
		}
		Parts.Add(Entry.Amount > 0
			? FText::Format(NSLOCTEXT("NUQuest", "RewardSummaryPart", "{0} {1}"), Entry.Label, FText::AsNumber(Entry.Amount))
			: Entry.Label);
	}
	return FText::Join(NSLOCTEXT("NUQuest", "RewardSummarySeparator", "  ·  "), Parts);
}

void UNUQuestWidget::HandleQuestAccepted(FName QuestId)
{
	bTrackerClosed = false;
	bTrackerExpanded = true;
	PendingIntroQuestId = QuestId;

	// 수락한 퀘스트를 트래커에 올린다.
	// 자동으로 고르지 않기로 했으므로, 이게 없으면 새 퀘스트가 트래커에 안 나타난다
	if (ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayer()))
	{
		PlayerController->TrackQuest(QuestId);
	}

	QuestFx.Queue(FNUQuestFx::EKind::Accepted, BuildTrackerTitleText(QuestId), BuildTrackerObjectiveText(QuestId));
}

void UNUQuestWidget::RefreshTracker()
{
	if (bRefreshingTracker)
	{
		if (!bTrackerRefreshQueued && GetWorld() != nullptr)
		{
			bTrackerRefreshQueued = true;
			GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				bTrackerRefreshQueued = false;
				RefreshTracker();
			}));
		}
		return;
	}
	TGuardValue<bool> RefreshGuard(bRefreshingTracker, true);
	if (!IsValid(TrackerPanel) || !IsValid(TrackerEntryBox) || TrackerEntryWidgetClass == nullptr)
	{
		ApplyTrackerPresentationState();
		return;
	}

	TArray<FName> TrackedQuestIds;
	CollectTrackedQuestIds(TrackedQuestIds);
	RebuildTrackerEntries(TrackedQuestIds);

	// 고정한 퀘스트가 없으면 트래커를 숨긴다.
	// 자동으로 하나 골라 채우지 않는다.
	// 플레이어가 전부 해제할 수 있어야 한다.
	ApplyTrackerPresentationState();
}

UNUQuestTrackerEntryWidget* UNUQuestWidget::FindTrackerEntry(FName QuestId) const
{
	for (UNUQuestTrackerEntryWidget* Entry : TrackerEntries)
	{
		if (IsValid(Entry) && Entry->GetQuestId() == QuestId)
		{
			return Entry;
		}
	}
	return nullptr;
}

void UNUQuestWidget::CollectTrackedQuestIds(TArray<FName>& OutQuestIds)
{
	OutQuestIds.Reset();

	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerController) || !IsValid(BoundQuestComponent))
	{
		return;
	}

	// 고정 목록을 순회하면서 제거하므로 사본을 돌린다
	const TArray<FName> TrackedQuestIds = PlayerController->GetTrackedQuestIds();

	TArray<FName> ReadyToTurnIn;
	TArray<FName> Active;

	for (const FName& QuestId : TrackedQuestIds)
	{
		const FNUQuestProgressState* Progress = BoundQuestComponent->FindQuestProgress(QuestId);

		if (Progress != nullptr && Progress->QuestState == ENUQuestState::ReadyToTurnIn)
		{
			ReadyToTurnIn.Add(QuestId);
			continue;
		}

		if (Progress != nullptr && Progress->QuestState == ENUQuestState::Active)
		{
			Active.Add(QuestId);
			continue;
		}

		if (Progress == nullptr)
		{
			UE_LOG(LogNUQuest, Warning, TEXT("트래커: 진행 정보가 없어 이번 갱신에서만 숨김 QuestId=%s"), *QuestId.ToString());
			continue;
		}

		// 완료·포기 등으로 표시할 수 없게 된 고정은 해제한다.
		// 갱신 중이므로 RefreshTracker를 다시 부르지 않는 쪽을 쓴다
		PlayerController->RemoveTrackedQuestSilent(QuestId);
	}

	// 보고 가능한 퀘스트를 위로 올린다. 그 안에서는 고정한 순서를 유지한다
	OutQuestIds.Reserve(ReadyToTurnIn.Num() + Active.Num());
	OutQuestIds.Append(ReadyToTurnIn);
	OutQuestIds.Append(Active);
}

void UNUQuestWidget::RebuildTrackerEntries(const TArray<FName>& QuestIds)
{
	TArray<TObjectPtr<UNUQuestTrackerEntryWidget>> NextEntries;
	TArray<FName> DisplayQuestIds;
	for (const FName& QuestId : QuestIds)
	{
		UNUQuestTrackerEntryWidget* Entry = FindTrackerEntry(QuestId);
		if (Entry == nullptr)
		{
			Entry = CreateWidget<UNUQuestTrackerEntryWidget>(this, TrackerEntryWidgetClass);
		}
		if (!IsValid(Entry))
		{
			continue;
		}
		NextEntries.Add(Entry);
		DisplayQuestIds.Add(QuestId);
	}

	const bool bEntriesChanged = NextEntries != TrackerEntries;
	TArray<FName> RemovedQuestIds;
	TArray<TObjectPtr<UNUQuestTrackerEntryWidget>> AddedEntries;
	if (bEntriesChanged)
	{
		TMap<UNUQuestTrackerEntryWidget*, UPanelSlot*> PreviousSlots;
		TArray<TSharedPtr<SWidget>> RetainedSlateWidgets;
		for (UNUQuestTrackerEntryWidget* Entry : TrackerEntries)
		{
			if (!IsValid(Entry))
			{
				continue;
			}
			if (NextEntries.Contains(Entry))
			{
				PreviousSlots.Add(Entry, Entry->Slot);
				RetainedSlateWidgets.Add(Entry->GetCachedWidget());
			}
			else
			{
				RemovedQuestIds.Add(Entry->GetQuestId());
			}
		}

		// InsertChildAt은 실행 중 Slate 순서를 갱신하지 않는다. 기존 위젯과 슬롯 설정을 보존해 다시 붙인다.
		TrackerEntryBox->ClearChildren();
		for (UNUQuestTrackerEntryWidget* Entry : NextEntries)
		{
			UPanelSlot* PreviousSlot = PreviousSlots.FindRef(Entry);
			UPanelSlot* EntrySlot = TrackerEntryBox->AddChild(Entry, PreviousSlot);
			if (PreviousSlot == nullptr)
			{
				if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(EntrySlot))
				{
					VerticalSlot->SetPadding(TrackerEntryPadding);
				}
				AddedEntries.Add(Entry);
			}
		}
		TrackerEntries = MoveTemp(NextEntries);
	}

	for (int32 Index = 0; Index < TrackerEntries.Num(); ++Index)
	{
		const FName QuestId = DisplayQuestIds[Index];
		const FNUQuestProgressState* Progress = IsValid(BoundQuestComponent)
			? BoundQuestComponent->FindQuestProgress(QuestId)
			: nullptr;
		const bool bReadyToTurnIn = Progress != nullptr && Progress->QuestState == ENUQuestState::ReadyToTurnIn;

		// 이미 보이던 줄이 보고 가능으로 바뀐 순간만 (새로 붙은 줄·처음 그리기는 제외)
		UNUQuestTrackerEntryWidget* Entry = TrackerEntries[Index];
		const bool bBecameReady = bReadyToTurnIn && Entry->GetQuestId() == QuestId && !Entry->IsReadyToTurnIn();

		const FText Title = BuildTrackerTitleText(QuestId);
		const FText Objective = BuildTrackerObjectiveText(QuestId);
		Entry->SetQuestDisplay(QuestId, Title, Objective, bReadyToTurnIn);
		if (bBecameReady)
		{
			Entry->PlayReadyFlash();
			QuestFx.Queue(FNUQuestFx::EKind::Ready, Title, Objective);
		}
	}
	for (const FName QuestId : RemovedQuestIds)
	{
		BP_OnTrackerEntryRemoved(QuestId);
	}
	for (UNUQuestTrackerEntryWidget* Entry : AddedEntries)
	{
		if (!PendingIntroQuestId.IsNone() && Entry->GetQuestId() == PendingIntroQuestId)
		{
			Entry->PlayIntro();
			PendingIntroQuestId = NAME_None;
		}
		BP_OnTrackerEntryAdded(Entry);
	}
	if (bEntriesChanged)
	{
		BP_OnTrackerEntriesChanged();
	}
}

void UNUQuestWidget::ApplyTrackerPresentationState()
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	const bool bHasEntries = IsValid(TrackerEntryBox) && TrackerEntryWidgetClass != nullptr && !TrackerEntries.IsEmpty();
	const auto Present = [UI](UWidget* Widget, ESlateVisibility NormalVisibility)
	{
		if (UI)
		{
			UI->PresentFieldHUDWidget(Widget, NormalVisibility);
		}
		else
		{
			Widget->SetVisibility(NormalVisibility);
		}
	};
	if (IsValid(TrackerPanel))
	{
		Present(TrackerPanel, !bHasEntries || bTrackerClosed ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (IsValid(TrackerReopenButton))
	{
		Present(TrackerReopenButton, bHasEntries && bTrackerClosed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (IsValid(TrackerEntryBox))
	{
		TrackerEntryBox->SetVisibility(!bTrackerClosed && bTrackerExpanded
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (IsValid(TrackerContent))
	{
		TrackerContent->SetVisibility(!bTrackerClosed && bTrackerExpanded
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	const FText CollapseIconText = !bTrackerExpanded
		? NSLOCTEXT("NUQuest", "TrackerExpandIcon", "▼")
		: NSLOCTEXT("NUQuest", "TrackerCollapseIcon", "▲");

	if (UButton* CollapseAsButton = Cast<UButton>(TrackerCollapseButton))
	{
		if (UTextBlock* IconText = Cast<UTextBlock>(CollapseAsButton->GetChildAt(0)))
		{
			IconText->SetText(CollapseIconText);
		}
	}
	else if (UNU_CommonButtonWidget* CollapseAsCommon = Cast<UNU_CommonButtonWidget>(TrackerCollapseButton))
	{
		CollapseAsCommon->SetLabelText(CollapseIconText);
	}
}

FText UNUQuestWidget::BuildTrackerTitleText(FName QuestId) const
{
	const UNUQuestDefinition* Definition = NUQuestWidget::FindDefinition(GetWorld(), QuestId);

	return Definition != nullptr && !Definition->Title.IsEmpty()
		? Definition->Title
		: FText::FromName(QuestId);
}

void UNUQuestWidget::ShowQuestOffer(FName QuestId)
{
	const UNUQuestDefinition* Definition = NUQuestWidget::FindDefinition(GetWorld(), QuestId);
	if (Definition == nullptr || !IsValid(OfferPanel))
	{
		return;
	}

	OfferedQuestId = QuestId;

	if (IsValid(OfferTitleText))
	{
		OfferTitleText->SetText(Definition->Title.IsEmpty() ? FText::FromName(QuestId) : Definition->Title);
	}

	if (IsValid(OfferSummaryText))
	{
		const FText Summary = !Definition->Summary.IsEmpty() ? Definition->Summary : Definition->Description;
		// 목록에 보이는 첫 목표와 같은 문장이면 요약을 반복하지 않는다.
		const FNUQuestObjectiveSpec* FirstVisible = Definition->Objectives.FindByPredicate([](const FNUQuestObjectiveSpec& Objective) { return !Objective.bHideInObjectiveList; });
		const FText ObjectiveDescription = FirstVisible != nullptr ? FirstVisible->Description : FText::GetEmpty();
		const bool bHideDuplicateSummary = NUQuestWidget::AreEquivalentDisplayTexts(Summary, ObjectiveDescription);
		OfferSummaryText->SetText(Summary);
		OfferSummaryText->SetVisibility(!Summary.IsEmpty() && !bHideDuplicateSummary
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (IsValid(OfferObjectiveText))
	{
		OfferObjectiveText->SetText(BuildObjectiveText(QuestId));
	}

	if (IsValid(OfferRewardText))
	{
		OfferRewardText->SetText(BuildRewardText(QuestId));
	}

	NUQuestWidget::PopulateOfferDetails(*this, *Definition,
		IsValid(BoundQuestComponent) ? BoundQuestComponent->FindQuestProgress(QuestId) : nullptr, RewardTooltipClass);
	NUQuestWidget::PopulateOfferGiver(*this, *Definition);

	UNUSFXSubsystem::Play(this, NU::SFX::Quest::Offer);
	SetOfferVisible(true);
}

void UNUQuestWidget::CloseQuestOffer()
{
	SetOfferVisible(false);
}

void UNUQuestWidget::SetOfferVisible(bool bVisible)
{
	if (IsValid(OfferPanel))
	{
		OfferPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->SetPopupButtonHover(OfferPanel, bVisible);
		if (bHideFieldHUDDuringOffer)
		{
			UI->RequestFieldHUDHidden(NUQuestWidget::CursorReason, bVisible, this);
		}
	}

	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayer());
	if (IsValid(PlayerController))
	{
		if (bVisible)
		{
			PlayerController->CloseQuestJournal();

			// 패드 모드면 수락 버튼에 포커스를 주고 D패드가 패널 밖으로 못 나가게 한다 (9/15 패드).
			// 매니저 창이 아니라 매니저가 못 주므로 여기서 직접
			if (PlayerController->IsGamepadMode() && IsValid(OfferPanel))
			{
				OfferPanel->SetAllNavigationRules(EUINavigationRule::Stop, NAME_None);
				UNUUIManager::FocusFirstButtonIn(OfferPanel);
			}
		}

		PlayerController->RequestCursor(NUQuestWidget::CursorReason, bVisible, this);
		PlayerController->RequestGameInputBlock(NUQuestWidget::InputBlockReason, bVisible, this);
	}

	if (!bVisible)
	{
		// 게시판 의뢰처럼 수락해야 목표가 채워지는 의뢰는 나중에를 고르면 대기도 지운다.
		if (IsValid(BoundQuestComponent) && !OfferedQuestId.IsNone())
		{
			BoundQuestComponent->CancelObjectiveOffer(OfferedQuestId);
		}
		OfferedQuestId = NAME_None;
	}
}

FText UNUQuestWidget::BuildTrackerObjectiveText(FName QuestId) const
{
	const UNUQuestDefinition* Definition = NUQuestWidget::FindDefinition(GetWorld(), QuestId);
	const FNUQuestProgressState* Progress = IsValid(BoundQuestComponent) ? BoundQuestComponent->FindQuestProgress(QuestId) : nullptr;
	if (Definition == nullptr || Progress == nullptr || !Progress->IsInProgress()) { return FText::GetEmpty(); }

	const UGameInstance* GameInstance = GetGameInstance();
	if (Progress->QuestState == ENUQuestState::ReadyToTurnIn)
	{
		// 보고 NPC와의 대화로 끝나는 퀘스트는 서버가 완료를 확정하는 동안에도 그 대화 목표 줄을 그대로 보여준다.
		if (const FNUQuestObjectiveSpec* TurnInTalk = Definition->FindTurnInTalkObjective())
		{
			return TurnInTalk->GetProgressText(TurnInTalk->RequiredCount);
		}

		const UNUQuestDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
		return Data != nullptr ? Data->GetTurnInDisplayText(QuestId) : FText::GetEmpty();
	}

	const UNUQuestEncounterSubsystem* Encounters = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestEncounterSubsystem>() : nullptr;
	const FNUQuestBattleServerState* Practice = Encounters != nullptr ? &Encounters->GetQuestBattleState() : nullptr;
	const bool bShowPractice = Practice != nullptr && Practice->QuestId > 0
		&& QuestId == FName(*FString::FromInt(Practice->QuestId))
		&& NUQuestBattle::IsPracticeKind(NUQuestBattle::FromServerBattleMode(Practice->BattleMode)) && Practice->PracticeRequired > 0;
	const FName PracticeObjectiveId = bShowPractice ? FName(*FString::FromInt(Practice->ObjectiveId)) : NAME_None;

	TArray<FString> Lines;
	Lines.Reserve(Definition->Objectives.Num());
	for (const FNUQuestObjectiveSpec& Objective : Definition->Objectives)
	{
		const FNUQuestObjectiveProgress* ObjectiveProgress = Progress->FindObjectiveProgress(Objective.ObjectiveId);
		// 완료한 목표는 지우고, 선행 조건을 충족한 목표는 모두 함께 표시한다.
		// 준비 단계("○○의 심부름")가 진행 중이면 그 줄만 보이고, 대화를 마치면 뒤 목표 줄이 나온다.
		if ((ObjectiveProgress != nullptr && ObjectiveProgress->IsComplete(Objective.RequiredCount))
			|| !Progress->IsObjectiveOpenForDisplay(*Definition, Objective))
		{
			continue;
		}

		FText Line = Objective.GetProgressText(ObjectiveProgress != nullptr ? ObjectiveProgress->CurrentCount : 0);
		// 연습 중에는 해당 목표 줄에 서버 횟수를 표시한다. 저장된 진행도는 변경하지 않는다.
		if (bShowPractice && Objective.ObjectiveId == PracticeObjectiveId)
		{
			const ENUQuestBattleKind PracticeKind = NUQuestBattle::FromServerBattleMode(Practice->BattleMode);
			const FText Label = PracticeKind == ENUQuestBattleKind::Parry
				? NSLOCTEXT("NUQuest", "ParryPractice", "패링 연습")
				: PracticeKind == ENUQuestBattleKind::Dodge
				? NSLOCTEXT("NUQuest", "DodgePractice", "회피 연습")
				: NSLOCTEXT("NUQuest", "AttackPractice", "기본 공격 연습");
			Line = FText::Format(NSLOCTEXT("NUQuest", "TrackerPracticeProgress", "{0}: {1}/{2}"),
				Label, FText::AsNumber(Practice->PracticeCount), FText::AsNumber(Practice->PracticeRequired));
		}
		if (Objective.bOptional)
		{
			Line = FText::Format(NSLOCTEXT("NUQuest", "TrackerOptionalObjective", "[선택] {0}"), Line);
		}
		Lines.Add(Line.ToString());
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText UNUQuestWidget::BuildObjectiveText(FName QuestId) const
{
	const UNUQuestDefinition* Definition = NUQuestWidget::FindDefinition(GetWorld(), QuestId);
	if (Definition == nullptr) { return FText::GetEmpty(); }
	const FNUQuestProgressState* Progress = IsValid(BoundQuestComponent) ? BoundQuestComponent->FindQuestProgress(QuestId) : nullptr;
	if (Progress != nullptr && Progress->QuestState == ENUQuestState::Completed)
	{
		return NUQuest::GetStateDisplayText(Progress->QuestState);
	}
	FNUQuestProgressState Preview;
	if (Progress == nullptr)
	{
		Preview.Initialize(*Definition, ENUQuestState::Active);
		Progress = &Preview;
	}
	// 새 의뢰 안내에서는 전부 미수락 상태라 [진행 가능] 배지가 항상 같은 말만 반복해서 붙지 않는다.
	const FText Objectives = Progress->GetObjectiveDisplayText(*Definition, false);
	const FText TurnIn = Progress->QuestState == ENUQuestState::ReadyToTurnIn
		? GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>()->GetTurnInDisplayText(QuestId)
		: FText::GetEmpty();
	return TurnIn.IsEmpty() ? Objectives : FText::Format(NSLOCTEXT("NUQuest", "ObjectivesWithTurnIn", "{0}\n{1}"), Objectives, TurnIn);
}

FText UNUQuestWidget::BuildRewardText(FName QuestId) const
{
	const UNUQuestDefinition* Definition = NUQuestWidget::FindDefinition(GetWorld(), QuestId);
	if (Definition == nullptr || Definition->Rewards.IsEmpty())
	{
		return NSLOCTEXT("NUQuest", "NoReward", "보상: 없음");
	}

	// 아이템 보상은 테이블에 이름이 없어 여기서 아이템 정의의 이름으로 채운다.
	// 인벤토리는 폰이 아니라 플레이어 스테이트에 붙어 있다.
	const ANUPlayerState* OwningPlayerState = GetOwningPlayerState<ANUPlayerState>();
	const UNUInventoryComponent* Inventory = OwningPlayerState != nullptr
		? OwningPlayerState->GetInventoryComponent()
		: nullptr;

	TArray<FString> RewardStrings;
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
		RewardStrings.Add(Line.GetDisplayText().ToString());
	}

	return RewardStrings.IsEmpty()
		? NSLOCTEXT("NUQuest", "UnsupportedReward", "보상: 없음")
		: FText::FromString(FString::Printf(TEXT("보상\n%s"), *FString::Join(RewardStrings, TEXT("\n"))));
}

void UNUQuestWidget::HandleQuestProgressChanged()
{
	RefreshTracker();
}

void UNUQuestWidget::HandleQuestOfferRequested(FName QuestId)
{
	ShowQuestOffer(QuestId);
}

void UNUQuestWidget::HandleAcceptClicked()
{
	if (IsValid(BoundQuestComponent) && !OfferedQuestId.IsNone())
	{
		BoundQuestComponent->AcceptQuestById(OfferedQuestId);
	}

	SetOfferVisible(false);
}

void UNUQuestWidget::HandleDeclineClicked()
{
	SetOfferVisible(false);
}

void UNUQuestWidget::HandleTrackerCollapseClicked()
{
	bTrackerExpanded = !bTrackerExpanded;
	ApplyTrackerPresentationState();
}

void UNUQuestWidget::HandleTrackerCloseClicked()
{
	bTrackerClosed = true;
	bTrackerExpanded = false;
	ApplyTrackerPresentationState();
}

void UNUQuestWidget::BindTrackerCloseButton()
{
	UWidget* CloseWidget = GetWidgetFromName(TEXT("TrackerCloseButton"));
	if (UButton* CloseButton = Cast<UButton>(CloseWidget))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTrackerCloseClicked);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButtonWidget->OnCloseClicked.AddUniqueDynamic(this, &ThisClass::HandleTrackerCloseClicked);
	}
}

void UNUQuestWidget::UnbindTrackerCloseButton()
{
	UWidget* CloseWidget = GetWidgetFromName(TEXT("TrackerCloseButton"));
	if (UButton* CloseButton = Cast<UButton>(CloseWidget))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleTrackerCloseClicked);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleTrackerCloseClicked);
	}
}

void UNUQuestWidget::HandleTrackerReopenClicked()
{
	bTrackerClosed = false;
	bTrackerExpanded = true;
	ApplyTrackerPresentationState();
}
