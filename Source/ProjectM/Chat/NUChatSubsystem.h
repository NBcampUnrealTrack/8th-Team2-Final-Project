// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Chat/NUChatTypes.h"
#include "NUChatSubsystem.generated.h"

class UEmberServerManager;
class UEmberChatManager;
enum class EEmberChatState : uint8;
enum class ENUPartyUpdateReason : uint8;
struct FNUPartySnapshot;

// 채팅창이 바인딩함, 이름은 여기서 해석해서 전달
DECLARE_MULTICAST_DELEGATE_FourParams(FNUOnChatMessage, ENUChatChannel, const FText&, const FText&, bool);

// 귓속말 대상이 정해짐(컨텍스트 메뉴 선택), PC가 받아서 전송 채널을 귓속말로 바꾸고 채팅 입력창을 열음
DECLARE_MULTICAST_DELEGATE_TwoParams(FNUOnWhisperTargetChanged, int64, const FString&);

// 채팅 메시지를 모으는 서브시스템.
UCLASS()
class PROJECTM_API UNUChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 채팅창이 입력을 확정했을 때 서버로 제출함
	void SubmitChat(ENUChatChannel Channel, const FString& Text);

	// 근접(지역) 채팅 전송, 길이 규약(1~100자)을 클라에서 먼저 검사하고 전송함
	void SendFieldChat(const FString& Text);

	// 내가 보낸 FieldChat의 응답
	// 근접 채팅은 내가 보낸 말이 응답으로 돌아옴
	void HandleFieldChatEcho(int32 ResultCode, const FString& Text);

	// 남이 말한 것 (SVR_Field_Chat), 이름이 실려 오지 않아 여기서 해석해야 함
	void HandleFieldChatReceived(int64 PlayerId, const FString& Text);

	
	// ── 채팅 서버(chat.proto) 경로 ───────────────────────────────────────────
	// 근접(Local)만 게임 스트림이고 나머지는 전부 채팅 스트림이다.

	// 전체 채팅 ("world" 채널)
	void SendWorldChat(const FString& Text);

	// 파티 채팅 ("party:{id}"). 파티가 없으면 안내만 하고 보내지 않는다.
	void SendPartyChat(const FString& Text);

	// 귓속말. 대상 idx 가 0이 아니면 idx 로, 아니면 이름으로 보낸다.
	// idx 쪽이 정확하다 — 이름은 캐릭터를 바꾸면 달라지지만 idx 는 안 바뀐다.
	void SendWhisper(int64 TargetCharacterIdx, const FString& TargetName, const FString& Text);

	// 지금 정해진 귓속말 대상에게 전송, 대상이 없으면 안내만
	void SendWhisperToCurrentTarget(const FString& Text);

	// 채널 발화 도착 (SVR_Chat_Message). **내 말도 돌아온다** — bIsMine 으로 구분한다.
	void HandleChannelMessage(const FString& ChannelId, const FString& PlayerName, const FString& Text, bool bIsMine);

	// 귓속말 도착 (SVR_Chat_Whisper). 이름·idx 는 항상 상대방이고 bOutgoing 이 방향이다.
	void HandleWhisper(int64 CharacterIdx, const FString& PlayerName, const FString& Text, bool bOutgoing);

	// 연결 끊김·전송 실패 등 채널 없는 알림
	void AddSystemMessage(const FText& Text);

	// 지금 내가 귓속말을 보내는 대상. 없으면 idx 0 · 빈 이름
	int64 GetWhisperTargetIdx() const { return WhisperTargetIdx; }
	const FString& GetWhisperTargetName() const { return WhisperTargetName; }

	// `/r` 이 대상으로 삼는 이름, 마지막으로 나에게 귓속말을 보낸 대상
	const FString& GetLastIncomingWhisperName() const { return LastIncomingWhisperName; }

	// 컨텍스트 메뉴의 '귓속말' 선택 시 대상을 잡고 OnWhisperTargetChanged 호출
	void BeginWhisperTo(int64 TargetCharacterIdx, const FString& TargetName);

public:
	FNUOnChatMessage OnChatMessage;
	FNUOnWhisperTargetChanged OnWhisperTargetChanged;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UEmberServerManager* GetServer() const;
	UEmberChatManager* GetChatServer() const;

	// ── 파티 채널 따라가기 ──
	// World 는 ChatLogin 때 서버가 자동 입장시키지만 party:{id} 는 직접 들어가야 함
	// 안 들어간 파티 채널에서 입력하면 1407

	void HandlePartyChanged(const FNUPartySnapshot& Snapshot, ENUPartyUpdateReason Reason, int64 TargetIdx);

	UFUNCTION()   // OnChatStateChanged Dynamic Delegate
	void HandleChatStateChanged(EEmberChatState NewState);

	// 들어가 있는 채널을 현재 파티에 맞게 설정
	void RefreshPartyChannel();

	// 보내기 전 공통 검사 (빈 문자열·길이). 통과하면 다듬은 본문을 OutText 에 담고 true.
	bool PrepareText(const FString& Text, FString& OutText);

	// 내 캐릭터 이름, 미선택이면 빈 Text
	FText GetMyDisplayName() const;

	// 원격 플레이어 이름, 시야에서 사라졌으면 id로 대체
	FText GetRemoteDisplayName(int64 PlayerId) const;
	
private:
	// 지금 들어가 있는 파티 채널, 없으면 빈 문자열
	FString JoinedPartyChannel;
	
	// 내가 보내는 대상. 받는 것과 무관함
	// 바꾸는 것은 컨텍스트 메뉴의 귓속말 · `/w 이름` · `/r`, 이 셋뿐
	int64 WhisperTargetIdx = 0;
	FString WhisperTargetName;

	// `/r` 전용, 받은 것만 기억함
	FString LastIncomingWhisperName;
};
