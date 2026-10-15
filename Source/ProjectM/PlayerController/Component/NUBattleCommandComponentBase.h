// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Components/ActorComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "NUBattleCommandComponentBase.generated.h"

class ANUBattleTargetIndicator;
class UNUBattleCommandComponentBase;
class UNUNaiaRosterComponent;
class UNUInventoryComponent;
struct FNUActiveSkillDefinition;
struct FNUBattleSkillInfo;

UENUM(BlueprintType)
enum class ENUCommandSelectionPhase : uint8
{
	Inactive,
	RootMenu,			// 공격/스킬/아이템/나이아 교체/도주 선택창
	SkillList,			// 스킬 목록 선택
	ItemList,			// 아이템 목록 선택
	NaiaList,			// 교체할 나이아 선택, 턴을 소비하지 않는 자유 행동
	TargetSelection,	// 단일 대상 선택
	Submitting,			// 서버 응답 대기
	WaitingForOthers	// 모든 명령 제출 후 다른 사람의 제출 대기
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUCommandSelectionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUCommandSubmissionFailed, ENUBattleCommandSubmitResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUBattleActionUnavailable, ENUBattleRootAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUNaiaSwapFailed, ENUNaiaSwapResult, Result);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FNUOnBattleCommandSessionChanged, UNUBattleCommandComponentBase*, bool);

struct FNUCommandSPRule
{
	bool bRepeatable = false;
	int32 FixedCost = 0;
	int32 CostPerHit = 0;
	int32 FixedHitCount = 1;
};

// 로컬/넷 전투가 공유하는 명령 선택 컴포넌트
// 전투 데이터 조회와 실제 제출만 파생 컴포넌트가 구현함
UCLASS(Abstract, ClassGroup=(Custom))
class PROJECTM_API UNUBattleCommandComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUBattleCommandComponentBase();

	void SetSessionActive(bool bActive);
	bool IsSessionActive() const { return bSessionActive; }

	void BeginCommandSelection(int32 TurnNumber);
	void HandleSubmitAccepted();
	void HandleSubmitRejected(ENUBattleCommandSubmitResult Result);
	void HandleNaiaSwapResult(ENUNaiaSwapResult Result);

	
	/* UI 입력 */
	
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void NavigateSelection(const FVector2D& Navigation);

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void ConfirmSelection();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectRootAction(ENUBattleRootAction Action);

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectPreviousTarget();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectNextTarget();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectNextItem();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectPreviousItem();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void AdjustSPSpend(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void ConfirmTarget();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void CancelSelection();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void ConfirmItem();

	
	/* 스킬 목록, 아이템 목록과 같은 방식으로 순회함 */
	
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectNextSkill();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectPreviousSkill();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void ConfirmSkill();

	
	/* 나이아 교체, 턴을 소비하지 않는 자유 행동 */
	
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void BeginNaiaSelection();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectNextNaia();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectPreviousNaia();

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void ConfirmNaia();

	
	/* UI 조회 */
	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	virtual FText GetAttackDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	virtual FText GetSkillDisplayName() const;
	
	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	ENUCommandSelectionPhase GetSelectionPhase() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	ENUBattleRootAction GetSelectedRootAction() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	int32 GetSelectionTurnNumber() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	int32 GetCurrentSourceCombatantId() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	AActor* GetCurrentSourceAvatar() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	int32 GetCurrentTargetCombatantId() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	AActor* GetCurrentTargetAvatar() const;

	// 캐릭터 닉네임·나이아 이름
	// 로컬/넷의 출처가 달라 파생 클래스가 정함
	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetCombatantName(int32 CombatantId) const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetCurrentSourceName() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetCurrentTargetName() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	TArray<int32> GetSelectableTargetIds() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	bool IsRootActionAvailable(ENUBattleRootAction Action) const;

	// 아직 서버에 보내지 않은 투입 SP, 게이지에 미리 반영해 보여주는 용도
	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	int32 GetPendingSPSpend() const;

	// 지금 투입량으로 나올 타수와, 현재 SP로 낼 수 있는 최대 타수
	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	int32 GetPendingHitCount() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	int32 GetMaxHitCount() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetCurrentItemDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetItemPositionText() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetCurrentSkillDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetSkillPositionText() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetCurrentSkillDescription() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetCurrentNaiaDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	FText GetNaiaPositionText() const;

	void SetTargetIndicatorClass(TSubclassOf<ANUBattleTargetIndicator> InTargetIndicatorClass);
	void RefreshTargetIndicator();
	void HideTargetIndicator();
	void DestroyTargetIndicator();

public:
	UPROPERTY(BlueprintAssignable, Category = "NU|Battle|Command")
	FOnNUCommandSelectionChanged OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "NU|Battle|Command")
	FOnNUCommandSubmissionFailed OnSubmissionFailed;

	UPROPERTY(BlueprintAssignable, Category = "NU|Battle|Command")
	FOnNUBattleActionUnavailable OnActionUnavailable;

	UPROPERTY(BlueprintAssignable, Category = "NU|Battle|Command")
	FOnNUNaiaSwapFailed OnNaiaSwapFailed;

	FNUOnBattleCommandSessionChanged OnBattleCommandSessionChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool IsBattleActive() const { return false; }
	virtual void RebuildControllableCombatants() {}
	virtual void CollectLivingCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const {}

	// 쓰러진 전투원만 부활 아이템의 대상, 비워 두면 부활 아이템이 목록에 안 뜬다
	virtual void CollectDownedCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const {}
	virtual AActor* GetCombatantAvatar(int32 CombatantId) const { return nullptr; }

	// 이 전투원의 표시용 이름. 못 찾으면 빈 FText를 주고, 번호로 대신 띄우는 것은 GetCombatantName이 맡는다
	virtual FText GetCombatantDisplayName(int32 CombatantId) const { return FText::GetEmpty(); }
	virtual float GetCombatantSP(int32 CombatantId) const { return 0.f; }
	virtual bool IsPlayerCombatant(int32 CombatantId) const { return false; }
	virtual void CollectOwnedActiveSkillIds(int32 CombatantId, TArray<int32>& OutSkillIds) const {}
	// 스킬을 쓸 수 있는지를 서버가 정하는 전투인지, 참이면 메뉴가 로컬 표로 못 돈다 · 받을 아군이 없다 · 전투당 횟수를 다 썼다로 거르지 않음
	// 누르면 서버가 거절하고, 그 사유는 로그로 남음
	virtual bool IsSkillAvailabilityServerDriven() const { return false; }
	virtual int32 ApplyStatusSPCost(int32 CombatantId, int32 BaseCost) const { return BaseCost; }
	// 스킬의 SP 소모량, 카드 같은 스킬 비용 보정까지 — 기본은 상태이상 보정과 같음
	virtual int32 ApplySkillSPCost(int32 CombatantId, int32 BaseCost) const { return ApplyStatusSPCost(CombatantId, BaseCost); }

	// 이번 전투에서 이 스킬을 쓴 횟수, 한도가 있는 스킬을 메뉴에서 뺄 때 씀
	// 넷 전투는 서버가 판정하고 클라는 세지 않아 0 (한도 없음과 같음)
	virtual int32 GetActiveSkillUseCount(int32 CombatantId, int32 SkillId) const { return 0; }
	virtual void CollectBattleUsableItems(TArray<FNUItemInstance>& OutItems) const;
	virtual bool GetPendingSPRule(FNUCommandSPRule& OutRule) const { return false; }
	virtual bool HasSkillActionOverride() const { return false; }
	virtual bool IsSkillActionOverrideAvailable() const { return false; }
	virtual ENUTargetPolicy GetSkillActionOverrideTargetPolicy() const { return ENUTargetPolicy::Self; }
	virtual bool SubmitPendingCommand(int32 SelectedTargetId) { return false; }
	virtual void RequestNaiaSwap(int32 PartySlot) {}

	// 명령 제출로 선택한 아이템을 이번 명령 단계 동안 예약해 둠
	// 소모는 액션 큐 재생 시점이라 제출만 한 아이템도 인벤에 그대로 남아 있어서,
	// 예약하지 않으면 1개 남은 포션을 두 전투원이 같이 고를 수 있음
	void ReserveSubmittedItem();
	void ClearItemReservations();

	// 예약한 수량만큼 목록의 개수를 깎고, 남지 않은 칸은 아예 빼 버림
	void ApplyItemReservations(TArray<FNUItemInstance>& OutItems) const;


	void OpenRootMenuForCurrentCombatant();
	void AdvanceToNextCombatant();
	void ResetSelection();
	void SetSelectionPhase(ENUCommandSelectionPhase NewPhase);
	void BroadcastSelectionChanged();

	APlayerState* GetLocalPlayerState() const;
	UNUInventoryComponent* GetInventoryComponent() const;
	UNUNaiaRosterComponent* GetNaiaRosterComponent() const;
	const FNUActiveSkillDefinition* FindSkillDefinition(int32 SkillId) const;
	bool TryGetSkillInfo(int32 SkillId, FNUBattleSkillInfo& OutInfo) const;

	// 나이아 정의의 표시 이름, 정의가 없거나 비었으면 빈 FText
	FText FindNaiaDisplayName(int32 NaiaId) const;

	// 지금 고른 캐릭터의 닉네임, 머리 위 이름표·정보창과 같은 출처
	FText GetPlayerCharacterDisplayName() const;

	int32 GetCurrentSkillId() const;
	float GetCurrentSourceSP() const;
	int32 GetAdjustedSkillSPCost(int32 SkillId) const;

	// 스킬 한 번의 SP 비용 — 남은 SP 를 전부 쓰는 스킬(3203 나이아 개화)은 지금 SP, 나머지는 상태이상 · 카드 보정을 거친 표 값
	// 목록 라벨과 예정 소모량이 같은 값을 봐야 해서 둘 다 여기를 부름
	int32 ResolveSkillSPCost(int32 SkillId, int32 TableCost) const;
	int32 GetAdjustedSPCostPerHit() const;

	void PushPendingSPSpendToAvatar();
	void ClearPendingSPSpend();

	void MoveRootSelection(int32 Direction);
	ENUBattleRootAction FindFirstAvailableRootAction() const;

	// 선택 시작
	// bDownedTargets 면 살아 있는 대상 대신 쓰러진 대상을 모은다 (부활)
	void BeginTargetSelection(ENUTargetPolicy Policy, bool bDownedTargets = false);
	void BeginAttackTargetSelection();
	void BeginItemSelection();
	void BeginItemTargetSelection(const FNUItemInstance& Item);
	
	// 스킬 목록을 열고, 고른 스킬로 대상 선택에 들어감
	// SkillId로 발동할 스킬 구분함
	void BeginSkillSelection();
	void BeginSkillTargetSelection(int32 SkillId);
	
	// 명령 제출
	void SubmitCurrentCommand(int32 SelectedTargetId);

	void CollectUsableSkills(TArray<int32>& OutSkillIds) const;

	// 아군에게 거는 직업 스킬을 받을 사람이 지금 있는지, 솔로에서는 없어서 메뉴에서 뺌
	bool HasJobSkillRecipient(int32 SkillId) const;

	// 전투당 횟수를 다 쓴 스킬인지 (3310 역방향 · 3328 대역전 · 3329 은자의 등불), 목록을 만들 때 가볍게 물을 수 있어야 함
	bool IsSkillUsesExhausted(const FNUBattleSkillInfo& Info) const;

	// 로컬은 GAS 사망 태그로 쓰러진 나이아를 가려내고, 넷은 태그가 없어 따로 기억한 것을 더한다
	virtual void CollectSwappableNaiaSlots(TArray<int32>& OutSlots) const;

	static bool RequiresTargetSelection(ENUTargetPolicy Policy);
	static ENUBattleTeam GetPolicyTeam(ENUTargetPolicy Policy);

protected:
	UPROPERTY(BlueprintReadOnly)
	ENUCommandSelectionPhase SelectionPhase = ENUCommandSelectionPhase::Inactive;

	UPROPERTY(BlueprintReadOnly)
	ENUBattleRootAction SelectedRootAction = ENUBattleRootAction::Attack;

	UPROPERTY(BlueprintReadOnly)
	int32 SelectionTurnNumber = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<int32> ControllableCombatantIds;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentCombatantIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	TArray<int32> SelectableTargetIds;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentTargetIndex = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Targeting")
	TSubclassOf<ANUBattleTargetIndicator> TargetIndicatorClass;

	UPROPERTY(Transient)
	TObjectPtr<ANUBattleTargetIndicator> TargetIndicator;

	UPROPERTY(BlueprintReadOnly)
	TArray<FNUItemInstance> SelectableItems;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentItemIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	TArray<int32> SelectableSkillIds;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentSkillIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	TArray<int32> SelectableNaiaSlots;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentNaiaIndex = 0;

	int32 LastNaiaSwapTurn = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 PendingSPSpend = 0;

	ENUBattleRootAction PendingRootAction = ENUBattleRootAction::Attack;
	int32 PendingSubmissionSourceId = INDEX_NONE;
	int32 LastPresentedSourceCombatantId = INDEX_NONE;
	FGuid PendingItemInstanceId;
	int32 PendingSkillId = 0;

	// 지금 고르는 대상이 쓰러진 전투원인지 (부활). 대상 표시가 숨은 아바타를 가려야 해서 들고 있음
	bool bTargetsDowned = false;

	// 이번 명령 단계에서 이미 제출된 아이템 수량 (행 이름 => 개수)
	// 실제 차감은 서버(넷)와 재생 큐가 하고, 이 값은 같은 턴의 다음 전투원에게만 보임
	TMap<FName, int32> ReservedItemCounts;

	// 위 예약이 어느 턴 것인지. SelectionTurnNumber 는 "다른 전투원 대기" 상태에서도
	// 갱신되므로 그 값만 보고 턴이 바뀐 것을 판단하면 지난 턴 예약이 남는다
	int32 ReservedItemTurn = INDEX_NONE;
	bool bSessionActive = false;
};
