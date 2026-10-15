// 1:1 거래 상태를 들고 있는 서브시스템 (파티와 같은 자리).
// 거래 패킷은 창이 닫혀 있어도, 어느 레벨에 있어도 도착할 수 있어 GameInstance 수명이다.
//
// 서버가 진실의 원천이다 --- 송신은 요청일 뿐이고, 상태는 SVR_Trade_* 푸시로만 바뀐다.
// UI는 이 서브시스템의 상태·델리게이트에만 의존하므로 패킷 형식 변경이 여기서 멈춘다.
#pragma once

#include "CoreMinimal.h"
#include "Net/EmberServerManager.h" // EEmberConnState (UFUNCTION 시그니처)
#include "Subsystems/GameInstanceSubsystem.h"
#include "Trade/NUTradeTypes.h"
#include "NUTradeSubsystem.generated.h"

class UNUInventoryComponent;

struct FGrpcRevGameTradeSide;

// 거래 단계가 바뀔 때 (창 열기/닫기 판단)
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnTradeStateChanged, ENUTradeState);

// 양쪽 제안이 바뀔 때 (칸 다시 그리기)
DECLARE_MULTICAST_DELEGATE(FNUOnTradeOfferChanged);

// 거래가 끝났을 때 (사유별 안내 --- 완료/거절/취소/멀어짐 등)
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnTradeEnded, ENUTradeEndReason);

// 요청이 거절당했을 때 (결과 코드 그대로 --- UI가 문구를 고른다)
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnTradeRequestFailed, int32);

UCLASS()
class PROJECTM_API UNUTradeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 송신 (UI가 호출) ---

	// 상대에게 거래 신청. 대상은 character_idx (필드에서 본 사람·파티원)
	void RequestTrade(int64 TargetIdx);

	// 받은 신청에 응답
	void RespondTrade(bool bAccept);

	// 내 거래 칸에 인벤 아이템을 올리거나(InvSlotNo >= 0) 내린다(-1)
	void SetTradeItem(int32 TradeSlot, int32 InvSlotNo);

	// 내 제안 잠금/해제
	void SetLock(bool bLock);

	// 최종 확정 (양쪽 잠금 후에만 의미가 있다)
	void ConfirmTrade();

	// 취소 (창 닫기 포함)
	void CancelTrade();

	// 전투에 들어갈 때 떠 있는 신청 팝업을 거절 처리한다 (파티의 DeclinePendingPrompt와 같은 자리).
	// 전투 중에는 팝업을 띄울 수 없어 답하지 못한 채 만료되기 때문
	void DeclinePendingInvite();

	// --- 수신 (EmberServerManager가 호출) ---

	void HandleTradeRequested(int64 FromIdx, const FString& FromName, int32 ExpireSec);
	void HandleTradeStarted(int64 TradeId, const FGrpcRevGameTradeSide& MeSide, const FGrpcRevGameTradeSide& OtherSide);
	void HandleTradeUpdated(const FGrpcRevGameTradeSide& MeSide, const FGrpcRevGameTradeSide& OtherSide, int32 InOfferVer);
	void HandleTradeEnded(int32 Reason);

	// 요청 응답의 결과 코드 (200이 아니면 거절 사유)
	void HandleTradeAck(int32 ResultCode);

	// --- 조회 (UI) ---

	ENUTradeState GetState() const { return State; }
	bool IsTrading() const { return State != ENUTradeState::Idle; }
	const FNUTradeOffer& GetMyOffer() const { return MyOffer; }
	const FNUTradeOffer& GetPartnerOffer() const { return PartnerOffer; }
	const FString& GetPartnerName() const { return PartnerName; }
	int64 GetPartnerIdx() const { return PartnerIdx; }

	// 양쪽이 잠갔고 아직 내가 확정하지 않았을 때만 확정 버튼이 살아난다
	bool CanConfirm() const
	{
		return State == ENUTradeState::Active && MyOffer.bLocked && PartnerOffer.bLocked && !MyOffer.bConfirmed;
	}

	// 인벤 칸이 거래에 올라가 있는지 (인벤 창이 회색 처리에 쓴다)
	bool IsInventorySlotOffered(int32 InvSlotNo) const;

	// 취소를 내가 눌렀는지 (상대가 취소했을 때만 안내를 띄우기 위해)
	bool WasCancelRequestedByMe() const { return bCancelRequestedByMe; }

	// 받은 신청의 남은 시간(초). Invited가 아니거나 만료 정보가 없으면 0.
	// 표시용이다 --- 실제 만료 판정은 이 서브시스템의 타이머가 한다 (파티와 같은 규칙)
	int32 GetInviteRemainSeconds() const;

	FNUOnTradeStateChanged OnTradeStateChanged;
	FNUOnTradeOfferChanged OnTradeOfferChanged;
	FNUOnTradeEnded OnTradeEnded;
	FNUOnTradeRequestFailed OnTradeRequestFailed;

private:
	void SetState(ENUTradeState NewState);
	void ResetSession();

	// 서버가 준 한쪽 제안을 6칸 배열로 편다 (서버는 올린 칸만 보낸다)
	void ApplySide(const FGrpcRevGameTradeSide& Side, FNUTradeOffer& OutOffer) const;

	// 원격 신청은 거래 UI를 한 번도 안 연 클라에도 도착한다 --- 방송 전에 구독자(UI 컨트롤러)를 세운다
	void EnsureUIController() const;

	// 조작이 막혔을 때 화면에 알린다 (로그만 남기면 사용자는 왜 안 되는지 모른다)
	void ShowNotice(const FText& Message) const;
	class UNUUIManager* GetUIManager() const;

	// 신청 만료 --- 서버도 같은 시각에 접으므로 응답을 보내지 않고 내 상태만 되돌린다
	void HandleInviteExpired();
	void ClearInviteExpireTimer();

	// 전투 중에는 팝업을 띄울 수 없다 (파티와 같은 판단 기준)
	bool IsInBattle() const;

	// 거래 중 접속이 끊기면 상태가 GameInstance에 남아 재거래가 막힌다 --- 세션 종료 시 정리
	UFUNCTION()
	void HandleServerStateChanged(EEmberConnState NewState);

	UEmberServerManager* GetServer() const;
	UNUInventoryComponent* GetLocalInventory() const;

	ENUTradeState State = ENUTradeState::Idle;

	FNUTradeOffer MyOffer;
	FNUTradeOffer PartnerOffer;

	int64 TradeId = 0;
	int64 PartnerIdx = 0;
	FString PartnerName;

	// 마지막으로 받은 SVR_Trade_Updated.offer_ver --- 확정에 그대로 실어 보낸다.
	// 안 채우면 서버가 1810으로 거절한다
	int32 OfferVer = 0;

	// 취소를 내가 눌렀는지. 서버는 누가 취소했는지 알려주지 않으므로(reason=2 하나),
	// 상대가 취소했을 때만 안내를 띄우려면 클라가 기억해야 한다
	bool bCancelRequestedByMe = false;

	// 잠금 해제를 내가 눌렀는지. 상대가 제안을 바꾸면 서버가 양쪽 잠금을 말없이 풀어
	// 확정 버튼이 이유 없이 꺼진 것처럼 보인다 --- 내가 푼 경우와 갈라야 안내가 안 시끄럽다
	bool bUnlockRequestedByMe = false;

	// 받은 신청의 만료 타이머 (서버가 준 expire_sec 그대로). Invited 동안만 산다
	FTimerHandle InviteExpireTimer;

#if !UE_BUILD_SHIPPING
	// 서버 패킷 없이 거래 창을 확인하는 개발용 명령 (nu.Trade.*)
	void RegisterDebugCommands();
	TArray<IConsoleCommand*> DebugCommands;
#endif
};
