// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/Net/NUBattleRewardServerTypes.h"
#include "NUBattleServerTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUBattleNetSubsystem.generated.h"

struct IConsoleCommand;
class UEmberServerManager;
enum class EEmberConnState : uint8;
struct FGrpcRevGameSVR_Battle_Started;
struct FGrpcRevGameSVR_Battle_TurnBegin;
struct FGrpcRevGameSVR_Battle_Submit;
struct FGrpcRevGameSVR_Battle_Order;
struct FGrpcRevGameSVR_Battle_Reaction;
struct FGrpcRevGameSVR_Battle_Action;
struct FGrpcRevGameSVR_Battle_TurnEnd;
struct FGrpcRevGameSVR_Battle_End;

// 전투 상태를 서버에서 갱신하면 최신 상태를 반영함 
DECLARE_MULTICAST_DELEGATE(FNUOnBattleNetStateChanged);
// ActorId · ActionSeq · 이벤트 목록
// 같은 ActionSeq 가 또 오면 새 행동이 아니라 이어지는 조각이므로 받는 쪽이 가려야 함
DECLARE_MULTICAST_DELEGATE_ThreeParams(FNUOnBattleNetActionReceived, int32, int32, const TArray<FNUServerBattleEvent>&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNUOnBattleNetTurnEndReceived, int32, const TArray<FNUServerBattleEvent>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnBattleNetEnded, int32);

// 패링·회피 창이 열렸음, 그 행동의 SVR_Battle_Action 보다 먼저 옴
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnBattleNetReactionReceived, const FNUServerBattleReactionWindow&);

// SVR_Battle_Started는 전투 시작뿐 아니라 나이아 교체·재동기화로도 옴
// 교체는 나간 전투원 번호를 버리고 들어온 쪽에 새 번호를 발급하므로, 전투원 번호를 키로
// 들고 있는 쪽(아바타 맵, 재생 큐 스냅샷)은 이 신호를 받아 자기 맵을 다시 맞춰야 함
DECLARE_MULTICAST_DELEGATE(FNUOnBattleNetCombatantsReplaced);

// BattleCommand 응답, 성공·거절 모두 옴. 거절이면 Ack는 비어 있음(CombatantId == INDEX_NONE)
// 어느 전투원의 거절인지는 보낸 쪽이 기억해야 함
DECLARE_MULTICAST_DELEGATE_TwoParams(FNUOnBattleNetCommandAck, ENUBattleCommandSubmitResult, const FNUServerCommandAck&);

// 파티원이 시작한 심볼 전투에 서버가 나를 같이 넣었음 (요청하지 않은 SVR_Battle_Started), 닿은 심볼 번호
// 시작한 쪽처럼 배경과 심볼을 정해야 해서 무대를 세우는 OnBattleStateChanged 보다 먼저 브로드캐스트됨
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnBattleNetPartyJoined, int64);

UCLASS()
class PROJECTM_API UNUBattleNetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	
	// ── 송신 ──
	
	// EncounterKind 1(심볼)이면 서버가 같은 공간의 파티원을 같이 넣음, SymbolId 는 파티원 클라가 같은 심볼을 찾는 번호
	void RequestBattleStart(int32 NaiaId, int32 EncounterKind = 0, int64 SymbolId = 0);

	// 일반 전투와 퀘스트 전투가 동시에 시작 요청을 보내지 않도록 하는 장치
	bool TryReserveBattleStart();
	bool IsBattleStartPending() const { return bBattleStartPending; }
	
	// BattleId는 서브시스템이 현재 전투 Id로 채움
	void SubmitCommand(FNUServerBattleCommandRequest Command);
	void SubmitReaction(FNUServerBattleReactionRequest Reaction);
	
	// 반응 창 하나에 대한 보고, 창 목록에서 그 창을 지우고 보낸다
	// 같은 창을 두 번 보고하면 서버가 1915로 거절하므로 지워야 함
	// PressTimeMs 는 창이 열린 뒤의 경과(ms), 안 눌렀으면 -1
	void ReportReaction(int32 ReactionId, bool bSuccess, int32 PressTimeMs);
	
	// 보고하지 않기로 한 창을 목록에서 버림 (안 눌렀고 기다리지도 않는 창)
	// 안 버리면 목록에 남고, 다음 턴에 ActionSeq 가 1부터 다시 시작하므로 그 턴의 창으로 잘못 뽑힘
	void DiscardReaction(int32 ReactionId);
	
	void RequestNaiaSwap(int32 PartySlot);
	
	// BattleId가 0이면 서버가 현재 로그인 캐릭터의 전투 id를 찾는다.
	void RequestSync();
	void RequestLeave();

	// 행동 연출(액션 큐)이 SVR_Battle_TurnEnd까지 다 끝났다는 보고
	// UNUBattleNetActionQueueSubsystem이 그 턴의 행동 연출을 마치고 큐가 빌 때 호출함
	// 이 응답부터 서버의 명령 제한 시간이 돌고, 이어서 SVR_Battle_TurnBegin이 옴
	void NotifyActionPlaybackFinished(int32 InTurn);
	
	
	// ── 수신, EmberServerManager가 호출 ──
	
	void HandleStarted(const FGrpcRevGameSVR_Battle_Started& Message);
	void HandleTurnBegin(const FGrpcRevGameSVR_Battle_TurnBegin& Message);
	void HandleSubmit(const FGrpcRevGameSVR_Battle_Submit& Message);
	void HandleOrder(const FGrpcRevGameSVR_Battle_Order& Message);
	void HandleReaction(const FGrpcRevGameSVR_Battle_Reaction& Message);
	void HandleAction(const FGrpcRevGameSVR_Battle_Action& Message);
	void HandleTurnEnd(const FGrpcRevGameSVR_Battle_TurnEnd& Message);
	void HandleEnd(const FGrpcRevGameSVR_Battle_End& Message, const FString& ResponseData);

	// BattleCommand 직접 응답
	void HandleCommandAck(int32 ResultCode, const FString& ResponseData);

	// BattleStart, Reaction, NaiaSwap, Sync, Leave의 단순 응답
	void HandleAck(int32 ResultCode, const TCHAR* RequestName);
	
	
	// ── 조회 ──
	
	bool IsInBattle() const;
	int64 GetBattleId() const;
	int32 GetTurn() const;
	int32 GetLastTimeoutMs() const;
	const TMap<int32, FNUServerCombatant>& GetCombatants() const;
	const TArray<int32>& GetPendingCombatantIds() const;
	const TArray<int32>& GetLastOrder() const;
	const FNUServerCommandAck& GetLastAck() const;

	// 마지막 전투의 승리 보상, 결과창이 읽는다
	// 전투 종료 정리(ResetBattleState)에 지우지 않는다 - 결과창은 연출이 다 끝난 뒤에 뜨므로
	// 그때는 이미 전투 상태가 비어 있음. 다음 전투가 시작될 때 비운다
	bool HasLastReward() const;
	const FNUServerBattleReward& GetLastReward() const;

	// 표시 반영(골드 추가/성장치 재조회)을 이미 했는지, 전투당 한 번만 해야 함
	bool IsLastRewardApplied() const;
	void MarkLastRewardApplied();
	
	const FNUServerCombatant* FindCombatant(int32 CombatantId) const;
	
	// 아직 보고하지 않은 반응 창 중 그 행동의 것을 HitIndex 순으로 모음
	// 창이 행동보다 먼저 오므로, 재생 큐가 그 행동을 시작할 때 이걸로 꺼내 씀
	void CollectReactionWindows(int32 ActionSeq, TArray<FNUServerBattleReactionWindow>& OutWindows) const;
	
	// 아직 보고하지 않은 창이 하나라도 있는지, 서버가 기다리는 중인지 판단할 때 씀
	bool HasPendingReactionWindow() const;
	
public:
	// OnBattleStateChanged보다 먼저 브로드캐스트됨 - 명단을 키로 쓰는 쪽이 먼저 맞춰져 있어야
	// 뒤이은 상태 변화 처리(선택 UI 갱신 등)가 올바른 전투원을 보게 됨
	FNUOnBattleNetCombatantsReplaced OnCombatantsReplaced;

	FNUOnBattleNetStateChanged OnBattleStateChanged;
	FNUOnBattleNetActionReceived OnBattleActionReceived;
	FNUOnBattleNetTurnEndReceived OnBattleTurnEndReceived;
	FNUOnBattleNetEnded OnBattleEnded;
	FNUOnBattleNetReactionReceived OnBattleReactionReceived;
	FNUOnBattleNetCommandAck OnCommandAck;
	FNUOnBattleNetPartyJoined OnPartyBattleJoined;

private:
	UFUNCTION()
	void HandleServerStateChanged(EEmberConnState NewState);
	
	UEmberServerManager* GetServer() const;
	void ResetBattleState();
	void ClearBattleStartPending(bool bClearStage);
	
	bool IsCurrentBattleMessage(int64 IncomingBattleId, const TCHAR* MessageName) const;
	
	// 요청 없이 온 Started 가 파티원이 시작한 심볼 전투인지, 시작한 사람이 내 파티원이 아니면 경고만 남김 (서버 판정을 따름)
	bool IsJoinedPartyBattle(const FGrpcRevGameSVR_Battle_Started& Message) const;

	// ActionSeq 는 이 이벤트가 그 턴의 몇 번째 행동 것인지, SP 를 순서대로 반영하는 데 씀
	// 턴 종료 묶음은 그 턴의 모든 행동 뒤이므로 int32 최대값을 넘김
	void ApplyEventToSnapshot(const FNUServerBattleEvent& Event, int32 ActionSeq);

	// SP 절대값을 ActionSeq 순서로만 반영함, 늦게 온 옛 행동의 값은 버림
	bool TryApplyMirrorSP(int32 CombatantId, int32 SPAfter, int32 ActionSeq);

	FNUServerBattleReward LastReward;
	bool bHasLastReward = false;
	bool bLastRewardApplied = false;

	
	// 로그용
	
	// 서버 연결과 로그인 상태를 검사함
	UEmberServerManager* GetLoggedInServer(const TCHAR* Operation) const;

	// 로그인 상태와 진행 중인 서버 전투를 함께 검사함
	UEmberServerManager* GetActiveBattleServer(const TCHAR* Operation) const;

	// NU.BattleNet 콘솔 디버그 명령
#if !UE_BUILD_SHIPPING
	void RegisterDebugCommands();
	void UnregisterDebugCommands();
	void DumpDebugState() const;
#endif
	
private:
	// 서버가 발급한 전투 Id, 0이면 현재 서버 전투 X
	int64 BattleId = 0;
	bool bBattleStartPending = false;
	bool bBattleStartAcknowledged = false;
	bool bBattleStartSyncPending = false;
	TWeakObjectPtr<UWorld> BattleStartWorld;
	
	int32 Turn = 0;
	int32 LastTimeoutMs = 0;
	
	TMap<int32, FNUServerCombatant> Combatants;

	// 전투원별로 SP 를 마지막에 갱신한 ActionSeq. 턴이 바뀔 때 비움(ActionSeq 가 1부터 다시 매겨짐)
	TMap<int32, int32> LastSPActionSeq;
	TArray<int32> PendingCombatantIds;
	TArray<int32> LastOrder;
	
	FNUServerCommandAck LastAck;

	// 열렸지만 아직 보고하지 않은 반응 창들
	// 창이 그 행동의 Action 보다 먼저 오기 때문에 받은 자리에서 바로 쓸 수가 없음
	// 재생 큐가 해당 ActionSeq 를 재생할 때 CollectReactionWindows 로 꺼내 감
	TArray<FNUServerBattleReactionWindow> PendingReactionWindows;

	// BattleCommand를 보낸 시점의 턴, 직접 응답이 푸시 스트림보다 늦게 와서
	// 이미 다음 턴이 시작된 뒤 도착하면 그 안의 Pending으로 다음 턴 상태를 덮어쓰면 안 됨
	int32 PendingCommandTurn = INDEX_NONE;

	// 마지막으로 보낸 전투 명령, 거절 응답엔 어떤 명령이었는지가 없어서 사유를 로그로 남길 때 씀
	FNUServerBattleCommandRequest LastCommandRequest;

	// 콘솔 디버그 명령들
#if !UE_BUILD_SHIPPING
	TArray<IConsoleCommand*> DebugCommands;
#endif
};
