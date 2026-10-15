// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Battle/NUBattleTypes.h"
#include "Battle/NUBattleActionContext.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUBattleGameplayAbility.generated.h"

class ANUBattleAvatar;
class UNUAbilitySystemComponent;

UCLASS()
class PROJECTM_API UNUBattleGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UNUBattleGameplayAbility();
	
	const FGameplayTag& GetAbilityId() const;
	ENUTargetPolicy GetTargetPolicy() const;
	int32 GetActionPriority() const;

	// 화면 상단 배너에 띄울 이름
	// 비어 있으면 AbilityId 의 마지막 조각으로 대체함, 안 채운 BP가 눈에 띄게
	FText GetDisplayName() const;

	
	/* 패링, 회피 반응창 */
	
	void RegisterParryPress(int32 CombatantId, float PressOffset);
	void RegisterDodgeRequest(int32 CombatantId);

	// 회피 성공 확률, 기본값 + 민첩 보정을 상한에서 자름
	float ResolveDodgeChance(const UNUAbilitySystemComponent* TargetASC, int32 CombatantId) const;
	
	int32 GetParrySPCost() const;
	int32 GetDodgeSPCost() const;
	
	bool IsParryable() const;	// CDO값, SkillId==0 일때만 사용
	const FNUParryTimingSpec& GetParryTiming() const;
	
	// 이번 어빌리티가 실제로 패링/회피 가능한지, ActionContext 기준
	// 액티브 스킬처럼 스펙을 공유하는 어빌리티는 CDO의 bParryable를 사용하면 안됨
	bool IsCurrentlyReactable() const;
	bool IsCurrentlyParryable() const;

	// 넷 전투가 어빌리티 인스턴스 없이 CDO에서 기본 연출만 읽어갈 때 사용함
	// 기본 공격의 검기처럼 DT_SkillVFX에 행이 없는 연출은 이 값들이 유일함
	// 연출 시간 값은 넷 프리셋이 따로 들고 있어 여기서 채우지 않음
	void ApplyPresentationDefaults(FNUBattleActionMotionSpec& OutSpec) const;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	
	// 인카운터가 실은 컨텍스트로 자기 필드를 덮어씀
	// 아래의 모션 구성, 타겟 해결, 효과 적용이 모두 해당 필드를 읽으므로
	// ActivateAbility 맨 앞에서 불러야 함
	void ApplyActionContext();

	// 이번 행동의 총 타수, 발동 순간 한 번 읽어 ASC 가 행동 내내 들고 있음 (단발 판정)
	// 반복 타격하는 어빌리티가 override 함
	virtual int32 GetActionHitCount() const { return 1; }

	virtual void PreApplyAbilityEffect();
	virtual void ApplyRuntimeEffect(AActor* TargetActor){}
	
	// 시전자에게 가는 런타임 효과, 대상 수와 무관하게 행동당 한 번 불림
	virtual void ApplySelfRuntimeEffects(){}
	
	// 대상 집합과 다른 편으로 가는 줄, 행동당 한 번 불림 (2008 불바다의 아군 상태 해제)
	virtual void ApplyCrossSideRuntimeEffects(){}
	virtual void ApplyEffectsByTiming(ENUEffectTiming Timing, const TArray<AActor*>& Targets);
	void ApplyEffectEntry(const FNUAbilityEffectEntry& Entry, UNUAbilitySystemComponent* SourceASC, AActor* TargetActor);
	
	// 아바타에 보낼 행동 모션 구성
	virtual void BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const;
	
	// 판정까지의 대기 시간
	virtual float GetPreHitDelay() const;
	
	// 예고 시간까지 타격 판정 시각, 반응 대기는 안더함
	// ParryImpactOffset 계산이 GetPreHitDelay를 그대로 쓰면 자기 자신을 참조하게 됨
	float GetImpactDelay() const;
	
	ANUBattleEncounter* GetBattleEncounter() const;
	
	// 타겟 정책이 런타임에 정해지는 어빌리티(아이템)는 이걸 override 해서 다시 해결하지 않게 함
	virtual void ResolveEffectTargets(TArray<AActor*>& OutTargets) const;
	
	// 대상 정책을 골라서 해석함, 스킬의 정책과 다른 편을 구할 때 씀
	void ResolveTeamTargets(ENUTargetPolicy Policy, TArray<AActor*>& OutTargets) const;
	
	
	/* 패링 관련 */
	
	void OpenInputWindows();
	void CacheTargetCombatantIds();
	void ResolveParryWindow();
	bool TryConsumeParryCost(UNUAbilitySystemComponent* TargetASC, const FNUBattleParticipant& Participant,
		int32 CombatantId);
	void ResolveDodgeWindow();

	// 확률형 회피 패시브를 굴려 그 공격을 통째로 빗나가게 함 (ex) 2819 발놀림)
	// 반응 창과 달리 입력도 SP도 필요 없고, 행동당 한 번만 처리됨
	void ResolvePassiveEvade();

	// 확률형 공격시 패시브를 굴려 대상에게 적용함 (ex) 2805 폭탄광)
	void ResolveOnAttackPassives();
	void ClearStateTags();
	
	
	// 타격 시점 진입, WaitDelay가 부름
	UFUNCTION()
	void HandleHitPoint();

	// 타격 1회분 효과 적용, 반복할 때는 이것만 다시 부름
	void ApplyHitEffects();
	
	// 스킬을 썼을 때 대상에게 쌓이는 직업 자원 (문양), 행동당 한 번
	void TriggerJobResourceGainOnSkillUse();
	
	// 타격당이 아니라 행동당 한 번인 자원 줄, 위와 같은 자리에서 부름
	void ApplyJobResourceRowsOnce();

	// 타격마다 시전자의 체력을 깎음, 자해 스킬만 값이 들어 있음
	void PaySelfHPCost();

	// 대상에게 피해 실행기를 거는 효과 항목이 있는지, 이번 턴 공격 횟수(3050 연쇄)를 셀 행동인지 가름
	bool IsAttackAction() const;

	// 대상에게 붙은 자원에 비례하는 계수 가산 (3140 마크 · 3241 문양), 대상마다 행동당 한 번 셈
	// 처음 센 값을 이 행동의 모든 타격에 쓰고, 자원은 그 대상에게 처음 피해가 들어간 뒤 소모함
	float ResolveTargetCoefficientAdd(UNUAbilitySystemComponent* SourceASC, UNUAbilitySystemComponent* TargetASC);
	void ConsumeTargetCoefficientResources(UNUAbilitySystemComponent* SourceASC, UNUAbilitySystemComponent* TargetASC);

	// 타격 1회가 끝나면 기본 Ability는 마무리로 감
	// 플레이어 어빌리티가 override 해서 다음 타격을 끼워 넣음
	virtual void OnHitApplied();

	// 마무리 대기 예약 -> HandleSequenceFinished
	void ScheduleFinish();

	UFUNCTION()
	void HandleSequenceFinished();

	FVector ResolveActionLocation(ANUBattleAvatar* Avatar, AActor* TargetActor) const;
	
	// 패링 창·타격·종료·아바타 재생이 모두 이 값을 사용함, ActionTime 을 직접 읽지 말 것
	float GetActionTime() const;

	// 위와 같은 이유로 HitTimeNormalized 를 직접 읽지 말 것, 전투 모션의 Hit 표시가 이김
	float GetHitTimeNormalized() const;
	
protected:
	// 어빌리티 종류
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Ability|Battle")
	FGameplayTag AbilityId;

	// 행동 실행 중 화면 상단에 띄울 이름
	// 기본 공격·도주는 코드가, 스킬과 아이템은 표가 이름을 주므로 여기 적어도 쓰이지 않음
	// 표에도 코드에도 없는 어빌리티에만 남아 있는 자리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Ability|Battle")
	FText DisplayName;
	
	// 타겟팅 정책
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Ability|Battle")
	ENUTargetPolicy TargetPolicy = ENUTargetPolicy::SingleEnemy;
	
	// 행동 우선도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Ability|Battle")
	int32 ActionPriority = 0;

	// 계수
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect")
	float DamageCoefficient = 1.f;
	
	// 합연산 풀에 더할 이 행동만의 값, 0.2 면 +20%
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect")
	float AdditiveDamageBonus = 0.f;
	
	// 곱연산 항 하나, 1 이면 곱하지 않음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.f;
	
	// 방어력 무시 비율(0~1), 스킬 표의 DefenseIgnore 가 들어옴
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect", meta = (ClampMin = "0", ClampMax = "1"))
	float DefenseIgnore = 0.f;
	
	// 흡혈 비율, 스킬 표의 Lifesteal 이 들어옴. 1 이면 가한 피해만큼 회복
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect", meta = (ClampMin = "0"))
	float Lifesteal = 0.f;

	// 타격마다 내는 자기 체력(최대 HP 비율), 직업 정의표의 HP_Cost 가 들어옴
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect", meta = (ClampMin = "0", ClampMax = "1"))
	float SelfHPCostRatio = 0.f;
	
	// 계수의 기준 축, 비어 있으면 공격력, 드루이드 스킬만 최대 HP 비율 사용
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect")
	FGameplayTag DamageBase;

	// 데미지 타입
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect")
	FGameplayTag DamageType = NU::Damage::Type::Physical;
	
	// 데미지 속성
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Effect")
	FGameplayTag AttackElement;
	
	// 적용할 GameplayEffect 들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Ability|Effect")
	TArray<FNUAbilityEffectEntry> AbilityEffects;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Ability|Presentation")
	ENUBattleActionMotion Motion;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Ability|Presentation")
	TObjectPtr<UPaperZDAnimSequence> ActionAnimation;
	
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation")
	float ApproachTime = 0.45f;
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation")
	float ActionTime = 0.6f;
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation")
	float ReturnTime = 0.25f;
	
	// 공격 애니메이션 중 실제 공격 판정이 일어나는 지점, 0 ~ 1.0 
	// 서버는 아바타 연출이 없어서 Timer로 판정 필요
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation", meta=(ClampMin="0", ClampMax="1"))
	float HitTimeNormalized = 0.5f;

	// 기본 연출 VFX, DT_SkillVFX 에 이 스킬의 행이 없을 때 그대로 쓰이는 값이라
	// 액티브 스킬 공용 실행기는 여기에 근접(슬래시) 기본값을 넣어두면 됨
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation|VFX")
	TSoftObjectPtr<UNiagaraSystem> CastVFX;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation|VFX")
	TSoftObjectPtr<UNiagaraSystem> ProjectileVFX;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation|VFX")
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Presentation|VFX")
	FLinearColor EffectColor = FLinearColor::White;
	
	UPROPERTY()
	TObjectPtr<AActor> CachedTargetActor;

	
	/* 패링, 회피 관련 값 */
	
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Parry")
	bool bParryable = false;
	
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Parry")
	FNUParryTimingSpec ParryTiming;
	
	// RPC왕복, 네트워크 지연 여유
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Parry", meta = (ClampMin = "0"))
	float ParryNetworkSlack = 0.15f;

	// 이 공격을 패링하는 데 드는 SP, 0이면 공짜
	// 타이밍이 맞아도 SP가 부족하면 회피와 같이 그냥 맞음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Parry", meta = (ClampMin = "0"))
	int32 ParrySPCost = 4;

	// 이 공격을 회피하는 데 드는 SP, 패링 불가 공격에서만 의미가 있음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Dodge", meta = (ClampMin = "0"))
	int32 DodgeSPCost = 6;
	
	// 회피 확률 = DodgeBaseChance + 민첩 x DodgeChancePerDexterity, 상한 DodgeMaxChance
	// 서버가 캐릭터 스탯으로 DodgeRate 를 따로 보내주므로, 그 값을 권위로 삼기로 정하면
	// ResolveDodgeChance 한 곳만 갈아끼우면 된다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Reaction", meta = (ClampMin = "0", ClampMax = "1"))
	float DodgeBaseChance = 0.4f;
	
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Reaction", meta = (ClampMin = "0", ClampMax = "1"))
	float DodgeChancePerDexterity = 0.015f;
	
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Reaction", meta = (ClampMin = "0", ClampMax = "1"))
	float DodgeMaxChance = 0.85f;

	// 접근 전 예고 시간, 모션 전체가 이만큼 뒤로 밀림
	// 회피는 이 구간부터 접근이 끝날 때까지 입력을 받고, 패링은 타격 순간에 맞춰 받음
	// 적이 여러 마리면 턴이 그만큼 길어지므로 짧게 두는 편이 좋음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Ability|Telegraph", meta = (ClampMin = "0"))
	float TelegraphTime = 0.3f;
	
	// 이번 활성화 연출 기준 임팩트 오프셋
	float ParryImpactOffset;
	
	// 이번 발동의 행동 시간 - 공격 모션이 있으면 그 길이, 없으면 ActionTime
	// 0 이면 아직 안 정했다는 뜻이라 GetActionTime 이 ActionTime 을 돌려줌
	float EffectiveActionTime = 0.f;

	// 이번 발동의 타격 지점 - 공격 모션에 Hit 표시가 있으면 그 지점, 없으면 HitTimeNormalized
	// 음수면 아직 안 정했다는 뜻이라 GetHitTimeNormalized 가 HitTimeNormalized 를 돌려줌
	float EffectiveHitTimeNormalized = -1.f;

	// 이번 발동의 시전 지점 - 공격 모션의 Cast 표시에서만 채워짐, 없으면 0(Acting 시작)
	float EffectiveCastTimeNormalized = 0.f;
	
	// 이번 발동의 컨텍스트, ApplyActionContext가 채움
	// 타격 시점의 ApplyRuntimeEffect가 읽으므로 발동 구간 내내 살아 있어야 함
	UPROPERTY()
	FNUBattleActionContext ActionContext;
	
private:
	// 공격할 타겟이 아닌 전투원의 패링·회피 입력을 걸러냄
	bool IsTargetedCombatant(int32 CombatantId) const;
	
private:
	// 타겟을 저장하는 배열, ActivateAbility에서 한 번 확정해 발송과 입력 필터가 같이 씀
	TArray<int32> TargetCombatantIds;
	
	// CombatantId, 창 시작 기준 입력 오프셋
	TMap<int32, float> ParryPresses;

	// 회피를 요청한 전투원, 눌렀는지만 확인하면 되므로 TSet
	TSet<int32> DodgeRequests;

	// State.Parried 또는 State.Dodging을 붙인 ASC, EndAbility에서 해당 태그 제거
	TArray<TWeakObjectPtr<UNUAbilitySystemComponent>> ParriedASCs;	// State.Parried를 붙인 ASC들
	TArray<TWeakObjectPtr<UNUAbilitySystemComponent>> DodgedASCs;   // State.Dodging를 붙인 ASC들

	// 이번 행동에서 대상마다 센 계수 가산과, 그 자원을 이미 소모한 대상 — 발동할 때 비움
	TMap<TWeakObjectPtr<UNUAbilitySystemComponent>, float> TargetCoefficientAdds;
	TSet<TWeakObjectPtr<UNUAbilitySystemComponent>> CoefficientConsumedTargets;
};
