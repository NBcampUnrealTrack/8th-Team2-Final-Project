// Fill out your copyright notice in the Description page of Project Settings.


#include "NUPartySubsystem.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "Chat/NUChatSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Net/EmberServerManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUParty, Log, All);

void UNUPartySubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PendingExpireTimer);
	}

	Super::Deinitialize();
}

UEmberServerManager* UNUPartySubsystem::PrepareSend(int64 TargetIdx)
{
	UEmberServerManager* Server = GetServer();
	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		ReportSystemMessage(NSLOCTEXT("NUParty", "NotConnected", "서버에 연결되어 있지 않습니다."));
		return nullptr;
	}

	// 시야를 벗어난 대상은 character_idx가 0으로 옴
	if (TargetIdx == 0)
	{
		ReportSystemMessage(NUParty::GetResultMessage(1601));
		return nullptr;
	}

	// 자기 자신의 경우 서버에서도 막지만 클라에서 막는것이 필요없는 통신 횟수 줄어들음
	if (TargetIdx == GetMyCharacterIdx())
	{
		ReportSystemMessage(NUParty::GetResultMessage(1607));
		return nullptr;
	}

	return Server;
}

void UNUPartySubsystem::Invite(int64 TargetIdx)
{
	if (UEmberServerManager* Server = PrepareSend(TargetIdx))
	{
		UE_LOG(LogNUParty, Log, TEXT("파티 초대 전송 target=%lld"), TargetIdx);
		Server->SendPartyInvite(TargetIdx);
	}
}

void UNUPartySubsystem::Apply(int64 TargetIdx)
{
	if (UEmberServerManager* Server = PrepareSend(TargetIdx))
	{
		UE_LOG(LogNUParty, Log, TEXT("파티 신청 전송 target=%lld"), TargetIdx);
		Server->SendPartyApply(TargetIdx);
	}
}

void UNUPartySubsystem::Respond(bool bAccept)
{
	// 답할 초대, 신청이 없음
	if (PendingFromIdx == 0)
	{
		return;
	}

	int64 TargetIdx = PendingFromIdx;
	ClosePrompt();   // 두 번 답하지 않도록 팝업 창 먼저 닫음

	UEmberServerManager* Server = GetServer();
	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		ReportSystemMessage(NSLOCTEXT("NUParty", "NotConnected", "서버에 연결되어 있지 않습니다."));
		return;
	}

	UE_LOG(LogNUParty, Log, TEXT("파티 응답 전송 target=%lld accept=%d"), TargetIdx, bAccept ? 1 : 0);
	Server->SendPartyRespond(TargetIdx, bAccept);
}

void UNUPartySubsystem::DeclinePendingPrompt()
{
	if (PendingFromIdx == 0)
	{
		return;
	}

	UE_LOG(LogNUParty, Log, TEXT("전투 진입으로 대기 중인 파티 요청 거절 from=%lld"), PendingFromIdx);

	ReportSystemMessage(NSLOCTEXT("NUParty", "PromptCanceledByBattle",
		"전투에 들어가 대기 중이던 파티 요청을 거절했습니다."));

	// Respond가 ClosePrompt까지 해주므로 팝업과 커서가 같이 정리됨
	Respond(false);
}

void UNUPartySubsystem::Leave()
{
	UEmberServerManager* Server = GetServer();
	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		return;
	}

	Server->SendPartyLeave();
}

void UNUPartySubsystem::Kick(int64 TargetIdx)
{
	if (UEmberServerManager* Server = PrepareSend(TargetIdx))
	{
		Server->SendPartyKick(TargetIdx);
	}
}

void UNUPartySubsystem::RequestMemberList()
{
	UEmberServerManager* Server = GetServer();
	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		return;
	}

	Server->SendPartyMemberList();
}

void UNUPartySubsystem::HandleAck(int32 ResultCode)
{
	// 성공은 넘어감
	if (ResultCode == NUParty::RC_SUCCESS)
	{
		return;
	}

	// 나머진 시스템 메세지 출력
	ReportSystemMessage(NUParty::GetResultMessage(ResultCode));
}

void UNUPartySubsystem::HandleMemberListResponse(int32 ResultCode, const FString& ResponseData)
{
	if (ResultCode != NUParty::RC_SUCCESS)
	{
		ReportSystemMessage(NUParty::GetResultMessage(ResultCode));
		return;
	}

	FNUPartySnapshot Parsed;
	if (!NUParty::ParseMemberList(ResponseData, Parsed))
	{
		UE_LOG(LogNUParty, Warning, TEXT("파티원 목록을 읽지 못했습니다: %s"), *ResponseData);
		return;
	}

	Party = MoveTemp(Parsed);
	OnPartyChanged.Broadcast(Party, ENUPartyUpdateReason::None, 0);
}

void UNUPartySubsystem::HandleRespondResponse(int32 ResultCode, const FString& ResponseData)
{
	if (ResultCode != NUParty::RC_SUCCESS)
	{
		ReportSystemMessage(NUParty::GetResultMessage(ResultCode));
		return;
	}

	// 거절이면 목록이 실려 오지 않음, 파싱 실패는 정상 경로
	FNUPartySnapshot Parsed;
	if (!NUParty::ParseMemberList(ResponseData, Parsed))
	{
		return;
	}

	Party = MoveTemp(Parsed);
	OnPartyChanged.Broadcast(Party, ENUPartyUpdateReason::Joined, GetMyCharacterIdx());
}

void UNUPartySubsystem::HandleUpdated(int64 PartyId, int32 Reason, int64 TargetIdx, const FString& ResponseData)
{
	const ENUPartyUpdateReason UpdateReason = static_cast<ENUPartyUpdateReason>(Reason);

	// 이름은 교체 전 목록에서 찾아야 함, 나간 사람은 새 목록에 없음
	FString TargetName;
	if (const FString* Found = Party.FindMemberName(TargetIdx))
	{
		TargetName = *Found;
	}

	FNUPartySnapshot Parsed;
	if (NUParty::ParseMemberList(ResponseData, Parsed))
	{
		// 가입 알림이면 새로 들어온 사람이라 이전 목록에 없음
		if (TargetName.IsEmpty())
		{
			if (const FString* Found = Parsed.FindMemberName(TargetIdx))
			{
				TargetName = *Found;
			}
		}

		Party = MoveTemp(Parsed);
	}
	else
	{
		// 푸시에 목록이 없으면 party_id만이라도 반영해서 UI가 남지 않게 함
		UE_LOG(LogNUParty, Warning, TEXT("파티 갱신 푸시에 목록이 없습니다 (reason=%d): %s"),
			Reason, *ResponseData);

		if (PartyId == 0)
		{
			Party = FNUPartySnapshot();
		}
		else
		{
			Party.PartyId = PartyId;
		}
	}

	UE_LOG(LogNUParty, Log, TEXT("파티 갱신 partyId=%lld reason=%d target=%lld 인원=%d"),
		Party.PartyId, Reason, TargetIdx, Party.Members.Num());

	FText Message = NUParty::GetUpdateMessage(UpdateReason, TargetName);
	if (!Message.IsEmpty())
	{
		ReportSystemMessage(Message);
	}

	OnPartyChanged.Broadcast(Party, UpdateReason, TargetIdx);
}

void UNUPartySubsystem::HandleInvited(int64 FromIdx, const FString& FromName, int32 ExpireSec)
{
	BeginPrompt(true, FromIdx, FromName, ExpireSec);
}

void UNUPartySubsystem::HandleApplied(int64 FromIdx, const FString& FromName, int32 ExpireSec)
{
	BeginPrompt(false, FromIdx, FromName, ExpireSec);
}

void UNUPartySubsystem::HandleRejected(const FString& FromName, bool bWasInvite)
{
	const FText Name = FText::FromString(FromName);

	ReportSystemMessage(bWasInvite
		? FText::Format(NSLOCTEXT("NUParty", "InviteRejected", "{0}님이 파티 초대를 거절했습니다."), Name)
		: FText::Format(NSLOCTEXT("NUParty", "ApplyRejected", "{0}님이 파티 신청을 거절했습니다."), Name));
}

const FNUPartySnapshot& UNUPartySubsystem::GetParty() const
{
	return Party;
}

int64 UNUPartySubsystem::GetPendingFromIdx() const
{
	return PendingFromIdx;
}

void UNUPartySubsystem::BeginPrompt(bool bIsInvite, int64 FromIdx, const FString& FromName, int32 ExpireSec)
{
	// 전투 중엔 팝업을 띄울 수 없게 자동으로 요청 거절 처리
	if (IsInBattle())
	{
		ReportSystemMessage(FText::Format(
			NSLOCTEXT("NUParty", "PromptInBattle", "전투 중이라 {0}님의 파티 요청을 거절했습니다."),
			FText::FromString(FromName)));

		// PendingFromIdx를 사용하지 않아서 Respond는 그냥 반환하므로 거절을 직접 보냄
		if (UEmberServerManager* Server = GetServer())
		{
			Server->SendPartyRespond(FromIdx, false);
		}
		return;
	}

	// 답하지 않은 것이 남아 있으면 닫고 새창을 띄움
	ClosePrompt();

	PendingFromIdx = FromIdx;

	UE_LOG(LogNUParty, Log, TEXT("파티 %s 수신 from=%lld [%s] %d초"),
		bIsInvite ? TEXT("초대") : TEXT("신청"), FromIdx, *FromName, ExpireSec);

	FText Name = FText::FromString(FromName);
	ReportSystemMessage(bIsInvite
		? FText::Format(NSLOCTEXT("NUParty", "Invited", "{0}님이 파티에 초대했습니다."), Name)
		: FText::Format(NSLOCTEXT("NUParty", "Applied", "{0}님이 파티 가입을 신청했습니다."), Name));

	OnPartyPrompt.Broadcast(bIsInvite, FromName, ExpireSec);

	// 서버가 준 값을 그대로 사용,
	// 시간 초과 시 PartyRespond가 1606으로 거부됨
	if (ExpireSec > 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			GI->GetTimerManager().SetTimer(
				PendingExpireTimer,
				FTimerDelegate::CreateUObject(this, &ThisClass::ClosePrompt),
				static_cast<float>(ExpireSec),
				false
				);
		}
	}
}

void UNUPartySubsystem::ClosePrompt()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PendingExpireTimer);
	}

	if (PendingFromIdx == 0)
	{
		return;
	}

	PendingFromIdx = 0;
	OnPartyPromptClosed.Broadcast();
}

UEmberServerManager* UNUPartySubsystem::GetServer() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

UNUChatSubsystem* UNUPartySubsystem::GetChat() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUChatSubsystem>() : nullptr;
}

int64 UNUPartySubsystem::GetMyCharacterIdx() const
{
	UGameInstance* GI = GetGameInstance();
	UNUCharacterManager* Characters = GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;

	return IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;
}

bool UNUPartySubsystem::IsInBattle() const
{
	// 서버에 보고하는 값을 가져와서 사용
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	const UFieldSyncSubsystem* Sync = World ? World->GetSubsystem<UFieldSyncSubsystem>() : nullptr;

	return IsValid(Sync) && Sync->GetMyActivity() == EFieldActivity::Battle;
}

void UNUPartySubsystem::ReportSystemMessage(const FText& Text)
{
	// 시스템 채팅으로 응답 처리
	// 이후에 UI 추가되면 UI에 띄우고 남길지 생각
	if (UNUChatSubsystem* Chat = GetChat())
	{
		Chat->AddSystemMessage(Text);
		return;
	}

	UE_LOG(LogNUParty, Warning, TEXT("%s"), *Text.ToString());
}
