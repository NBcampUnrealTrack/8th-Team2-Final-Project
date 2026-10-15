// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Battle/Net/NUBattleServerTypes.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "NUBattleNetActionQueueSubsystem.generated.h"

class ANUBattleAvatar;
class ANUPlayerController;
class UNUBattleNetStageSubsystem;
class UNUBattleNetSubsystem;
class UNUInventoryComponent;
class UNUProgressionComponent;
struct FNUItemDefRow;

struct FNUNetQueuedAction
{
	int32 ActorId = INDEX_NONE;

	// 턴 안에서 1부터, 같은 값이 또 오면 새 행동이 아니라 이어지는 조각
	// 반응 창(SVR_Battle_Reaction)이 이 번호로 어느 행동의 창인지 가리킴
	int32 ActionSeq = 0;

	TArray<FNUServerBattleEvent> Events;
	bool bMotionless = false;
};

// 같은 타격에 열린 반응 창 묶음
struct FNUNetReactionGroup
{
	TArray<FNUServerBattleReactionWindow> Windows;

	// 행동이 시작된 뒤 이 창이 열려야 하는 시각, 도전 구간(바가 보이기 시작하는 지점)의 시작 시각
	float OpenDelay = 0.f;

	bool bParryable = false;
	bool bMustReport = false;
};

// 재생에 필요한 전투원 정보를 큐가 따로 들고 있는 사본
// 서버는 전투 종료를 알린 직후 UNUBattleNetSubsystem의 전투원 목록을 비우는데,
// 마지막 행동 연출은 그 뒤에도 재생돼야 해서 미러 상태에 기대면 안 됨
struct FNUNetCombatantSnapshot
{
	// 0 아군, 1 적
	int32 Side = 0;

	// 0 캐릭터, 1 나이아, 2 적. 회피 확률을 어디서 얻을지 이 값으로 갈림
	int32 Kind = 0;

	// 포획한 나이아 이름을 결과창에 띄우는 데 씀
	// 서버 미러는 전투가 끝나면 비는데 포획 연출은 그 뒤에 재생되므로 사본이 들고 있어야 함
	int32 NaiaId = 0;

	// 이벤트를 재생하면서만 갱신하는 표시용 값, 서버 스냅샷을 그대로 따라가지 않음
	int32 Health = 0;
	int32 SP = 0;

	int32 MaxHealth = 0;
	int32 MaxSP = 0;

	// 위와 같은 이유로 들고 있는 상태이상 사본
	// 서버 미러를 바로 그리면 아직 재생 안 한 타격의 화상이 미리 붙음
	TArray<FNUServerBattleStatus> Statuses;

	// 직업 자원 · 상태 사본, 위와 같은 이유로 이벤트가 재생되는 시점에 갱신함
	// 서버 미러를 바로 그리면 아직 안 맞은 타격의 템포가 미리 오르고 번아웃이 미리 뜸
	TArray<FNUServerJobResource> Resources;
	TArray<FNUServerJobState> JobStates;
};

// 큐에 남은 행동 연출이 다 재생됐다고 알리는 델리게이트
DECLARE_MULTICAST_DELEGATE(FNUOnBattleNetPlaybackDrained);

// 서버가 확정한 이벤트를 화면 연출 속도에 맞춰 한 행동씩 재생함
UCLASS()
class PROJECTM_API UNUBattleNetActionQueueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 재생하거나 재생을 기다리는 행동이 남아 있는지, 전투 종료를 미룰지 판단할 때 물어봄
	bool IsPlaybackBusy() const;

	// 넷 전투의 반응 창이 지금 열려 있는지, 패링 입력을 어디로 보낼지 가르는 데 씀
	bool IsReactionWindowOpen() const;

	// 창이 열려 있는 동안의 패링 입력, PressOffset 은 창이 열린 뒤의 경과(초)
	void HandleReactionPress(float PressOffset);

	// 결과창을 확인해서 닫았음, 붙잡아 둔 무대 철거를 여기서 이어감
	void NotifyResultDismissed();

	// 창이 열려 있는 동안의 회피 입력
	// 회피는 타이밍이 아니라 확률이라 경과를 판정에 쓰지 않고 서버 기록용으로만 실어 보냄
	void HandleDodgePress();

	// 명령이 승인된 시점에 서버가 이미 확정한 SP를 바로 반영함
	// 실제 SPChanged 이벤트는 그 행동이 재생될 때만 오므로, 승인 직후 예정 SP 표시가 걷히면
	// 아직 안 깎인 예전 SP로 잠깐 되돌아가 보이는 문제를 막기 위해 명령 컴포넌트가 호출함
	void ApplySP(int32 CombatantId, int32 SP);
public:
	// 명령 컴포넌트에서 바인딩 해서, 다음 턴 메뉴를 재생이 연출이 끝난 뒤에만 열도록 함
	FNUOnBattleNetPlaybackDrained OnPlaybackDrained;

private:
	void HandleBattleStateChanged();

	// 전투원 명단이 통째로 갈렸을 때(나이아 교체·재동기화) 표시용 스냅샷을 새 명단으로 다시 만듦
	// 안 하면 교체로 들어온 전투원의 번호가 없어서 ApplyHealth/ApplySP가 조용히 무시됨
	void HandleCombatantsReplaced();
	void HandleActionReceived(int32 ActorId, int32 ActionSeq, const TArray<FNUServerBattleEvent>& Events);
	void HandleTurnEndReceived(int32 Turn, const TArray<FNUServerBattleEvent>& Events);
	void HandleBattleEnded(int32 Reason);

	void TryPlayNext();
	void PlayAction(FNUNetQueuedAction&& Action);
	void PlayNextHitSlot();
	void FinishCurrentAction();
	void ResetQueue();

	// 미뤄둔 전투 종료를 실제로 처리함, 큐가 다 비었거나 한쪽이 전멸한 뒤에만 불림
	void FinishBattlePlayback(int32 Reason);

	// 서버가 이미 지급한 보상을 적용함
	void ApplyServerBattleRewards();

	// 결과창에 넘길 값을 모아 띄움, 큐를 비우기 전에 불러야 포획 기록이 남아 있음
	void ShowBattleResult(int32 Reason);

	// 무대 철거를 실제로 시작함, 결과창이 떠 있으면 닫힐 때까지 미룸
	void ContinueTeardown();

	// 지금까지 재생한 이벤트 기준으로 한쪽 진영이 전멸했는지
	// 종료 통지 뒤 남은 행동을 버릴지 판단하는 데만 씀
	bool IsSideWiped() const;

	void BuildHitSlots(const TArray<FNUServerBattleEvent>& Events, TArray<TArray<FNUServerBattleEvent>>& OutHitSlots) const;
	FNUBattleActionMotionSpec BuildMotionSpec(const FNUNetQueuedAction& Action, int32 HitCount) const;
	FVector ResolveActionLocation(int32 ActorId, int32 TargetId, ENUBattleActionMotion Motion) const;
	int32 ResolveSkillId(const TArray<FNUServerBattleEvent>& Events) const;

	// 행동의 종류를 드러내는 이벤트(포획·도주·아이템)를 찾음, 없으면 공격
	const FNUServerBattleEvent* FindActionKindEvent(const TArray<FNUServerBattleEvent>& Events) const;

	// 서버가 보낸 숫자 item_id 로 아이템 정의를 찾음, 배너 이름과 소모 반영이 이걸로 돔
	// 인벤토리에 그 아이템이 없어도 정의는 표에 있으므로 이름은 항상 나옴
	const FNUItemDefRow* FindItemDefinition(int32 ItemId) const;

	// 성공한 아이템 사용·포획으로 아이템이 실제로 하나 줄어든 것을 인벤토리에 반영함
	void NotifyItemConsumed(int32 ItemId) const;

	UNUInventoryComponent* GetLocalInventory() const;
	UNUProgressionComponent* GetLocalProgression() const;

	// 캐릭터 회피율의 출처인 서버 스탯 스냅샷을 아직 안 받았으면 받아둠
	void EnsureCharacterStatSnapshot() const;

	// 전투원 하나의 회피 확률
	// 캐릭터는 서버가 캐릭터 스탯으로 준 DodgeRate 를 쓰고, 나이아는 아직 값이 없어 기본값을 사용함
	float ResolveDodgeChance(int32 CombatantId) const;

	// 로컬처럼 행동을 시작할 때 무슨 행동인지 배너를 띄움
	// 로컬은 명령과 어빌리티에서 이름을 얻지만 넷에는 둘 다 없어 이벤트로 종류를 가림
	void ShowActionBannerFor(const FNUNetQueuedAction& Action, int32 HitCount) const;
	int32 ResolveTargetId(const TArray<FNUServerBattleEvent>& Events) const;

	void ApplyEvent(const FNUServerBattleEvent& Event);

	// 이 행동에 열릴 반응 창을 서버에서 받아 타격별로 묶고 첫 창을 예약함
	// 창은 그 행동의 SVR_Battle_Action 보다 먼저 도착해 있으므로 여기서 꺼내 쓰면 됨
	void ScheduleReactionWindows(float FirstHitDelay, float HitInterval);
	void ScheduleNextReactionOpen();
	void OpenNextReactionWindow();
	void CloseReactionWindow();

	// 열려 있던 묶음의 결과를 창마다 보고함, bPressed 가 거짓이면 무입력(-1)
	void ReportReactionGroup(bool bPressed, bool bTimingSuccess, float PressOffset);

	// 패링/회피 성공으로 돌아오는 환급량, 반올림
	// 서버는 실수, 클라는 정수를 기준치로 삼는 차이가 있어서 이후에 맞추는것이 필요함
	int32 ResolveReactionRefund(int32 Damage) const;

	// 이미 들어간 피해에 환급을 얹어 표시를 되돌림, 판정이 타격보다 늦게 들어온 경우
	void CreditReactionRefund(int32 CombatantId, int32 Damage);

	void AdvanceReactionGroup();
	void ClearReactionWindows();

	// 미뤄 둔 반응 결과(PendingReactionPopups)를 지금 띄움, 행동이 끝날 때까지 타격이 안 온 방어자용
	void FlushReactionPopups();

	// 아직 보고하지 않은 must_report 창이 남아 있는지, 행동을 끝낼지 판단할 때 씀
	bool HasUnreportedMustReport() const;

	// 로컬 전투가 GE 에 달아 두고 쓰는 GameplayCue 를 넷 전투에서도 그대로 재생함
	// 넷 아바타에는 ASC 가 없지만 GameplayCueManager 는 대상 액터만 받으므로 부를 수 있음
	void PlayBattleCue(FGameplayTag CueTag, int32 TargetId, int32 SourceId, int32 Magnitude) const;
	void ApplyHealth(int32 CombatantId, int32 Health);

	// 서버가 보낸 체력을 반영함
	// 서버의 절대값은 전부 환급 전 기준이라, 이번 턴에 클라가 미리 반영한 환급을 얹어 넣어야함
	// 그냥 넣으면 그 턴의 회복·추가 타격이 올 때마다 환급이 사라졌다 돌아옴
	void ApplyServerHealth(int32 CombatantId, int32 Health);

	// 표시용 사본의 상태이상을 아이콘 뷰로 바꿔 아바타에 반영함
	void PushStatusViews(int32 CombatantId) const;

	// 직업 자원 · 상태 사본을 체력바에 그림, PushStatusViews 와 같은 이유로 미러가 아니라 재생 큐의 사본을 그림
	void PushJobResourceViews(int32 CombatantId) const;
	void PushAllJobResourceViews() const;
	void ShowPopup(int32 CombatantId, const FNUCombatPopupData& PopupData) const;
	void InitializeCombatantSnapshots();
	void RefreshLatestVitals();

	ANUPlayerController* GetLocalPlayerController() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNUBattleNetSubsystem> BattleNet = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNUBattleNetStageSubsystem> StageSubsystem = nullptr;

	TArray<FNUNetQueuedAction> PendingActions;
	FNUNetQueuedAction CurrentAction;
	TArray<TArray<FNUServerBattleEvent>> ActiveHitSlots;
	TMap<int32, FNUNetCombatantSnapshot> CombatantSnapshots;

	// 직업 자원 · 상태를 마지막으로 그린 턴
	// 상태는 턴 번호만으로 대기 → 켜짐 · 남은 턴이 바뀌므로 턴이 바뀌면 이벤트가 없어도 다시 그림
	int32 JobViewTurn = INDEX_NONE;

	int32 NextHitSlotIndex = 0;
	float ActiveHitInterval = 0.f;
	bool bIsPlaying = false;

	// 서버가 전투 종료를 알렸지만 재생할 행동이 남아 있는 상태
	// 서버는 처치 행동과 종료를 거의 같이 보내서, 바로 정리하면 마지막 타격이 화면에 안 나옴
	bool bEndPending = false;
	int32 PendingEndReason = 0;

	// 방금 재생한 행동 연출 종료(도트·만료)의 turn, 큐가 비는 시점에 이 값으로만 BattleActionAck를 보냄
	// 행동 하나가 끝나 큐가 잠깐 비는 것과 턴 종료까지 다 끝난 것을 구분해야 함 - 아무 때나
	// 보내면 서버가 기다리는 turn과 달라 거절(1905)되거나, 아직 안 끝난 턴을 끝났다고 보고하게 됨
	int32 PendingAckTurn = INDEX_NONE;

	// 이 행동의 반응 창 묶음들, 타격 순서대로
	TArray<FNUNetReactionGroup> ReactionGroups;
	int32 NextReactionGroupIndex = 0;

	// 지금 열려 있는 묶음, INDEX_NONE 이면 열린 창이 없음
	int32 OpenReactionGroupIndex = INDEX_NONE;

	// 반응 창의 시각을 재는 기준, 행동 연출이 시작된 시각
	float ReactionActionStartTime = 0.f;

	// 이번 전투에서 포획에 성공한 나이아 테이블의 id, 같은 종을 여러 마리 잡으면 그만큼 쌓임
	TArray<int32> CapturedNaiaIds;

	// 결과창이 떠 있어 무대 철거를 붙잡아 둔 상태
	bool bResultScreenOpen = false;

	// 지금 열려 있는 창이 열린 시각, 회피 입력의 경과를 재는 데 씀
	// 패링은 UNUParryComponent 가 따로 재므로 사용 X
	float ReactionWindowOpenTime = 0.f;

	// 클라가 성공으로 판정했지만 아직 그 타격이 재생되지 않은 전투원
	// 서버는 전액 피해를 먼저 보내고 환급을 모든 연출이 끝난 뒤에 보내서, 그대로 두면
	// 체력이 다 깎인 다음에 다시 차오르는 것처럼 보임. 그 사이를 클라 판정으로 미리 메움
	TSet<int32> PendingReactionDefenders;

	// 그 판정이 회피였는지, 회피는 전액 무효고 패링은 절반
	// 한 묶음의 창은 종류가 같아서 묶음마다 하나면 충분함
	bool bPendingReactionFullAvoid = false;

	// 이번 턴에 클라가 미리 반영해 둔 환급량
	TMap<int32, int32> PredictedRefundOffset;

	// 창이 열려 있는 동안 전투원별로 실제 적용한 피해량
	// 판정이 타격보다 늦게 들어왔을 때 환급을 얼마 얹을지 알아야 해서 들고 있음
	TMap<int32, int32> CurrentWindowDamage;

	// 판정은 했지만 그 방어자에게 아직 타격이 안 들어와 미뤄 둔 반응 결과 팝업 (패링 · 회피 성공 · 실패 · SP 부족)
	// 로컬 전투처럼 타격 순간에 띄워야 패링 성공 연출(히트스톱 · 버스트)이 공격이 닿는 순간에 나옴
	TMap<int32, FNUCombatPopupData> PendingReactionPopups;

	FTimerHandle HitTimerHandle;
	FTimerHandle FinishTimerHandle;
	FTimerHandle ReactionOpenTimerHandle;
	FTimerHandle ReactionCloseTimerHandle;
};
