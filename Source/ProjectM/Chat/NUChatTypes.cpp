// Fill out your copyright notice in the Description page of Project Settings.


#include "NUChatTypes.h"

#include "Engine/Texture2D.h"


TArray<FNUChatTab> NUChat::MakeDefaultTabs()
{
	TArray<FNUChatTab> Tabs;
	
	Tabs.Add({ NSLOCTEXT("NUChat", "TabAll", "일반"), AllChannels});
	Tabs.Add({ NSLOCTEXT("NUChat", "TabLocal",  "지역"),   ChannelBit(ENUChatChannel::Local) });
	Tabs.Add({ NSLOCTEXT("NUChat", "TabWorld",  "월드"),   ChannelBit(ENUChatChannel::World) });
	Tabs.Add({ NSLOCTEXT("NUChat", "TabParty",  "파티"),   ChannelBit(ENUChatChannel::Party) });
	Tabs.Add({ NSLOCTEXT("NUChat", "TabWhisper",  "귓속말"),   ChannelBit(ENUChatChannel::Whisper) });
	Tabs.Add({ NSLOCTEXT("NUChat", "TabSystem", "시스템"), ChannelBit(ENUChatChannel::System) });

	return Tabs;
}

// 채널 이름을 받아오는 함수
FText NUChat::GetChannelLabel(ENUChatChannel Channel)
{
	switch (Channel)
	{
	case ENUChatChannel::Local:		return NSLOCTEXT("NUChat", "ChannelLocal",   "지역");
	case ENUChatChannel::World:		return NSLOCTEXT("NUChat", "ChannelWorld",   "월드");
	case ENUChatChannel::Party:		return NSLOCTEXT("NUChat", "ChannelParty",   "파티");
	case ENUChatChannel::Whisper:	return NSLOCTEXT("NUChat", "ChannelWhisper", "귓속말");
	case ENUChatChannel::System:	return NSLOCTEXT("NUChat", "ChannelSystem",  "시스템");
	default:						return NSLOCTEXT("NUChat", "ChannelUnknown", "정의되지않은채널");
	}
}

// 채널별 색상을 받아오는 함수
FLinearColor NUChat::GetChannelColor(ENUChatChannel Channel)
{
	switch (Channel)
	{
	case ENUChatChannel::Local:		return FLinearColor::White;
	case ENUChatChannel::World:		return FLinearColor(0.f, 0.8f, 0.f, 1.f);
	case ENUChatChannel::Party:		return FLinearColor(0.45f, 0.9f, 1.0f, 0.9f);
	case ENUChatChannel::Whisper:	return FLinearColor(0.8f, 0.f, 0.7f, 1.f);
	case ENUChatChannel::System:	return FLinearColor(1.f, 1.f, 0.f, 0.7f);
	default:						return FLinearColor::White;
	}
}

UTexture2D* NUChat::GetGeneralIcon()
{
	return LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Developers/MinkiCho/Art/FieldHUD/Chat/T_Chat_Icon_General.T_Chat_Icon_General"));
}

UTexture2D* NUChat::GetChannelIcon(ENUChatChannel Channel)
{
	const TCHAR* IconPath = nullptr;
	switch (Channel)
	{
	case ENUChatChannel::Local:
		IconPath = TEXT("/Game/Developers/MinkiCho/Art/FieldHUD/Chat/T_Chat_Icon_Local.T_Chat_Icon_Local");
		break;
	case ENUChatChannel::World:
		IconPath = TEXT("/Game/Developers/MinkiCho/Art/FieldHUD/Chat/T_Chat_Icon_World.T_Chat_Icon_World");
		break;
	case ENUChatChannel::Party:
		IconPath = TEXT("/Game/Developers/MinkiCho/Art/FieldHUD/Chat/T_Chat_Icon_Party.T_Chat_Icon_Party");
		break;
	case ENUChatChannel::Whisper:
		IconPath = TEXT("/Game/Developers/MinkiCho/Art/FieldHUD/Chat/T_Chat_Icon_Whisper.T_Chat_Icon_Whisper");
		break;
	case ENUChatChannel::System:
		IconPath = TEXT("/Game/Developers/MinkiCho/Art/FieldHUD/Chat/T_Chat_Icon_System.T_Chat_Icon_System");
		break;
	default:
		return nullptr;
	}

	return LoadObject<UTexture2D>(nullptr, IconPath);
}

const TArray<ENUChatChannel>& NUChat::GetSendableChannels()
{
	static const TArray<ENUChatChannel> Channels = {
		ENUChatChannel::Local,
		ENUChatChannel::World,
		ENUChatChannel::Party,
		ENUChatChannel::Whisper
	};
	return Channels;
}

FText NUChat::GetResultMessage(int32 ResultCode)
{
	// gRPC 문서의 ResultCode 표에서 채팅이 실제로 받는 값들
	switch (ResultCode)
	{
	case 1401: return NSLOCTEXT("NUChat", "RC1401", "보낼 내용이 없습니다.");
	case 1402: return NSLOCTEXT("NUChat", "RC1402", "메시지가 너무 깁니다. (최대 100자까지)");
	case 1404: return NSLOCTEXT("NUChat", "RC1404", "필드에 있어야 지역 채팅을 할 수 있습니다.");
	case 1405: return NSLOCTEXT("NUChat", "RC1405", "채팅 서버에 아직 로그인되지 않았습니다.");
	case 1406: return NSLOCTEXT("NUChat", "RC1406", "채널 이름이 올바르지 않습니다.");
	case 1407: return NSLOCTEXT("NUChat", "RC1407", "들어가 있지 않은 채널입니다.");
	case 1408: return NSLOCTEXT("NUChat", "RC1408", "들어갈 수 있는 채널 수를 넘었습니다.");
	case 1409: return NSLOCTEXT("NUChat", "RC1409", "대상이 접속해 있지 않습니다.");
	case 1410: return NSLOCTEXT("NUChat", "RC1410", "자기 자신에게는 귓속말할 수 없습니다.");
	case 1411: return NSLOCTEXT("NUChat", "RC1411", "내 파티가 아닙니다.");

	default:
		return FText::Format(
			NSLOCTEXT("NUChat", "RCUnknown", "채팅이 거부되었습니다. (코드 {0})"),
			FText::AsNumber(ResultCode));
	}
}

namespace
{
	// 귓속말 명령어, 한글 별칭도 같이 처리
	const TCHAR* WhisperPrefixes[] = { TEXT("/w "), TEXT("/ㅈ ") };
	const TCHAR* ReplyPrefixes[]   = { TEXT("/r "), TEXT("/ㄱ ") };
}

bool NUChat::ParseWhisperCommand(const FString& Line, FString& OutName, FString& OutText)
{
	FString Rest;
	for (const TCHAR* Prefix : WhisperPrefixes)
	{
		// 대소문자 구분 없이 시작 문자 확인
		if (Line.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			// Substr
			Rest = Line.Mid(FCString::Strlen(Prefix));
			break;
		}
	}

	// 명령어가 아니거나 "/w " 뒤가 비어 있는 경우
	if (Rest.IsEmpty())
	{
		return false;
	}

	Rest.TrimStartInline();

	// 캐릭터 이름에는 공백이 없음
	int32 SpaceIndex = INDEX_NONE;
	if (!Rest.FindChar(TEXT(' '), SpaceIndex))
	{
		// 공백이 없다 -> 이름만 입력한 경우로 판단함
		// 이름만 치고 본문이 없는 경우. 명령어로는 인식하되 본문은 비워서 돌려줌
		OutName = Rest;
		OutText.Empty();
		return true;
	}

	OutName = Rest.Left(SpaceIndex);
	OutText = Rest.Mid(SpaceIndex + 1).TrimStartAndEnd();
	return true;
}

bool NUChat::IsReplyCommandPrefix(const FString& Line)
{
	for (const TCHAR* Prefix : ReplyPrefixes)
	{
		if (Line.Equals(Prefix, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

FString NUChat::MakeWhisperCommand(const FString& TargetName)
{
	return FString::Printf(TEXT("%s%s "), WhisperPrefixes[0], *TargetName);
}
