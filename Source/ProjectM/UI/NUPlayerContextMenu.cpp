// Fill out your copyright notice in the Description page of Project Settings.


#include "NUPlayerContextMenu.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "CharacterSelect/UI/NUCharacterProfileUIController.h"
#include "Chat/NUChatSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "InputCoreTypes.h"
#include "Party/NUPartySubsystem.h"
#include "PlayerController/NUPlayerController.h"
#include "Trade/UI/NUTradeUIController.h"
#include "UI/NUUIManager.h"


namespace NUPlayerContext
{
	int32 FieldActions()
	{
		return ActionBit(ENUPlayerContextAction::CharacterInfo)
			| ActionBit(ENUPlayerContextAction::Whisper)
			| ActionBit(ENUPlayerContextAction::PartyInvite)
			| ActionBit(ENUPlayerContextAction::Trade)
			| ActionBit(ENUPlayerContextAction::AddFriend)
			| ActionBit(ENUPlayerContextAction::Cancel);
	}

	int32 PartyMemberActions(bool bIsLeader)
	{
		// 이미 같은 파티라 초대는 뺌
		int32 Mask = ActionBit(ENUPlayerContextAction::CharacterInfo)
			| ActionBit(ENUPlayerContextAction::Whisper)
			| ActionBit(ENUPlayerContextAction::Trade)
			| ActionBit(ENUPlayerContextAction::AddFriend)
			| ActionBit(ENUPlayerContextAction::Cancel);

		if (bIsLeader)
		{
			Mask |= ActionBit(ENUPlayerContextAction::PartyKick);
		}

		return Mask;
	}

	int32 PartySelfActions()
	{
		return ActionBit(ENUPlayerContextAction::PartyLeave)
			| ActionBit(ENUPlayerContextAction::Cancel);
	}
}

void UNUPlayerContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// 원격플레이어가 시야를 벗어나거나 접속을 끊는 경우 델리게이트 바인딩
	if (UWorld* World = GetWorld())
	{
		if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
		{
			Sync->OnRemotePlayerDespawned.AddDynamic(this, &ThisClass::HandleRemotePlayerDespawned);
		}
	}

	TargetDisplayName = FText();
	Target = FNUPlayerContextTarget();
	bIsOpen = false;
}

void UNUPlayerContextMenu::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
		{
			Sync->OnRemotePlayerDespawned.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

void UNUPlayerContextMenu::Open(const FNUPlayerContextTarget& InTarget, const FVector2D& ScreenPos, AActor* FollowActor)
{
	Target = InTarget;
	TargetDisplayName = InTarget.DisplayName;
	bIsOpen = true;

	// WBP가 이름과 항목 표시를 열릴때 갱신함
	OnTargetChanged();

	// 화면 밖으로 나가지 않게 클릭 지점의 위치에 따라 열리는 방향 변경
	FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	FVector2D Alignment(
		ScreenPos.X > ViewportSize.X * 0.5f ? 1.f : 0.f,
		ScreenPos.Y > ViewportSize.Y * 0.5f ? 1.f : 0.f);

	SetAlignmentInViewport(Alignment);
	SetPositionInViewport(ScreenPos);	// GetMousePosition은 픽셀 좌표
	SetVisibility(ESlateVisibility::Visible);

	// 매니저에 창으로 등록한다 --- 마우스 커서(Alt키를 떼도 보이게), 패드 첫 항목 포커스, 메뉴 안 D패드,
	// ESC·B 닫기, 대상 캐릭터 따라다니기를 매니저가 맡는다.
	// LB/RB로 다른 사람에게 옮기면 따라갈 캐릭터가 바뀌므로 등록을 다시 한다
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this);

		FNUUIWindowParams Params;
		Params.Layer = NUUILayer::Menu;
		Params.bVisibilityDriven = true; // 뷰포트에 계속 두고 보이기로 여닫는 창
		Params.AnchorActor = FollowActor;
		UI->OpenWindow(this, Params, FSimpleDelegate::CreateUObject(this, &ThisClass::Close));
	}
}

void UNUPlayerContextMenu::Close()
{
	if (!IsOpen()) return;

	bIsOpen = false; // 매니저의 닫기 요청으로 다시 들어오지 않게 먼저 내린다

	// 커서 해제와 포커스 되돌리기는 매니저가 한다. 숨기기 전에 불러야 매니저가 창을 찾는다 (상주 창은 가시성으로 판정)
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this);
	}
	SetVisibility(ESlateVisibility::Collapsed);
	Target = FNUPlayerContextTarget();
}

bool UNUPlayerContextMenu::IsOpen() const
{
	return bIsOpen;
}

int64 UNUPlayerContextMenu::GetTargetPlayerId() const
{
	return Target.PlayerId;
}

ESlateVisibility UNUPlayerContextMenu::GetActionVisibility(ENUPlayerContextAction Action) const
{
	return (Target.ActionMask & NUPlayerContext::ActionBit(Action)) != 0
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed;
}

void UNUPlayerContextMenu::ExecuteAction(ENUPlayerContextAction Action)
{
	// Close가 Target을 비우므로 미리 복사해둠
	FNUPlayerContextTarget Picked = Target;
	Close();

	switch (Action)
	{
	case ENUPlayerContextAction::CharacterInfo:
		// 시야 밖 대상(파티 목록)도 조회된다 --- 서버가 존 제한 없이 준다 (1:1 거래와 같은 방식)
		if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
		{
			if (UNUCharacterProfileUIController* ProfileUI = UI->GetController<UNUCharacterProfileUIController>())
			{
				ProfileUI->ShowProfile(Picked.CharacterIdx);
			}
		}
		break;

	case ENUPlayerContextAction::Whisper:
		// 대상만 잡고 귓속말 채팅창이 열림
		if (UNUChatSubsystem* Chat = GetChatSubsystem())
		{
			Chat->BeginWhisperTo(Picked.CharacterIdx, Picked.DisplayName.ToString());
		}
		break;

	case ENUPlayerContextAction::PartyInvite:
		// 시야를 벗어났으면 0이 넘어가고 Invite가 걸러낸다
		if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
		{
			PartySubsystem->Invite(Picked.CharacterIdx);
		}
		break;

	case ENUPlayerContextAction::Trade:
		// 시야를 벗어났으면 0이 넘어가고 RequestTrade가 걸러낸다 (파티 초대와 같은 방식)
		if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
		{
			if (UNUTradeUIController* TradeUI = UI->GetController<UNUTradeUIController>())
			{
				TradeUI->RequestTradeWith(Picked.CharacterIdx);
			}
		}
		break;

	case ENUPlayerContextAction::AddFriend:
		UE_LOG(LogTemp, Warning, TEXT("친구 추가 요청 : %lld"), Picked.CharacterIdx);
		break;

	case ENUPlayerContextAction::PartyLeave:
		if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
		{
			PartySubsystem->Leave();
		}
		break;

	case ENUPlayerContextAction::PartyKick:
		// 리더가 아니면 서버가 ResultCode 1605로 돌려줌
		if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
		{
			PartySubsystem->Kick(Picked.CharacterIdx);
		}
		break;

	case ENUPlayerContextAction::Cancel:	// 위에서 이미 공용으로 Close 해서 break만
		break;

	default:
		break;
	}
}

FReply UNUPlayerContextMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	// 시야 밖 파티원(PlayerId 0)으로 연 메뉴는 필드에서 고른 게 아니라 넘길 사람이 없다
	if ((Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder) && Target.PlayerId != 0)
	{
		if (ANUPlayerController* PC = GetOwningNUPlayerController())
		{
			PC->OpenNearbyPlayerContextMenu(Key == EKeys::Gamepad_RightShoulder ? 1 : -1);
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNUPlayerContextMenu::HandleRemotePlayerDespawned(int64 PlayerId)
{
	// 시야 밖 파티원을 대상으로 열렸으면 PlayerId가 0이라 여기 걸리지 않음
	if (IsOpen() && Target.PlayerId != 0 && Target.PlayerId == PlayerId)
	{
		Close();
	}
}

ANUPlayerController* UNUPlayerContextMenu::GetOwningNUPlayerController() const
{
	ANUPlayerController* PC = Cast<ANUPlayerController>(GetOwningPlayer());
	return PC;
}

UNUPartySubsystem* UNUPlayerContextMenu::GetPartySubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
}

UNUChatSubsystem* UNUPlayerContextMenu::GetChatSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUChatSubsystem>() : nullptr;
}
