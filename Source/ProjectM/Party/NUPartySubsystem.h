// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUPartyTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUPartySubsystem.generated.h"

class UEmberServerManager;
class UNUChatSubsystem;

// 파티 구성이 바뀔 때, 파티창이 다시 그림 (파티구성, 변경된 이유, 변경된 id)
DECLARE_MULTICAST_DELEGATE_ThreeParams(FNUOnPartyChanged, const FNUPartySnapshot&, ENUPartyUpdateReason, int64);

// 초대·신청을 받았을 때 (bIsInvite, 보낸 사람 이름, 유효시간 초)
DECLARE_MULTICAST_DELEGATE_ThreeParams(FNUOnPartyPrompt, bool, const FString&, int32);

// 답했거나 만료됐을 때 팝업 닫기
DECLARE_MULTICAST_DELEGATE(FNUOnPartyPromptClosed);

// 파티 상태를 들고 있는 서브시스템.
UCLASS()
class PROJECTM_API UNUPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/* --- 송신용 --- */
	
	// 대상의 character_idx. 내 파티가 없으면 이때 서버가 만들어 줌
	// 파티 생성 시 내가 리더가 됨
	void Invite(int64 TargetIdx);

	// 그 파티원 아무나의 character_idx, 알림은 리더에게 감
	void Apply(int64 TargetIdx);

	// 받아둔 초대·신청에 응답함
	void Respond(bool bAccept);
	
	// 전투에 진입 시 팝업이 떠있다면 거절하고 닫는 함수, PlayerController가 호출함
	void DeclinePendingPrompt();
	
	void Leave();

	// 리더만 가능, 아니면 1605 ResultCode 반환
	void Kick(int64 TargetIdx);

	// 어긋남 보정용, 접속이 끊기면 파티도 사라지므로 복구용은 아님
	void RequestMemberList();

	
	/* --- 수신용 (EmberServerManager가 호출) --- */

	void HandleMemberListResponse(int32 ResultCode, const FString& ResponseData);

	// 수락이면 파티원 목록이 실려 옴, 거절이면 ResultCode만
	void HandleRespondResponse(int32 ResultCode, const FString& ResponseData);

	// ResultCode만 오는 요청들 (초대·신청·탈퇴·추방)
	// 받은 응답을 처리함
	void HandleAck(int32 ResultCode);

	void HandleInvited(int64 FromIdx, const FString& FromName, int32 ExpireSec);
	void HandleApplied(int64 FromIdx, const FString& FromName, int32 ExpireSec);
	void HandleUpdated(int64 PartyId, int32 Reason, int64 TargetIdx, const FString& ResponseData);
	void HandleRejected(const FString& FromName, bool bWasInvite);

	const FNUPartySnapshot& GetParty() const;

	// 답을 기다리는 초대·신청이 있으면 그 상대의 character_idx, 없으면 0
	int64 GetPendingFromIdx() const;

public:
	FNUOnPartyChanged OnPartyChanged;
	FNUOnPartyPrompt OnPartyPrompt;
	FNUOnPartyPromptClosed OnPartyPromptClosed;

private:
	UEmberServerManager* GetServer() const;
	UNUChatSubsystem* GetChat() const;

	// 내 캐릭터 idx, 미선택이면 0
	int64 GetMyCharacterIdx() const;

	// 전투 중이면 팝업을 띄울 수 없음
	bool IsInBattle() const;

	// 파티 UI가 생기기 전까지 사용자에게 알림
	void ReportSystemMessage(const FText& Text);

	// 보내기 전 공통 검사, 통과하면 서버 매니저 반환
	UEmberServerManager* PrepareSend(int64 TargetIdx);

	// 초대·신청을 받았을 때 공용 처리
	void BeginPrompt(bool bIsInvite, int64 FromIdx, const FString& FromName, int32 ExpireSec);
	void ClosePrompt();

private:
	FNUPartySnapshot Party;

	// PartyRespond가 상대방을 알아야 함, 0이면 답할 것이 없음
	int64 PendingFromIdx = 0;

	// 서버가 준 만료시간 뒤에 팝업을 자동으로 닫음
	FTimerHandle PendingExpireTimer;
};
