// Development preview entry for PIE and Standalone. Page data remains owned by each WBP.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "UI/CharacterMenu/NUCharacterMenuBase.h"

#include "Components/NamedSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "UI/NUUIManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUCharacterMenuPreview, Log, All);

namespace NUCharacterMenuPreview
{
// Keep the shell and its design pages in the same asset folder.
static FString ClassPath(const TCHAR* AssetName)
{
	return FString::Printf(TEXT("/Game/Developers/MinkiCho/UI/CharacterMenu/%s.%s_C"), AssetName, AssetName);
}

struct FPage
{
	ENUCharacterMenuSection Section;
	const TCHAR* Name;
	const TCHAR* AssetName;
};

static constexpr FPage Pages[] =
{
	{ ENUCharacterMenuSection::Character, TEXT("Character"), TEXT("WBP_CharacterInfoPage") },
	{ ENUCharacterMenuSection::Equipment, TEXT("Equipment"), TEXT("WBP_CharacterEquipmentPage") },
	{ ENUCharacterMenuSection::Inventory, TEXT("Inventory"), TEXT("WBP_CharacterInventoryPage") },
	{ ENUCharacterMenuSection::Stats, TEXT("Stats"), TEXT("WBP_CharacterStatsPage") },
	{ ENUCharacterMenuSection::Traits, TEXT("Traits"), TEXT("WBP_CharacterTraitsPage") },
	{ ENUCharacterMenuSection::Naia, TEXT("Naia"), TEXT("WBP_CharacterNaiaPage") }
};

static void Open(const TArray<FString>& Args, UWorld* World)
{
	const FPage* InitialPage = &Pages[0];
	if (!Args.IsEmpty())
	{
		InitialPage = nullptr;
		for (const FPage& Page : Pages)
		{
			if (Args[0].Equals(Page.Name, ESearchCase::IgnoreCase)) InitialPage = &Page;
		}
	}
	if (!InitialPage || Args.Num() > 1)
	{
		UE_LOG(LogNUCharacterMenuPreview, Warning, TEXT("Usage: nu.CharacterMenu.Preview [Character|Equipment|Inventory|Stats|Traits|Naia]"));
		return;
	}
	APlayerController* PC = World && World->IsGameWorld() ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	UNUUIManager* UI = PC && PC->IsLocalController() ? UNUUIManager::Get(PC) : nullptr;
	if (!UI)
	{
		UE_LOG(LogNUCharacterMenuPreview, Warning, TEXT("Start PIE or Standalone and wait for the local player before opening the preview."));
		return;
	}

	const TSubclassOf<UNUCharacterMenuBase> ShellClass = LoadClass<UNUCharacterMenuBase>(nullptr,
		*ClassPath(TEXT("WBP_CharacterMenuBase")));
	if (!ShellClass)
	{
		UE_LOG(LogNUCharacterMenuPreview, Error, TEXT("Character menu preview shell could not be loaded: %s"),
			*ClassPath(TEXT("WBP_CharacterMenuBase")));
		return;
	}
	TArray<TSubclassOf<UUserWidget>> PageClasses;
	for (const FPage& Page : Pages)
	{
		const FString Path = ClassPath(Page.AssetName);
		const TSubclassOf<UUserWidget> PageClass = LoadClass<UUserWidget>(nullptr, *Path);
		if (!PageClass)
		{
			UE_LOG(LogNUCharacterMenuPreview, Error, TEXT("Preview page could not be loaded: %s"), *Path);
			return;
		}
		PageClasses.Add(PageClass);
	}

	UNUCharacterMenuBase* Shell = nullptr;
	for (UUserWidget* Window : UI->GetShownWindows(NUUILayer::Window))
	{
		if (Window && Window->GetClass() == ShellClass.Get())
		{
			Shell = Cast<UNUCharacterMenuBase>(Window);
			break;
		}
	}
	const bool bCreatedShell = Shell == nullptr;
	if (!Shell) Shell = UNUCharacterMenuBase::Show(PC, ShellClass, InitialPage->Section);
	UNamedSlot* Slot = Shell ? Cast<UNamedSlot>(Shell->GetWidgetFromName(TEXT("NamedSlot_PageContent"))) : nullptr;
	if (!Slot)
	{
		if (Shell && bCreatedShell) Shell->RequestClose();
		UE_LOG(LogNUCharacterMenuPreview, Error, TEXT("Preview window or its page slot could not be created."));
		return;
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Pages); ++Index)
	{
		Shell->SelectSection(Pages[Index].Section);
		if (UWidget* Existing = Slot->GetContent(); Existing && Existing->GetClass() == PageClasses[Index].Get()) continue;
		UUserWidget* Page = CreateWidget<UUserWidget>(PC, PageClasses[Index]);
		if (!Page)
		{
			if (bCreatedShell) Shell->RequestClose();
			UE_LOG(LogNUCharacterMenuPreview, Error, TEXT("Preview page could not be created: %s"), Pages[Index].Name);
			return;
		}
		Shell->RegisterPage(Pages[Index].Section, Page);
	}
	Shell->SelectSection(InitialPage->Section);
	UE_LOG(LogNUCharacterMenuPreview, Log, TEXT("Preview ready: %d pages, selected=%s, world=%s, reused=%s"),
		static_cast<int32>(UE_ARRAY_COUNT(Pages)), InitialPage->Name, *World->GetMapName(), bCreatedShell ? TEXT("false") : TEXT("true"));
}

static FAutoConsoleCommandWithWorldAndArgs GPreviewCommand(
	TEXT("nu.CharacterMenu.Preview"),
	TEXT("Open the six character-menu design pages. Optional section: Character, Equipment, Inventory, Stats, Traits or Naia. Close with X or Escape."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Open));
}

#endif // !UE_BUILD_SHIPPING
