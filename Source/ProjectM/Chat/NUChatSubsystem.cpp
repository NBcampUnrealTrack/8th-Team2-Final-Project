// Fill out your copyright notice in the Description page of Project Settings.


#include "Chat/NUChatSubsystem.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Field/Net/FieldRemoteCharacter.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Net/EmberChatManager.h"
#include "Net/EmberServerManager.h"
#include "Party/NUPartySubsystem.h"
#include "Party/NUPartyTypes.h"

namespace
{
	// proto에서 정한 규약, FieldChat.text 는 1~100자
	// 위반하면 서버가 거부하므로 여기서 미리 걸러내기
	constexpr int32 CHAT_TEXT_MAX_LEN = 100;

	// 전체 채팅 채널 id. ChatLogin 시 서버가 자동 입장시켜 주므로 Join 을 따로 보내지 않는다.
	const FString CHAT_CHANNEL_WORLD = TEXT("world");
}

void UNUChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 서브시스템은 생성 순서가 정해져 있지 않음
	// 이 서브시스템이 먼저 생길경우 아래 두 서브시스템을 먼저 생성하게 함
	// 아래에서 바인딩을 해야하므로 먼저 생성하는것이 필수
	Collection.InitializeDependency<UNUPartySubsystem>();
	Collection.InitializeDependency<UEmberChatManager>();

	Super::Initialize(Collection);

	if (UNUPartySubsystem* PartySubsystem = GetGameInstance()->GetSubsystem<UNUPartySubsystem>())
	{
		PartySubsystem->OnPartyChanged.AddUObject(this, &ThisClass::HandlePartyChanged);
	}

	if (UEmberChatManager* ChatServer = GetChatServer())
	{
		ChatServer->OnChatStateChanged.AddDynamic(this, &ThisClass::HandleChatStateChanged);
	}
}

void UNUChatSubsystem::Deinitialize()
{
	if (UNUPartySubsystem* PartySubsystem = GetGameInstance()->GetSubsystem<UNUPartySubsystem>())
	{
		PartySubsystem->OnPartyChanged.RemoveAll(this);
	}

	if (UEmberChatManager* ChatServer = GetChatServer())
	{
		ChatServer->OnChatStateChanged.RemoveAll(this);
	}

	Super::Deinitialize();
}

void UNUChatSubsystem::HandlePartyChanged(const FNUPartySnapshot& Snapshot,
	ENUPartyUpdateReason Reason, int64 TargetIdx)
{
	RefreshPartyChannel();
}

void UNUChatSubsystem::HandleChatStateChanged(EEmberChatState NewState)
{
	if (NewState != EEmberChatState::Ready)
	{
		// 스트림이 끊기면 서버 쪽 멤버십도 사라짐, 다시 Ready 가 되면 새로 들어가야 함
		JoinedPartyChannel.Empty();
		return;
	}

	// 파티가 먼저 생기고 채팅 로그인이 나중에 끝날 수 있음
	RefreshPartyChannel();
}

void UNUChatSubsystem::RefreshPartyChannel()
{
	UEmberChatManager* ChatServer = GetChatServer();
	if (!IsValid(ChatServer) || !ChatServer->IsReady())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UNUPartySubsystem* PartySubsystem = GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
	int64 PartyId = IsValid(PartySubsystem) ? PartySubsystem->GetParty().PartyId : 0;

	FString Desired = PartyId != 0
		? FString::Printf(TEXT("party:%lld"), PartyId)
		: FString();

	if (JoinedPartyChannel == Desired)
	{
		return;
	}

	// 파티에서 빠져도 채널에서 자동으로 나가지지 않음
	// 수동으로 나갔다고 알려야 함
	if (!JoinedPartyChannel.IsEmpty())
	{
		ChatServer->SendLeaveChannel(JoinedPartyChannel);
	}

	if (!Desired.IsEmpty())
	{
		ChatServer->SendJoinChannel(Desired);
	}

	JoinedPartyChannel = Desired;
}

void UNUChatSubsystem::SubmitChat(ENUChatChannel Channel, const FString& Text)
{
	FString WhisperName;
	FString WhisperText;
	if (NUChat::ParseWhisperCommand(Text, WhisperName, WhisperText))
	{
		if (WhisperText.IsEmpty())
		{
			AddSystemMessage(NSLOCTEXT("NUChat", "WhisperNoBody", "보낼 내용을 함께 입력하세요. (/w 이름 내용)"));
			return;
		}

		SendWhisper(0, WhisperName, WhisperText);
		return;
	}

	// 보내기만 하고 그리는건 따로
	// 내 말은 응답(지역) 또는 푸시(지역 외 나머지)로 돌아옴
	switch (Channel)
	{
	case ENUChatChannel::Local:
		// 근접은 내가 보낸 말이 FieldChat "응답"에 실려 옴, 푸시 X
		SendFieldChat(Text);
		break;

	case ENUChatChannel::World:
		SendWorldChat(Text);
		break;

	case ENUChatChannel::Party:
		SendPartyChat(Text);
		break;

	case ENUChatChannel::Whisper:
		SendWhisperToCurrentTarget(Text);
		break;

	default:
		break;
	}
}

void UNUChatSubsystem::BeginWhisperTo(int64 TargetCharacterIdx, const FString& TargetName)
{
	if (TargetCharacterIdx == 0 && TargetName.IsEmpty())
	{
		return;
	}

	// 대상만 잡고 채팅창을 귓속말로 열어줌
	WhisperTargetIdx = TargetCharacterIdx;
	WhisperTargetName = TargetName;

	OnWhisperTargetChanged.Broadcast(WhisperTargetIdx, WhisperTargetName);
}

bool UNUChatSubsystem::PrepareText(const FString& Text, FString& OutText)
{
	OutText = Text.TrimStartAndEnd();
	if (OutText.IsEmpty()) return false;

	// 길이가 너무 길어 서버가 거부할 걸 미리 막음, 채널·귓속말도 같은 1~100자 규약
	if (OutText.Len() > CHAT_TEXT_MAX_LEN)
	{
		AddSystemMessage(FText::Format(
			NSLOCTEXT("NUChat", "TooLong", "메시지가 너무 깁니다. ({0}/{1}자)"),
			FText::AsNumber(OutText.Len()), FText::AsNumber(CHAT_TEXT_MAX_LEN)));
		return false;
	}
	return true;
}

void UNUChatSubsystem::SendFieldChat(const FString& Text)
{
	FString Trimmed;
	if (!PrepareText(Text, Trimmed)) return;

	UEmberServerManager* Server = GetServer();
	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		AddSystemMessage(NSLOCTEXT("NUChat", "NotConnected", "서버에 연결되어 있지 않습니다."));
		return;
	}

	Server->SendFieldChat(Trimmed);
}

void UNUChatSubsystem::HandleFieldChatEcho(int32 ResultCode, const FString& Text)
{
	// 응답 코드를 반드시 봐야함
	// 거부되면 내 말이 남들에게 가지 않았는데 화면에 남을 경우 보낸거로 착각할 수 있음
	if (ResultCode != NUChat::RC_SUCCESS)
	{
		AddSystemMessage(FText::Format(
			NSLOCTEXT("NUChat", "SendRejected", "채팅이 거부되었습니다. (코드 {0})"),
			FText::AsNumber(ResultCode)));
		return;
	}

	if (Text.IsEmpty()) return;

	// 서버가 정제한 본문으로 그린다
	OnChatMessage.Broadcast(ENUChatChannel::Local, GetMyDisplayName(), FText::FromString(Text), true);
}

void UNUChatSubsystem::HandleFieldChatReceived(int64 PlayerId, const FString& Text)
{
	if (Text.IsEmpty()) return;

	// 게임 인스턴스 서브시스템이라 월드는 게임 인스턴스에서 찾음
	UNUSFXSubsystem::Play(GetGameInstance(), NU::SFX::Social::Chat);
	OnChatMessage.Broadcast(ENUChatChannel::Local, GetRemoteDisplayName(PlayerId), FText::FromString(Text), false);
}

/////////////////////////////////////////////////////////////////
// 채팅 서버 경로 (전체 · 파티 · 귓속말)
/////////////////////////////////////////////////////////////////

void UNUChatSubsystem::SendWorldChat(const FString& Text)
{
	FString Trimmed;
	if (!PrepareText(Text, Trimmed)) return;

	if (UEmberChatManager* ChatServer = GetChatServer())
	{
		// 발화한 것을 여기서 그리지 않는다 — 채팅 서버는 내 말도 SVR_Chat_Message 로 돌려준다.
		ChatServer->SendChat(CHAT_CHANNEL_WORLD, Trimmed);
	}
}

void UNUChatSubsystem::SendPartyChat(const FString& Text)
{
	FString Trimmed;
	if (!PrepareText(Text, Trimmed)) return;

	UGameInstance* GI = GetGameInstance();
	UNUPartySubsystem* PartySubsystem = GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
	if (!IsValid(PartySubsystem) || !PartySubsystem->GetParty().IsExistParty())
	{
		AddSystemMessage(NSLOCTEXT("NUChat", "NoParty", "파티에 속해 있지 않습니다."));
		return;
	}

	// 채널 id 규약: "party:{partyId}". 채팅 서버가 Redis 의 PARTYOF 로 소속을 검사한다.
	const FString ChannelId = FString::Printf(TEXT("party:%lld"), PartySubsystem->GetParty().PartyId);

	if (UEmberChatManager* ChatServer = GetChatServer())
	{
		ChatServer->SendChat(ChannelId, Trimmed);
	}
}

void UNUChatSubsystem::SendWhisper(int64 TargetCharacterIdx, const FString& TargetName, const FString& Text)
{
	FString Trimmed;
	if (!PrepareText(Text, Trimmed)) return;

	if (TargetCharacterIdx == 0 && TargetName.IsEmpty())
	{
		AddSystemMessage(NSLOCTEXT("NUChat", "NoWhisperTarget", "귓속말 대상을 지정하세요."));
		return;
	}

	UEmberChatManager* ChatServer = GetChatServer();
	if (!IsValid(ChatServer)) return;

	// "/w 이름" 으로 들어와 idx 가 비었어도, 지금 대상과 같은 사람이면 알고 있는 idx 를 사용함
	// 컨텍스트 메뉴가 준 idx 가 명령어 텍스트를 거치면서 사라지는 것을 막음
	if (TargetCharacterIdx == 0 && WhisperTargetIdx != 0
		&& !TargetName.IsEmpty() && TargetName.Equals(WhisperTargetName))
	{
		TargetCharacterIdx = WhisperTargetIdx;
	}

	// 보내는 대상 지정
	WhisperTargetIdx = TargetCharacterIdx;
	if (!TargetName.IsEmpty())
	{
		WhisperTargetName = TargetName;
	}

	if (TargetCharacterIdx != 0)
	{
		ChatServer->SendWhisperByIdx(TargetCharacterIdx, Trimmed);
	}
	else
	{
		ChatServer->SendWhisperByName(TargetName, Trimmed);
	}
}

void UNUChatSubsystem::SendWhisperToCurrentTarget(const FString& Text)
{
	if (WhisperTargetIdx == 0 && WhisperTargetName.IsEmpty())
	{
		AddSystemMessage(NSLOCTEXT("NUChat", "NoWhisperTarget", "귓속말 대상을 지정하세요."));
		return;
	}
	SendWhisper(WhisperTargetIdx, WhisperTargetName, Text);
}

void UNUChatSubsystem::HandleChannelMessage(const FString& ChannelId, const FString& PlayerName, const FString& Text, bool bIsMine)
{
	if (Text.IsEmpty()) return;

	// channel_id 로 어느 탭에 그릴지 정한다. "party:" 로 시작하면 파티, 아니면 전체.
	// (길드 채널은 아직 탭이 없어 전체로 보낸다 — 길드 시스템이 생기면 여기에 분기를 더한다)
	const ENUChatChannel Channel = ChannelId.StartsWith(TEXT("party:"))
		? ENUChatChannel::Party
		: ENUChatChannel::World;

	if (!bIsMine)
	{
		UNUSFXSubsystem::Play(GetGameInstance(), NU::SFX::Social::Chat);
	}
	OnChatMessage.Broadcast(Channel, FText::FromString(PlayerName), FText::FromString(Text), bIsMine);
}

void UNUChatSubsystem::HandleWhisper(int64 CharacterIdx, const FString& PlayerName, const FString& Text, bool bOutgoing)
{
	if (Text.IsEmpty()) return;

	// 귓속말을 보낸거라면
	// 서버에서 내가 보낸것도 나한테 오므로 bOutGoing으로 구분
	if (bOutgoing)
	{
		// 이름으로 보내서 idx 가 비어 있으면 echo 가 주는 값으로 채움
		// 이름은 캐릭터를 바꾸면 달라지지만 idx 는 안 바뀜
		if (CharacterIdx != 0 && WhisperTargetIdx == 0 && WhisperTargetName.Equals(PlayerName))
		{
			WhisperTargetIdx = CharacterIdx;
		}
	}
	else if (!PlayerName.IsEmpty())
	{
		// 받은 것은 `/r` (답장) 대상만 갱신함
		LastIncomingWhisperName = PlayerName;
	}

	// 표시는 방향을 알 수 있게 한다 — 둘 다 상대 이름이라 그냥 그리면 구분이 안 된다.
	const FText Display = bOutgoing
		? FText::Format(NSLOCTEXT("NUChat", "WhisperTo", "{0}님에게"), FText::FromString(PlayerName))
		: FText::Format(NSLOCTEXT("NUChat", "WhisperFrom", "{0}님으로부터"), FText::FromString(PlayerName));

	if (!bOutgoing)
	{
		UNUSFXSubsystem::Play(GetGameInstance(), NU::SFX::Social::Chat);
	}
	OnChatMessage.Broadcast(ENUChatChannel::Whisper, Display, FText::FromString(Text), bOutgoing);
}

void UNUChatSubsystem::AddSystemMessage(const FText& Text)
{
	OnChatMessage.Broadcast(ENUChatChannel::System, FText::GetEmpty(), Text, false);
}

UEmberServerManager* UNUChatSubsystem::GetServer() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

UEmberChatManager* UNUChatSubsystem::GetChatServer() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEmberChatManager>() : nullptr;
}

FText UNUChatSubsystem::GetMyDisplayName() const
{
	UGameInstance* GI = GetGameInstance();
	UNUCharacterManager* Characters = GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	if (!IsValid(Characters)) return FText::GetEmpty();

	// 캐릭터를 고르지 않았다면 빈 Text가 옴
	FNUCharacterSummary Selected;
	if (!Characters->GetSelectedCharacter(Selected)) return FText::GetEmpty();

	return FText::FromString(Selected.Name);
}

FText UNUChatSubsystem::GetRemoteDisplayName(int64 PlayerId) const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	UFieldSyncSubsystem* Sync = World ? World->GetSubsystem<UFieldSyncSubsystem>() : nullptr;

	if (IsValid(Sync))
	{
		if (AFieldRemoteCharacter* Remote = Sync->FindRemotePlayer(PlayerId))
		{
			return Remote->GetDisplayName();
		}
	}

	// 말한 직후 시야를 벗어났을 수 있음
	// 이 경우엔 Remote는 없지만 PlayerId는 있으므로 이를 표시
	return FText::FromString(FString::Printf(TEXT("Player %lld"), PlayerId));
}
