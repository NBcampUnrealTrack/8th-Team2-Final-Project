// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "Battle/NUBattleTypes.h"
#include "Battle/Enemy/NUEnemyPatternTypes.h"
#include "Battle/Formation/NUBattleFormationTypes.h"
#include "GameFramework/Info.h"
#include "Battle/Interface/NUBattleEventInterface.h"
#include "NUBattleEncounter.generated.h"

struct FNUEnemySelectedAction;
struct FNUBattleActionContext;
class ULevelSequence;
class UPaperZDAnimSequence;
class ANUBattleAvatar;
class UNUEnemyAttackAbility;
struct FGameplayEventData;
struct FNUItemDefRow;
class UNUBattleGameplayAbility;
class UNUAbilitySystemComponent;
class UAbilitySystemComponent;
class UGameplayAbility;
struct FNUBattleCommandRequest;
struct FAbilityEndedData;
class UNUNaiaDataSubsystem;
struct FNUEnemyPattern;
class ANUBattleEncounter;
class ANUPlayerState;
class UWorld;
class UNUBattleDefinition;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBattlePhaseChanged, ENUBattlePhase, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUTransitionCue, ENUTransitionCue);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEncounterCompleted, ANUBattleEncounter*, ENUBattleResult);

// 제출받은 명령 구조체
struct FNUResolvedAction
{
	FNUBattleCommandRequest Command;	// 명령
	int32 Priority = 0;					// 우선순위 (도주, 선제 공격 등에 사용)
	float Speed = 0.f;					// 속도
	bool bForceLast = false;			// 둔화, 우선순위와 속도를 무시하고 맨 뒤로 보냄
	bool bForceFirst = false;			// 선제공격, 둔화를 뺀 나머지를 무시하고 맨 앞으로 보냄
	bool bHallucinated = false;			// 환각, 원래 행동을 버리고 자기 진영에 기본 공격
};

struct FNUExperienceGrowth;

// 포획에 성공한 나이아 구조체, 서버에서만 사용함
struct FNUPendingCapture
{
	int32 TargetCombatantId = INDEX_NONE;	// 포획한 나이아의 전투원 ID
	int32 NaiaId = 0;						// 나이아 ID
	int32 Level = 1;						// 레벨

	// 포획한 플레이어, 전투 중 이탈할 수 있어 WeakPtr
	TWeakObjectPtr<ANUPlayerState> Owner;
};

UCLASS(NotPlaceable, Blueprintable)
class PROJECTM_API ANUBattleEncounter : public AInfo
{
	GENERATED_BODY()

public:
	ANUBattleEncounter();
	// 외부 처리자는 기존 Subsystem을 참조한다. 전투마다 새 컴포넌트를 생성하지 않는다.
	INUBattleEventInterface* GetBattleEventHandler() const { return BattleEventHandler.GetInterface(); }

	// 인정 임계치가 있는 적의 체력 하한(최대 체력 x 비율), 임계치가 없거나 적이 아니면 0
	// 체력 어트리뷰트가 피해를 받을 때 이 아래로 내리지 않아 인정하는 적은 쓰러지지 않음
	float GetConcessionHealthFloor(const UAbilitySystemComponent* ASC) const;
	UNUBattleDefinition* GetBattleDefinition() const { return BattleDefinition; }
	virtual bool ConfigureBattleDefinition(UNUBattleDefinition* Definition);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버에서 초기화하는 함수
	// 전투에 참여하는 파티원 목록을 받아서 등록함
	bool InitializeEncounter(const TArray<FNUBattleParticipant>& InitialParticipants);

	// 이 전투가 쓸 배경 서브레벨, 스폰 직후 요청에서 받음
	// InitializeEncounter 가 전환 연출중에 해당 레벨을 로드함
	void SetBattleStageLevel(const TSoftObjectPtr<UWorld>& StageLevel);
	
	// 클라이언트로부터 명령을 제출받는 함수
	// 제출 결과를 반환함, 클라가 성공했는지, 거절됐다면 왜 거절됐는지 알 수 있어야 함
	ENUBattleCommandSubmitResult TrySubmitCommand(APlayerController* Requester,
		const FNUBattleCommandRequest& Request);
	
	void RequestFlee(int32 RequesterCombatantId);

	FGuid GetEncounterId() const;
	
	ENUBattlePhase GetPhase() const;
	
	int32 GetTurnNumber() const;

	// 라운드 끝 처리(상태이상 지속 턴 감소 · 도트 · 자원 감쇠) 중인지
	// 그 사이에 켜지는 상태(지맥 붕괴)는 다음 턴부터 시작함
	bool IsAdvancingRound() const;
	
	int32 GetActionSequence() const;
	
	ENUTransitionCue GetTransitionCue() const;
	ULevelSequence* GetTransitionSequence(ENUTransitionCue Cue) const;
	ENUBattleResult GetBattleResult() const;
	
	const FNUBattleParticipant* FindFirstParticipant(ENUBattleTeam Team) const;
	const FNUBattleParticipant* FindRandomParticipant(ENUBattleTeam Team) const;
	
	const UNUBattleGameplayAbility* FindBattleAbility(const FNUBattleCommandRequest& Command) const;
	const FNUItemDefRow* FindItemDef(const FNUBattleCommandRequest& Command) const;
	
	bool GetParticipant(int32 CombatantId, FNUBattleParticipant& OutParticipant) const;

	// ASC 주인(플레이어 · 정령은 상태 액터, 적은 아바타 자신)으로 전투원 ID 를 찾음, 없으면 INDEX_NONE
	int32 FindCombatantIdByOwner(const AActor* ASCOwner) const;
	void GetLivingCombatantIds(ENUBattleTeam Team, TArray<int32>& OutTargetIds) const;

	// 쓰러진 전투원만 (부활 대상 선택). 살아 있는 목록과 짝이라 같이 둔다
	void GetDownedCombatantIds(ENUBattleTeam Team, TArray<int32>& OutTargetIds) const;

	// 실행 대기 중인 행동 순서를 CombatantId 목록으로 반환한다.
	// BuildActionQueue 가 우선순위 -> 속도 -> CombatantId 순으로 정렬해둔 ActionQueue 를 그대로 옮겨 담을 뿐이라,
	// ActionExecution 페이즈가 아닐 때는 비어 있거나 지난 턴의 순서가 남아 있을 수 있다 — 호출부가 페이즈로 판단해야 한다.
	void GetUpcomingTurnOrder(TArray<int32>& OutCombatantIds) const;

	// 지금 실행 중인 행동이 이 ASC 의 것이면 그 행동에 쓴 SP, 아니면 0
	// 행동 중이 아닐 때(도트·턴 경계) 0이 나오는 것이 맞음 - 2834 는 그 행동으로 처치만 판정함
	int32 GetRunningActionSPSpend(const UNUAbilitySystemComponent* ASC) const;

	// 이 ASC 의 행동이 이번 턴에 아직 남았는지, 지금 실행 중인 행동도 남은 것으로 셈
	// 남았으면 그 행동이 끝날 때 SP 가 상한까지 참 — 받은 SP 를 지금 SP 에 더할지 가르는 데 씀 (3240 지맥 개방)
	bool HasActionLeftThisTurn(const UNUAbilitySystemComponent* ASC) const;

	void GetCommandableCombatantIds(const APlayerState* OwningPlayerState, TArray<int32>& OutCombatantIds) const;
	
	FVector GetActionLocation(int32 SourceCombatantId, int32 TargetCombatantId, ENUBattleActionMotion Motion) const;
	
	// 어빌리티 판정 시점에 대상을 다시 확정할 때 사용하는 함수
	void ResolveAbilityTargetActors(int32 SourceCombatantId, ENUTargetPolicy Policy, int32 SelectedTargetId,
		TArray<AActor*>& OutTargetActors) const;

	// 포획 판정과 실행 처리
	// 지금은 교감 아이템에서 호출함
	// StoneMagnitude 는 아이템 표의 UseMagnitude(5/10/20/50), 교감석 배율로 환산됨
	// OutShakes 는 굴린 흔들림 결과, 연출이 이 개수만큼 흔들면 됨
	ENUCaptureResult TryCapture(int32 SourceCombatantId, int32 TargetCombatantId,
		float StoneMagnitude, TArray<bool>& OutShakes);

	// 포획으로 빠진 전투원인지, 보상 계산 시 처치와 구분하기 위해 사용
	bool IsCaptured(int32 CombatantId) const;

	// 이 전투원이 플레이어 본인인지 (정령이 아닌지)
	// 포획과 나이아 교체가 둘 다 플레이어 전용이라 공용
	UFUNCTION(BlueprintPure, Category = "NU|Battle")
	bool IsPlayerCombatant(int32 CombatantId) const;

	// 이 스킬 하나가 막혀 있는지, 실명처럼 전부 막는 것과 속성만 막는 것을 같이 봄
	// 실명은 액티브 전부를 막고 2109 수중 감옥은 그 속성만 막아서 다름
	bool IsSkillBlocked(int32 CombatantId, int32 SkillId) const;

	// 전투 중 나이아 교체, 턴을 소비하지 않는 자유 행동이라 명령 큐에 들어가지 않음
	ENUNaiaSwapResult ApplyNaiaSwap(APlayerState* Requester, int32 ToPartySlot);

	// 요청자가 조종하는 정령 전투원 ID, 교체 대상 슬롯을 찾을 때 사용함
	// 정령이 참가하지 않았으면 INDEX_NONE
	int32 FindSpiritCombatantId(const APlayerState* OwningPlayerState) const;
	
	// ANUPlayerController::ServerTryParry 경유함
	void RegisterParryPress(const APlayerState* Request, int32 ForActionSequence, float PressOffset);

	// ANUPlayerController::ServerTryDodge 경유함, 패링 불가 공격에만 유효
	void RegisterDodgeRequest(const APlayerState* Request, int32 ForActionSequence);

	// 화면 상단 행동 배너 띄우기 Multicast
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowActionBanner(const FNUBattleActionBanner& Banner);

public:
	/* 델리게이트 */
	
	// 전투 페이즈 전환 요청
	FOnBattlePhaseChanged OnBattlePhaseChanged;

	// 화면 전환 연출 요청
	FOnNUTransitionCue OnTransitionCue;
	FOnEncounterCompleted OnEncounterCompleted;

protected:
	UPROPERTY(Transient)
	TScriptInterface<INUBattleEventInterface> BattleEventHandler;

	UPROPERTY(Transient)
	TObjectPtr<UNUBattleDefinition> BattleDefinition;

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION()
	void OnRep_TransitionCue();

	void SetTransitionCue(ENUTransitionCue NewCue);
	
	UFUNCTION()
	void OnRep_TurnNumber();
	
	UFUNCTION()
	void OnRep_BattleResult();
	
	void SpawnBattleParticipants();

	// 배경 서브레벨 로딩을 서브시스템에 요청함
	void RequestBattleStage();

	// 올린 배경이 보이는 상태인지, 서브시스템이 없으면 기다릴 방법이 없어 true
	bool IsBattleStageReady() const;

	// 배경이 준비됐거나 기다릴 시간을 다 썼으면 true
	// false 면 TransitionTimer 를 다시 걸어둔 뒤이므로 호출부는 그대로 물러나면 됨
	bool WaitForBattleStage();

	// 전투 배경의 조명으로 맞바꾸거나 필드 조명으로 되돌림
	// 되돌리기를 놓치면 다음 전투까지 필드가 어두운 채로 남으므로 전투 종료시 반드시 호출해야 함
	void SetBattleStageLightingActive(bool bActive);

	// 스폰 직후 진형 중심을 구해둠, 전진 연출의 기준으로 사용함
	// 아무도 움직이지 않은 시점에 한 번만 불러야 함
	void CacheFormationCenters();

	// 시전자 진영에서 상대 진영으로 가는 단위 벡터
	// 진형에서 유도하므로 좌우·깊이·사선 배치를 같은 코드로 처리함
	// 진형 중심을 못 구했으면 false
	bool GetAdvanceDirection(ENUBattleTeam Team, FVector& OutDirection) const;

	void BeginCommandSelection();
	bool HasAllRequestedCommands();
	virtual bool ShouldWaitForPlayerCommands() const { return true; }
	virtual void GenerateEnemyCommands();

	// 적이 이번에 낼 어빌리티를 선택함
	// 패턴이 없거나 선택 못하면 기본 공격 사용
	virtual FNUEnemySelectedAction SelectEnemyAbility(const FNUBattleParticipant& Participant);

	// 전투 데이터 표에서 이 몬스터의 패턴을 찾음, 등록되지 않았으면 nullptr
	const FNUEnemyPattern* FindEnemyPattern(FName EnemyId) const;

	// 어빌리티 행동에 맞는 배너를 생성함
	FNUBattleActionBanner BuildActionBanner(const FNUBattleCommandRequest& Command,
		const FNUBattleActionContext& Context,
		UNUAbilitySystemComponent* SourceASC, 
		const FGameplayAbilitySpecHandle& AbilityHandle);

	void BuildActionQueue();
	// 전용 인카운터가 공용 정렬 뒤 필수 행동의 순서를 조정할 수 있다.
	virtual void SortActionQueue();
	void ExecuteNextAction();
	void ExecuteAbilityCommand(const FNUBattleCommandRequest& Command);
	void ExecuteItemCommand(const FNUBattleCommandRequest& Command);
	void CheckBattleState();

	// 한쪽 진영이 전멸했으면 전투를 끝내고 true
	// 행동 뒤와 턴 종료 도트 뒤 두 번 판정해야 하므로 분리함
	bool TryFinishByElimination();

	// 살아 있는 적이 모두 인정 임계치에 닿았으면 인정 연출을 틀고, 그 길이만큼 기다렸다 승리로 끝냄
	// 연출이 도는 동안에도 true 를 돌려 다음 행동 · 턴으로 넘어가지 않게 함
	bool TryFinishByConcession();
	void FinishByConcession();

	// 이 적이 인정 임계치에 닿았는지, 임계치가 없는 전투원은 false
	bool IsConceded(int32 CombatantId) const;

	// 이 적 전투원의 패턴, 적이 아니거나 패턴이 없으면 nullptr
	const FNUEnemyPattern* FindParticipantPattern(const FNUBattleParticipant& Participant) const;

	// 인정 시퀀스를 전투 시작 암전 중에 불러 둠, 인정하는 순간 처음 불러오면 동기 로드로 끊김
	void PreloadConcessionAnimations();

	// 전투원마다 가진 어빌리티 · 스킬의 행동 연출을 전투 시작 암전 중에 아바타에 불러 둠
	void PreloadActionVFX();

	void FinishBattle(ENUBattleResult Result);

	// 나이아 표에 적힌 처치 보상을 합산함
	// 서버 규칙과 같게 승리일 때만이고 포획한 적은 처치 보상에서 제외함
	void CollectLocalRewards(int64& OutExperience, int64& OutGold) const;

	// 모은 보상을 실제로 반영하고 결과창에 쓸 성장 변화를 채움, 반영했으면 true
	// 외부 성장 세션이 붙어 있으면 서버가 권위라 아무것도 하지 않음
	bool ApplyLocalRewards(int64 Experience, int64 Gold, FNUExperienceGrowth& OutGrowth);

	// 결과창을 띄우고 확인을 기다림, 창이 안 뜨는 종료(패배·도주)면 곧바로 전환으로 넘어감
	void ShowBattleResultOrFinish();
	void HandleBattleResultDismissed();

	// 화면 전환(페이드아웃)부터의 뒷부분, 포획 연출을 기다렸다가 들어올 수 있어 갈라둠
	void BeginFinishTransition();

	// 재생 중인 포획 흔들림 중 가장 오래 남은 시간
	float GetCaptureShakeRemainingSeconds() const;
	void CompleteFinishBattle();
	void NotifyEncounterCompleted();
	
	/* 전투 결과 이벤트 발행 및 적 처치 감지 */

	// 전투 승리 결과 BattleEventSubsystem에 발행
	void PublishBattleWonEvent();
	// 적 사망 태그 변경을 감지하는 델리게이트
	void BindEnemyDefeatEvents();
	void UnbindEnemyDefeatEvents();
	// Dead 태그로 처치 여부 확인
	void HandleEnemyDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	// 처치된 적을 찾아 enemy defeated 이벤트 발행
	void BroadcastEnemyDefeatedEvents();
	// 전투 결과를 받을 아군 플레이어 목록
	void GatherEligiblePlayerStates(TArray<TWeakObjectPtr<APlayerState>>& OutPlayerStates) const;


	// 포획해둔 나이아를 로스터에 등록함, 전투 결과와 무관하게 한 번만 실행함
	void CommitPendingCaptures();

	// 대기 나이아의 턴 기반 효과를 정리하고 죽어 있으면 되살림
	// 참가자 루프가 대기중인 나이아를 못 잡아 디버프와 사망이 영구히 남는 것을 막음
	void RestoreWaitingNaiaAfterBattle();

	// 전투 종료 후 현재 파티 0번으로 필드 나이아를 다시 생성
	// 전투 중 교체하면 OriginAvatars 가 교체 전 필드 나이아를 들고 있는 문제가 있음
	void RespawnActiveSpiritFieldAvatars();
	
	// 참가한 플레이어 스테이트(나이아 소유자)만 모아주는 헬퍼 함수
	void CollectOwnerStates(TSet<ANUPlayerState*>& OutOwnerStates) const;
	
	void SetBattlePhase(ENUBattlePhase NewPhase);
	
	void AbortInitialize();
	
	const FNUBattleParticipant* FindParticipant(int32 CombatantId) const;
	FNUBattleParticipant* FindParticipant(int32 CombatantId);
	
	bool IsAlive(int32 CombatantId) const;
	bool HasLivingTeam(ENUBattleTeam Team) const;

	// 살아있는 플레이어 전투원이 있는지, 패배 판정 전용
	bool HasLivingPlayer() const;

	// 배경 세트가 자리를 정해두었으면 해당 값을, 없으면 아래 두 함수의 계산값을 씀
	FTransform GetAllySpawnTransform(int32 PartySlotIndex, int32 PartyCount, bool bIsSpirit) const;
	FTransform GetEnemySpawnTransform(int32 SlotIndex, int32 EnemyCount) const;

	// 세트가 정한 자리를 물어봄, 없으면 false 라 호출부가 기본 계산으로 처리함
	bool ResolveStageFormation(ENUBattleFormationRow Row, int32 SlotIndex, int32 SlotCount,
		FTransform& OutTransform) const;
	
	bool ValidateInitialParticipants(const TArray<FNUBattleParticipant>& InitialParticipants,
		const UNUNaiaDataSubsystem* NaiaDataSubsystem) const;
	
	TSubclassOf<ANUBattleAvatar> ResolveAllyAvatarClass(const AActor* ASCOwner, bool& bOutIsSpirit) const;
	
	// 전투원의 ASC, 없으면 nullptr
	UNUAbilitySystemComponent* FindCombatantASC(int32 CombatantId) const;

	// ASC에서 전투원 한 명의 Speed를 읽음, ASC가 없다면 0
	float GetCombatantSpeed(int32 CombatantId) const;

	// 이 전투원이 행동 순서를 맨 뒤로 밀리는 상태이상을 갖고 있는지 (둔화)
	bool HasForceLastAction(int32 CombatantId) const;
	
	// 이 전투원이 선제공격(2837)으로 맨 앞에 오는지, true면 그 자리에서 전투당 1회를 소모함
	// 묻는 것이 곧 소모라 const가 아님, 전투원마다 턴에 한 번만 불러야 함
	bool ConsumeFirstStrike(int32 CombatantId);
	
	// 행동 자체가 불가능한지 (기절), 확정 판정이라 몇 번 물어도 같은 답이 나옴
	bool HasActionBlockingStatus(int32 CombatantId) const;
	
	// 액티브 스킬을 쓸 수 없는지 (실명), 기본 공격은 가능함
	bool IsActiveSkillBlocked(int32 CombatantId) const;
	
	// 행동 실패 확률 판정 (마비), 확률이므로 행동 직전에 한 번만 불러야 함
	bool RollActionFailure(int32 CombatantId) const;

	// 환각 확률 판정, 마비보다 뒤에 불러야 함
	bool RollHallucination(int32 CombatantId) const;

	// 실행 중인 행동을 자기 진영 무작위 대상에게 하는 기본 공격으로 바꿈
	// 대상을 못 잡으면 false, 호출부가 행동을 취소해야 함
	bool ApplyHallucinationToCurrentAction(FNUBattleCommandRequest& InOutCommand);

	// 지금 실행 중인 행동이 이 전투원의 환각 행동인지
	// ResolveTargets 가 대상 정책을 자기 진영으로 덮을지 판단하는 데 사용함
	bool IsHallucinatedAction(int32 CombatantId) const;
	
	// 행동이 그냥 넘어가면 플레이어가 이유를 알 수 없어 아바타 위에 알림
	void ShowActionFailPopup(int32 CombatantId);
	
	float ResolveActionSpeed(const FNUBattleCommandRequest& Command);
	int32 ResolveActionPriority(const FNUBattleCommandRequest& Command);
	ENUTargetPolicy ResolveCommandTargetPolicy(const FNUBattleCommandRequest& Command) const;
	bool ResolveTargets(const FNUBattleParticipant& Source, ENUTargetPolicy Policy,
		int32 SelectedTargetId, TArray<int32>& OutTargetIds) const;
	bool ResolveActionContext(const FNUBattleCommandRequest& Command, FNUBattleActionContext& Out) const;
	
	void HandleCommandTimeOut();
	void FillDefaultCommand(const FNUBattleParticipant& Source);
	
	float GetTransitionSeconds() const;
	
	UNUBattleGameplayAbility* GetActiveBattleAbility() const;
	
	
	// -----------------------
	//		행동 비동기 처리
	
	// 어빌리티를 발동하고 완료를 대기 설정하는 함수
	// true일땐 진행, false일땐 return 하면 됨
	bool TriggerActionAbility(UNUAbilitySystemComponent* SourceASC, const FGameplayAbilitySpecHandle& AbilityHandle,
		const FGameplayEventData& EventData);
	
	void ScheduleNextAction();

	// 방금 발동한 어빌리티가 아직 실행 중인지
	// CommitAbility 실패로 즉시 끝난 경우, 어빌리티가 즉시 종료됨
	// 그러면 UnbindAbilityEnded() 에서 ActiveAbilityHandle를 Reset 처리하므로 현재 발동중인 어빌리티가 없는 상태가 됨
	bool IsActionAbilityRunning() const;

	// 명령에 필요한 SP를 검증하고 즉시 차감함
	// 같은 전투원의 이전 명령이 남아 있으면 먼저 환불함, 통과하면 AcceptedRequest.SPSpend 를 확정함
	bool ReserveSPForCommand(FNUBattleCommandRequest& AcceptedRequest);

	// 제출했던 명령에서 사용한 SP를 되돌림
	void RefundReservedSP(int32 CombatantId);

	// 행동이 끝난 전투원의 SP를 전량 회복함
	void RefillSPAfterAction(int32 ActionIndex);

	// 전투 시작 시 전원 SP 최대로 회복하고 시작
	void RefillSPForAll();

	// 전투원의 SP를 최대치로 되돌림, 죽었으면 아무것도 하지 않음
	void RefillSP(int32 CombatantId);
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void UnbindAbilityEnded();
	void HandleActionTimeout();
	
protected:
	// 전투 관리자 Id
	UPROPERTY(Replicated)
	FGuid EncounterId;
	
	// 현재 전투 페이즈
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ENUBattlePhase Phase = ENUBattlePhase::None;
	
	// 현재 턴
	UPROPERTY(ReplicatedUsing = OnRep_TurnNumber)
	int32 TurnNumber = 0;
	
	// 전투 참가자
	UPROPERTY(Replicated)
	TArray<FNUBattleParticipant> Participants;
	
	// 행동 순서
	UPROPERTY(Replicated)
	int32 ActionSequence = 0;
	
	// 플레이어들의 원본 아바타 액터
	UPROPERTY()
	TMap<int32, TObjectPtr<AActor>> OriginAvatars;

	// 적 전투원별 패턴 진행 상태 (쿨타임, 사용 횟수, 페이즈)
	// 서버만 쓰고 클라가 알 필요가 없어 복제하지 않음
	TMap<int32, FNUEnemyPatternRuntime> EnemyPatternRuntimes;

	// 포획한 나이아, 전투가 끝날 때 로스터에 등록함
	// 보상 계산이 처치와 구분할 때도 이 배열을 봄 (처치 카운팅 X, 포획 카운팅 O)
	// 서버만 쓰고 클라가 알 필요가 없어 복제하지 않음
	TArray<FNUPendingCapture> PendingCaptures;

	// 같은 대상에게 교감석을 던진 횟수, 성공하면 지움
	// 전투 안에서만 누적된다 - 전투를 넘겨 이어가려면 서버가 세야 함
	TMap<int32, int32> CaptureAttemptsByCombatant;

	// 이번 턴에 나이아 교체를 쓴 전투원
	// 교체가 SP를 채우므로 제한이 없으면 반복 교체가 SP 충전 꼼수가 됨
	// 서버만 쓰고 복제하지 않음, BeginCommandSelection 에서 턴마다 비움
	TSet<int32> NaiaSwappedCombatantIds;

	// 남은 SP 를 전부 쓰는 스킬(3203 나이아 개화)을 쓴 전투원, 그 라운드엔 행동 뒤 SP 가 안 참
	// 다음 명령 선택이 열릴 때 채우고 비움
	TSet<int32> DeferredSPRefillIds;

	// 같은 적의 사망 태그가 중복 통지되더라도 처치 이벤트는 한 번만 발행
	TSet<int32> ReportedDefeatedEnemyCombatants;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle> EnemyDeathDelegateHandles;
	
	UPROPERTY(ReplicatedUsing = OnRep_BattleResult)
	ENUBattleResult BattleResult = ENUBattleResult::None;

	// 결과창이 떠 있어 화면 전환을 붙잡아 둔 상태
	bool bResultScreenOpen = false;

	// 처치 보상을 이미 지급했는지
	// TryFinishByElimination 에 이미 끝났는지 확인이 없어 종료 경로가 두 번 들어올 수 있음
	bool bRewardsGranted = false;
	
	// 플레이어로부터 예약된 명령들
	TMap<int32, FNUBattleCommandRequest> PendingCommands;
	
	// 실제 속도 계산에 의해 실행될 명령들
	TArray<FNUResolvedAction> ActionQueue;
	
	int32 CurrentActionIndex = 0;
	int32 NextCombatantId = 0;
	
	bool bFleeRequested = false;

	// CheckBattleState 가 라운드 끝 처리를 도는 동안만 켜짐
	bool bAdvancingRound = false;
	bool bCompletionNotified = false;

	// 인정 연출이 도는 중, 타이머가 끝나면 승리로 끝냄
	bool bConcessionPending = false;
	FTimerHandle ConcessionTimer;

	// 인정 시퀀스를 다 튼 뒤 낮춘 자세로 머무는 시간
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle", meta = (ClampMin = "0.0"))
	float ConcessionHoldSeconds = 1.5f;

	// 미리 불러 둔 인정 시퀀스, 가비지 컬렉션에 안 걸리게 들고 있음
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPaperZDAnimSequence>> PreloadedConcessionAnimations;
	
	// 행동 선택 제한 시간, 초과 시 미제출 전투원에게 기본 공격을 할당
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle", meta = (ClampMin = "1"))
	float MaxCommandSeconds = 20.f;
	
	// 한 턴 시간제한 타이머
	FTimerHandle CommandLimitTimer;
	
	// 화면 전환 타이머
	FTimerHandle TransitionTimer;
	
	// 현재 재생해야 할 전환 연출
	UPROPERTY(ReplicatedUsing = OnRep_TransitionCue)
	ENUTransitionCue TransitionCue;

	// 암전(투명 -> 검정). Completion Mode = Keep State
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Transition")
	TSoftObjectPtr<ULevelSequence> FadeOutSequence;

	// 암전 해제(검정 -> 투명)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Transition")
	TSoftObjectPtr<ULevelSequence> FadeInSequence;
	
	// 시퀀스를 못읽으면 사용할 시간
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Transition", meta = (ClampMin = "0"))
	float TransitionFallbackSeconds = 0.5f;
	
	// 서버<->클라 지연이 있으니 서버가 조금 더 기다리는 시간
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Transition", meta = (ClampMin = "0"))
	float TransitionNetworkSlack = 0.15f;
	
	// 요청에서 받은 배경 서브레벨, 비어 있으면 배경을 바꾸지 않음
	// 랜덤은 인카운터 존, 심볼은 AFieldEnemy 가 각자 자기 것을 넘김
	TSoftObjectPtr<UWorld> BattleStageLevel;

	// 배경을 기다리기 시작한 시각, 음수면 아직 기다린 적 없음
	float StageWaitStartTime = -1.f;

	// 이 시간을 넘으면 배경 없이 전투를 시작함, 에셋 경로가 틀려도 멈추지 않게 하기 위함
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Stage", meta = (ClampMin = "0"))
	float MaxStageWaitSeconds = 5.f;

	// 배경을 다시 확인하는 주기
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Stage", meta = (ClampMin = "0.01"))
	float StageRetrySeconds = 0.1f;

	// 이 인카운터가 세트 조명을 맞바꾼 상태인지
	// 전투가 끝나도 복제 여유를 위해 LifeSpan 동안 살아 있어서, 그 사이에 다음 전투가 시작되면
	// 이 인카운터의 EndPlay 가 다음 전투의 조명을 되돌려 버림
	// 서브시스템 플래그는 맞바꿔져 있는지만 알고 누가 했는지를 모르므로 여기서 소유권을 들어야 함
	bool bStageLightingActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Battle|Camera")
	TObjectPtr<class UCameraComponent> BattleCamera;
	
	// 어빌리티 종료를 확인하기 위한 델리게이트 핸들
	FDelegateHandle AbilityEndedHandle;
	
	// 현재 실행중인 어빌리티의 ASC
	TWeakObjectPtr<UNUAbilitySystemComponent> ActiveAbilityASC;
	
	// 실행중인 어빌리티 Spec Handle
	FGameplayAbilitySpecHandle ActiveAbilityHandle;
	
	// 행동 종료 타이머
	FTimerHandle ActionTimeoutHandle;

	// 연출이 끝날 때까지 다음 행동을 미룰 때 사용 (포획 흔들림)
	FTimerHandle ActionDelayHandle;
	
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Action")
	float MaxActionSeconds = 10.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Formation")
	float ActionLineOffset = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Formation")
	float TargetApproachDistance = 120.f;

	// 스폰 직후에 구한 진형별 중심, 전진 방향을 여기서 유도함
	FVector AllyFormationCenter = FVector::ZeroVector;
	FVector EnemyFormationCenter = FVector::ZeroVector;
	bool bFormationCentersValid = false;

	// 임시 테스트 용도
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Avatar")
	TSubclassOf<ANUBattleAvatar> PlayerBattleAvatarClass;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Avatar")
	TSubclassOf<ANUBattleAvatar> SpiritBattleAvatarClass;
	
	UPROPERTY(EditAnywhere, Category = "NU|Battle|Enemy")
	TSubclassOf<class ANUBattleEnemyAvatar> EnemyClass;
};
