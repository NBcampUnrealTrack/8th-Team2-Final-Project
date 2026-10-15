// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "NUGAS/Interface/NUCombatantInterface.h"
#include "GameFramework/Info.h"
#include "NUSpiritState.generated.h"

struct FNUNaiaInstance;
struct FNUNaiaDefinition;
class ANUFieldSpirit;
class ANUPlayerState;
class ANUBattleEncounter;
class UNUAbilitySystemComponent;

UCLASS(Blueprintable)
class PROJECTM_API ANUSpiritState : public AInfo, public IAbilitySystemInterface, public INUCombatantInterface
{
	GENERATED_BODY()

public:
	ANUSpiritState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UNUAbilitySystemComponent* GetNUASC() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 로스터에서 넘기는 개체 데이터로 초기화
	void InitializeSpirit(ANUPlayerState* InOwningPlayerState, const FNUNaiaInstance& Entry);
	
	// 초기 Attribute 값 적용하는 함수
	void ApplyInitialVitals();
	void ApplyInitialCombatStats();
	
	// 초기 어빌리티 부여
	void GiveStartupAbilities();
	
	void SetSpiritId(int32 NewSpiritId);
	
	int32 GetSpiritId() const;
	int64 GetInstanceId() const;
	int32 GetCurrentLevel() const;
	
	// 나이아가 배운 액티브 스킬 반환
	// 클라이언트의 스킬 메뉴와 보관함 스냅샷이 같이 읽음
	const TArray<int32>& GetActiveSkillIds() const;
	int32 GetPassiveSkillId() const;
	
	// 테스트용, 콘솔이 스킬 하나를 임시로 붙임 (nu.Damage.GrantSkill)
	void GrantActiveSkillForDebug(int32 SkillId);
	
	// 서버가 올리는 성장 값, 클라는 들고 있다가 보관함 스냅샷으로 그대로 돌려줌
	// 애정도는 스탯 보정에 사용함
	int32 GetExperience() const;
	int32 GetAffection() const;
	
	// 필드 아바타 생성을 보장하는 함수
	bool EnsureFieldAvatar(AActor* FollowTarget);
	
	void DestroyFieldAvatar();
	
protected:
	virtual void BeginPlay() override;
	
private:
	UFUNCTION()
	void OnRep_SpiritId();

	// 필드 동기화에 정령이 바뀌었음을 알림 
	void NotifyFieldSpiritChanged();
	
	const FNUNaiaDefinition* FindNaiaDefinition();
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Ability", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<class UNUStatAttributeSet> StatusSet;
	
	UPROPERTY()
	TObjectPtr<class UNUVitalAttributeSet> VitalSet;
	
	UPROPERTY()
	TObjectPtr<class UNUCombatAttributeSet> CombatSet;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|GameplayEffect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UGameplayEffect> InitVitalsEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|GameplayEffect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> InitCombatStatsEffect;
	
	// 소유한 플레이어
	UPROPERTY(Replicated)
	TObjectPtr<ANUPlayerState> OwningPlayerState;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Spirit|Avatar", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ANUFieldSpirit> FieldSpiritClass;
	
	// SetSpiritId는 서버에서만 불리므로, 클라는 OnRep으로 필드 동기화에 알린다
	UPROPERTY(ReplicatedUsing = OnRep_SpiritId)
	int32 SpiritId = 0;
	
	UPROPERTY(Replicated)
	int64 InstanceId = 0;
	
	UPROPERTY(Replicated)
	int32 Level = 0;
	
	// 스킬 메뉴를 클라이언트가 그려야 하므로 복제함
	// ASC 쪽 사본은 서버 검증용, 지금은 둘 다 InitializeSpirit 한 곳에서 채움
	UPROPERTY(Replicated)
	TArray<int32> ActiveSkillIds;
	
	// 고정된 패시브, 나이아 정보 화면에 그려야 하므로 위와 같이 복제함
	UPROPERTY(Replicated)
	int32 PassiveSkillId = 0;
	
	// 파티에 올라와 있는 동안 성장 값이 실려 있는 자리
	UPROPERTY(Replicated)
	int32 Experience = 0;
	
	// 애정도는 ApplyInitialVitals·ApplyInitialCombatStats 가 스탯 보정으로 읽음
	UPROPERTY(Replicated)
	int32 Affection = 0;
};
