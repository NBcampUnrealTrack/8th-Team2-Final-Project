#include "Trade/UI/NUTradeUIController.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/NUInventoryComponent.h"
#include "TimerManager.h"
#include "Trade/NUTradeSubsystem.h"
#include "Trade/UI/NUTradeWindowWidget.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUTradeUI, Log, All);

void UNUTradeUIController::OnInitialize()
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->OnTradeStateChanged.AddUObject(this, &UNUTradeUIController::HandleTradeStateChanged);
		Trade->OnTradeEnded.AddUObject(this, &UNUTradeUIController::HandleTradeEnded);
		Trade->OnTradeRequestFailed.AddUObject(this, &UNUTradeUIController::HandleRequestFailed);
	}
}

void UNUTradeUIController::OnDeinitialize()
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->OnTradeStateChanged.RemoveAll(this);
		Trade->OnTradeEnded.RemoveAll(this);
		Trade->OnTradeRequestFailed.RemoveAll(this);
	}
	DismissInviteDialog();
	CloseTradeWindow();
}

void UNUTradeUIController::HandleTradeEnded(ENUTradeEndReason Reason)
{
	// 창은 상태가 Idle로 바뀌며 이미 닫혔다 --- 여기서는 왜 끝났는지만 알린다
	FText Message;
	switch (Reason)
	{
	case ENUTradeEndReason::Completed:
		Message = NSLOCTEXT("NUTrade", "EndCompleted", "거래가 완료되었습니다.");
		break;

	case ENUTradeEndReason::CompletedPending:
		// 실패가 아니다 --- 받을 아이템은 서버에 남아 있고 인벤을 비우면 들어온다
		Message = NSLOCTEXT("NUTrade", "EndPending", "거래가 완료되었습니다. 가방이 가득 차 일부는 나중에 받을 수 있습니다.");
		break;

	case ENUTradeEndReason::Declined:
		Message = NSLOCTEXT("NUTrade", "EndDeclined", "상대가 거래를 거절했습니다.");
		break;

	case ENUTradeEndReason::Disconnected:
		Message = NSLOCTEXT("NUTrade", "EndDisconnected", "상대의 접속이 끊겨 거래가 취소되었습니다.");
		break;

	case ENUTradeEndReason::TooFar:
		Message = NSLOCTEXT("NUTrade", "EndTooFar", "거리가 멀어져 거래가 취소되었습니다.");
		break;

	case ENUTradeEndReason::ItemMismatch:
		Message = NSLOCTEXT("NUTrade", "EndMismatch", "올린 아이템이 바뀌어 거래가 취소되었습니다.");
		break;

	case ENUTradeEndReason::Expired:
		Message = NSLOCTEXT("NUTrade", "EndExpired", "시간이 지나 거래가 취소되었습니다.");
		break;

	case ENUTradeEndReason::Failed:
		Message = NSLOCTEXT("NUTrade", "EndFailed", "오류로 거래가 취소되었습니다. 아이템은 그대로입니다.");
		break;

	case ENUTradeEndReason::Cancelled:
		{
			// 내가 누른 취소는 이미 아는 일이라 조용히 넘긴다.
			// 상대가 취소하면 창이 갑자기 닫히므로 반드시 알려야 한다
			const UNUTradeSubsystem* Trade = GetTradeSubsystem();
			if (Trade != nullptr && Trade->WasCancelRequestedByMe())
			{
				UE_LOG(LogNUTradeUI, Log, TEXT("거래 취소됨 (내 요청)"));
				return;
			}
			Message = NSLOCTEXT("NUTrade", "EndCancelledByPartner", "상대가 거래를 취소했습니다.");
			break;
		}

	default:
		UE_LOG(LogNUTradeUI, Log, TEXT("거래 종료 [사유=%d]"), static_cast<int32>(Reason));
		return;
	}

	UE_LOG(LogNUTradeUI, Log, TEXT("%s"), *Message.ToString());
	UNUSFXSubsystem::Play(GetOwningPlayerController(), Reason == ENUTradeEndReason::Completed || Reason == ENUTradeEndReason::CompletedPending
		? NU::SFX::Trade::Complete : NU::SFX::Trade::Cancel);
	if (UNUUIManager* UI = GetManager())
	{
		UI->ShowNotice(Message);
	}
	OnTradeMessage.Broadcast(Message);
}

void UNUTradeUIController::HandleRequestFailed(int32 ResultCode)
{
	// 서버 결과 코드 (gRPC 가이드 §14.2) --- 모르는 코드는 숫자 그대로 보여준다
	FText Message;
	switch (ResultCode)
	{
	case 1800: Message = NSLOCTEXT("NUTrade", "Err1800", "상대를 찾을 수 없습니다."); break;
	case 1801: Message = NSLOCTEXT("NUTrade", "Err1801", "이미 다른 거래가 진행 중입니다."); break;
	case 1802: Message = NSLOCTEXT("NUTrade", "Err1802", "상대가 너무 멀리 있습니다."); break;
	case 1803: Message = NSLOCTEXT("NUTrade", "Err1803", "자신과는 거래할 수 없습니다."); break;
	case 1804: Message = NSLOCTEXT("NUTrade", "Err1804", "진행 중인 거래가 없습니다."); break;
	case 1805: Message = NSLOCTEXT("NUTrade", "Err1805", "거래 신청 시간이 지났습니다."); break;
	case 1806: Message = NSLOCTEXT("NUTrade", "Err1806", "올릴 수 없는 칸입니다."); break;
	case 1807: Message = NSLOCTEXT("NUTrade", "Err1807", "빈 칸은 올릴 수 없습니다."); break;
	case 1808: Message = NSLOCTEXT("NUTrade", "Err1808", "이미 다른 칸에 올린 아이템입니다."); break;
	case 1809: Message = NSLOCTEXT("NUTrade", "Err1809", "양쪽 모두 잠금해야 확정할 수 있습니다."); break;
	case 1810: Message = NSLOCTEXT("NUTrade", "Err1810", "제안이 바뀌어 확정하지 못했습니다. 다시 확인해 주세요."); break;
	case 1811: Message = NSLOCTEXT("NUTrade", "Err1811", "올린 아이템이 바뀌어 거래가 취소되었습니다."); break;
	case 1812: Message = NSLOCTEXT("NUTrade", "Err1812", "거래 처리에 실패했습니다. 잠시 후 다시 시도해 주세요."); break;
	case 1813: Message = NSLOCTEXT("NUTrade", "Err1813", "거래 기능이 잠시 막혀 있습니다."); break;
	default:
		Message = FText::Format(NSLOCTEXT("NUTrade", "ErrOther", "거래 요청이 거절되었습니다. ({0})"), ResultCode);
		break;
	}

	UE_LOG(LogNUTradeUI, Warning, TEXT("%s"), *Message.ToString());
	if (UNUUIManager* UI = GetManager())
	{
		UI->ShowNotice(Message);
	}
	OnTradeMessage.Broadcast(Message);
}

void UNUTradeUIController::RequestTradeWith(int64 TargetIdx)
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->RequestTrade(TargetIdx);
	}
}

void UNUTradeUIController::CancelTrade()
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->CancelTrade();
	}
}

void UNUTradeUIController::OnServerSessionEnded()
{
	// 거래 창은 bCloseOnSessionEnd로 매니저가 이미 닫았다 --- 참조만 정리한다
	TradeWindow = nullptr;
	// 신청 창은 매니저 대상이 아니라 직접 닫는다 (세션이 없으면 응답도 무의미)
	DismissInviteDialog();
}

void UNUTradeUIController::HandleTradeStateChanged(ENUTradeState NewState)
{
	switch (NewState)
	{
	case ENUTradeState::Invited:
		// 상대가 신청 --- "[이름]에게 거래 요청이 왔습니다" 수락/거절
		ShowInviteDialog();
		break;

	case ENUTradeState::Active:
	case ENUTradeState::Confirming:
		DismissInviteDialog();
		OpenTradeWindow();
		break;

	case ENUTradeState::Idle:
		// 거절·취소·완료 공통 --- 떠 있는 거래 UI를 전부 내린다
		DismissInviteDialog();
		CloseTradeWindow();
		break;

	default:
		// Requesting(내가 신청) --- 상대 응답 대기, 아직 보여줄 창이 없다
		break;
	}
}

void UNUTradeUIController::OpenTradeWindow()
{
	APlayerController* PC = GetOwningPlayerController();
	UNUUIManager* UI = GetManager();
	if (PC == nullptr || UI == nullptr)
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUTradeWindowWidget> WindowClass = Settings ? Settings->TradeWindowClass.LoadSynchronous() : nullptr;
	if (WindowClass == nullptr)
	{
		UE_LOG(LogNUTradeUI, Warning, TEXT("거래 창 WBP 미지정 --- 프로젝트 세팅 > 게임 > NU UI"));
		return;
	}

	// 레벨이 바뀌었으면 이전 월드의 위젯은 버리고 새로 만든다
	GetOrCreateWidget(TradeWindow, WindowClass);
	if (TradeWindow == nullptr)
	{
		return;
	}

	// 통합 캐릭터 메뉴는 화면을 다 덮고 소지품 키도 막는다 --- 거래 창 아래에 남으면 올릴 아이템을 고를 소지품 창을 못 연다
	const ANUPlayerState* State = PC->GetPlayerState<ANUPlayerState>();
	if (UNUInventoryComponent* Inventory = State != nullptr ? State->GetInventoryComponent() : nullptr)
	{
		Inventory->CloseCharacterMenu();
	}

	FNUUIWindowParams Params;
	Params.bDimBackground = true;       // 거래 중에는 다른 조작을 유도하지 않는다
	Params.bCloseOnSessionEnd = true;   // 세션이 끊기면 거래도 무효
	UI->OpenWindow(TradeWindow, Params, FSimpleDelegate::CreateUObject(this, &UNUTradeUIController::CancelTrade));
}

void UNUTradeUIController::CloseTradeWindow()
{
	if (TradeWindow == nullptr)
	{
		return;
	}
	if (UNUUIManager* UI = GetManager())
	{
		UI->CloseWindow(TradeWindow);
	}
}

void UNUTradeUIController::ShowInviteDialog()
{
	APlayerController* PC = GetOwningPlayerController();
	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (PC == nullptr || Trade == nullptr)
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUConfirmDialog> DialogClass = Settings ? Settings->TradeInviteDialogClass.LoadSynchronous() : nullptr;
	if (DialogClass == nullptr)
	{
		UE_LOG(LogNUTradeUI, Warning, TEXT("거래 신청 창 WBP 미지정 --- 프로젝트 세팅 > 게임 > NU UI"));
		return;
	}

	DismissInviteDialog();
	InviteDialog = UNUConfirmDialog::Show(
		PC, DialogClass,
		BuildInviteMessage(*Trade),
		NSLOCTEXT("NUTrade", "InviteAccept", "수락"),
		FSimpleDelegate::CreateUObject(this, &UNUTradeUIController::HandleInviteAccepted),
		FSimpleDelegate::CreateUObject(this, &UNUTradeUIController::HandleInviteDeclined),
		NSLOCTEXT("NUTrade", "InviteDecline", "거절"));
	UNUSFXSubsystem::Play(PC, NU::SFX::Trade::Invite);

	// 남은 시간을 모르면 언제 사라질지 몰라 수락을 미루게 된다 (파티 팝업과 같은 안내)
	if (InviteDialog != nullptr && Trade->GetInviteRemainSeconds() > 0)
	{
		PC->GetWorldTimerManager().SetTimer(
			InviteCountdownTimer,
			FTimerDelegate::CreateUObject(this, &UNUTradeUIController::UpdateInviteCountdown),
			1.0f, true);
	}
}

FText UNUTradeUIController::BuildInviteMessage(const UNUTradeSubsystem& Trade) const
{
	const FText Name = FText::FromString(Trade.GetPartnerName());
	const int32 Remain = Trade.GetInviteRemainSeconds();

	// 서버가 만료 시간을 안 준 경우도 있어 남은 시간은 있을 때만 붙인다
	return Remain > 0
		? FText::Format(NSLOCTEXT("NUTrade", "InviteMessageTimed", "{0}님이 거래를 신청했습니다. ({1}초)"),
			Name, FText::AsNumber(Remain))
		: FText::Format(NSLOCTEXT("NUTrade", "InviteMessage", "{0}님이 거래를 신청했습니다."), Name);
}

void UNUTradeUIController::UpdateInviteCountdown()
{
	const UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (InviteDialog == nullptr || Trade == nullptr)
	{
		return;
	}

	InviteDialog->SetMessage(BuildInviteMessage(*Trade));
}

void UNUTradeUIController::DismissInviteDialog()
{
	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetWorldTimerManager().ClearTimer(InviteCountdownTimer);
	}

	if (InviteDialog != nullptr)
	{
		// 콜백 없이 닫는다 --- 신청이 이미 무효인 경로라 거절 응답을 보내면 안 된다
		InviteDialog->Dismiss();
		InviteDialog = nullptr;
	}
}

void UNUTradeUIController::HandleInviteAccepted()
{
	InviteDialog = nullptr; // 창은 스스로 닫힌 뒤 호출된다
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->RespondTrade(true);
	}
}

void UNUTradeUIController::HandleInviteDeclined()
{
	InviteDialog = nullptr;
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->RespondTrade(false);
	}
}

UNUTradeSubsystem* UNUTradeUIController::GetTradeSubsystem() const
{
	const APlayerController* PC = GetOwningPlayerController();
	const UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNUTradeSubsystem>() : nullptr;
}
