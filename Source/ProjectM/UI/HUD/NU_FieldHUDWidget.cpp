// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/NU_FieldHUDWidget.h"

#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"

void UNU_FieldHUDWidget::InitializeWidget()
{
	// 베이스가 ASC 를 구독하고 현재 전투 상태를 1회 흘려주므로,
	// 그 과정에서 ApplyFieldVisibility 까지 이미 끝난다.
	Super::InitializeWidget();

	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnFieldHUDVisibilityChanged.RemoveAll(this);
		UI->OnFieldHUDVisibilityChanged.AddUObject(this, &ThisClass::RefreshHUDPresentation);
	}
	if (HUDVisibilityButton)
	{
		HUDVisibilityButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHUDVisibilityClicked);
	}
	RefreshHUDPresentation();

	// 재구성 중복 구독 방지 --- 해제 선행 후 구독. 없는 아이콘(Optional)은 조용히 건너뛴다
	if (IsValid(QuestJournalButton))
	{
		QuestJournalButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleQuestJournalClicked);
		QuestJournalButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleQuestJournalClicked);
	}
	if (IsValid(CharacterInfoButton))
	{
		CharacterInfoButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleCharacterInfoClicked);
		CharacterInfoButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleCharacterInfoClicked);
	}
	if (IsValid(StatsButton))
	{
		StatsButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleStatsClicked);
		StatsButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleStatsClicked);
	}
	if (IsValid(GrowthTreeButton))
	{
		GrowthTreeButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleGrowthTreeClicked);
		GrowthTreeButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleGrowthTreeClicked);
	}
	if (IsValid(InventoryButton))
	{
		InventoryButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleInventoryClicked);
		InventoryButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleInventoryClicked);
	}
	if (IsValid(NaiaRosterButton))
	{
		NaiaRosterButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleNaiaRosterClicked);
		NaiaRosterButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleNaiaRosterClicked);
	}
	if (IsValid(GameMenuButton))
	{
		GameMenuButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleGameMenuClicked);
		GameMenuButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleGameMenuClicked);
	}
}

void UNU_FieldHUDWidget::ReleaseWidget()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnFieldHUDVisibilityChanged.RemoveAll(this);
		UI->ForgetFieldHUDWidget(FieldHUDContent);
	}
	if (HUDVisibilityButton)
	{
		HUDVisibilityButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleHUDVisibilityClicked);
	}
	Super::ReleaseWidget();
}

void UNU_FieldHUDWidget::HandleHUDVisibilityClicked()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		if (!UI->HasFieldHUDHideRequests())
		{
			UI->SetFieldHUDVisible(!UI->IsFieldHUDUserVisible());
		}
	}
}

void UNU_FieldHUDWidget::RefreshHUDPresentation()
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	const bool bVisible = !UI || UI->IsFieldHUDUserVisible();
	const FText ToggleLabel = bVisible
		? NSLOCTEXT("NUFieldHUD", "HideHUD", "HUD 숨기기")
		: NSLOCTEXT("NUFieldHUD", "ShowHUD", "HUD 표시");
	if (HUDVisibilityButton)
	{
		HUDVisibilityButton->SetIsEnabled(!UI || !UI->HasFieldHUDHideRequests());
		HUDVisibilityButton->SetToolTipText(ToggleLabel);
	}
	if (HUDVisibilityIcons)
	{
		HUDVisibilityIcons->SetActiveWidgetIndex(bVisible ? 0 : 1);
	}
	if (EyeBackdrop)
	{
		EyeBackdrop->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (FieldHUDContent)
	{
		if (UI)
		{
			if (UPanelWidget* Content = Cast<UPanelWidget>(FieldHUDContent))
			{
				UI->PresentFieldHUDGroup(Content);
			}
			else
			{
				UI->PresentFieldHUDWidget(FieldHUDContent, ESlateVisibility::SelfHitTestInvisible);
			}
		}
		else
		{
			FieldHUDContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
	if (HUDVisibilityLabel)
	{
		HUDVisibilityLabel->SetText(ToggleLabel);
	}

	// 숨김을 풀었을 때도 모서리가 다시 들어온다
	if (bVisible && !bLastUserVisible && bWasShown)
	{
		PlayIntro();
	}
	bLastUserVisible = bVisible;
}

void UNU_FieldHUDWidget::ActivateEntry(FName EntryId)
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	if (UI == nullptr)
	{
		return;
	}

	const TArray<FNUGameMenuEntry> Entries = UI->GetGameMenuEntries();
	const FNUGameMenuEntry* Entry = Entries.FindByPredicate([EntryId](const FNUGameMenuEntry& E)
	{
		return E.EntryId == EntryId;
	});

	if (Entry != nullptr)
	{
		Entry->OnActivated.ExecuteIfBound();
	}
}

void UNU_FieldHUDWidget::HandleQuestJournalClicked() { ActivateEntry(TEXT("QuestJournal")); }
void UNU_FieldHUDWidget::HandleCharacterInfoClicked() { ActivateEntry(TEXT("CharacterInfo")); }
void UNU_FieldHUDWidget::HandleStatsClicked() { ActivateEntry(TEXT("Stats")); }
void UNU_FieldHUDWidget::HandleGrowthTreeClicked() { ActivateEntry(TEXT("GrowthTree")); }
void UNU_FieldHUDWidget::HandleInventoryClicked() { ActivateEntry(TEXT("Inventory")); }
void UNU_FieldHUDWidget::HandleNaiaRosterClicked() { ActivateEntry(TEXT("NaiaRoster")); }

void UNU_FieldHUDWidget::HandleGameMenuClicked()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OpenGameMenu();
	}
}

void UNU_FieldHUDWidget::PlayIntro()
{
	IntroMotion.Finish();
	IntroMotion.Add(HudTopLeft, 0.f, 0.5f, FVector2D(-28.f, 0.f));
	IntroMotion.Add(HudTopRight, 0.06f, 0.5f, FVector2D(28.f, 0.f));
	IntroMotion.Add(HudBottomLeft, 0.12f, 0.45f, FVector2D(0.f, 18.f));
	IntroMotion.Add(HudBottomRight, 0.12f, 0.45f, FVector2D(0.f, 18.f));
	IntroMotion.Start();
}

void UNU_FieldHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	IntroMotion.Advance(InDeltaTime);
}

void UNU_FieldHUDWidget::ApplyFieldVisibility()
{
	Super::ApplyFieldVisibility();
	RefreshHUDPresentation();

	// 필드 HUD가 다시 보이게 된 순간(처음, 전투에서 돌아옴)만 등장 연출
	const bool bShown = ShouldShowFieldUI();
	if (bShown && !bWasShown)
	{
		PlayIntro();
	}
	bWasShown = bShown;

	BP_OnFieldHUDRefreshed(ShouldShowFieldUI());
}
