// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "NUAbilitySystemInitVitalData.h"
#include "Battle/NUBattleTypes.h"
#include "Battle/NUBattleActionContext.h"
#include "Battle/Data/NUPassiveDefinition.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "NUAbilitySystemComponent.generated.h"


struct FNUStatusEffectDefinition;
struct FNUStatusEffectView;
struct FNUJobResourceView;
struct FNUActiveEffectRow;
class UNUCombatAttributeSet;
class UNUVitalAttributeSet;
class ANUBattleEncounter;
namespace NUActiveSkill { struct FDamageParams; }
enum class ENUJobResourceGainTrigger : uint8;
struct FNUJobResourceDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleEncounterChanged, ANUBattleEncounter*);
DECLARE_MULTICAST_DELEGATE(FOnStatusEffectsChanged);
DECLARE_MULTICAST_DELEGATE(FOnShieldChanged);
DECLARE_MULTICAST_DELEGATE(FOnJobResourcesChanged);
// 갬블러 카드를 뽑았을 때 (카드 이름 — 대길 · 길 · 흉 · 대흉)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJobCardDrawn, FName);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UNUAbilitySystemComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 초기 능력 부여
	void GiveStartupAbilities();
	
	// 초기 체력/SP 적용
	bool ApplyInitialVitals(const FNUAbilitySystemInitVitalData& InitData, TSubclassOf<UGameplayEffect> InitEffectClass, int Level);
	
	// 초기 전투 능력치 적용
	bool ApplyInitialCombatStats(const FNUAbilitySystemInitCombatData& InitData, TSubclassOf<UGameplayEffect> InitEffectClass, int Level);
	
	void RefreshAbilityActorInfo();
	
	AActor* GetCurrentAvatar() const;
	void SetCurrentAvatar(AActor* NewAvatar);
	
	ANUBattleEncounter* GetBattleEncounter() const;
	void SetBattleEncounter(ANUBattleEncounter* NewEncounter);

	const UNUVitalAttributeSet* GetVitalAttributeSet() const;
	const UNUCombatAttributeSet* GetCombatAttributeSet() const;
	
	// AbilityId 로 보유한 전투 어빌리티 SpecHandle 반환하는 함수
	FGameplayAbilitySpecHandle FindBattleAbilityHandle(const FGameplayTag& InAbilityId);
	
	// 전투 어빌리티 발동하는 함수
	bool TriggerBattleAbility(const FGameplayAbilitySpecHandle AbilityHandle, const FGameplayTag& AbilityId,
		const FGameplayEventData& EventData);
	
	// 사망 확인
	bool IsAlive() const;
	
	// 인자로 받은 체력으로 부활하는 함수
	void ReviveIfDead(float ReviveHealth);
	
	// 턴 기반 GameplayEffect 적용 
	FActiveGameplayEffectHandle ApplyTurnBasedEffect(const FGameplayEffectSpecHandle& SpecHandle,
		UNUAbilitySystemComponent* TargetASC, int32 Turns, int32 StatusEffectId);

	// 스탯 변경 GE 를 턴 기반으로 걸되, 같은 시전자가 같은 스킬로 대상에게 건 같은 축의 이전 것은 지우고 대체함
	// 다시 걸어도 겹치지 않고 지속만 새로 잡힘 (3111 견제 화살 · 3411 포효), 다타 스킬이 타격마다 걸어도 한 번분만 남음
	void ApplyReplacingStatModifier(const FGameplayEffectSpecHandle& SpecHandle,
		UNUAbilitySystemComponent* TargetASC, int32 Turns, int32 SkillId, ENUStatusAffectedStat Stat);
	
	// 보유한 턴 기반 GameplayEffect 감소
	void AdvanceTurnBasedEffects();
	
	// 보유한 턴 기반 GameplayEffect 정리, 전투 종료시 제거
	void ClearTurnBasedEffects();
	
	// 상태이상 부여, Change가 1미만이면 확률 판정, DurationOverride 가 0이면 정의에서 읽기
	FActiveGameplayEffectHandle TryApplyStatusEffect(UNUAbilitySystemComponent* TargetASC, int32 StatusEffectId,
		float Chance = 1.f, int32 DurationOverride = 0);
	
	// 상태이상 해제, 실제로 지운 개수를 돌려줌 (ex) 2104 정화의 물결)
	// NU.State.Ailment 태그가 달린 것만 해제함, 버프와 턴 범위 스탯 변경은 건드리지 않음
	int32 CureStatusEffects(int32 Count);
	
	// 상태이상에 면역인지 확인
	bool IsImmuneStatusEffect(const FNUStatusEffectDefinition& Definition);
	
	// 회복을 대상에게 보냄, 즉발 회복과 턴마다 회복이 같은 GE 를 씀
	// 패시브의 매 턴 회복도 이 함수를 부를 것이므로 턴 처리 안에 회복을 넣지 않고 이 함수를 호출함
	void SendHeal(UNUAbilitySystemComponent* TargetASC, UNUAbilitySystemComponent* SourceASC,
		ENUStatusAffectedStat Stat, float Value, bool bIsPercent);
	
	// 턴마다 회복하는 효과를 검, 같은 스킬이 이미 걸려 있으면 중첩하지 않고 남은 턴을 다시 채움
	void ApplyTurnBasedHeal(UNUAbilitySystemComponent* TargetASC, const FNUActiveEffectRow& Row);

	// 특정 상태이상이 걸려있는지 확인함
	// FNUStatusEffectDefinition을 인자로 받는 람다를 넘기는 방식으로 사용하면 됨
	bool HasStatusEffectMatching(TFunctionRef<bool(const FNUStatusEffectDefinition&)> Predicate) const;

	// 걸려있는 상태이상의 축 값을 모두 더함
	// 같은 축을 켜는 상태이상이 둘 이상 걸릴 수 있는 비율 축에 사용함
	float SumStatusEffectAxis(TFunctionRef<float(const FNUStatusEffectDefinition&)> Selector) const;

	// 상시로 켜져 있는 "주는 피해 +X%" 패시브를 전부 더함, 0.2 면 +20%
	// 합연산 풀에 들어갈 값이라 곱하지 않고 더함
	// 발동 조건이 붙은 패시브(공격시·저체력시 등)는 그 시점에서 따로 실어야 함
	float SumAlwaysDamageBonus() const;

	// 속성 위력 패시브를 더함, 이번 공격의 속성이 맞고 발동 조건을 만족한 것만 합연산
	float SumElementPowerBonus(const FGameplayTagContainer& AttackTags) const;
	
	// 공격이 닿은 뒤 행동당 한 번 호출, 확률로 상태이상을 거는 공격시 패시브 (ex) 2805 폭탄광)
	void TriggerOnAttackPassives(UNUAbilitySystemComponent* TargetASC);
	
	// 맞기 직전 공격당 한 번 - 확률로 그 공격을 통째로 피했는지 (ex) 2819 발놀림)
	bool RollPassiveEvade() const;
	
	// 상시로 켜져 있는 직업 스킬의 "주는 피해 +X%" 를 더함, 합연산 풀로 들어감
	// 속도 차이를 보는 구간(3101 선행)이 있어 대상을 같이 받아야 함, 없으면 그 줄만 빠짐
	// 같은 편이 들고 있는 아군 범위 줄(3403)도 이 전투원에게 닿으면 더함, 값과 조건은 줄 주인 기준 (아래 세 함수도 같음)
	float SumJobDamageBonus(const UNUAbilitySystemComponent* TargetASC) const;

	// 상시 직업 스킬의 곱연산 "주는 피해" 항, 없으면 1
	// 합연산 풀과 자리가 다름, 섞으면 x1.167 이 +16.7% 가 되어 값이 달라짐
	float GetJobDamageMultiplier(const UNUAbilitySystemComponent* TargetASC) const;

	// 공격마다 값이 갈려 상시 스탯 GE 에 못 싣는 치명타 줄의 합, 실행기가 공격 순간 어트리뷰트 값에 더함
	// 조건(3114 단발 치명타 피해 +39%p)이나 자원 · 잃은 HP 1당(3015 예지 · 3039 잔향)이 붙은 줄 — GE 로 걸면 그때 값으로 굳음
	float SumJobPerAttackCriticalBonus(ENUStatusAffectedStat Stat, const UNUAbilitySystemComponent* TargetASC) const;

	// 이번 공격의 대상 상태를 봐야 켜지는 `주는 피해` 패시브를 더함 (ex) 2803 잔열)
	// 공격시 트리거는 지금 이 자리가 그 사건이라 여기서 판정함
	float SumConditionalDamageBonus(const UNUAbilitySystemComponent* TargetASC) const;
	
	// 공격시 흡혈 패시브를 더함 (ex) 2806 흡혈), 없으면 0
	float SumLifestealBonus() const;
	
	// 이 전투원이 받는 피해에 곱해질 배율, 감소가 없으면 1
	// 공격자에게 붙은 자원을 봐야 켜지는 직업 줄이 있음 (3148 마크 5중첩 · 3248 문양 상한인 적에게 받는 피해 x0.8), 모르면 그 줄만 빠짐
	float CalcDamageTakenMultiplier(float AttackerSpeed, const UNUAbilitySystemComponent* AttackerASC = nullptr) const;

	// 이 전투원이 받는 회복에 곱해질 배율 (3034 극점 -30%), 전투 밖이면 1
	// 스킬 · 아이템 · 흡혈 · 턴 회복이 전부 IncomingHealing 한 자리로 모여 거기서 곱함
	float GetHealReceivedMultiplier() const;

	// 이 전투원에게 걸리는 보호막에 곱해질 배율, 같은 편 전원의 "아군 보호막" 줄을 곱함 (3444 하나된 존재 x2)
	float CalcShieldReceivedMultiplier() const;

	// 이 전투원이 캐릭터(플레이어)인지, 나이아와 적은 거짓
	bool IsPlayerCharacter() const;

	// 이 전투원이 시전자일 때 줄의 대상 범위가 같은 편 후보에게 닿는지
	// Ally 는 자신 · 나이아를 뺀 캐릭터, AllyAndNaia 는 자신만 뺌, Party 는 전원. 후보는 같은 편만 넣을 것
	bool DoesJobScopeReach(ENUJobTargetScope Scope, const UNUAbilitySystemComponent* Candidate) const;
	
	// 스킬로 공격할 때 켜지는 전투당 1회 곱연산 항을 돌려주고 그 자리에서 소모함 (ex) 2836 최후의 일격)
	// 체크하면서 기회를 소모하므로 행동당 한 번만 불러야 함
	float ConsumeOnceDamageMultiplier();
	
	// 이번 행동의 기본 계수에 더할 가산분, 자원에 비례함 (3029 영력 1당 +3%p)
	// 표가 소모라고 적은 줄은 여기서 자원을 쓰므로 행동당 한 번만 불러야 함
	float ConsumeJobCoefficientAdd(int32 SkillId);
	
	// 이번 행동의 합연산 풀에 더할 직업 줄 중 무엇 1당이 붙은 것 (3133 템포 1당 +5.8% · 3432 동화율 1당 +3%)
	// 발동할 때 자원을 봐야 해서 컨텍스트가 못 실음, 1당이 없는 줄은 컨텍스트가 표만 보고 이미 실었음
	float SumJobActionDamageBonus(int32 SkillId) const;
	
	// 대상에게 붙은 자원(3140 마크 · 3241 문양)에 비례하는 계수 가산, 대상마다 따로 셈
	// 발동할 때는 대상을 몰라 ConsumeJobCoefficientAdd 가 이 줄을 건너뜀, 소모는 아래가 따로 함
	float SumTargetJobCoefficientAdd(int32 SkillId, const UNUAbilitySystemComponent* TargetASC) const;
	
	// 위 줄 중 표가 소모라고 적은 것의 자원을 대상에게서 씀, 그 대상에게 피해가 들어간 뒤 한 번
	void ConsumeTargetJobCoefficientResources(int32 SkillId, UNUAbilitySystemComponent* TargetASC);
	
	// 행동당 한 번만 적용하는 자원 줄
	void ApplyActionJobResourceRows(int32 SkillId);
	
	// 죽을 피해를 한 번 버티는 패시브·직업 스킬이 켜지는지 보고 켜지면 소모함
	// 판정 기준이 맞기 전 체력이라 이미 깎인 값으로는 볼 수 없어 비율을 받음
	// OutSurvivalHealth 는 버틴 뒤 남는 체력 — 보통 1 이지만 표가 따로 적기도 함 (3448 불멸)
	bool ConsumeDeathPrevention(float HealthRatioBefore, float& OutSurvivalHealth);
	
	// 죽음을 막는 직업 스킬 (3027 불굴 · 3044 아직 죽지 않았다 · 3448 불멸)
	// 패시브가 안 켜졌을 때 이어서 봄
	bool ConsumeJobDeathPrevention(float HealthRatioBefore, float& OutSurvivalHealth);
	
	// 그 노드가 버틴 뒤 남기는 체력, 표가 안 적었으면 1
	float FindDeathPreventSurvivalHealth(int32 JobSkillId) const;
	
	// 전투당 1회 패시브를 이번 전투에서 이미 썼는지
	bool HasUsedOncePerBattle(int32 PassiveId) const;

	// 선제공격이 켜지는지 보고 켜지면 소모함 (전투당 1회)
	// 묻는 것이 곧 소모라 행동 순서를 짤 때 전투원마다 한 번만 불러야 함
	bool ConsumeFirstStrike();

	// 전투가 열릴 때 한 번 발동하는 패시브를 적용함, 인카운터가 첫 턴에 전투원마다 부름 (ex) 2833 동기화)
	// 턴시작 패시브보다 먼저 와야 복사한 패시브가 그 턴부터 발동됨
	void ApplyBattleStartPassives();

	// 턴이 열릴 때 발동하는 패시브를 적용함, 인카운터가 전투원마다 부름 (ex) 2840 생명의 숨결)
	void ApplyTurnStartPassives();

	// 갬블러 [운명 읽기] — 이번 턴에 뽑은 카드, 안 뽑았으면 NAME_None
	FName GetCurrentJobCard() const;

	// 턴이 열릴 때 카드를 한 장 뽑음, OnCardDrawn 줄을 가진 전투원만 뽑음 (로컬 전투, 넷 전투는 서버가 뽑음)
	// 확률은 NU Battle Data 의 GamblerCardOdds 를 LUK 로 읽음
	void DrawJobCard();

	// 적을 처치했을 때 발동하는 패시브를 적용함, 사망 판정이 처치자에게 부름 (ex) 2839 사냥본능)
	void ApplyOnKillPassives();
	
	// 쓰러졌을 때 발동하는 패시브를 적용함, 사망 판정이 쓰러진 쪽에게 부름 (ex) 2802 포자 살포)
	void TriggerOnDefeatedPassives();
	
	// 맞은 직후 타격당 한 번 호출, 맞고 나서 거는 피격시 패시브 (ex) 2801 점액)
	// 회피(2819)와 받는 피해 감소(2838)는 피해를 계산하기 전에 이미 끝나 여기 없음
	void TriggerOnHitTakenPassives(UNUAbilitySystemComponent* AttackerASC);
	
	// 이 전투 동안만 유지되는 스탯 변경을 자신에게 검
	// 만료 턴이 없어 턴 기반에 못 넣는 것이 여기로 옴, 전투가 바뀔 때 같이 제거됨
	void ApplyBattleScopedStatModifier(UNUAbilitySystemComponent* SourceASC,
		ENUStatusAffectedStat Stat, float Value, bool bIsPercent);

	// 남은 턴이 있는 스탯 변경을 자신에게 검 (ex) 2834 에너지 순환의 MaxSP)
	// 상태이상이 아니라 아이콘은 뜨지 않고 도트·정화 판정도 타지 않음, 턴만 같이 줄어듦
	void ApplyTurnScopedStatModifier(UNUAbilitySystemComponent* SourceASC,
		ENUStatusAffectedStat Stat, float Value, bool bIsPercent, int32 Turns);
	
	// 스킬이 주는 자원을 받음 (ex) 2007 불의 의지의 SP 10)
	// SP는 행동당 예산이라 그냥 더해줄 자리가 없어서 상한을 올리고 다음 회복 시 얹어짐
	// 이번 턴 행동을 이미 마친 쪽은 다음 회복이 다음 행동 뒤라 지금 SP 에도 더함 (3240 지맥 개방 — 아군 SP 8)
	void GainResource(UNUAbilitySystemComponent* SourceASC,
		ENUStatusAffectedStat Stat, float Value, bool bIsPercent, int32 Turns);


	/* 보호막 — 체력보다 먼저 닳는 흡수량 */

	// 보호막을 검, 같은 스킬이 이미 걸려 있으면 중첩하지 않고 값과 턴을 다시 채움
	// bIsPercent 면 대상 최대 체력 기준이고, 이 자리에서 절대값으로 풀어 담음
	void ApplyShield(UNUAbilitySystemComponent* SourceASC, int32 SourceSkillId, float Value, bool bIsPercent, int32 Turns);

	// 들어온 피해에서 보호막이 먹은 양을 돌려주고 그만큼 깎음
	// 부르는 것이 곧 소모라 피해 한 번에 한 번만 불러야 함
	float AbsorbWithShield(float IncomingDamage);

	// 지금 남아 있는 보호막 총량, 표시와 조건 판정에 사용
	float GetShieldAmount() const;
	bool HasShield() const;
	const TArray<FNUShieldInstance>& GetShields() const;


	/* 반사 피해 — 걸린 쪽이 공격하면 가한 만큼 되받는 디버프 */

	// 반사를 검, 같은 스킬이 이미 걸려 있으면 갱신함
	// 적에게 거는 것이므로 이 함수는 공격할 쪽의 ASC에서 호출함
	// bIsPercent면 가한 피해의 비율이고 아니면 고정 피해
	void ApplyReflect(UNUAbilitySystemComponent* SourceASC, int32 SourceSkillId,
		float Ratio, int32 Turns);

	// 맞힌 직후 타격당 한 번, 가한 피해만큼 자신이 되받음
	void TriggerReflectOnAttack(float DamageDealt);

	const TArray<FNUReflectInstance>& GetReflects() const;
	
	
	/* 속성 위력 — 그 속성으로 공격할 때만 켜지는 곱연산 항 */
	
	// 속성 위력 배수를 검, 같은 스킬이 이미 걸려 있으면 갱신함
	// RequiredUses 가 0보다 크면 그 속성 스킬을 그만큼 더 쓴 뒤에 켜짐 (2407 과전류)
	void ApplyElementPowerMod(UNUAbilitySystemComponent* SourceASC, int32 SourceSkillId,
		const FGameplayTag& Element, float Multiplier, int32 Turns, int32 RequiredUses = 0);
	
	// 이번 전투에서 그 속성 스킬을 쓴 것을 셈, 행동당 한 번 불러야 함
	void NoteElementSkillUsed(const FGameplayTag& Element);
	
	int32 GetElementSkillUseCount(const FGameplayTag& Element) const;
	
	// 이번 공격의 속성에 걸린 배수를 전부 곱해 돌려줌, 없으면 1
	float GetElementPowerMultiplier(const FGameplayTagContainer& AttackTags) const;
	
	const TArray<FNUElementPowerInstance>& GetElementPowers() const;

	// 몇 턴 동안 받는 피해를 줄이거나 늘림, 액티브가 자기에게 거는 곱연산 항
	// 같은 스킬을 다시 걸면 중첩하지 않고 덮어씀
	void ApplyDamageTakenMod(int32 SourceSkillId, float Multiplier, int32 Turns);

	const TArray<FNUDamageTakenModInstance>& GetDamageTakenMods() const;
	
	
	/* 자기 구속 상태 — 스킬이 스스로를 묶어두는 것 (2210 개화) */
	
	// 상태를 걸거나 이미 있으면 턴을 다시 채움
	void ApplySelfBoundState(int32 SourceSkillId, int32 Turns, bool bBlocksAction);
	
	// 그 상태인 동안 얹히는 흡혈, 상태가 없으면 아무 일도 안 함
	void SetSelfBoundLifesteal(int32 SourceSkillId, float Lifesteal);
	
	// 풀릴 때 그 스킬의 피해를 터뜨리도록 표시함
	void SetSelfBoundRelease(int32 SourceSkillId);
	
	// 그 상태가 얼마나 남았는지, 없으면 0
	// 지속이 "상태가 붙어 있는 동안" 인 줄들이 이 값을 자기 턴 수로 씀
	int32 GetSelfBoundRemainingTurns(int32 SourceSkillId) const;
	
	bool IsSelfBoundActionBlocked() const;

	const TArray<FNUSelfBoundState>& GetSelfBoundStates() const;
	
	
	/* 직업 자원 — 영력 · 동조율 · 템포 · 마크 등 규칙은 자원표(DT_JobResource)에 있음 */
	
	// 자원표가 아는 이름이면 현재 수치를 담고 true
	// 쌓이지 않는 자원(파생값 · 전직 노드가 정하는 자리)은 false
	bool TryGetJobResource(FName ResourceName, int32& OutAmount) const;
	
	// 자원 상한, 스탯 비례면 그 스탯을 읽어 계산함. 셀 수 없으면 0
	int32 GetJobResourceMax(FName ResourceName) const;
	
	bool IsJobResourceAtMax(FName ResourceName) const;
	
	// 자원을 더하거나 뺌, 0 과 상한 사이로 잘림. 실제로 바뀐 양을 돌려줌
	// 계측은 bApplyGainModifiers 를 꺼서 획득 배수 · 턴당 상한을 안 거침
	int32 AddJobResource(FName ResourceName, int32 Delta, bool bApplyGainModifiers = true);

	// 안 쌓인 턴 셈을 0 으로 돌림, 수치는 그대로 (3137 약점 관측 — 마크의 3턴 대기를 처음부터)
	void ResetJobResourceIdleTurns(FName ResourceName);

	// 살아 있는 상대 중 그 자원을 가장 많이 든 전투원, 동률이면 앞선 쪽
	// 아무도 안 들었으면 살아 있는 첫 상대(0 으로 셈), 상대가 없으면 nullptr
	// 자원표의 SelfSideCountFrom 이 MostStacked 인 자원(문양)을 자기 편 줄이 셀 때 씀
	const UNUAbilitySystemComponent* FindMostStackedOpponent(FName ResourceName) const;

	// 이 전투원이 든 자원이 바뀌었을 때 상대의 자원 비례 스탯 줄을 다시 세게 함, 이름을 비우면 자원 아무거나
	// 상대가 "가장 많이 쌓인 적" 의 문양으로 세는 줄을 들고 있음 (3207 문양 1당 SP 상한 -2), 죽어서 판정에서 빠질 때도 부름
	// 같은 상대들에게 "가장 많이 쌓인 적의 자원이 0 이 되면 켜지는 상태" 도 보게 함 (3207 지맥 붕괴)
	// bFromDeath 는 죽어서 판정에서 빠진 경우 - 사망으로 사라진 자원은 0 이 된 것으로 안 침
	void RefreshOpponentResourceScaledStats(FName ResourceName = NAME_None, bool bFromDeath = false);
	
	const TArray<FNUJobResourceInstance>& GetJobResources() const;
	
	// 화면에 그릴 직업 자원 · 상태
	// OutGauges - 자기 칸 자원(영력 · 템포 · 동조율), 그 자원을 여는 노드를 가졌거나 쌓인 적이 있으면 0 이어도 넣음, 자원 번호 순
	// OutBadges - 대상에게 붙는 자원(마크 · 약점 · 문양, 1 이상일 때만) 다음에 켜졌거나 대기 중인 상태(번아웃 · 지맥 붕괴)
	void GetJobResourceViews(TArray<FNUJobResourceView>& OutGauges, TArray<FNUJobResourceView>& OutBadges) const;
	
	// 자원표의 획득 통로, 그 통로에 걸린 자원을 이 전투원의 직업 · 전직 노드에 맞게 획득함
	// TargetASC 는 대상에게 붙는 자원(마크 · 약점 · 문양)이 갈 곳, 없으면 그런 자원은 건너뜀
	// bFromOwnAction 은 이 전투원이 지금 행동하는 중인지
	void TriggerJobResourceGain(ENUJobResourceGainTrigger Trigger,
		UNUAbilitySystemComponent* TargetASC = nullptr, bool bFromOwnAction = true);
	
	// 이 전투원의 직업
	FName GetJobClass() const;
	
	
	/* 속성을 가려 거는 디버프 — 맞는 쪽에 붙음 (2109 수중 감옥) */
	
	// 같은 스킬이라도 속성이 다르면 따로 걸림, 2109 는 물과 불 두 줄을 같이 걸음
	void ApplyElementAffliction(UNUAbilitySystemComponent* SourceASC, int32 SourceSkillId,
		const FGameplayTag& Element, float DamageTakenMultiplier, bool bBlocksSkill, int32 Turns);
	
	// 이번 공격의 속성으로 받는 피해 배수, 없으면 1
	float GetElementDamageTakenMultiplier(const FGameplayTagContainer& AttackTags) const;
	
	// 그 속성 액티브 스킬이 막혀 있는지, 무속성 스킬은 막히지 않음
	bool IsElementSkillBlocked(const FGameplayTag& Element) const;
	
	const TArray<FNUElementAfflictionInstance>& GetElementAfflictions() const;
	
	
	/* 공격시 부여 — 스킬이 몇 턴짜리로 걸어두는 "때릴 때마다" (2007 · 2008) */
	
	void ApplyOnAttackGrant(int32 SourceSkillId, int32 StatusEffectId, float Chance,
		const FGameplayTagContainer& Elements, bool bIncludeBasicAttack, int32 Turns);
	
	// 공격이 닿은 뒤 대상마다 한 번, 이번 공격이 조건에 맞으면 상태이상을 굴림
	// 패시브 쪽(TriggerOnAttackPassives)과 같은 자리에서 불리지만 목록이 달라 함수를 나눔
	void TriggerOnAttackGrants(UNUAbilitySystemComponent* TargetASC,
		const FGameplayTag& AttackElement, bool bIsSkill);
	
	const TArray<FNUOnAttackGrant>& GetOnAttackGrants() const;
	
	
	// 걸려 있는 상태이상중 가장 높은 값
	float GetCaptureBonus() const;

	// 걸려있는 상태이상 정의 목록
	void GetActiveStatusEffects(TArray<const FNUStatusEffectDefinition*>& OutDefinitions) const;

	// 아이콘 표시용 상태이상 목록, StatusEffectId 오름차순
	// TurnBasedEffects 가 TMap 이라 순회 순서가 보장되지 않으므로 반드시 정렬해서 넘겨야 함
	void GetActiveStatusViews(TArray<FNUStatusEffectView>& OutViews) const;

	// 상태이상 보정이 붙은 SP 소모량, 젖음이 걸려있으면 늘어남
	// 명령 선택 화면의 표시와 서버의 차감이 반드시 같아야 함
	int32 ApplyStatusSPCost(int32 BaseCost) const;

	// 직업 스킬의 SP 소모량, 상태이상 보정에 카드가 바꾸는 비용(3301 운명 읽기 — 대길 -40% · 흉 +30%)을 더함
	// 패링 · 회피 비용은 스킬이 아니라서 ApplyStatusSPCost 를 그대로 씀
	int32 ApplySkillSPCost(int32 BaseCost) const;


	/* 스킬 소유 */

	// 서버가 저장한 스킬 목록을 그대로 받음
	void SetActiveSkillIds(const TArray<int32>& InSkillIds);
	const TArray<int32>& GetActiveSkillIds() const;
	
	// 지금 이 전투원에게 켜져 있는 패시브 목록
	// 나이아는 포획 때 굴린 하나, 캐릭터는 직업과 성장 노드에서 온 여러개
	void SetPassiveSkillIds(const TArray<int32>& InPassiveIds);
	const TArray<int32>& GetPassiveSkillIds() const;
	
	// 보유 패시브의 효과 줄, 계측과 판정이 같이 씀
	const TArray<FNUOwnedPassiveEffect>& GetOwnedPassiveEffects() const;

	// 성장 노드로 얻은 직업 스킬 목록, 플레이어만 가짐
	void SetJobSkillIds(const TArray<int32>& InSkillIds);
	const TArray<int32>& GetJobSkillIds() const;

	// 보유 직업 스킬의 효과 줄, 계측과 판정이 같이 씀
	const TArray<FNUOwnedJobEffect>& GetOwnedJobEffects() const;
	
	// 이 전투원이 그 스킬을 액티브로 쓸 수 있는지
	// 나이아 목록과 직업 목록은 칸이 갈려 있고, 직업 칸에는 패시브도 같이 들어 있어
	// 정의표의 SkillType 까지 봐야함
	bool HasActiveSkill(int32 SkillId) const;


	/* 실행기 */

	// 이 클래스의 SpecHandle을 찾고 없으면 그때 부여함
	// 스킬 하나당 스펙 하나가 아니라 실행 방식 하나당 스펙 하나로 처리함
	FGameplayAbilitySpecHandle FindOrGiveExecutor(TSubclassOf<UNUBattleGameplayAbility> ExecutorClass);


	/* 이번 행동의 컨텍스트 */

	// 인카운터가 발동 직전에 싣고 어빌리티가 ActivateAbility에서 가져감
	// 행동은 한 번에 하나씩만 실행되므로 슬롯 하나로 충분함
	void SetPendingActionContext(const FNUBattleActionContext& Context);

	// 비우지 않고 확인만 함, 발동을 시작할지 판단하는 데 사용
	const FNUBattleActionContext& PeekActionContext() const;

	// 읽으면서 비움, 다음 행동으로 새지 않게 하기 위함
	FNUBattleActionContext ConsumeActionContext();

	// 발동에 실패했을 때 인카운터가 직접 비움
	void ClearPendingActionContext();

	// 어빌리티가 꺼내 간 컨텍스트와 이번 행동의 총 타수를 행동이 끝날 때까지 들고 있음
	// 위의 대기 슬롯은 발동 순간 비므로, 타격 시점에 판정하는 쪽(단발 · 명시 자원 줄)은 이쪽을 읽어야 함
	void BeginCurrentAction(const UNUBattleGameplayAbility* Ability, const FNUBattleActionContext& Context,
		int32 HitCount);

	// 시작한 어빌리티가 끝낼 때만 비움
	// 시간 초과로 다음 행동이 먼저 시작됐으면 늦게 끝난 쪽이 그 행동을 지우면 안 됨
	void EndCurrentAction(const UNUBattleGameplayAbility* Ability);

	// 행동 중이 아니면 bValid 가 꺼진 빈 컨텍스트
	const FNUBattleActionContext& GetCurrentActionContext() const;

	// 행동 중이 아니면 0, 도트 · 반사처럼 행동 밖에서 들어온 피해는 단발로 치지 않음
	int32 GetCurrentActionHitCount() const;


	/* 이번 턴 공격 횟수 — 조건 HitCountAtLeast (3050 연쇄 · 3105 연사) */

	// 공격 행동의 타격이 끝날 때마다 하나, 대상 수와 무관함
	// 셈이 기준에 닿는 순간 한 번 쌓이는 패시브 자원 줄(3105 연사 — 이번 턴 두 번째 공격에 템포 +2)도 여기서 처리함
	void NoteAttackHitDealt();

	// 타격 중에 보면 지금 타격은 아직 안 센 값, "6회 이상 공격하면 7회차부터" 가 6 >= 6 으로 맞음
	int32 GetHitsDealtThisTurn() const;

	// 계측 전용 — 기본 SP 로는 한 턴 7타가 안 나와 셈을 직접 맞춤
	// 기준에 닿는 순간 도는 줄(3105)은 여기서 안 돌림, 기준보다 하나 적게 맞추고 때려야 보임
	void SetHitsDealtThisTurn(int32 Count);


	/* 전투당 1회 상태 — 조건에 들어갈 때 켜지고 벗어나면 끝남 (3027 불굴 · 3044 아직 죽지 않았다) */

	// 체력이 바뀔 때마다 부름, 조건 참 & 아직 안 켜짐 -> 켜짐, 켜짐 & 조건 거짓 -> 끝남(이번 전투에 다시 안 켜짐)
	// 피해 계산 때만 보면 깎였다가 맞기 전에 회복한 것을 놓쳐 두 번 켜짐
	void RefreshJobOnceStates();

	bool IsJobOnceStateActive(int32 JobSkillId) const;

	// 이번 전투에 한 번이라도 켜졌던 것, 계측이 켜짐 · 끝남을 찍는 데 씀
	const TSet<int32>& GetUsedJobOnceStates() const;


	/* 자원이 켜는 상태 — 템포가 상한에 닿으면 번아웃 (전량 소멸하고 다음 턴 페널티), 자원표의 상태 칸이 규칙을 정함 */

	// 그 상태가 지금 켜져 있는지, 전투 밖이면 거짓, 조건 DuringState · AfterState 가 이걸 읽음
	bool IsJobStateActive(FName StateName) const;

	// 조건 AfterState — 턴 제한이 없는 상태(임계)는 끝난 뒤 TurnsAfter 턴째인지, 기간이 있는 상태(번아웃)는 켜져 있는지
	// 번아웃은 상태 자체가 "다음 턴 1턴짜리" 라 켜져 있는 동안이 곧 그 턴이고, 임계는 소모된 다음 턴이 그 턴임
	bool IsJobStateAfterEnd(FName StateName, int32 TurnsAfter) const;

	// 자원표의 상태 칸대로 상태를 등록함, 이미 있으면 끝 턴만 늘림 (번아웃 중에 또 상한에 닿으면 페널티가 한 턴 더)
	// bForceImmediate 는 계측 전용 - 시작 시점 규칙을 무시하고 지금 턴부터 켬
	void EnterJobState(const FNUJobResourceDefinition& Definition, bool bForceImmediate = false);

	// 켜진 상태를 지금 닫음, 턴 제한이 없는 상태가 자원 소모로 끝날 때 씀 (임계)
	// 닫은 턴을 기억해 AfterState 가 "끝난 뒤 N턴째" 를 알게 함
	void EndJobState(FName StateName);

	// 마법 스킬을 쓰면 0 이 되는 자원(DecayType OnMagicUse)을 비우고 그 상태를 닫음 (17018 임계)
	// 필살기와 기본 공격은 마법으로 치지 않음, 행동당 한 번 불러야 함
	void ConsumeJobResourcesOnMagicUse(int32 SkillId, const TArray<FNUJobSkillEffectRow>& Effects);

	// 새 턴이 열릴 때 부름, 끝 턴이 지난 상태를 닫고 상태 스탯을 다시 셈
	// 명령을 고르기 전이라 SP 상한이 이번 턴 명령에 반영됨
	void AdvanceJobStates(int32 TurnNumber);

	const TArray<FNUJobStateInstance>& GetJobStates() const;

	// 상태를 켜는 "가장 많이 쌓인 적" 판정이 마지막으로 본 값, 자원 이름별 (3207 문양)
	const TMap<FName, int32>& GetLastSeenMostStacked() const;


	/* 액티브 전투당 N회 — 스킬을 쓴 횟수, 한도는 표가 들고 있음 (3310 역방향 · 3328 대역전 · 3329 은자의 등불 · 3228 심층 명상) */

	// 행동이 실제로 나갈 때 한 번 부름, 제출이 아니라 실행이라 기절 등으로 못 나간 행동은 안 셈
	void NoteActiveSkillUsed(int32 SkillId);

	int32 GetActiveSkillUseCount(int32 SkillId) const;

	const TMap<int32, int32>& GetActiveSkillUseCounts() const;

public:
	FOnBattleEncounterChanged OnBattleEncounterChanged;

	// 상태이상이 붙거나 스택이 오르거나 만료될 때 알림, 표시를 다시 그리는 용도
	FOnStatusEffectsChanged OnStatusEffectsChanged;
	
	// 보호막이 붙거나 닳거나 만료될 때 알림, 체력바 위에 겹쳐 그릴 용도
	FOnShieldChanged OnShieldChanged;
	
	// 직업 자원 수치나 직업 상태가 바뀔 수 있을 때 알림, 표시를 다시 그리는 용도
	// 획득 · 소모 · 감쇠 · 상태 켜짐 · 끝남 · 전투 정리에서 부름, 안 바뀌었어도 올 수 있어 받는 쪽이 비교함
	FOnJobResourcesChanged OnJobResourcesChanged;

	// 카드를 뽑았을 때 알림, 표시를 붙이는 용도
	FOnJobCardDrawn OnJobCardDrawn;
	
private:
	UFUNCTION()
	void OnRep_CurrentAvatar();
	
	UFUNCTION()
	void OnRep_Encounter();
	
	void GrantTurnBasedEffects(const FActiveGameplayEffectHandle& Handle,
		int32 Turns, int32 StatusEffectId, UNUAbilitySystemComponent* SourceASC);
	
	void ApplyStatusTickDamage(const FNUTurnBasedEffect& Effect);
	
	// 자기 구속 상태가 풀릴 때 그 스킬의 피해를 대상 전체에게 터뜨림
	void ReleaseSelfBoundState(const FNUSelfBoundState& State);
	
	// 어빌리티를 거치지 않고 스킬 피해를 한 대상에게 보냄
	void SendSkillDamage(UNUAbilitySystemComponent* TargetASC,
		const FNUActiveSkillDefinition& Definition, const NUActiveSkill::FDamageParams& DamageParams);
	
	// 턴 기반 회복적용
	void ApplyTurnBasedHealTick(const FNUTurnBasedEffect& Effect);

	// 상태이상 목록이 바뀌었음을 알림, 부여/스택/만료/정리 네 곳에서 부름
	void NotifyStatusEffectsChanged();
	
	// 직업 자원 · 상태가 바뀌었음을 알림
	void NotifyJobResourcesChanged();
	
	// 스택이 상한에 닿았을 때 터지는 피해, 폭발과 감전
	// 터지면 스택을 소멸시켜야 하므로 GE 제거까지 이 함수가 함
	void ApplyStatusStackBurst(const FActiveGameplayEffectHandle& Handle,
		const FNUStatusEffectDefinition& Definition, UNUAbilitySystemComponent* SourceASC);
	
	// 상태이상 피해를 대상에게 보냄, 틱 피해와 스택 폭발과 광역이 같은 GE 를 씀
	void SendStatusDamage(UNUAbilitySystemComponent* TargetASC,
		UNUAbilitySystemComponent* SourceASC, float Damage);
	
	// 광역 피해를 받을 같은 진영 전투원들, 본인은 제외함
	void CollectSplashTargets(TArray<UNUAbilitySystemComponent*>& OutTargets) const;
	
	// 반대 진영의 살아 있는 전투원들
	void CollectOpposingTargets(TArray<UNUAbilitySystemComponent*>& OutTargets) const;

	// 이 전투원에게 닿을 수 있는 상시 직업 줄의 주인들, 자신과 살아 있는 같은 편 (전투 밖이면 자신만)
	// 주인마다 줄을 따로 골라야 함 - 구간 묶음 열쇠가 스킬 번호라 배열을 합치면 같은 스킬을 가진 두 주인이 한 묶음으로 뭉침
	void CollectJobRowOwners(TArray<const UNUAbilitySystemComponent*>& OutOwners) const;
	
	void CollectTeamTargets(bool bSameTeam, TArray<UNUAbilitySystemComponent*>& OutTargets) const;

	// 이 전투원의 전투원 ID, 없으면 INDEX_NONE
	// 아군은 전투 진입 때 CurrentAvatar 에 전투 아바타가 들어가지만 적은 CurrentAvatar 를 안 써서 참가자 목록에서 주인으로 찾음
	int32 ResolveCombatantId() const;
	
	FActiveGameplayEffectHandle FindStatusEffectHandle(int32 StatusEffectId);
	void AddStatusEffectStack(const FActiveGameplayEffectHandle& Handle, int32 MaxStack, int32 Turns, 
		UNUAbilitySystemComponent* SourceASC);
	
	const FNUStatusEffectDefinition* FindStatusEffectDefinition(int32 StatusId) const;
	
	// 이 상태이상을 해제하는 아이템과 막는 패시브 행들, 없으면 nullptr
	
	// 이름·등급만 사용, 무엇을 하는지는 효과 줄이 정함
	const FNUPassiveDefinition* FindPassiveDefinition(int32 PassiveId) const;

	// 로그에 쓸 이름, 정의를 못 찾으면 번호를 돌려줌
	FString GetPassiveName(int32 PassiveId) const;

	// 보유 패시브의 효과 줄을 한 줄로 펴서 다시 담음, 목록이 바뀔 때마다 부름
	void RebuildOwnedPassiveEffects();
	void RebuildOwnedJobEffects();

	// 보유 패시브 중 이 효과 타입의 첫 줄, 없으면 nullptr
	const FNUPassiveEffectRow* FindOwnedPassiveEffect(ENUPassiveEffectType EffectType, int32& OutPassiveId) const;

	// 확률 판정, TriggerChance 가 0이면 조건만 맞으면 확정 발동
	// 사건이 한 번 일어날 때 한 번만 불러야 함, 값을 묻는 자리에서 부르면 안 됨
	// 아직 안 썼으면 참을 돌려주며 그 자리에서 쓴 것으로 표시함
	// 부르는 것이 곧 소모라 발동이 확정된 자리에서만 불러야 함
	bool TryConsumeOncePerBattle(int32 PassiveId);
	
	bool RollPassiveChance(const FNUPassiveEffectRow& Effect) const;

	// 거는 상태이상의 지속 턴에 더할 값 (ex) 2820 잔향)
	// 상태이상을 거는 자리 한 곳에서만 부름, 여러 곳에서 부르면 같은 부여에 두 번 붙음
	int32 SumStatusDurationBonus() const;
	
	// 이 패시브의 발동 조건이 지금 만족되는지
	// 값을 묻는 자리(피해 계산 등)에서 부르는 것이라, 그 자리에서 판정할 수 있는 트리거만 참이 됨
	bool IsPassiveTriggerSatisfied(const FNUPassiveEffectRow& Effect) const;
	
	// 보유 패시브 중 조건 없이 켜지는 스탯 증감을 모아 GE 하나로 걸음
	// 상태이상시 조건이 붙은 행도 조건이 맞을 때 같이 실림
	// 목록이 바뀔 때마다 다시 호출함, 이전것을 지우고 새로 거는 방식
	void ApplyPassiveStatModifiers();

	// 상태이상 유무를 조건으로 켜지는 스탯 패시브를 다시 걸음 (ex) 2827 고난의 갑주)
	// 조건이 뒤집힐 때만 다시 걸음
	void RefreshStatusConditionPassives();

	// 그 트리거를 단 회복 패시브를 모아 적용함, 턴시작과 처치시가 같이 사용함
	void ApplyHealPassivesFor(ENUPassiveTrigger Trigger);
	
	// 2833 동기화 - 상대 진영에서 하나를 골라 그 보유 패시브를 이번 전투 동안 같이 가짐
	// SelfPassiveId 는 동기화 자신의 번호, 복사 목록에서 빼려고 받음
	void ApplySynchronizePassive(int32 SelfPassiveId);

	// 2834 에너지 순환 - 처치한 그 행동에 쓴 SP 의 일부만큼 다음 턴 MaxSP 를 올림
	// 회복이 아니라 상한을 올리는 행이라 ApplyHealPassivesFor 목록을 안 타고 따로 있음
	void ApplyEnergyCyclePassive();

	// 스탯 변경 GE 를 만들어 걸고 핸들을 돌려줌, 수명은 부르는 쪽이 정함
	// 전투 범위와 턴 범위가 이 한 곳을 같이 씀, 갈라두면 축을 채우는 규칙이 두 벌이 됨
	FActiveGameplayEffectHandle ApplyStatModifierEffect(UNUAbilitySystemComponent* SourceASC,
		ENUStatusAffectedStat Stat, float Value, bool bIsPercent);

	// 보호막·반사·속성 위력의 남은 턴을 함께 줄임
	// 턴 경계가 하나뿐이라 AdvanceTurnBasedEffects 안에서 부름, 부르는 곳을 늘리면 빠뜨리게 됨
	// 이번 행동이 그 자원을 명시로 주는지, 주면 직업 기본 축적은 넘김
	bool ActionGrantsJobResource(FName ResourceName) const;
	
	// 자원표가 적은 감소를 한 턴만큼 진행함, 턴 경계에서 한 번
	void AdvanceJobResourceDecay();

	// 턴이 닫힐 때 상대의 줄이 이 전투원의 자원 수만큼 주는 지속 피해 (3207 문양 1중첩당 마법 공격력 x52%)
	// 방어력 · 치명타 · 회피 · 패링을 안 거치는 상태이상 도트와 같은 길로 보냄, 감쇠보다 먼저 불러야 이번 턴 수로 셈
	void ApplyJobResourceTickDamage();
	
	// 이 자원이 상시로 감소하지 않는지 (3035 영력 고정)
	bool HasPermanentDecayImmunity(FName ResourceName) const;
	
	// 기간제 감소 면역을 자원 칸에 걺, 칸이 없으면 생성함
	void ApplyJobResourceDecayImmunity(const FNUJobSkillEffectRow& Row);
	
	// 기간제 획득 배수를 자원 칸에 걺 (3437 대지의 뿌리)
	void ApplyJobResourceGainBonus(const FNUJobSkillEffectRow& Row);
	
	// 자원 칸을 찾고 없으면 생성함 — 수치가 0 이어도 버프는 걸려야 함
	FNUJobResourceInstance* FindOrAddJobResourceSlot(FName ResourceName);
	
	// 자원표가 정한 자연 충전량을 보유 줄이 갈아끼울 수 있음 (3127 각성 촉진 — AG 20 -> 29)
	// 표가 StackType 으로 가름 — 비어 있으면 값이 곧 새 획득량
	int32 ResolveNaturalGainAmount(FName ResourceName, int32 BaseAmount) const;
	
	// 획득량에 붙는 배수와 턴당 상한, 소모에는 안 걸림
	int32 ApplyJobResourceGainModifiers(const FNUJobResourceInstance& Instance, int32 Delta) const;
	
	void AdvanceBattleAxes();
	
	// 이 전투에만 유지되는 상태를 전부 되돌림, 전투 경계와 전투 종료 정리 양쪽에서 부름
	void ClearBattleScopedState();
	
	// 자원 수치에 비례하는 스탯 줄(3104 템포 3당 SP 상한 +1)의 값을 다시 셈, 이름을 비우면 자원 아무거나
	// 상시 스탯 GE 에 실으면 GE 를 걸 때 값으로 굳어서 따로 걸어 두고 자원이 바뀔 때마다 부름
	void RefreshResourceScaledStats(FName ResourceName = NAME_None);

	// 상태가 켜져 있는 동안만 붙는 상시 스탯 줄(3104 번아웃 SP 상한 -70%)의 값을 다시 셈, 켜진 상태의 줄만 실림
	// 자원 비례 스탯과 같은 모양 - 걸어 둔 GE 는 지우지 않고 값만 고침 (지웠다 다시 걸면 현재 SP 가 깎임)
	void RefreshJobStateStats();

	// 상대 중 가장 많이 쌓인 쪽의 자원이 1 이상에서 0 이 되면 켜지는 상태를 봄 (3207 지맥 붕괴 - 자원표 DropToZero)
	// 그 자원을 여는 전직 노드를 가진 전투원만, 전투 시작의 0 에서는 안 켜짐
	void EvaluateDropToZeroStates(bool bFromDeath);

	// 상대가 가진 "이 전투원에게 붙은 자원 1당" 스탯 줄의 값을 다시 셈 (3107 호라이즌 — 마크 1중첩당 속도 -5)
	// 자원이 대상에게 붙어 있어 줄 주인이 아니라 자원을 든 쪽이 GE 를 들고 있음, 이 전투원의 자원이나 상대의 보유 줄이 바뀔 때 부름
	void RefreshOpponentScaledStats();
	
private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentAvatar)
	TObjectPtr<AActor> CurrentAvatar;
	
	UPROPERTY(ReplicatedUsing = OnRep_Encounter)
	TObjectPtr<ANUBattleEncounter> Encounter;
	
	UPROPERTY(EditAnywhere, Category = "NU|Ability")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
	
	// 턴 단위로 지속되는 GameplayEffect 목록들
	// 화상, 둔화, 감전, 기절, 방어 등
	UPROPERTY()
	TMap<FActiveGameplayEffectHandle, FNUTurnBasedEffect> TurnBasedEffects;

	// 보유한 액티브 스킬, 서버 전용
	// 어빌리티가 ServerOnly라 복제하지 않음
	UPROPERTY()
	TArray<int32> ActiveSkillIds;

	// 켜져 있는 패시브들, 위와 같은 이유로 복제하지 않음
	UPROPERTY()
	TArray<int32> PassiveSkillIds;

	// 보유 패시브의 효과 줄을 펴서 담아 둔 것, SetPassiveSkillIds 가 다시 채움
	// 표를 재임포트하면 서브시스템의 배열이 재할당되므로 포인터를 들고 있으면 안됨
	TArray<FNUOwnedPassiveEffect> OwnedPassiveEffects;

	// 성장 노드로 얻은 직업 스킬, 위와 같은 이유로 복제하지 않음
	UPROPERTY()
	TArray<int32> JobSkillIds;

	// 보유 직업 스킬의 효과 줄, SetJobSkillIds 가 다시 채움
	TArray<FNUOwnedJobEffect> OwnedJobEffects;

	// 이번 전투에서 이미 쓴 전투당 1회 패시브들, 전투에 들어갈 때 비움
	UPROPERTY()
	TSet<int32> UsedOncePerBattlePassives;
	
	// 이 전투 동안만 유지되는 GE, 전투가 끝나면 제거
	TArray<FActiveGameplayEffectHandle> BattleScopedEffects;

	// 체력보다 먼저 닳는 흡수량, 부여한 스킬마다 한 칸
	UPROPERTY()
	TArray<FNUShieldInstance> Shields;

	// 걸려 있는 반사 피해, 부여한 스킬마다 한 칸
	UPROPERTY()
	TArray<FNUReflectInstance> Reflects;
	
	// 걸려 있는 속성 위력 배수, 부여한 스킬마다 한 칸
	UPROPERTY()
	TArray<FNUElementPowerInstance> ElementPowers;

	// 액티브 스킬로 건 턴 기반 받는 피해 배수, 상시 적용 효과는 여기로 오지 않음
	UPROPERTY()
	TArray<FNUDamageTakenModInstance> DamageTakenMods;
	
	// 이번 전투에서 속성별로 스킬을 쓴 횟수, 전투가 끝나면 비움
	UPROPERTY()
	TMap<FGameplayTag, int32> ElementSkillUseCounts;
	
	// 스스로를 묶고 있는 상태들, 사용한 스킬마다 한 칸
	UPROPERTY()
	TArray<FNUSelfBoundState> SelfBoundStates;
	
	// 직업 자원, 자원표가 Reset 이라고 한 것은 전투가 끝나면 비움
	UPROPERTY()
	TArray<FNUJobResourceInstance> JobResources;
	
	// 속성을 가려 걸린 디버프, 스킬과 속성 짝마다 한 칸
	UPROPERTY()
	TArray<FNUElementAfflictionInstance> ElementAfflictions;
	
	// 스킬이 걸어둔 공격시 부여, 건 스킬마다 한 칸
	UPROPERTY()
	TArray<FNUOnAttackGrant> OnAttackGrants;
	
	// 2833 동기화가 목록을 늘리기 전의 보유 패시브
	// 나이아 상태는 전투 밖에서도 살아 있어, 되돌리지 않으면 복사한 패시브가 다음 전투까지 따라감
	UPROPERTY()
	TArray<int32> PassiveSkillIdsBeforeSync;

	bool bPassiveSkillIdsSynced = false;

	// 상시 스탯 증감 GE, 다시 걸 때 지우려고 들고 있음	
	FActiveGameplayEffectHandle PassiveStatHandle;

	// 자원 비례 스탯 GE, 자원이 바뀔 때 지우지 않고 값만 고치려고 들고 있음
	FActiveGameplayEffectHandle ResourceScaledStatHandle;

	// 자원이 켠 상태들, 전투를 정리할 때 비움
	UPROPERTY()
	TArray<FNUJobStateInstance> JobStates;

	// 턴 제한 없는 상태가 마지막으로 닫힌 턴, 상태 이름별 — 조건 AfterState 가 읽음, 전투를 정리할 때 비움
	TMap<FName, int32> JobStateEndTurns;

	// 상태 스탯 GE, 상태가 바뀔 때 지우지 않고 값만 고치려고 들고 있음
	FActiveGameplayEffectHandle JobStateStatHandle;

	// 가장 많이 쌓인 적의 자원이 마지막으로 본 값, 자원 이름별, 전투를 정리할 때 비움
	TMap<FName, int32> LastSeenMostStacked;

	// 상태이상시 조건이 붙은 스탯 패시브를 갖고 있는지, 보유 목록이 바뀔 때 갱신함
	bool bHasStatusConditionPassive = false;

	// 위 GE 를 걸 때 사용한 `상태이상이 걸려 있었는가` 값
	// 지금 상태와 다를 때만 다시 걸음
	bool bStatusConditionApplied = false;

	// 실행기 클래스별 SpecHandle, 처음 사용할 때 부여함
	UPROPERTY()
	TMap<TSubclassOf<UNUBattleGameplayAbility>, FGameplayAbilitySpecHandle> ExecutorHandles;

	// 발동 직전에 채워지고 ActivateAbility가 가져가면 비워짐
	UPROPERTY()
	FNUBattleActionContext PendingActionContext;

	// 지금 실행 중인 행동, BeginCurrentAction 과 EndCurrentAction 사이에만 채워짐
	UPROPERTY()
	FNUBattleActionContext CurrentActionContext;

	int32 CurrentActionHitCount = 0;

	TWeakObjectPtr<const UNUBattleGameplayAbility> CurrentActionAbility;

	// 이번 턴에 공격한 횟수, 턴이 닫힐 때와 전투를 정리할 때 0
	int32 HitsDealtThisTurn = 0;

	// 이번 턴에 뽑은 갬블러 카드, 전투가 끝나면 비움
	FName CurrentJobCard;

	// 전투당 1회 상태 — 지금 켜진 것과 이번 전투에 이미 켜졌던 것, 전투를 정리할 때 비움
	TSet<int32> ActiveJobOnceStates;
	TSet<int32> UsedJobOnceStates;

	// 액티브 스킬을 이번 전투에서 쓴 횟수, 전투를 정리할 때 비움
	// 전투당 1회 패시브 집합(UsedOncePerBattlePassives)과 따로 둠 - 그쪽은 bool 이고 직업 죽음 방지와 공유함
	TMap<int32, int32> ActiveSkillUsesThisBattle;

	// 상대 줄로 걸린 스탯 GE (3107 마크 1중첩당 속도 -5), 마크가 바뀔 때 지우지 않고 값만 고치려고 들고 있음
	FActiveGameplayEffectHandle OpponentScaledStatHandle;
};
