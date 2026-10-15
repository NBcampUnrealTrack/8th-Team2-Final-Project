// 1:1 거래 데이터 타입. 서버 스펙(gRPC 가이드 §14)과 1:1로 맞춘다.
#pragma once

#include "CoreMinimal.h"
#include "NUTradeTypes.generated.h"

// 한 거래 세션의 진행 단계
UENUM(BlueprintType)
enum class ENUTradeState : uint8
{
	// 거래 중이 아님
	Idle,

	// 내가 신청하고 답을 기다리는 중
	Requesting,

	// 상대가 신청해 와서 수락/거절을 골라야 함
	Invited,

	// 양쪽이 아이템을 올리는 중 (거래 창이 떠 있는 단계)
	Active,

	// 양쪽 다 확정해 서버가 교환을 처리하는 중
	Confirming
};

// 거래 종료 사유 (SVR_Trade_Ended.reason). 값은 서버 TradeEndReason과 1:1
UENUM(BlueprintType)
enum class ENUTradeEndReason : uint8
{
	// 교환 성사
	Completed = 0,

	// 성사됐지만 인벤이 꽉 차 일부가 보류됨 --- 실패가 아니다
	CompletedPending = 1,

	// 누군가 취소
	Cancelled = 2,

	// 신청이 거절됨 (거래 창은 열린 적이 없다)
	Declined = 3,

	// 한쪽이 접속을 끊음 · 캐릭터 교체
	Disconnected = 4,

	// 거리가 멀어짐
	TooFar = 5,

	// 올린 아이템이 인벤에서 바뀜
	ItemMismatch = 6,

	// 방치되어 만료
	Expired = 7,

	// 서버 오류 (아이템은 원위치)
	Failed = 8
};

// 거래 칸 하나 (양쪽이 각자 이만큼 올린다)
USTRUCT(BlueprintType)
struct FNUTradeSlot
{
	GENERATED_BODY()

	// 비어 있으면 NAME_None
	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	FName ItemRowName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	int32 Count = 0;

	// 올린 사람의 인벤 칸 (표시용 --- 내 인벤에서 어느 칸이 올라갔는지 회색 처리에 쓴다)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	int32 InvSlotNo = INDEX_NONE;

	bool IsEmpty() const { return ItemRowName.IsNone() || Count <= 0; }
};

// 한쪽의 제안 (내 것 / 상대 것 각각 하나씩)
USTRUCT(BlueprintType)
struct FNUTradeOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	TArray<FNUTradeSlot> Slots;

	// 잠금하면 상대가 확정 버튼을 누를 수 있다.
	// 잠금 뒤 어느 쪽이든 제안을 바꾸면 서버가 양쪽 잠금을 함께 푼다 (사기 방지)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	bool bLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Trade")
	bool bConfirmed = false;
};

namespace NUTrade
{
	// 한쪽이 올릴 수 있는 칸 수 (서버 trade_slot 0~5)
	constexpr int32 SlotCount = 6;

	// 거래 신청 유효시간 (서버 expire_sec 기본값)
	constexpr int32 InviteExpireSec = 60;
}
