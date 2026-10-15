// Fill out your copyright notice in the Description page of Project Settings.

#include "Net/EmberChatManager.h"
#include "Net/EmberServerManager.h"
#include "Chat/NUChatSubsystem.h"
#include "TurboLinkGrpcManager.h"
#include "TurboLinkGrpcUtilities.h"
#include "TurboLinkGrpcConfig.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogEmberChat, Log, All);

namespace EmberChatCfg
{
	// 로컬 개발에는 nginx 가 없어 채팅 서버에 직접 붙는다. 원격은 게임과 같은 :7000 에
	// 붙고 nginx 가 gRPC 경로(/rev.chat.ChatService/)로 갈라준다.
	constexpr int32 LocalChatPort = 7100;

	// 게임 핑과 별개다. 두 스트림 모두 각자 핑을 보내야 한다.
	constexpr float PING_INTERVAL_SEC = 30.0f;

	constexpr int32 RC_SUCCESS = 200;

	// 서버가 만들어 둔 기본 채널. ChatLogin 시 서버가 자동 입장시켜 주므로 클라가 Join 하지 않아도 된다.
	const FString CHANNEL_WORLD = TEXT("world");
}

/////////////////////////////////////////////////////////////////
// 수명주기
/////////////////////////////////////////////////////////////////

void UEmberChatManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEmberChatManager::Deinitialize()
{
	Disconnect();
	Super::Deinitialize();
}

void UEmberChatManager::SetState(EEmberChatState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	OnChatStateChanged.Broadcast(NewState);
}

int64 UEmberChatManager::NowUnixSeconds() const
{
	return FDateTime::UtcNow().ToUnixTimestamp();
}

UEmberServerManager* UEmberChatManager::GetServer() const
{
	UGameInstance* GI = GetGameInstance();
	return GI != nullptr ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

UNUChatSubsystem* UEmberChatManager::GetChat() const
{
	UGameInstance* GI = GetGameInstance();
	return GI != nullptr ? GI->GetSubsystem<UNUChatSubsystem>() : nullptr;
}

/////////////////////////////////////////////////////////////////
// 접속
/////////////////////////////////////////////////////////////////

FString UEmberChatManager::GetChatEndPoint() const
{
	UEmberServerManager* Server = GetServer();
	if (!IsValid(Server))
	{
		return FString();
	}

	// 원격은 게임과 같은 주소·포트 (nginx 가 경로로 분기).
	// 로컬은 nginx 가 없으므로 채팅 서버 포트로 직접 붙는다.
	const FString Host = Server->GetServerHost();
	const bool bIsLocal = Host == TEXT("127.0.0.1") || Host == TEXT("localhost");
	const int32 Port = bIsLocal ? EmberChatCfg::LocalChatPort : Server->GetServerPort();
	return FString::Printf(TEXT("%s:%d"), *Host, Port);
}

void UEmberChatManager::ApplyChatEndPoint()
{
	// GameService 와 같은 방식 — 접속 직전에 코드 값으로 ini 를 덮는다.
	if (UTurboLinkGrpcConfig* Config = GetMutableDefault<UTurboLinkGrpcConfig>())
	{
		Config->ServiceEndPoint.Add(TEXT("ChatService"), GetChatEndPoint());
	}
	UE_LOG(LogEmberChat, Log, TEXT("채팅 서버 대상: %s"), *GetChatEndPoint());
}

void UEmberChatManager::Connect()
{
	if (bStreamStarted || State == EEmberChatState::Connecting)
	{
		return;
	}

	if (!IsValid(GetServer()))
	{
		UE_LOG(LogEmberChat, Error, TEXT("EmberServerManager 없음"));
		return;
	}

	ApplyChatEndPoint();
	SetState(EEmberChatState::Connecting);

	UTurboLinkGrpcManager* Manager = UTurboLinkGrpcUtilities::GetTurboLinkGrpcManager(this);
	if (Manager == nullptr)
	{
		UE_LOG(LogEmberChat, Error, TEXT("TurboLinkGrpcManager 없음"));
		SetState(EEmberChatState::Failed);
		return;
	}

	Service = Cast<UChatService>(Manager->MakeService(TEXT("ChatService")));
	if (Service == nullptr)
	{
		UE_LOG(LogEmberChat, Error, TEXT("ChatService 생성 실패 (endpoint 설정 확인)"));
		SetState(EEmberChatState::Failed);
		return;
	}

	// MakeService 는 이름이 같으면 같은 객체를 준다 — 재접속 때 중복 바인딩되지 않게 AddUnique.
	Service->OnServiceStateChanged.AddUniqueDynamic(this, &UEmberChatManager::HandleServiceStateChanged);
	Service->Connect();
	UE_LOG(LogEmberChat, Log, TEXT("채팅 서버 연결 시작"));
}

void UEmberChatManager::NotifyCharacterSelected()
{
	bCharacterReady = true;

	// 스트림이 아직 안 열렸으면 열릴 때 알아서 보낸다(HandleServiceStateChanged).
	if (!bStreamStarted || !IsValid(Client))
	{
		return;
	}

	// **캐릭터를 바꿨으면 ChatLogin 을 다시 보내야 한다.** 채팅 세션의 신원은 로그인 때
	// 스냅샷한 값이라 그대로 두면 영영 옛 캐릭터 이름으로 발화하고 귓속말도 옛 신원으로 닿는다.
	// 서버는 두 번째 ChatLogin 을 에러가 아니라 신원 갱신으로 처리하고 채널 멤버십도 유지한다.
	SendChatLogin();
}

void UEmberChatManager::Disconnect()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PingTimer);
	}
	if (Client != nullptr && StreamHandle.Value != 0)
	{
		Client->TryCancel(StreamHandle);
	}
	StreamHandle = FGrpcContextHandle();
	Client = nullptr;
	Service = nullptr;
	bStreamStarted = false;
	bCharacterReady = false;
	MyPlayerName.Empty();
	MyPlayerId = 0;
	SetState(EEmberChatState::Idle);
}

void UEmberChatManager::HandleServiceStateChanged(EGrpcServiceState NewServiceState)
{
	UE_LOG(LogEmberChat, Log, TEXT("ChatServiceState=%d"), (int32)NewServiceState);

	if (NewServiceState == EGrpcServiceState::Ready && !bStreamStarted)
	{
		bStreamStarted = true;

		Client = Service->MakeClient();
		if (Client == nullptr)
		{
			UE_LOG(LogEmberChat, Error, TEXT("ChatServiceClient 생성 실패"));
			SetState(EEmberChatState::Failed);
			return;
		}

		Client->OnChatResponse.AddUniqueDynamic(this, &UEmberChatManager::HandleChatResponse);
		StreamHandle = Client->InitChat(); // 양방향 스트림 열기

		SetState(EEmberChatState::LoggingIn);

		// 캐릭터를 이미 골랐으면 바로 로그인하고, 아니면 고를 때까지 스트림만 열어둔다.
		// (스트림 열림과 캐릭터 선택은 순서가 정해져 있지 않다)
		if (bCharacterReady)
		{
			SendChatLogin();
		}
	}
	else if (NewServiceState == EGrpcServiceState::TransientFailure || NewServiceState == EGrpcServiceState::Shutdown)
	{
		// **게임이 끊긴 게 아니다.** 채팅만 끊겼을 수 있으므로 게임 쪽은 건드리지 않는다.
		if (UNUChatSubsystem* Chat = GetChat())
		{
			Chat->AddSystemMessage(NSLOCTEXT("NUChat", "ChatDisconnected", "채팅 서버 연결이 끊겼습니다."));
		}
		bStreamStarted = false;
		SetState(EEmberChatState::Failed);
	}
}

/////////////////////////////////////////////////////////////////
// 송신
/////////////////////////////////////////////////////////////////

FGrpcRevChatChatMessage UEmberChatManager::MakeEnvelope() const
{
	FGrpcRevChatChatMessage Msg;

	UEmberServerManager* Server = GetServer();
	Msg.UserId = IsValid(Server) ? Server->GetUserId() : FString();
	Msg.Timestamp = NowUnixSeconds();

	// proto3 optional 은 값과 _xxxCase 를 **둘 다** 세팅해야 마샬링이 전송한다.
	// 빼먹으면 토큰이 안 실려 채팅 로그인이 401 로 거부된다 (게임 스트림과 같은 함정).
	Msg._token.Token = IsValid(Server) ? Server->GetSessionToken() : FString();
	Msg._token._tokenCase = EGrpcRevChatChatMessage_token::Token;

	return Msg;
}

void UEmberChatManager::SendChatLogin()
{
	if (!IsValid(Client))
	{
		return;
	}

	FGrpcRevChatChatMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevChatChatMessageMessage_type::ChatLogin;
	Req.Message_type.ChatLogin = MakeShared<FGrpcRevChatChatLogin>();

	Client->Chat(StreamHandle, Req);
}

void UEmberChatManager::SendPing()
{
	if (!IsValid(Client) || !IsReady())
	{
		return;
	}

	FGrpcRevChatChatMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevChatChatMessageMessage_type::Ping;
	Req.Message_type.Ping = MakeShared<FGrpcRevChatChatPing>();
	Req.Message_type.Ping->ClientTime = FDateTime::UtcNow().ToUnixTimestamp() * 1000;

	Client->Chat(StreamHandle, Req);
}

void UEmberChatManager::SendChat(const FString& ChannelId, const FString& Text)
{
	if (!IsValid(Client) || !IsReady())
	{
		if (UNUChatSubsystem* Chat = GetChat())
		{
			Chat->AddSystemMessage(NSLOCTEXT("NUChat", "ChatNotReady", "채팅 서버에 연결되어 있지 않습니다."));
		}
		return;
	}

	FGrpcRevChatChatMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevChatChatMessageMessage_type::ChatSend;
	Req.Message_type.ChatSend = MakeShared<FGrpcRevChatChatSend>();
	Req.Message_type.ChatSend->ChannelId = ChannelId;
	Req.Message_type.ChatSend->Text = Text;

	Client->Chat(StreamHandle, Req);
}

void UEmberChatManager::SendWhisperByIdx(int64 TargetCharacterIdx, const FString& Text)
{
	if (!IsValid(Client) || !IsReady() || TargetCharacterIdx == 0)
	{
		if (UNUChatSubsystem* Chat = GetChat())
		{
			Chat->AddSystemMessage(NSLOCTEXT("NUChat", "ChatNotReady", "채팅 서버에 연결되어 있지 않습니다."));
		}
		return;
	}

	FGrpcRevChatChatMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevChatChatMessageMessage_type::ChatWhisper;
	Req.Message_type.ChatWhisper = MakeShared<FGrpcRevChatChatWhisper>();
	// target_idx 가 0이 아니면 서버는 이것만 보고 target_name 은 무시한다.
	Req.Message_type.ChatWhisper->TargetIdx.Value = static_cast<uint64>(TargetCharacterIdx);
	Req.Message_type.ChatWhisper->Text = Text;

	Client->Chat(StreamHandle, Req);
}

void UEmberChatManager::SendWhisperByName(const FString& TargetName, const FString& Text)
{
	if (!IsValid(Client) || !IsReady() || TargetName.IsEmpty())
	{
		if (UNUChatSubsystem* Chat = GetChat())
		{
			Chat->AddSystemMessage(NSLOCTEXT("NUChat", "ChatNotReady", "채팅 서버에 연결되어 있지 않습니다."));
		}
		return;
	}

	FGrpcRevChatChatMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevChatChatMessageMessage_type::ChatWhisper;
	Req.Message_type.ChatWhisper = MakeShared<FGrpcRevChatChatWhisper>();
	Req.Message_type.ChatWhisper->TargetName = TargetName;
	Req.Message_type.ChatWhisper->Text = Text;

	Client->Chat(StreamHandle, Req);
}

void UEmberChatManager::SendJoinChannel(const FString& ChannelId)
{
	if (!IsValid(Client) || !IsReady() || ChannelId.IsEmpty())
	{
		return;
	}

	FGrpcRevChatChatMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevChatChatMessageMessage_type::JoinChannel;
	Req.Message_type.JoinChannel = MakeShared<FGrpcRevChatJoinChannel>();
	Req.Message_type.JoinChannel->ChannelId = ChannelId;

	Client->Chat(StreamHandle, Req);
}

void UEmberChatManager::SendLeaveChannel(const FString& ChannelId)
{
	if (!IsValid(Client) || !IsReady() || ChannelId.IsEmpty())
	{
		return;
	}

	FGrpcRevChatChatMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevChatChatMessageMessage_type::LeaveChannel;
	Req.Message_type.LeaveChannel = MakeShared<FGrpcRevChatLeaveChannel>();
	Req.Message_type.LeaveChannel->ChannelId = ChannelId;

	Client->Chat(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 수신
/////////////////////////////////////////////////////////////////

void UEmberChatManager::HandleChatResponse(FGrpcContextHandle Handle, const FGrpcResult& Result, const FGrpcRevChatChatMessage& Response)
{
	if (Result.Code != EGrpcResultCode::Ok)
	{
		UE_LOG(LogEmberChat, Warning, TEXT("채팅 스트림 오류: %d %s"), (int32)Result.Code, *Result.Message);
		return;
	}

	const int32 ResultCode = Response._result_code.ResultCode;

	switch (Response.Message_type.Message_typeCase)
	{
	case EGrpcRevChatChatMessageMessage_type::ChatLogin:
		Handle_ChatLogin(ResultCode, Response);
		break;

	// 발화 응답은 result_code 만 의미가 있다 — 본문은 SVR_Chat_Message 로 따로 온다
	// (근접 채팅과 반대다. 거긴 응답에 본문이 실려 온다)
	case EGrpcRevChatChatMessageMessage_type::ChatSend:
	case EGrpcRevChatChatMessageMessage_type::ChatWhisper:
		if (ResultCode != EmberChatCfg::RC_SUCCESS)
		{
			if (UNUChatSubsystem* Chat = GetChat())
			{
				Chat->AddSystemMessage(NUChat::GetResultMessage(ResultCode));
			}
		}
		break;

	// 채널 입장은 파티 상태를 보고 자동으로 나간다. 
	// 실패하면 클라의 파티 상태가 서버와 어긋난 것이라 조용히 넘기면 안 된다 (party: 는 1411 로 막힌다)
	case EGrpcRevChatChatMessageMessage_type::JoinChannel:
	case EGrpcRevChatChatMessageMessage_type::LeaveChannel:
		if (ResultCode != EmberChatCfg::RC_SUCCESS)
		{
			UE_LOG(LogEmberChat, Warning, TEXT("채널 입/퇴장 실패 (%d) %s"),
				ResultCode, *Response._result_message.ResultMessage);

			if (UNUChatSubsystem* Chat = GetChat())
			{
				Chat->AddSystemMessage(NUChat::GetResultMessage(ResultCode));
			}
		}
		break;

	case EGrpcRevChatChatMessageMessage_type::Ping:
		break;

	// ── 서버 푸시 ──
	case EGrpcRevChatChatMessageMessage_type::SvrChatMessage:
		Handle_SVR_Chat_Message(Response);
		break;

	case EGrpcRevChatChatMessageMessage_type::SvrChatWhisper:
		Handle_SVR_Chat_Whisper(Response);
		break;

	case EGrpcRevChatChatMessageMessage_type::SvrChatKick:
		Handle_SVR_Chat_Kick(Response);
		break;

	default:
		break;
	}
}

void UEmberChatManager::Handle_ChatLogin(int32 ResultCode, const FGrpcRevChatChatMessage& Response)
{
	if (ResultCode != EmberChatCfg::RC_SUCCESS)
	{
		UE_LOG(LogEmberChat, Warning, TEXT("ChatLogin 실패 (%d) %s"), ResultCode, *Response._result_message.ResultMessage);
		SetState(EEmberChatState::Failed);
		if (UNUChatSubsystem* Chat = GetChat())
		{
			Chat->AddSystemMessage(NSLOCTEXT("NUChat", "ChatLoginFailed", "채팅 서버 로그인에 실패했습니다."));
		}
		return;
	}

	if (Response.Message_type.ChatLogin.IsValid())
	{
		// 이름은 **서버가 정본**이다 (게임 서버가 Redis 에 써둔 값). 클라가 만들지 않는다.
		MyPlayerId = static_cast<int64>(Response.Message_type.ChatLogin->PlayerId.Value);
		MyPlayerName = Response.Message_type.ChatLogin->PlayerName;
	}

	SetState(EEmberChatState::Ready);
	UE_LOG(LogEmberChat, Log, TEXT("채팅 로그인 완료 [%s] (player %lld)"), *MyPlayerName, MyPlayerId);

	// 핑은 30초 주기. 게임 핑과 별개이며, 채팅 서버가 이걸로 게임 세션 생존도 재확인한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().SetTimer(PingTimer, this, &UEmberChatManager::SendPing,
			EmberChatCfg::PING_INTERVAL_SEC, true);
	}
}

void UEmberChatManager::Handle_SVR_Chat_Message(const FGrpcRevChatChatMessage& Response)
{
	if (!Response.Message_type.SvrChatMessage.IsValid())
	{
		return;
	}

	const FGrpcRevChatSVR_Chat_Message& D = *Response.Message_type.SvrChatMessage;
	if (UNUChatSubsystem* Chat = GetChat())
	{
		// **내 말도 돌아온다** (근접 채팅과 반대). 그래서 보낼 때 그리지 않고 여기서만 그린다.
		const bool bIsMine = MyPlayerId != 0 && static_cast<int64>(D.PlayerId.Value) == MyPlayerId;
		Chat->HandleChannelMessage(D.ChannelId, D.PlayerName, D.Text, bIsMine);
	}
}

void UEmberChatManager::Handle_SVR_Chat_Whisper(const FGrpcRevChatChatMessage& Response)
{
	if (!Response.Message_type.SvrChatWhisper.IsValid())
	{
		return;
	}

	const FGrpcRevChatSVR_Chat_Whisper& D = *Response.Message_type.SvrChatWhisper;
	if (UNUChatSubsystem* Chat = GetChat())
	{
		// player_name / character_idx 는 **항상 상대방**이다 (내가 보낸 echo 여도).
		// is_outgoing 으로 방향만 구분한다.
		Chat->HandleWhisper(
			static_cast<int64>(D.CharacterIdx.Value),
			D.PlayerName,
			D.Text,
			D.IsOutgoing);
	}
}

void UEmberChatManager::Handle_SVR_Chat_Kick(const FGrpcRevChatChatMessage& Response)
{
	const int32 Reason = Response.Message_type.SvrChatKick.IsValid()
		? Response.Message_type.SvrChatKick->Reason : 0;

	UE_LOG(LogEmberChat, Warning, TEXT("채팅 세션 강제 종료 (%d)"), Reason);

	if (UNUChatSubsystem* Chat = GetChat())
	{
		Chat->AddSystemMessage(FText::Format(
			NSLOCTEXT("NUChat", "ChatKicked", "채팅 세션이 종료되었습니다. ({0})"),
			FText::AsNumber(Reason)));
	}

	// 강퇴 직후 스트림이 닫힌다. 같은 스트림으로 재시도해봐야 소용없다.
	Disconnect();
}
