// Development-only entry for the shop design WBP. No shop session or transaction is started.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerController/NUPlayerController.h"
#include "Shop/UI/NUShopPageWidget.h"
#include "UI/NUUIManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUShopPreview, Log, All);

namespace NUShopPreview
{
static constexpr const TCHAR* WidgetClassPath =
	TEXT("/Game/Developers/MinkiCho/UI/Shop/WBP_ShopPage.WBP_ShopPage_C");

static void Execute(const TArray<FString>& Args, UWorld* World)
{
	const bool bClose = Args.Num() == 1 && Args[0].Equals(TEXT("Close"), ESearchCase::IgnoreCase);
	if (!Args.IsEmpty() && !bClose)
	{
		UE_LOG(LogNUShopPreview, Warning, TEXT("Usage: nu.Shop.Preview [Close]"));
		return;
	}

	APlayerController* PC = World && World->IsGameWorld() ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	UNUUIManager* UI = PC && PC->IsLocalController() ? UNUUIManager::Get(PC) : nullptr;
	if (!UI)
	{
		UE_LOG(LogNUShopPreview, Warning, TEXT("Start PIE or Standalone and wait for the local player before opening the preview."));
		return;
	}

	const TSubclassOf<UUserWidget> WidgetClass = LoadClass<UUserWidget>(nullptr, WidgetClassPath);
	if (!WidgetClass)
	{
		UE_LOG(LogNUShopPreview, Error, TEXT("Shop preview could not be loaded: %s"), WidgetClassPath);
		return;
	}

	for (UUserWidget* Window : UI->GetShownWindows(NUUILayer::Window))
	{
		if (!Window || Window->GetClass() != WidgetClass.Get()) continue;
		if (bClose)
		{
			UI->CloseWindow(Window);
			UE_LOG(LogNUShopPreview, Log, TEXT("Preview closed: world=%s"), *World->GetMapName());
		}
		else
		{
			if (UNUShopPageWidget* Page = Cast<UNUShopPageWidget>(Window))
			{
				const ANUPlayerController* NUController = Cast<ANUPlayerController>(PC);
				Page->SetMerchantActor(NUController ? NUController->GetLastDialogueSource() : nullptr);
			}
			UE_LOG(LogNUShopPreview, Log, TEXT("Preview already open: world=%s"), *World->GetMapName());
		}
		return;
	}
	if (bClose) return;

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!Widget)
	{
		UE_LOG(LogNUShopPreview, Error, TEXT("Shop preview widget could not be created."));
		return;
	}

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	// Cursor and ESC use the window manager defaults; the WBP supplies its own dim layer.
	UI->OpenWindow(Widget, Params);
	UE_LOG(LogNUShopPreview, Log, TEXT("Preview ready: world=%s. Close with X, Escape, or nu.Shop.Preview Close. Design preview only."), *World->GetMapName());
}

static FAutoConsoleCommandWithWorldAndArgs GPreviewCommand(
	TEXT("nu.Shop.Preview"),
	TEXT("Open the shop design preview in PIE or Standalone. Optional Close argument closes it. No real transactions."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Execute));
}

#endif // !UE_BUILD_SHIPPING
