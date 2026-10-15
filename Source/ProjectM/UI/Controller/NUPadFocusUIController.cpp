#include "UI/Controller/NUPadFocusUIController.h"

#include "Algo/Find.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/ListView.h"
#include "Components/ScrollBox.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "UI/NUUIGeometry.h"
#include "UI/NUUIManager.h" // NUUILayer
#include "UI/NUUISettings.h"

void UNUPadFocusUIController::SetActive(bool bActive)
{
	if (bActive == TickHandle.IsValid())
	{
		return;
	}
	if (bActive)
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UNUPadFocusUIController::TickFrame));
		return;
	}
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();
	if (Frame != nullptr)
	{
		Frame->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNUPadFocusUIController::OnPlayerControllerChanged(APlayerController* NewPC)
{
	DropFrame();
}

void UNUPadFocusUIController::OnDeinitialize()
{
	SetActive(false);
	DropFrame();
}

bool UNUPadFocusUIController::AdjustFocus(const TSharedPtr<SWidget>& Focused)
{
	UNUUIManager* UIManager = GetManager();
	UWidget* Owner = UIManager != nullptr ? UIManager->FindWidgetForSlate(Focused) : nullptr;
	// 창 루트(대화창처럼 창 자체가 포커스를 잡는 것)나 창 밖의 낯선 UserWidget에는 안 그린다. 칸 같은 창 안 중첩 위젯은 그린다
	if (Focused.IsValid() && Focused->GetType() == TEXT("SObjectWidget"))
	{
		const UUserWidget* User = Cast<UUserWidget>(Owner);
		if (User == nullptr || UIManager->IsWindowOpen(User))
		{
			return true;
		}
	}
	if (UListView* List = Cast<UListView>(Owner))
	{
		// 목록 자체에 섰다 --- 고른 항목(없으면 첫 항목)으로 목록의 항목 이동을 시킨다. 목록이 다음 틱에 그 줄에 포커스를 주고
		// 이후 위아래는 목록이 스스로 항목 단위로 옮긴다 (한 번 더 눌러야 줄이 잡히던 것)
		if (List->GetNumItems() > 0)
		{
			int32 Index = List->GetIndexForItem(List->GetSelectedItem());
			if (Index == INDEX_NONE)
			{
				Index = 0;
				List->SetSelectedIndex(0);
			}
			List->NavigateToIndex(Index);
		}
		return true;
	}
	if (Cast<UScrollBox>(Owner) != nullptr)
	{
		UNUUIManager::FocusFirstFocusableIn(Owner);
		return true;
	}
	// 목록 줄에 바로 섰으면(옆 조작부에서 D패드로 들어옴) 그 항목을 고른 상태로 맞춘다 --- 목록의 다음 위아래 이동이 여기서 이어진다
	if (IUserObjectListEntry* Entry = Cast<IUserObjectListEntry>(Owner))
	{
		UListView* List = Cast<UListView>(Entry->GetOwningListView());
		UObject* Item = Entry->GetListItem<UObject>();
		if (List != nullptr && Item != nullptr && List->GetSelectedItem() != Item)
		{
			List->SetSelectedItem(Item);
		}
	}
	return false;
}

void UNUPadFocusUIController::DropFrame()
{
	if (Frame != nullptr)
	{
		Frame->RemoveFromParent();
		Frame = nullptr;
	}
}

bool UNUPadFocusUIController::TickFrame(float DeltaTime)
{
	APlayerController* PC = GetOwningPlayerController();
	if (PC == nullptr || !FSlateApplication::IsInitialized())
	{
		return true;
	}

	// 포커스가 게임 뷰포트 안의 버튼에 있을 때만 그린다 --- 에디터 UI나 창 자체에 가 있으면 숨긴다
	TSharedPtr<SWidget> Focused;
	FSlateApplication::Get().ForEachUser([&Focused](FSlateUser& User)
	{
		if (!Focused.IsValid())
		{
			Focused = User.GetFocusedWidget();
		}
	});
	const TSharedPtr<SWidget> Viewport = GEngine != nullptr && GEngine->GameViewport != nullptr ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
	// 글자칸(커서가 보인다)과 AdjustFocus가 거른 것(그릇·창 루트)만 빼고 포커스가 서는 조작부 전부에 그린다
	static const FName SkipTypes[] = { TEXT("SEditableText"), TEXT("SEditableTextBox"), TEXT("SMultiLineEditableText"), TEXT("SMultiLineEditableTextBox") };
	const bool bSkipType = Focused.IsValid() && Algo::Find(SkipTypes, Focused->GetType()) != nullptr;
	if (Focused != LastFocused.Pin())
	{
		// 칸에서 칸으로 옮긴 소리 --- 창이 열리고 닫히며 게임 화면과 오가는 포커스는 뺀다
		if (LastFocused.IsValid() && Focused.IsValid() && LastFocused.Pin() != Viewport && Focused != Viewport)
		{
			UNUSFXSubsystem::Play(PC, NU::SFX::UI::Cursor);
		}
		LastFocused = Focused;
		bSkipFrame = AdjustFocus(Focused);
	}
	const bool bButtonInViewport = Focused.IsValid() && Viewport.IsValid() && Focused != Viewport && !bSkipType && !bSkipFrame
		&& FSlateApplication::Get().HasFocusedDescendants(Viewport.ToSharedRef());

	FVector2D Position, Size;
	if (!bButtonInViewport || !NUUIGeometry::GetViewportRect(PC, *Focused, Position, Size))
	{
		if (Frame != nullptr)
		{
			Frame->SetVisibility(ESlateVisibility::Collapsed);
		}
		return true;
	}

	if (Frame != nullptr && Frame->GetWorld() != PC->GetWorld())
	{
		DropFrame();
	}
	if (Frame == nullptr)
	{
		const UNUUISettings* Settings = GetDefault<UNUUISettings>();
		const TSubclassOf<UUserWidget> FrameClass = Settings != nullptr ? Settings->PadFocusFrameClass.LoadSynchronous() : nullptr;
		if (FrameClass == nullptr)
		{
			return true; // 미지정 = 안 그린다
		}
		Frame = CreateWidget<UUserWidget>(PC, FrameClass);
		if (Frame == nullptr)
		{
			return true;
		}
		Frame->AddToViewport(NUUILayer::Menu);
		Frame->SetAlignmentInViewport(FVector2D::ZeroVector);
	}

	Frame->SetPositionInViewport(Position, false);
	Frame->SetDesiredSizeInViewport(Size);
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}
