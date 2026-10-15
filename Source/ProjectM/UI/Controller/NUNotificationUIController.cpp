#include "UI/Controller/NUNotificationUIController.h"

#include "Audio/NUSFXTags.h"

#include "UI/NUUIManager.h"

UNURewardPopup* UNUNotificationUIController::ShowReward(TSubclassOf<UNURewardPopup> PopupClass, const FText& InTitle, const TArray<FNURewardEntry>& InEntries)
{
	if (PopupClass == nullptr || InEntries.IsEmpty())
	{
		return nullptr;
	}

	// 이미 떠 있으면 겹치지 않게 큐에 넣는다
	if (ActivePopup != nullptr)
	{
		FNURewardPopupRequest& Request = PendingRequests.AddDefaulted_GetRef();
		Request.PopupClass = PopupClass;
		Request.Title = InTitle;
		Request.Entries = InEntries;
		return nullptr;
	}

	return ShowImmediate(PopupClass, InTitle, InEntries);
}

UNURewardPopup* UNUNotificationUIController::ShowImmediate(TSubclassOf<UNURewardPopup> PopupClass, const FText& InTitle, const TArray<FNURewardEntry>& InEntries)
{
	APlayerController* PC = GetOwningPlayerController();
	UNUUIManager* UI = GetManager();
	if (PC == nullptr || UI == nullptr)
	{
		return nullptr;
	}

	UNURewardPopup* Popup = CreateWidget<UNURewardPopup>(PC, PopupClass);
	if (Popup == nullptr)
	{
		return nullptr;
	}

	// AddToViewport(OpenWindow) 시점의 NativeConstruct가 읽으므로 먼저 채운다
	Popup->SetupReward(InTitle, InEntries);
	ActivePopup = Popup;

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Popup;
	Params.bBlockGameInput = true;
	Params.bCloseOnEscape = false; // ESC/Enter/Space는 팝업이 포커스를 잡고 직접 처리한다
	Params.OpenSFX = NU::SFX::Quest::Reward;
	UI->OpenWindow(Popup, Params);

	return Popup;
}

void UNUNotificationUIController::NotifyPopupClosed(UNURewardPopup* Popup)
{
	if (Popup == nullptr || ActivePopup != Popup)
	{
		return; // 이미 정리된 팝업
	}

	ActivePopup = nullptr;
	if (UNUUIManager* UI = GetManager())
	{
		UI->CloseWindow(Popup);
	}
	else
	{
		Popup->RemoveFromParent();
	}

	ShowNextPending();
}

void UNUNotificationUIController::ShowNextPending()
{
	while (!PendingRequests.IsEmpty())
	{
		const FNURewardPopupRequest Request = PendingRequests[0];
		PendingRequests.RemoveAt(0);

		if (!Request.PopupClass.IsValid())
		{
			continue;
		}
		if (ShowImmediate(Request.PopupClass.Get(), Request.Title, Request.Entries) != nullptr)
		{
			return;
		}
	}
}

void UNUNotificationUIController::OnPlayerControllerChanged(APlayerController* NewPC)
{
	// 위젯은 월드와 함께 죽었고 큐의 요청도 이전 레벨의 것이다
	ActivePopup = nullptr;
	PendingRequests.Reset();
}

void UNUNotificationUIController::OnDeinitialize()
{
	if (ActivePopup != nullptr)
	{
		if (UNUUIManager* UI = GetManager())
		{
			UI->CloseWindow(ActivePopup);
		}
		ActivePopup = nullptr;
	}
	PendingRequests.Reset();
}
