// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleCommandComponentBase.h"
#include "NUBattleCommandComponent.generated.h"

class ANUBattleEncounter;

// 기존 UE 서버 권위 전투용
// 클래스와 기본 서브오브젝트 이름은 기존 PlayerController BP 호환을 위해 유지
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUBattleCommandComponent : public UNUBattleCommandComponentBase
{
	GENERATED_BODY()

public:
	UNUBattleCommandComponent();

	void SetEncounter(ANUBattleEncounter* NewEncounter);
	virtual FText GetAttackDisplayName() const override;
	virtual FText GetSkillDisplayName() const override;

	void HandleSubmissionResult(
		FGuid EncounterId,
		int32 TurnNumber,
		int32 SourceCombatantId,
		ENUBattleCommandSubmitResult Result);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool IsBattleActive() const override;
	virtual void RebuildControllableCombatants() override;
	virtual void CollectLivingCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const override;
	virtual void CollectDownedCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const override;
	virtual AActor* GetCombatantAvatar(int32 CombatantId) const override;
	virtual FText GetCombatantDisplayName(int32 CombatantId) const override;
	virtual float GetCombatantSP(int32 CombatantId) const override;
	virtual bool IsPlayerCombatant(int32 CombatantId) const override;
	virtual void CollectOwnedActiveSkillIds(int32 CombatantId, TArray<int32>& OutSkillIds) const override;
	virtual int32 ApplyStatusSPCost(int32 CombatantId, int32 BaseCost) const override;
	virtual int32 ApplySkillSPCost(int32 CombatantId, int32 BaseCost) const override;
	virtual int32 GetActiveSkillUseCount(int32 CombatantId, int32 SkillId) const override;
	virtual void CollectBattleUsableItems(TArray<FNUItemInstance>& OutItems) const override;
	virtual bool GetPendingSPRule(FNUCommandSPRule& OutRule) const override;
	virtual bool HasSkillActionOverride() const override;
	virtual bool IsSkillActionOverrideAvailable() const override;
	virtual ENUTargetPolicy GetSkillActionOverrideTargetPolicy() const override;
	FGameplayTag ResolveSkillAbilityId() const;
	virtual bool SubmitPendingCommand(int32 SelectedTargetId) override;
	virtual void RequestNaiaSwap(int32 PartySlot) override;

private:
	void BindEncounter();
	void UnbindEncounter();
	void HandleBattlePhaseChanged(ENUBattlePhase NewPhase, int32 NewTurnNumber);

	bool GetParticipant(int32 CombatantId, FNUBattleParticipant& OutParticipant) const;
	FGameplayTag ResolveAttackAbilityId() const;
	const class UNUPlayerBattleAbility* FindPendingAbility() const;
	const class UNUAbilitySystemComponent* GetCombatantASC(int32 CombatantId) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ANUBattleEncounter> Encounter;
};
