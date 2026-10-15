// 서버 네트워킹 매니저 — 서버 주소/포트 관리 + gRPC(TurboLink) 연결 + 응답 핸들러 디스패치를 한 곳에서.
// 서버 주소/포트는 .ini가 아니라 EmberServerManager.cpp 상단 상수(EmberServerCfg)에서 관리한다.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SRevGame/GameService.h"
#include "SRevGame/GameClient.h"
#include "EmberServerManager.generated.h"

struct FNUServerBattleCommandRequest;
struct FNUServerBattleReactionRequest;
struct FNUServerNaiaMoveRequest;

UENUM(BlueprintType)
enum class EEmberConnState : uint8
{
	Idle,
	Connecting,     // 서비스 채널 연결 대기
	Authenticating, // AuthUser 전송/대기
	Creating,       // CreateUser 전송/대기
	LoggingIn,      // LoginUser 전송/대기
	LoggedIn,       // 로그인 완료 (핑 주기 시작)
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmberConnStateChanged, EEmberConnState, NewState);

// 재배치(ItemSort) 요청에 실을 칸 하나. 서버 ItemSlotEntry와 1:1
struct FNUServerSlotEntry
{
	int32 SlotNo = 0;
	int32 ItemId = 0;
	int32 ItemCount = 0;
};

/// <summary>서버 접속·인증 플로우를 관리하는 게임 인스턴스 서브시스템. 어디서든 GetSubsystem&lt;UEmberServerManager&gt;()로 접근.</summary>
UCLASS()
class PROJECTM_API UEmberServerManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ── 서버 주소/포트 (실제 값은 EmberServerManager.cpp의 EmberServerCfg에서 관리) ──
	UFUNCTION(BlueprintPure, Category = "Ember|Net")
	FString GetServerHost() const;

	UFUNCTION(BlueprintPure, Category = "Ember|Net")
	int32 GetServerPort() const;

	/** "host:port" 형태의 접속 엔드포인트. */
	UFUNCTION(BlueprintPure, Category = "Ember|Net")
	FString GetServerEndPoint() const;

	/** 서버에 접속해 인증→(생성)→로그인→핑 플로우를 자동 진행한다. */
	UFUNCTION(BlueprintCallable, Category = "Ember|Net")
	void Connect(const FString& InUserId);

	/**
	 * 구글 계정으로 접속한다 (Auth/NUGoogleAuthSubsystem 이 받은 ID 토큰).
	 * bServerVerifiesToken = false: 지금 서버 --- InUserId(구글 계정에서 만든 id)와 임시 인증 키로 접속한다.
	 * bServerVerifiesToken = true : AuthUser.auth_key 에 ID 토큰을 싣고, 서버가 응답 user_id 로 정한 계정 id 를 받아 쓴다
	 *                              (Docs/SERVER_REQUEST_GOOGLE_LOGIN.md). 재접속 때는 토큰을 새로 받아 다시 싣는다.
	 */
	void ConnectWithGoogle(const FString& InUserId, const FString& IdToken, bool bServerVerifiesToken);

	// 이름 로그인 전에: 이전 구글 로그인의 자격(ID 토큰, 서버 id 채택)을 지운다
	void ResetCredential() { AuthKey.Empty(); bAdoptServerUserId = false; }

	/** 연결 종료. */
	UFUNCTION(BlueprintCallable, Category = "Ember|Net")
	void Disconnect();

	/**
	 * 끊긴 뒤 같은 계정으로 다시 붙는다. 세션은 스트림 하나의 수명이라 AuthUser부터 다시 탄다.
	 * 비밀번호가 필요 없는 구조(AuthUser가 서버에서 재료를 받아 토큰을 만든다)라 UserId만으로 된다.
	 * 끊긴 적이 없거나 계정을 모르면 아무것도 안 하고 false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ember|Net")
	bool TryReconnect();

	/** 로그인이 끝나 게임 패킷을 보낼 수 있는 상태인지. */
	UFUNCTION(BlueprintPure, Category = "Ember|Net")
	bool IsLoggedIn() const { return State == EEmberConnState::LoggedIn; }

	/** 접속 계정 id. 채팅 서버 로그인에 그대로 쓴다. */
	UFUNCTION(BlueprintPure, Category = "Ember|Net")
	const FString& GetUserId() const { return UserId; }

	// 최근 패킷 왕복 시간의 평균(ms), 맨 처음 접속해서 아직 못쟀으면 0
	int32 GetAverageRttMs() const { return FMath::RoundToInt(AverageRttMs); }
	
	/**
	 * 게임 세션 토큰(Bearer). **채팅 서버 로그인 재료다** — 채팅 서버는 인증을 직접 하지 않고
	 * 게임 서버가 Redis 에 써둔 값과 이 토큰이 같은지만 본다.
	 */
	const FString& GetSessionToken() const { return SessionToken; }

	// ── 캐릭터 송신 (UNUCharacterManager가 호출한다. 게임 코드는 그쪽 API를 쓸 것) ──
	void SendCharacterList();
	// 외형 4종은 서버가 저장·에코만 한다 (범위 검사 없음 --- 클라가 정한 값이 곧 정답)
	void SendCharacterCreate(const FString& InName, int32 JobId, int32 RaceId, int32 Gender, int32 HairId, int32 FaceId);

	// 이름만 확인한다 (140) --- 캐릭터를 만들지 않는다. 생성창 [중복확인]·랜덤 이름용
	void SendCharacterNameCheck(const FString& InName);

	// 삭제 예약 취소 (141) --- 24시간이 지나기 전이면 되돌릴 수 있다
	void SendCharacterDeleteCancel(int64 CharacterIdx);

	// 남의 캐릭터 정보 조회 (142) --- 존 제한 없음, 내 캐릭터도 조회된다
	void SendCharacterProfile(int64 CharacterIdx);

	// 아이템 버리기 (66) --- 되돌릴 수 없다. 결과 칸은 SVR_Item_Slot으로 온다
	void SendItemDiscard(int32 Container, int32 SlotNo, int32 Count);
	void SendCharacterSelect(int64 CharacterIdx);
	void SendCharacterDelete(int64 CharacterIdx);
	void SendCharacterStat();
	void SendCharacterStatAssign(int32 Strength, int32 Dexterity, int32 Intelligence, int32 Luck, int32 Vitality);

	// ── 캐릭터 성장 송신 (UNUProgressionNetSubsystem 전용) ──
	void SendCharacterProgression(int32 GraphVersion);
	void SendCharacterJobSelect(int32 JobId, int32 GraphVersion, int64 ExpectedRevision, const FString& MutationId);
	void SendCharacterNodeAcquire(int32 NodeId, int32 ChoiceAdvancementId, int32 GraphVersion, int64 ExpectedRevision, const FString& MutationId);

	// ── 인벤토리 송신 (UNUInventoryComponent가 호출한다. 게임 코드는 그쪽 API를 쓸 것) ──
	// container: 1=인벤, 2=캐릭터창고, 3=계정창고 (서버 enum과 1:1)
	void SendInventoryList(int32 Container);
	void SendItemMove(int32 FromContainer, int32 FromSlot, int32 ToContainer, int32 ToSlot);

	// 장비는 컨테이너 4(Equipment)에 들어가고 칸 번호가 곧 부위다 (EquipSlot 0~7).
	// 해제할 때 벗은 장비가 들어갈 인벤 칸은 서버가 고른다 — 클라가 지정하지 않는다
	void SendItemEquip(int32 InvSlot, int32 EquipSlot);
	void SendItemUnequip(int32 EquipSlot);

	// 저장소 하나를 통째로 재배치. 배치는 클라가 정하고(정렬 기준이 클라 DataTable에만 있다)
	// 서버는 아이템별 총량과 칸별 스택 상한만 검증한다. 빈 칸은 넣지 않는다
	void SendItemSort(int32 Container, const TArray<FNUServerSlotEntry>& Slots);

	// 스택 나누기. ToSlot이 -1이면 서버가 빈 칸을 고른다. 목적지는 비어 있어야 한다
	void SendItemSplit(int32 Container, int32 FromSlot, int32 ToSlot, int32 Count);

	// ── 상점 송신 (UNUInventoryComponent가 호출한다) --- 서버가 품목표로 금액을 계산·검증한다 (가이드 §17, 9/7) ──
	// 구매 (180). total_price는 클라 표 총액 --- 서버 계산과 다르면 2208, result_message에 서버 금액이 온다
	void SendShopBuy(int32 ShopId, int32 ItemId, int32 Count, int32 TotalPrice);

	// 판매 (181). shop_id는 되팔기율(BuybackRate) 근거라 필수
	void SendShopSell(int32 SlotNo, int32 Count, int32 TotalGain, int32 ShopId);

	// ── 퀘스트 송신 (UNUQuestComponent가 호출한다. 게임 코드는 그쪽 API를 쓸 것) ──
	void SendQuestList();
	void SendQuestAccept(int32 QuestId);
	void SendQuestObjective(int32 QuestId, int32 ObjectiveId, int32 CurrentCount);
	void SendQuestComplete(int32 QuestId, int32 NpcId = 0);

	// 퀘스트 특수 인카운터 전용 송신. 일반 전투 시작과 분리한다.
	void SendQuestBattleStart(int32 NaiaId, int32 QuestId, int32 ObjectiveId, int32 BattleMode);

	// 클라이언트 판정 진정 도전. 시작 승인(105)으로 도전 ID를 받고, 로컬 진정 전투가 끝나면 결과(106)를 보낸다.
	void SendQuestCalmStart(int32 QuestId, int32 ObjectiveId);
	void SendQuestCalmResult(int32 QuestId, int32 ObjectiveId, const FString& AttemptId, bool bSuccess);

	// ── 1:1 거래 송신 (UNUTradeSubsystem이 호출한다. 게임 코드는 그쪽 API를 쓸 것) ──
	void SendTradeRequest(int64 TargetIdx);
	void SendTradeRespond(bool bAccept, int64 FromIdx);
	void SendTradeSetItem(int32 TradeSlot, int32 InvSlotNo);
	void SendTradeLock(bool bLock);
	void SendTradeConfirm(int32 OfferVer);
	void SendTradeCancel();

	// ── 필드 위치 동기화 송신 (UFieldSyncSubsystem이 호출한다. 게임 코드는 그쪽 API를 쓸 것) ──
	// 전송 계층이라 게임 enum이 아니라 int32로 받는다 (값은 서버 enum과 1:1).
	void SendFieldEnter(const FVector& Location, int32 Facing, int32 LocationId);
	void SendFieldMove(const FVector& Location, int32 Facing, int32 Reason);
	void SendFieldLeave();
	void SendFieldActivity(int32 Activity);
	void SendFieldSpirit(int32 SpiritId);

	// 근접 채팅. 응답에 정제된 본문이 실려 오므로 응답도 반드시 처리해야 한다
	void SendFieldChat(const FString& Text);

	// ── 파티 송신 (UNUPartySubsystem이 호출한다. 게임 코드는 그쪽 API를 쓸 것) ──
	// 대상 지목은 전부 character_idx (PlayerId가 아님)
	void SendPartyInvite(int64 TargetIdx);
	void SendPartyApply(int64 TargetIdx);
	void SendPartyRespond(int64 TargetIdx, bool bAccept);
	void SendPartyLeave();
	void SendPartyKick(int64 TargetIdx);
	void SendPartyMemberList();

	// ── 전투 송신 ──
	// UNUBattleNetSubsystem이 호출함
	// EncounterKind 1(심볼)이면 서버가 같은 공간의 파티원을 같이 넣음, SymbolId 는 서버가 Started · End 에 되돌려줌
	void SendBattleStart(int32 NaiaId, int32 EncounterKind = 0, int64 SymbolId = 0);
	
	void SendBattleCommand(const FNUServerBattleCommandRequest& Command);
	void SendBattleReaction(const FNUServerBattleReactionRequest& Reaction);
	
	void SendBattleNaiaSwap(int64 BattleId, int32 PartySlot);
	void SendBattleSync(int64 BattleId);
	void SendBattleLeave(int64 BattleId);

	// 행동 연출을 SVR_Battle_TurnEnd까지 다 재생했다는 보고, 여기서부터 명령 제한 시간을 잼
	// battle_id는 없음(서버가 이미 알고 있음), turn은 방금 받은 TurnEnd의 turn 그대로
	void SendBattleActionAck(int32 Turn);

	// ── 회복 송신 ──
	// UNURecoveryNetSubsystem이 호출함
	void SendNpcHeal(int32 NpcId, const FString& MutationId);
	void SendNpcRest(int32 NpcId, const FString& MutationId);
	void SendRecoveryStatus();
	void SendRecoveryPointRegister(int32 LocationId, const FString& MutationId);

	// ── 공간 이동 송신 ──
	void SendPortalMove(int32 PortalId, int32 SourceSide, const FString& MutationId, int32 ToLocationId, int32 ToAreaId, const FString& EntryPointId);
	void SendPortalArrive(const FString& TransitionId, bool bSuccess, int32 Failure, const FVector& Location, int32 Facing);
	void SendLocationMove(int32 FromLocationId, int32 ToLocationId, int32 GateId, const FString& MutationId, const FVector& Location, int32 Facing);
	void SendLocationRestore(const FString& TransitionId);

	// ── 나이아 로스터 송신 ──
	// UNUNaiaNetSubsystem이 호출함
	void SendNaiaList();	// 파티+보관함 전체 조회
	void SendNaiaMove(const FNUServerNaiaMoveRequest& Move);
	void SendNaiaStarterSelect(int32 NaiaId, const FString& MutationId);


	UPROPERTY(BlueprintReadOnly, Category = "Ember|Net")
	EEmberConnState State = EEmberConnState::Idle;

	UPROPERTY(BlueprintAssignable, Category = "Ember|Net")
	FOnEmberConnStateChanged OnStateChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// ── gRPC(TurboLink) 리소스 ──
	UPROPERTY()
	TObjectPtr<UGameService> Service = nullptr;

	UPROPERTY()
	TObjectPtr<UGameServiceClient> Client = nullptr;

	FGrpcContextHandle StreamHandle;

	// ── 세션 상태 ──
	FString UserId;
	FString SessionToken;
	int64 AuthBaseTimeStamp = 0;

	// AuthUser 에 싣는 자격 (기본: 이름 로그인 = 구글 플랫폼 + 임시 키). 구글 로그인이면 ID 토큰
	FString AuthKey;
	// 서버가 인증 응답의 user_id 로 계정 id 를 정해 준다 (구글 토큰 검증 서버). 그 id 로 세션 토큰을 만든다
	bool bAdoptServerUserId = false;

	// 재접속 전에 구글 ID 토큰을 새로 받아 AuthKey 에 넣고 Connect 를 이어간다
	void ReconnectWithFreshGoogleToken();
	bool bStreamStarted = false;
	FTimerHandle PingTimer;

	// ── 지연 시간 ──
	double PingSentSeconds = 0.0;	// 핑을 보낸 시각, 0이면 대기중인 핑이 없음
	double AverageRttMs = 0.0;		// 최근 패킷 왕복 시간 평균(ms)
	int32 RttSampleCount = 0;		// 평균에 반영할 표본 수, MAX_RTT_SAMPLES 까지만 증가함
	
	// ── 내부 유틸 ──
	void SetState(EEmberConnState NewState);
	int64 NowUnixSeconds() const;
	// 코드에서 관리하는 서버 주소/포트를 TurboLink 설정에 적용 (접속 직전 호출). ini 대신 이걸로 결정.
	void ApplyServerEndPoint();
	// 평균 지연시간 갱신
	void UpdateRtt();
	
	// ── gRPC 콜백 ──
	UFUNCTION()
	void HandleServiceStateChanged(EGrpcServiceState NewServiceState); // Ready면 스트림 열고 인증 시작

	UFUNCTION()
	void HandleGameResponse(FGrpcContextHandle Handle, const FGrpcResult& Result, const FGrpcRevGameGameMessage& Response); // 응답 수신 → 디스패치

	// ── 송신 (요청 빌드 + 전송) ──
	FGrpcRevGameGameMessage MakeEnvelope() const; // UserId/Timestamp/Token 공통 헤더
	void SendAuthUser();
	void SendCreateUser();
	void SendLoginUser();
	void SendPing();

	// ── 응답 핸들러 (HandleGameResponse가 메시지 타입별로 호출) ──
	void Handle_AuthUser(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CreateUser(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_LoginUser(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_Ping(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_Kick(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	// ── 캐릭터 응답 → UNUCharacterManager로 전달 ──
	void Handle_CharacterList(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterCreate(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterSelect(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterDelete(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterStat(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterStatAssign(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterProgression(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterJobSelect(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterNodeAcquire(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	/** 캐릭터 목록/선택 상태를 들고 있는 서브시스템 (없으면 nullptr). */
	class UNUCharacterManager* GetCharacterManager() const;
	class UNUProgressionComponent* GetLocalProgressionComponent() const;
	class UNUProgressionNetSubsystem* GetProgressionNetSubsystem() const;

	// ── 인벤토리 응답/푸시 → 로컬 UNUInventoryComponent로 전달 ──
	void Handle_InventoryList(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ItemMove(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ItemSort(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ItemEquip(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ItemUnequip(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ItemSplit(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ShopBuy(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ShopSell(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Item_Slot(const FGrpcRevGameGameMessage& Response);

	/** 로컬 플레이어의 인벤토리 컴포넌트 (PlayerState 부착, 없으면 nullptr). */
	class UNUInventoryComponent* GetLocalInventory() const;

	// ── 퀘스트 응답 → 로컬 UNUQuestComponent로 전달 ──
	void Handle_QuestList(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_QuestAccept(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_QuestObjective(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_QuestComplete(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_QuestCalmStart(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_QuestCalmResult(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Quest_Updated(const FGrpcRevGameGameMessage& Response);

	/** 로컬 플레이어의 퀘스트 컴포넌트 (PlayerState 부착, 없으면 nullptr). */
	class UNUQuestComponent* GetLocalQuestComponent() const;

	// ── 필드 푸시 수신 → UFieldSyncSubsystem으로 전달 ──
	void Handle_SVR_Field_Appeared(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Field_Disappeared(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Field_Moved(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Field_Activity(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Field_Spirit(const FGrpcRevGameGameMessage& Response);
	void Handle_FieldEnter(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	// 내가 보낸 FieldChat의 응답, 근접 채팅은 내가 보낸말이 나에게 옴
	void Handle_FieldChat(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	// 남이 말한 것. 이름은 실려 오지 않는다.
	void Handle_SVR_Field_Chat(const FGrpcRevGameGameMessage& Response);

	// ── 파티 ──
	// 초대·신청·탈퇴·추방은 응답이 ResultCode뿐이라 하나로 받음
	// 목록은 SVR_Party_Updated 푸시로 오므로 여기서 할 일은 실패를 알리는 것뿐이다
	void Handle_PartyAck(int32 ResultCode);

	// 수락이면 파티원 목록이 실려 온다
	void Handle_PartyRespond(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_PartyMemberList(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	void Handle_SVR_Party_Invited(const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterNameCheck(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterDeleteCancel(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_CharacterProfile(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_ItemDiscard(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Party_Applied(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Party_Updated(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Party_Rejected(const FGrpcRevGameGameMessage& Response);

	// ── 1:1 거래 ──
	// 요청 6종은 응답이 ResultCode뿐이다 — 실제 상태 변화는 SVR_Trade_* 푸시로 온다
	void Handle_TradeAck(int32 ResultCode);

	void Handle_SVR_Trade_Requested(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Trade_Started(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Trade_Updated(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Trade_Ended(const FGrpcRevGameGameMessage& Response);

	
	// ── 전투 ──
	
	// BattleCommand는 서버 확정값이 response_data에 들어 있음
	void Handle_BattleCommand(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	// 나머지 전투 요청은 ResultCode만 처리함
	void Handle_BattleReaction(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_BattleAck(int32 ResultCode, const TCHAR* RequestName);

	void Handle_SVR_Battle_Started(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Battle_TurnBegin(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Battle_Submit(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Battle_Order(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Battle_Reaction(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Battle_Action(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Battle_TurnEnd(const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Battle_End(const FGrpcRevGameGameMessage& Response);

	
	// ── 나이아 로스터 ──
	
	// NaiaList는 요청 자리에 오지만 response_data JSON이 아니라 타입 있는 본문으로 옴
	void Handle_NaiaList(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	// NaiaMove는 result_code만, 실제 결과는 SVR_Naia_Updated 푸시로 옴
	void Handle_NaiaAck(int32 ResultCode, const TCHAR* RequestName);
	void Handle_NaiaStarterSelect(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	void Handle_SVR_Naia_Updated(const FGrpcRevGameGameMessage& Response);


	// ── 회복 응답/푸시 → UNURecoveryNetSubsystem으로 전달 ──
	void Handle_NpcHeal(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_NpcRest(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_RecoveryStatus(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_RecoveryPointRegister(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_SVR_Respawn_Result(const FGrpcRevGameGameMessage& Response);

	// ── 공간 이동 응답 → 현재 월드의 전환 담당 객체로 전달 ──
	void Handle_PortalMove(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_PortalArrive(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_LocationMove(int32 ResultCode, const FGrpcRevGameGameMessage& Response);
	void Handle_LocationRestore(int32 ResultCode, const FGrpcRevGameGameMessage& Response);

	/** 1:1 거래 상태를 들고 있는 서브시스템 (없으면 nullptr). */
	class UNUTradeSubsystem* GetTradeSubsystem() const;

	/** 현재 월드의 필드 동기화 서브시스템 (없으면 nullptr). */
	class UFieldSyncSubsystem* GetFieldSync() const;

	/** 채팅 메시지를 모으는 서브시스템 (없으면 nullptr). */
	class UNUChatSubsystem* GetChatSubsystem() const;

	/** 파티 상태를 들고 있는 서브시스템 (없으면 nullptr). */
	class UNUPartySubsystem* GetPartySubsystem() const;
	
	/** 전투 상태 들고 있는 서브시스템 (없으면 nullptr). */
	class UNUBattleNetSubsystem* GetBattleNetSubsystem() const;

	/** 나이아 로스터 상태 들고 있는 서브시스템 (없으면 nullptr). */
	class UNUNaiaNetSubsystem* GetNaiaNetSubsystem() const;

	/** 회복 상태를 들고 있는 서브시스템 (없으면 nullptr). */
	class UNURecoveryNetSubsystem* GetRecoveryNetSubsystem() const;

	/** 현재 월드의 공간 전환 서브시스템 (없으면 nullptr). */
	class UNUWorldTransitionSubsystem* GetWorldTransitionSubsystem() const;
};
