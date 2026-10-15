// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SRevChat/ChatService.h"
#include "SRevChat/ChatClient.h"
#include "EmberChatManager.generated.h"

class UEmberServerManager;
class UNUChatSubsystem;

/**
 * 채팅 스트림 연결 상태. 게임 스트림(EEmberConnState)과 별개다.
 */
UENUM(BlueprintType)
enum class EEmberChatState : uint8
{
	Idle,        // 아직 안 붙음
	Connecting,  // 서비스 연결 중
	LoggingIn,   // 스트림은 열렸고 ChatLogin 응답 대기
	Ready,       // ChatLogin 성공 — 발화 가능
	Failed,      // 실패 (재시도는 CharacterSelect 때 다시 일어난다)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmberChatStateChanged, EEmberChatState, NewState);

/**
 * 채팅 서버(chat.proto) 전송 담당. 게임 스트림을 담당하는 UEmberServerManager 의 채팅판이다.
 *
 * 게임 스트림과 **완전히 별개인 두 번째 스트림**이다. 클라는 스트림을 2개 연다:
 *   게임 = /rev.game.GameService/  ·  채팅 = /rev.chat.ChatService/
 * 원격(EC2)은 nginx 가 gRPC 경로로 갈라주므로 **주소·포트가 게임과 같다**.
 * 로컬 개발에는 nginx 가 없어 채팅 서버(7100)로 직접 붙는다 — GetChatEndPoint() 참고.
 *
 * 채팅 서버는 인증을 직접 하지 않는다. 게임 서버가 Redis 에 써둔 세션 토큰과 신원을 읽어
 * 검증하므로, **게임 로그인 + 캐릭터 선택이 끝난 뒤에** Connect() 해야 한다
 * (전에 부르면 CHAT_NOT_LOGGED_IN(1405) 이 온다).
 */
UCLASS()
class PROJECTM_API UEmberChatManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 채팅 서버 접속 시작. 게임 접속(UEmberServerManager::Connect)과 **같은 시점에** 부른다.
	 *
	 * 스트림은 여기서 바로 열지만 ChatLogin 은 **캐릭터 선택이 끝난 뒤에** 보낸다.
	 * 채팅 서버가 게임 서버의 Redis 신원(CHAT:{userId})을 읽는데, 캐릭터 선택 전에는
	 * 이름이 비어 있어 1405 로 거부되기 때문이다. 스트림을 미리 열어두면 선택 직후
	 * 왕복 없이 바로 발화할 수 있다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ember|Chat")
	void Connect();

	/**
	 * 캐릭터 선택이 끝났음을 알린다 — 여기서 ChatLogin 을 보낸다.
	 * 캐릭터를 **바꿔서** 다시 선택한 경우에도 불러야 한다(신원 갱신).
	 */
	void NotifyCharacterSelected();

	UFUNCTION(BlueprintCallable, Category = "Ember|Chat")
	void Disconnect();

	UFUNCTION(BlueprintPure, Category = "Ember|Chat")
	bool IsReady() const { return State == EEmberChatState::Ready; }

	/** 채팅 서버 엔드포인트. 원격은 게임과 같고(nginx 분기), 로컬만 7100 직결. */
	UFUNCTION(BlueprintPure, Category = "Ember|Chat")
	FString GetChatEndPoint() const;

	// ── 송신 ─────────────────────────────────────────────────────────────────

	/** 채널 발화. channel_id 는 "world" / "guild:{id}" / "party:{id}". */
	void SendChat(const FString& ChannelId, const FString& Text);

	/** 귓속말 — 캐릭터 idx 로. 이름과 달리 안 바뀌므로 이쪽이 정확하다. */
	void SendWhisperByIdx(int64 TargetCharacterIdx, const FString& Text);

	/** 귓속말 — 캐릭터 이름으로. 채팅창에 직접 이름을 친 경우에만 쓴다. */
	void SendWhisperByName(const FString& TargetName, const FString& Text);

	void SendJoinChannel(const FString& ChannelId);
	void SendLeaveChannel(const FString& ChannelId);

	UPROPERTY(BlueprintReadOnly, Category = "Ember|Chat")
	EEmberChatState State = EEmberChatState::Idle;

	UPROPERTY(BlueprintAssignable, Category = "Ember|Chat")
	FOnEmberChatStateChanged OnChatStateChanged;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// ── gRPC(TurboLink) 리소스 ──
	UPROPERTY()
	TObjectPtr<UChatService> Service = nullptr;

	UPROPERTY()
	TObjectPtr<UChatServiceClient> Client = nullptr;

	FGrpcContextHandle StreamHandle;

	bool bStreamStarted = false;
	FTimerHandle PingTimer;

	/**
	 * 캐릭터 선택이 끝났는가. 스트림이 열리는 시점과 캐릭터를 고르는 시점은 순서가 정해져
	 * 있지 않아서(먼저 고르고 스트림이 늦게 열릴 수도 있다) 양쪽에서 이 값을 보고
	 * 둘 다 준비됐을 때 ChatLogin 을 보낸다.
	 */
	bool bCharacterReady = false;

	/** ChatLogin 이 돌려준 내 표시 이름. 서버가 정본이라 클라가 만들지 않는다. */
	FString MyPlayerName;
	int64 MyPlayerId = 0;

	void SetState(EEmberChatState NewState);
	int64 NowUnixSeconds() const;
	void ApplyChatEndPoint();

	UEmberServerManager* GetServer() const;
	UNUChatSubsystem* GetChat() const;

	// ── gRPC 콜백 ──
	UFUNCTION()
	void HandleServiceStateChanged(EGrpcServiceState NewServiceState);

	UFUNCTION()
	void HandleChatResponse(FGrpcContextHandle Handle, const FGrpcResult& Result, const FGrpcRevChatChatMessage& Response);

	// ── 송신 헬퍼 ──
	FGrpcRevChatChatMessage MakeEnvelope() const;
	void SendChatLogin();
	void SendPing();

	// ── 응답/푸시 처리 ──
	void Handle_ChatLogin(int32 ResultCode, const FGrpcRevChatChatMessage& Response);
	void Handle_SVR_Chat_Message(const FGrpcRevChatChatMessage& Response);
	void Handle_SVR_Chat_Whisper(const FGrpcRevChatChatMessage& Response);
	void Handle_SVR_Chat_Kick(const FGrpcRevChatChatMessage& Response);
};
