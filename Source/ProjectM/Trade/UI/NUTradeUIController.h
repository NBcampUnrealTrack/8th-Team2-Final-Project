// 1:1 거래 UI 컨트롤러 --- 거래 창과 신청 팝업을 소유한다.
// 거래 데이터(NUTradeSubsystem)는 GameInstance 수명이고, 이 컨트롤러는 그 상태를 구독해 창을 여닫는다.
#pragma once

#include "CoreMinimal.h"
#include "Trade/NUTradeTypes.h"
#include "UI/Controller/NUUIController.h"
#include "NUTradeUIController.generated.h"

class UNUConfirmDialog;
class UNUTradeSubsystem;
class UNUTradeWindowWidget;

// 거래 진행 중 알릴 것 (완료·거절·거리 초과 등). 토스트 UI가 붙기 전까지는 로그만 남는다
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnTradeMessage, const FText&);

UCLASS()
class PROJECTM_API UNUTradeUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	// 컨텍스트 메뉴의 "1:1 거래"가 부른다
	void RequestTradeWith(int64 TargetIdx);

	// 거래 창의 닫기·취소 버튼
	void CancelTrade();

	// 안내 문구 (완료·거절·거리 초과 등) --- 토스트 UI가 붙으면 여기에 물린다
	FNUOnTradeMessage OnTradeMessage;

	virtual void OnServerSessionEnded() override;

protected:
	virtual void OnInitialize() override;
	virtual void OnDeinitialize() override;

private:
	void HandleTradeStateChanged(ENUTradeState NewState);

	// 거래가 끝난 이유를 안내한다 (보류는 실패가 아니라 성공 안내)
	void HandleTradeEnded(ENUTradeEndReason Reason);

	// 신청이 서버에 거절당했을 때 (거리·이미 거래 중 등)
	void HandleRequestFailed(int32 ResultCode);

	void OpenTradeWindow();
	void CloseTradeWindow();

	// 신청 수락/거절 창 (Invited 단계)
	void ShowInviteDialog();
	void DismissInviteDialog();

	// 남은 시간을 1초마다 문구에 다시 쓴다. 만료 판정은 서브시스템 타이머가 하고 여기는 표시만 한다
	void UpdateInviteCountdown();
	FText BuildInviteMessage(const UNUTradeSubsystem& Trade) const;
	void HandleInviteAccepted();
	void HandleInviteDeclined();

	UNUTradeSubsystem* GetTradeSubsystem() const;

	// 거래 창 --- 프로젝트 세팅(NU UI)의 TradeWindowClass로 만든다
	UPROPERTY(Transient)
	TObjectPtr<UNUTradeWindowWidget> TradeWindow;

	// 신청 창 --- Modal이라 매니저 세션 정리 대상이 아니므로 컨트롤러가 직접 닫는다
	UPROPERTY(Transient)
	TObjectPtr<UNUConfirmDialog> InviteDialog;

	// 신청 창이 떠 있는 동안만 도는 표시용 타이머
	FTimerHandle InviteCountdownTimer;
};
