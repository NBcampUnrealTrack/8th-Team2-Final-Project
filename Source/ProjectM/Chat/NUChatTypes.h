// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUChatTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class ENUChatChannel : uint8
{
	Local,		// 지역 채팅, 보이는 사람들끼리 - game.proto FieldChat
	World,		// 월드 채팅 - chat.proto "world"
	Party,		// 파티 채팅 - chat.proto "party:{id}"
	Whisper,	// 귓속말 - chat.proto ChatWhisper
	System		// 서버 알림, 공지, 에러
};

// 채팅 라인 구조체
USTRUCT()
struct FNUChatLine
{
	GENERATED_BODY()
	
	// 어느 채널인지
	ENUChatChannel Channel = ENUChatChannel::System;
	
	// 보낸 사람
	FText SenderName;
	
	// 내용
	FText Text;
	
	// 내가 보낸것인지, 근접은 응답/푸시로 구분
	// 귓속말에서도 사용
	bool bIsMine = false;
};

// 채팅 탭, 비트마스킹을 이용해서 나중에 커스텀 탭 처리할 수 있게 함
USTRUCT(BlueprintType)
struct FNUChatTab
{
	GENERATED_BODY()
	
	// 탭 이름
	UPROPERTY()
	FText Label;
	
	// 보여줄 채널 비트마스크
	UPROPERTY()
	int32 ChannelMask = 0;
};

namespace NUChat
{
	constexpr int32 ChannelBit(ENUChatChannel Channel)
	{
		return 1 << static_cast<int32>(Channel);
	}
	
	// 모든 채널 허용, 일반탭이 사용함
	// 채널 추가시 여기도 추가해야 함
	constexpr int32 AllChannels =
		  ChannelBit(ENUChatChannel::Local)
		| ChannelBit(ENUChatChannel::World)
		| ChannelBit(ENUChatChannel::Party)
		| ChannelBit(ENUChatChannel::Whisper)
		| ChannelBit(ENUChatChannel::System);
	
	PROJECTM_API TArray<FNUChatTab> MakeDefaultTabs();

	// 전송할 수 있는 채널, System은 서버가 주는 것이라 빠짐
	// Tab 키 순환 순서로도 사용함
	PROJECTM_API const TArray<ENUChatChannel>& GetSendableChannels();

	
	// 로그 줄 / 전송 채널 버튼 / 채널 선택 목록이 같이 사용함
	
	PROJECTM_API FText GetChannelLabel(ENUChatChannel Channel);
	PROJECTM_API FLinearColor GetChannelColor(ENUChatChannel Channel);
	PROJECTM_API UTexture2D* GetGeneralIcon();
	PROJECTM_API UTexture2D* GetChannelIcon(ENUChatChannel Channel);

	// 서버 성공 ResultCode
	constexpr int32 RC_SUCCESS = 200;

	// 서버 ResultCode를 사용자에게 보일 문구로 변환
	PROJECTM_API FText GetResultMessage(int32 ResultCode);

	// ── 채팅 명령어 ──
	// 귓속말 대상 지정, 캐릭이름엔 공백이 없어서 2개 잘라내기 하면 됨

	// "/w 이름 본문" 을 갈라냄, 명령어가 아니면 false, 본문이 없으면 true지만 OutText가 빔
	PROJECTM_API bool ParseWhisperCommand(const FString& Line, FString& OutName, FString& OutText);

	// "/r " 처럼 답장 명령어를 치는 중인지. 입력창에서 "/w {이름} " 으로 갈아끼울 때 사용
	PROJECTM_API bool IsReplyCommandPrefix(const FString& Line);

	// 위 치환으로 만들 문자열
	PROJECTM_API FString MakeWhisperCommand(const FString& TargetName);
}

