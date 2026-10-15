// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleCommandComponentBase.h"
#include "NUBattleNetCommandComponent.generated.h"

class UNUBattleNetActionQueueSubsystem;
class UNUBattleNetCommandSubsystem;
class UNUBattleNetStageSubsystem;
class UNUBattleNetSubsystem;

// 넷 서버 전투용
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUBattleNetCommandComponent : public UNUBattleCommandComponentBase
{
	GENERATED_BODY()

public:
	UNUBattleNetCommandComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool IsBattleActive() const override;
	virtual void RebuildControllableCombatants() override;
	virtual void CollectLivingCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const override;
	virtual void CollectDownedCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const override;
	virtual void CollectSwappableNaiaSlots(TArray<int32>& OutSlots) const override;
	virtual AActor* GetCombatantAvatar(int32 CombatantId) const override;
	virtual FText GetCombatantDisplayName(int32 CombatantId) const override;
	virtual float GetCombatantSP(int32 CombatantId) const override;
	virtual bool IsPlayerCombatant(int32 CombatantId) const override;
	virtual void CollectOwnedActiveSkillIds(int32 CombatantId, TArray<int32>& OutSkillIds) const override;
	virtual bool IsSkillAvailabilityServerDriven() const override { return true; }
	virtual int32 ApplyStatusSPCost(int32 CombatantId, int32 BaseCost) const override;
	virtual bool GetPendingSPRule(FNUCommandSPRule& OutRule) const override;
	virtual bool SubmitPendingCommand(int32 SelectedTargetId) override;
	virtual void RequestNaiaSwap(int32 PartySlot) override;

private:
	// 스킬 목록이 비는 사유를 로그로 남김, 같은 전투원의 같은 사유는 한 번만 찍음
	// IsRootActionAvailable(Skill) 이 UI 폴링으로 매 프레임 불릴 수 있어 그대로 찍으면 로그가 묻힘
	void ReportSkillListDiagnostic(int32 CombatantId, int32 ReasonCode, const FString& Detail) const;

	// 이번 전투에서 쓰러진 나이아를 기억함
	// 교체로 전투에서 빠지면 서버 전투원 목록에서도 사라져 쓰러졌던 사실을 알 길이 없어짐
	// 그대로 두면 교체 목록에 다시 뜨고, 내면 서버가 1908 로 거절해 미리 차단함
	void RecordDownedNaia();

	// 이번 전투에서 쓰러진 나이아 인스턴스, 전투가 끝나면 비움
	TSet<int64> DownedNaiaInstanceIds;

	UNUBattleNetSubsystem* GetBattleNet() const;
	UNUBattleNetCommandSubsystem* GetCommandNet() const;
	UNUBattleNetStageSubsystem* GetStageNet() const;
	UNUBattleNetActionQueueSubsystem* GetActionQueueNet() const;

	void EvaluateSession();

	// 서버 상태를 로컬 선택 UI에 반영함, 이전 턴 행동이 아직 재생 중이면 미루고,
	// 행동 연출 재생이 끝난 뒤 다시 호출함
	// (SVR_Battle_TurnBegin이 화면 연출보다 먼저 와서, 안 그러면 메뉴가 이전 턴 타격 도중에 뜸)
	void SyncCommandSelection();

	void HandleBattleStateChanged();
	void HandleStageReadyChanged(bool bReady);
	void HandleCommandTurnChanged(int32 CombatantId);
	void HandleCommandRejected(int32 CombatantId, ENUBattleCommandSubmitResult Result);
	void HandlePlaybackDrained();

private:
	// ReportSkillListDiagnostic 의 중복 억제용, 마지막으로 찍은 전투원과 사유
	mutable int32 LastSkillDiagnosticCombatantId = INDEX_NONE;
	mutable int32 LastSkillDiagnosticReason = INDEX_NONE;
};

