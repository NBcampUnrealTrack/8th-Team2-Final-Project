// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "NUGAS/Interface/NUCombatantInterface.h"
#include "GameFramework/PlayerState.h"
#include "NUPlayerState.generated.h"

class UNUNaiaRosterComponent;
class ANUSpiritState;
class ANUBattleEncounter;
class UNUAbilitySystemComponent;
class UNUEquipmentComponent;
class UNUInventoryComponent;
class UNUJobComponent;
class UNUPlayerSkillComponent;
class UNUProgressionComponent;
class UNUQuestComponent;
class UNUWorldLocationComponent;
class UNURecoveryComponent;

UCLASS()
class PROJECTM_API ANUPlayerState : public APlayerState, public IAbilitySystemInterface, public INUCombatantInterface
{
	GENERATED_BODY()
	
public:
	ANUPlayerState();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UNUAbilitySystemComponent* GetNUASC() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 초기 어빌리티 부여
	void GiveStartupAbilities();
	
	ANUSpiritState* GetActiveSpiritState() const;

	void SetActiveSpiritState(ANUSpiritState* NewSpiritState);

	UNUInventoryComponent* GetInventoryComponent() const;

	UNUEquipmentComponent* GetEquipmentComponent() const;

	UNUQuestComponent* GetQuestComponent() const;

	UFUNCTION(BlueprintPure, Category = "NU|Job")
	UNUJobComponent* GetJobComponent() const;

	UFUNCTION(BlueprintPure, Category = "NU|Skill")
	UNUPlayerSkillComponent* GetPlayerSkillComponent() const;

	UNUProgressionComponent* GetProgressionComponent() const;
	
	UNUNaiaRosterComponent* GetNaiaRosterComponent() const;
	
	void TryEnsureActiveSpiritFieldAvatar();

	// 활성 정령이 확정/변경되면 필드 동기화에 알리는 함수
	void NotifyFieldSpiritChanged();

	// 나이아 꺼내기 여부, 꺼내지 않으면 필드 아바타를 만들지 않고
	// 필드 동기화에도 정령 id 를 0으로 보냄
	void SetNaiaSummoned(bool bSummoned);
	bool IsNaiaSummoned() const;
	
protected:
	virtual void BeginPlay() override;
	
private:
	UFUNCTION()
	void OnRep_ActiveSpiritState();
	
	UFUNCTION()
	void OnRep_NaiaSummoned();
	
private:
	/* 컴포넌트 */
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Ability", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUQuestComponent> QuestComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUWorldLocationComponent> WorldLocationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Recovery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNURecoveryComponent> RecoveryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Job", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUJobComponent> JobComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUPlayerSkillComponent> PlayerSkillComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Progression", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUProgressionComponent> ProgressionComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Naia", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUNaiaRosterComponent> NaiaRosterComponent;
	
	
	/* AttributeSet */
	
	UPROPERTY()
	TObjectPtr<class UNUStatAttributeSet> StatusSet;
	
	UPROPERTY()
	TObjectPtr<class UNUVitalAttributeSet> VitalSet;
	
	UPROPERTY()
	TObjectPtr<class UNUCombatAttributeSet> CombatSet;
	
	/* 그 외 변수 */
	
	// SetActiveSpiritState는 서버에서만 불리므로, 클라는 OnRep으로 필드 동기화에 알린다
	UPROPERTY(ReplicatedUsing = OnRep_ActiveSpiritState)
	TObjectPtr<ANUSpiritState> ActiveSpiritState;
	
	// 나이아를 꺼내 뒀는지, 필드 아바타 생성 여부를 결정함
	UPROPERTY(ReplicatedUsing = OnRep_NaiaSummoned)
	bool bNaiaSummoned = false;
};
