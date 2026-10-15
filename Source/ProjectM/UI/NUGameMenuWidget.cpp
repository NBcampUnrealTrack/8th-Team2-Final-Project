#include "UI/NUGameMenuWidget.h"
#include "UI/NUGameMenuMotion.h"

#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUGameMenuTileEntry.h"
#include "UI/NUKeyBindingsWindow.h" // NUKeyBindings::GetMappedKeyText
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TileView.h"
#include "Engine/World.h"
#include "PlayerController/NUPlayerController.h"
#include "TimerManager.h"

/////////////////////////////////////////////////////////////////
// UNUGameMenuWidget
/////////////////////////////////////////////////////////////////

UNUGameMenuWidget::UNUGameMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNUGameMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeHubMotto();

	if (IsValid(MainMenuTileView))
	{
		// Tile rows use buttons, not list selection. Route directional navigation
		// to the UI manager instead of letting Slate consume it as row selection.
		MainMenuTileView->SetIsGamepadScrollingEnabled(false);
	}

	if (IsValid(OptionsButton)) OptionsButton->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleOptionsClicked);
	if (IsValid(CharacterSelectButton)) CharacterSelectButton->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleCharacterSelectClicked);
	if (IsValid(QuitGameButton)) QuitGameButton->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleQuitGameClicked);
	if (IsValid(HubCloseButton)) HubCloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHubCloseClicked);
	RefreshHubProfile();
	NUGameMenuMotion::Start(this, MainMenuTileView);

	// Tile entries are generated after Construct. Let them appear before main's
	// focus helper selects the first button; otherwise it falls back to the footer.
	const TWeakObjectPtr<UNUGameMenuWidget> WeakMenu(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakMenu]()
	{
		if (UNUGameMenuWidget* Menu = WeakMenu.Get(); Menu && Menu->IsInViewport())
		{
			// The virtualized icon entries do not exist when OpenWindow first scans the menu.
			if (UNUUIManager* UI = UNUUIManager::Get(Menu->GetOwningPlayer()))
			{
				UI->SetPopupButtonHover(Menu, true);
			}
			const ANUPlayerController* PC = Cast<ANUPlayerController>(Menu->GetOwningPlayer());
			if (PC && PC->IsGamepadMode())
			{
				UNUUIManager::FocusFirstButtonIn(Menu->MainMenuTileView);
			}
		}
	});
}

void UNUGameMenuWidget::NativeDestruct()
{
	NUGameMenuMotion::Stop(this);
	if (UEditableTextBox* Motto = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("HubMottoInput"))))
	{
		Motto->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleHubMottoCommitted);
	}
	if (IsValid(OptionsButton)) OptionsButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleOptionsClicked);
	if (IsValid(CharacterSelectButton)) CharacterSelectButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleCharacterSelectClicked);
	if (IsValid(QuitGameButton)) QuitGameButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleQuitGameClicked);
	if (IsValid(HubCloseButton)) HubCloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleHubCloseClicked);
	ReleaseHubPortrait();

	Super::NativeDestruct();
}

void UNUGameMenuWidget::RefreshEntries()
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	if (UI == nullptr)
	{
		return;
	}

	ShownEntries = UI->GetGameMenuEntries();
	// Reference-2 navigation order. Registry categories and actions remain unchanged.
	const TArray<FName> ReferenceOrder = { TEXT("CharacterInfo"), TEXT("Equipment"), TEXT("Stats"),
		TEXT("Skills"), TEXT("GrowthTree"), TEXT("NaiaRoster"), TEXT("QuestJournal"), TEXT("WorldMap"), TEXT("Inventory") };
	ShownEntries.StableSort([&ReferenceOrder](const FNUGameMenuEntry& A, const FNUGameMenuEntry& B)
	{
		const int32 AIndex = ReferenceOrder.IndexOfByKey(A.EntryId);
		const int32 BIndex = ReferenceOrder.IndexOfByKey(B.EntryId);
		return (AIndex == INDEX_NONE ? MAX_int32 : AIndex) < (BIndex == INDEX_NONE ? MAX_int32 : BIndex);
	});

	if (IsValid(MainMenuTileView))
	{
		TArray<UNUGameMenuTileItem*> Items;
		for (const FNUGameMenuEntry& Entry : ShownEntries)
		{
			if (Entry.Category == ENUGameMenuCategory::System)
			{
				continue;
			}
			UNUGameMenuTileItem* Item = NewObject<UNUGameMenuTileItem>(this);
			Item->EntryId = Entry.EntryId;
			Item->Label = Entry.Label;
			Item->Shortcut = Entry.ShortcutMapping.IsNone()
				? FText::GetEmpty()
				: NUKeyBindings::GetMappedKeyText(GetOwningPlayer(), Entry.ShortcutMapping);
			Item->Icon = MenuEntryIcons.FindRef(Entry.EntryId);
			Item->Menu = this;
			Items.Add(Item);
		}
		MainMenuTileView->SetListItems(Items);
		MainMenuTileView->ScrollToTop();
	}
	BindEntryButton(OptionsButton, TEXT("Options"));
	BindEntryButton(CharacterSelectButton, TEXT("CharacterSelect"));
	BindEntryButton(QuitGameButton, TEXT("QuitGame"));
	RefreshHubProfile();
}

void UNUGameMenuWidget::BindEntryButton(UNU_CommonButtonWidget* Button, FName EntryId)
{
	if (!IsValid(Button))
	{
		return;
	}

	const FNUGameMenuEntry* Entry = ShownEntries.FindByPredicate([EntryId](const FNUGameMenuEntry& E)
	{
		return E.EntryId == EntryId;
	});

	if (Entry == nullptr)
	{
		// 아직 등록 안 됐거나(기능 없음) 등록 해제된 항목 --- WBP에 자리는 있지만 숨긴다
		Button->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Button->SetVisibility(ESlateVisibility::Visible);
	// Icon-only system buttons keep just the tooltip; the label would push the icon off-centre.
	Button->SetLabelText(Button->bIconOnly ? FText::GetEmpty() : Entry->Label);
	Button->SetToolTipText(Entry->Label);

	// 표기는 등록된 글자가 아니라 지금 매핑된 키 --- 키 설정에서 바꾸면 여기도 따라온다
	Button->SetShortcutText(Button->bIconOnly || Entry->ShortcutMapping.IsNone()
		? FText::GetEmpty()
		: NUKeyBindings::GetMappedKeyText(GetOwningPlayer(), Entry->ShortcutMapping));
}

void UNUGameMenuWidget::ActivateEntry(FName EntryId)
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	if (UI == nullptr)
	{
		return;
	}
	const TArray<FNUGameMenuEntry> CurrentEntries = UI->GetGameMenuEntries();
	const FNUGameMenuEntry* Entry = CurrentEntries.FindByPredicate([EntryId](const FNUGameMenuEntry& E)
	{
		return E.EntryId == EntryId;
	});

	if (Entry == nullptr)
	{
		return; // 클릭된 사이 등록이 풀린 경우 --- 조용히 무시
	}

	// 닫으면서 재빌드·파괴가 일어나도 안전하게 복사해 둔다
	FSimpleDelegate Action = Entry->OnActivated;

	// 메뉴를 먼저 닫고 델리게이트를 실행한다 --- 메뉴의 이동 차단이 풀린 뒤라야
	// 기능 쪽 차단 검사(인벤 토글 등의 IsGameInputBlocked)에 걸리지 않는다
	const TWeakObjectPtr<UNUUIManager> WeakUI(UI);
	FSimpleDelegate Finish = FSimpleDelegate::CreateLambda([WeakUI, Action]()
	{
		if (UNUUIManager* Manager = WeakUI.Get())
		{
			Manager->CloseGameMenu();
			Action.ExecuteIfBound();
		}
	});
	if (!NUGameMenuMotion::DeferClose(this, Finish)) { Finish.ExecuteIfBound(); }
}

void UNUGameMenuWidget::HandleOptionsClicked() { ActivateEntry(TEXT("Options")); }
void UNUGameMenuWidget::HandleCharacterSelectClicked() { ActivateEntry(TEXT("CharacterSelect")); }
void UNUGameMenuWidget::HandleQuitGameClicked() { ActivateEntry(TEXT("QuitGame")); }

void UNUGameMenuWidget::HandleHubCloseClicked()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseGameMenu();
	}
}
