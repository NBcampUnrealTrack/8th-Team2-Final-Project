#include "UI/NUItemContextMenu.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

namespace
{
	// 메뉴 밖을 누르면 메뉴를 닫는다. 누름은 삼키지 않는다 --- 다른 칸을 누르면 그 칸이 바로 받는다.
	// 위젯의 마우스 처리로는 못 받는다: 메뉴 루트는 화면을 덮는 캔버스라 몸체 밖 누름이 아래 창으로 지나가고,
	// 막는 판을 깔면 닫을 수는 있어도 그 누름이 아래로 못 가 한 번 더 눌러야 한다
	class FNUItemMenuOutsideClick : public IInputProcessor
	{
	public:
		FNUItemMenuOutsideClick(UNUItemContextMenu* InMenu, const UWidget* InBody) : Menu(InMenu), Body(InBody) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			// 아직 한 번도 안 그려진 몸체는 크기가 0이라 어디를 눌러도 밖이다
			if (Menu.IsValid() && (!Body.IsValid() || !Body->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition())))
			{
				Menu->Close();
			}
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("NUItemMenuOutsideClick"); }

	private:
		TWeakObjectPtr<UNUItemContextMenu> Menu;
		TWeakObjectPtr<const UWidget> Body;
	};
}

UNUItemContextMenu* UNUItemContextMenu::Show(UUserWidget* OwnerWindow, TArray<FNUItemMenuEntry> InEntries, const FVector2D& ScreenPos)
{
	APlayerController* OwningPlayer = OwnerWindow != nullptr ? OwnerWindow->GetOwningPlayer() : nullptr;
	UNUUIManager* UI = UNUUIManager::Get(OwningPlayer);
	if (UI == nullptr || InEntries.IsEmpty())
	{
		return nullptr;
	}
	const TSubclassOf<UNUItemContextMenu> MenuClass = GetDefault<UNUUISettings>()->ItemContextMenuClass.LoadSynchronous();
	if (MenuClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[아이템 메뉴] ItemContextMenuClass 미지정 --- 프로젝트 세팅(NU UI) 확인"));
		return nullptr;
	}

	// 메뉴는 하나만 --- 패드 X처럼 마우스 누름 없이 다시 열려도 앞의 것이 남지 않게
	for (UUserWidget* Open : UI->GetShownWindows(NUUILayer::Popup))
	{
		if (UNUItemContextMenu* Previous = Cast<UNUItemContextMenu>(Open))
		{
			Previous->Close();
		}
	}

	UNUItemContextMenu* Menu = CreateWidget<UNUItemContextMenu>(OwningPlayer, MenuClass);
	if (Menu == nullptr)
	{
		return nullptr;
	}
	Menu->Entries = MoveTemp(InEntries);
	Menu->PendingScreenPos = ScreenPos;

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Popup;
	Params.OwnerWindow = OwnerWindow; // 연 창이 닫히면 같이 닫힌다
	UI->OpenWindow(Menu, Params);
	return Menu;
}

void UNUItemContextMenu::Close()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this);
	}
	else
	{
		RemoveFromParent();
	}
}

void UNUItemContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	ItemBox->ClearChildren();
	if (ButtonClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[아이템 메뉴] ButtonClass 미지정 --- WBP_ItemContextMenu 기본값 확인"));
	}
	for (int32 Index = 0; ButtonClass != nullptr && Index < Entries.Num(); ++Index)
	{
		UNU_CommonButtonWidget* Button = CreateWidget<UNU_CommonButtonWidget>(this, ButtonClass);
		if (Button == nullptr)
		{
			continue;
		}
		Button->Tag = Index;
		Button->SetLabelText(Entries[Index].Label);
		Button->OnCommonClickedWithTag.AddDynamic(this, &UNUItemContextMenu::HandleEntryClicked);
		ItemBox->AddChild(Button);
	}
	PlaceAt(PendingScreenPos);

	if (FSlateApplication::IsInitialized())
	{
		OutsideClickWatcher = MakeShared<FNUItemMenuOutsideClick>(this, MenuBG);
		FSlateApplication::Get().RegisterInputPreProcessor(OutsideClickWatcher);
	}
}

void UNUItemContextMenu::NativeDestruct()
{
	if (OutsideClickWatcher.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(OutsideClickWatcher);
	}
	OutsideClickWatcher.Reset();
	Super::NativeDestruct();
}

void UNUItemContextMenu::HandleEntryClicked(int32 Tag)
{
	// 먼저 닫는다 --- 동작이 다른 창(수량·확인)을 열면 그 창이 맨 위여야 한다
	FSimpleDelegate Action = Entries.IsValidIndex(Tag) ? Entries[Tag].Action : FSimpleDelegate();
	Close();
	Action.ExecuteIfBound();
}

void UNUItemContextMenu::PlaceAt(const FVector2D& ScreenPos)
{
	UCanvasPanelSlot* BodySlot = Cast<UCanvasPanelSlot>(MenuBG->Slot);
	if (BodySlot == nullptr)
	{
		return;
	}
	// 항목을 다 넣은 뒤라 원하는 크기를 잴 수 있다 --- 화면 오른쪽·아래로 넘치면 안쪽으로 당긴다
	MenuBG->ForceLayoutPrepass();
	const FVector2D MenuSize = MenuBG->GetDesiredSize();
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(Scale, KINDA_SMALL_NUMBER);
	FVector2D Position = ScreenPos;
	Position.X = FMath::Clamp(Position.X, 0.f, FMath::Max(0.f, ViewportSize.X - MenuSize.X));
	Position.Y = FMath::Clamp(Position.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - MenuSize.Y));
	BodySlot->SetAutoSize(true);
	BodySlot->SetPosition(Position);
}
