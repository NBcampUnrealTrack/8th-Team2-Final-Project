// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleGameplayAbility.h"

#include "Battle/Presentation/NUBattleActionPresentation.h"
#include "Battle/Interface/NUBattleEventInterface.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUSkillVFXProfile.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUStatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/Execution/NUDamageExecution.h"
#include "PlayerController/NUPlayerController.h"
#include "Battle/Data/NUJobResourceDefinition.h"

UNUBattleGameplayAbility::UNUBattleGameplayAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

const FGameplayTag& UNUBattleGameplayAbility::GetAbilityId() const
{
	return AbilityId;
}

ENUTargetPolicy UNUBattleGameplayAbility::GetTargetPolicy() const
{
	return TargetPolicy;
}

int32 UNUBattleGameplayAbility::GetActionPriority() const
{
	return ActionPriority;
}

FText UNUBattleGameplayAbility::GetDisplayName() const
{
	// 기본 공격과 도주는 표에 이름이 없어 코드에서 부여함
	if (AbilityId.MatchesTagExact(NU::Ability::Battle::BasicAttack))
	{
		return NUBattleActionName::GetBasicAttack();
	}

	if (AbilityId.MatchesTagExact(NU::Ability::Battle::Flee))
	{
		return NUBattleActionName::GetFlee();
	}

	// 나머지는 표가 이름을 덮어씀 - 스킬은 액티브 스킬 CSV, 아이템은 아이템 CSV
	// 여기까지 오는 것은 표에 걸리지 않은 어빌리티라 BP 값과 태그 이름을 차례로 씀
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	// NU.Ability.Battle.Naia.SelfHeal -> SelfHeal
	if (AbilityId.IsValid())
	{
		return FText::FromName(AbilityId.GetTagLeafName());
	}

	return FText::GetEmpty();
}

void UNUBattleGameplayAbility::RegisterParryPress(int32 CombatantId, float PressOffset)
{
	if (!ActionContext.bParryable)
	{
		return;
	}

	// 이 공격이 노리지 않는 전투원의 입력은 버림
	if (!IsTargetedCombatant(CombatantId))
	{
		return;
	}

	// 판정에 더 가까운 입력만 유지
	if (const float* Existing = ParryPresses.Find(CombatantId))
	{
		if (FMath::Abs(*Existing - ParryImpactOffset) 
			<= FMath::Abs(PressOffset - ParryImpactOffset))
		{
			return;
		}
	}
	
	ParryPresses.Add(CombatantId, PressOffset);
}

void UNUBattleGameplayAbility::RegisterDodgeRequest(int32 CombatantId)
{
	// 패링 가능한 공격은 회피 대상이 아님
	if (ActionContext.bParryable) 
	{
		return;
	}

	// 표적이 아닌 전투원까지 등록하면 맞지도 않을 공격에 SP를 사용하게 됨
	// 내 정령만 표적이면 정령 몫만 등록되고 캐릭터 SP는 그대로 남음
	if (!IsTargetedCombatant(CombatantId))
	{
		return;
	}

	DodgeRequests.Add(CombatantId);
}

float UNUBattleGameplayAbility::ResolveDodgeChance(const UNUAbilitySystemComponent* TargetASC,
	int32 CombatantId) const
{
	if (!IsValid(TargetASC))
	{
		return 0.f;
	}

	// 나이아는 표에 민첩이 없어서 0으로 읽힌다 - 기본 확률만 갖는다
	// 적 아바타는 StatAttributeSet 자체가 없어 어트리뷰트 조회가 무효라 가드가 필요함
	// (지금은 아군만 반응하므로 타지 않지만, 적 회피가 생기면 여기가 첫 함정이 된다)
	// 기획에서 나이아 회피율이 정해지면 이 함수에서 갈라야 함
	float Dexterity = 0.f;
	
	const FGameplayAttribute DexterityAttribute = UNUStatAttributeSet::GetDexterityAttribute();
	if (TargetASC->HasAttributeSetForAttribute(DexterityAttribute))
	{
		Dexterity = TargetASC->GetNumericAttribute(DexterityAttribute);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("회피 확률: 전투원 %d 에 민첩 어트리뷰트가 없어 기본 확률만 사용함"), CombatantId);
	}

	const float Chance = DodgeBaseChance + Dexterity * DodgeChancePerDexterity;
	return FMath::Clamp(Chance, 0.f, DodgeMaxChance);
}

int32 UNUBattleGameplayAbility::GetParrySPCost() const
{
	return ParrySPCost;
}

int32 UNUBattleGameplayAbility::GetDodgeSPCost() const
{
	return DodgeSPCost;
}

bool UNUBattleGameplayAbility::IsParryable() const
{
	return bParryable;
}

const FNUParryTimingSpec& UNUBattleGameplayAbility::GetParryTiming() const
{
	return ParryTiming;
}

bool UNUBattleGameplayAbility::IsCurrentlyReactable() const
{
	return ActionContext.bReactable;
}

bool UNUBattleGameplayAbility::IsCurrentlyParryable() const
{
	return ActionContext.bReactable && ActionContext.bParryable;
}

void UNUBattleGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                               const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                               const FGameplayEventData* TriggerEventData)
{
	// 이전 활성화에서 패링/닷지 태그가 남았다면 정리
	ClearStateTags();
	
	// 아래의 모션 구성, 타겟 해결, 효과 적용에서 필드를 읽으므로
	// 컨텍스트를 먼저 반영해야 함
	ApplyActionContext();

	// 타격 시점에 판정하는 쪽(단발 · 명시 자원 줄)이 읽도록 행동이 끝날 때까지 ASC 에 둠
	// 컨텍스트가 없는 기본 공격도 SP 추가 타격이 있어 타수는 따로 넘김
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
	if (IsValid(SourceASC))
	{
		SourceASC->BeginCurrentAction(this, ActionContext, GetActionHitCount());
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedTargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;

	// 행동 시간과 타격·시전 지점을 먼저 정함, 바로 아래 패링 창 / 타격 대기 / 종료가 전부 이 값을 사용함
	// 공격 모션이 있으면 그 길이와 모션의 Hit·Cast 표시, 없으면 어빌리티에 적힌 값 사용
	// 넷 전투(UNUBattleNetActionQueueSubsystem::PlayAction)와 같은 함수로 정함
	EffectiveActionTime = 0.f;
	EffectiveHitTimeNormalized = -1.f;
	EffectiveCastTimeNormalized = 0.f;
	if (const ANUBattleAvatar* TimingAvatar = Cast<ANUBattleAvatar>(ActorInfo->AvatarActor))
	{
		FNUBattleActionMotionSpec Probe;
		BuildActionMotionSpec(Probe);
		TimingAvatar->ResolveMotionTiming(Probe);

		EffectiveActionTime = Probe.ActionTime;
		EffectiveHitTimeNormalized = Probe.HitTimeNormalized;
		EffectiveCastTimeNormalized = Probe.CastTimeNormalized;
	}
	
	// 반응 가능한 행동은 대상을 캐시하고 창을 열음
	if (ActionContext.bReactable)
	{
		CacheTargetCombatantIds();
		ParryImpactOffset = GetImpactDelay();
		OpenInputWindows();
	}
	
	if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(ActorInfo->AvatarActor))
	{
		FNUBattleActionMotionSpec Spec;
		BuildActionMotionSpec(Spec);
		Spec.ActionLocation = ResolveActionLocation(Avatar, CachedTargetActor);
		
		Avatar->MulticastPlayAction(Spec);
	}
	
	UAbilityTask_WaitDelay* WaitHit = 
		UAbilityTask_WaitDelay::WaitDelay(this, GetPreHitDelay());
	WaitHit->OnFinish.AddDynamic(this, &UNUBattleGameplayAbility::HandleHitPoint);
	WaitHit->ReadyForActivation();
}

void UNUBattleGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearStateTags();
	
	// 다음 행동의 타격이 이 행동의 컨텍스트와 타수로 판정되지 않게 비움
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
	if (IsValid(SourceASC))
	{
		SourceASC->EndCurrentAction(this);
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UNUBattleGameplayAbility::ApplyActionContext()
{
	UNUAbilitySystemComponent* ASC =
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());

	ActionContext = IsValid(ASC) ? ASC->ConsumeActionContext() : FNUBattleActionContext();

	// 지난 행동에서 대상마다 센 계수 가산이 남지 않게 비움
	TargetCoefficientAdds.Reset();
	CoefficientConsumedTargets.Reset();

	// 컨텍스트가 없으면 CDO에 작성한 값으로 처리함
	// 현재 기본공격, 적 공격, 아이템, 도주가 대상
	if (!ActionContext.bValid)
	{
		return;
	}

	TargetPolicy = ActionContext.TargetPolicy;
	Motion = ActionContext.Motion;
	DamageCoefficient = ActionContext.DamageCoefficient;
	AdditiveDamageBonus = ActionContext.AdditiveDamageBonus;
	DamageMultiplier = ActionContext.DamageMultiplier;
	DefenseIgnore = ActionContext.DefenseIgnore;
	Lifesteal = ActionContext.Lifesteal;
	SelfHPCostRatio = ActionContext.SelfHPCostRatio;
	DamageType = ActionContext.DamageType;
	DamageBase = ActionContext.DamageBase;
	AttackElement = ActionContext.AttackElement;
	AbilityEffects = ActionContext.Effects;

	// 전투당 1회 곱연산 패시브(2836 최후의 일격)는 스킬로 공격할 때만 켜져야 함
	// 스킬이 아니면 SkillId 가 0이므로 이걸로 체크함
	if (ActionContext.SkillId > 0 && IsValid(ASC))
	{
		DamageMultiplier *= ASC->ConsumeOnceDamageMultiplier();
		
		// 자원에 비례하는 합연산 "주는 피해" 줄 (3133 템포 1당 +5.8%), 발동 순간의 자원으로 셈
		// 아래 계수 가산이 자원을 소모하기 전이어야 함, 스킬이 타격 뒤에 소모하는 것(템포 절반)도 그 전 값으로 셈
		AdditiveDamageBonus += ASC->SumJobActionDamageBonus(ActionContext.SkillId);
		
		// 자원에 비례해 기본 계수에 더하는 줄 (3029 영력 투사 = 255% + 영력 1당 3%p)
		// 계수에 직접 더함
		DamageCoefficient += ASC->ConsumeJobCoefficientAdd(ActionContext.SkillId);
		
		// 속성 스킬을 쓴 것을 셈 (2407 과전류 - 번개를 한 번 더 쓰면 위력 ×2가 켜짐)
		// 행동당 한 번 도는 자리라 여기서 확인함, 타격마다 도는 자리에 넣으면 연타 시 여러번 카운팅 됨
		ASC->NoteElementSkillUsed(ActionContext.AttackElement);

		// 전투당 N회 스킬의 횟수 (3310 역방향), 여기까지 온 것만 셈
		ASC->NoteActiveSkillUsed(ActionContext.SkillId);
	}
}

void UNUBattleGameplayAbility::PreApplyAbilityEffect()
{
	if (!ActionContext.bReactable)
	{
		return;
	}
	
	if (ActionContext.bParryable)
	{
		ResolveParryWindow();
	}
	else
	{
		ResolveDodgeWindow();
	}
}

void UNUBattleGameplayAbility::HandleHitPoint()
{
	// 행동 당 1회 미리 발동, 패링·회피 판정이 여기서 끝남
	// 반복 타격은 이를 다시 부르면 안 되므로 반복 타격은 ApplyHitEffects만 다시 호출하는 방식
	PreApplyAbilityEffect();

	// 확률형 회피는 피해가 들어가기 전에 처리해줘야 함
	ResolvePassiveEvade();

	ApplyHitEffects();

	// 확률형 공격시 패시브. 다타 스킬은 ApplyHitEffects 만 다시 부르므로
	// 여기 두면 굴림이 타격 수에 휘둘리지 않음
	ResolveOnAttackPassives();
	
	// 스킬을 썼을 때 쌓이는 자원 (문양), 대상에게 붙으므로 대상마다 한 번
	// 반복 타격은 ApplyHitEffects 만 다시 부르므로 여기 두면 행동당 한 번 쌓임
	TriggerJobResourceGainOnSkillUse();
	
	// 타격당이 아닌 자원 줄 (3406 동화율 총 +1 · 3133 템포 절반 · 3237 감쇠 면역)
	ApplyJobResourceRowsOnce();

	OnHitApplied();
}

void UNUBattleGameplayAbility::ApplyHitEffects()
{
	if (ANUBattleEncounter* Encounter = GetBattleEncounter(); Encounter != nullptr && Encounter->GetBattleEventHandler() != nullptr)
	{
		Encounter->GetBattleEventHandler()->NotifyAbilityHit(Encounter, Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()), AbilityId);
	}
	TArray<AActor*> Targets;
	ResolveEffectTargets(Targets);

	ApplyEffectsByTiming(ENUEffectTiming::BeforeHit, Targets);
	ApplyEffectsByTiming(ENUEffectTiming::OnHit, Targets);

	// 스킬이 요구하는 자기 체력, 대상 루프 밖이라 전체 공격이어도 한 번만 냄
	PaySelfHPCost();

	// 런타임에 GE가 결정되는 경우 처리 (아이템)
	// 시전자에게 가는 효과를 대상 루프 안에 두면 안 됨 - 전체 공격이면 적 수만큼 겹쳐 걸림
	ApplySelfRuntimeEffects();
	
	// 대상 집합과 다른 편을 가리키는 줄이 있으면 그 편을 따로 구해서 적용함
	ApplyCrossSideRuntimeEffects();
	
	for (AActor* Target : Targets)
	{
		ApplyRuntimeEffect(Target);
	}

	ApplyEffectsByTiming(ENUEffectTiming::AfterHit, Targets);
	
	// 이번 턴 공격 횟수 (3050 연쇄 · 3105 연사), 대상 수와 무관하게 타격 한 번에 하나
	// 이 타격의 피해가 다 들어간 뒤라, 타격 중의 조건 판정은 지금 타격을 뺀 셈을 봄
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
	if (IsValid(SourceASC) && Targets.Num() > 0 && IsAttackAction())
	{
		SourceASC->NoteAttackHitDealt();
	}
}

// 스킬 사용으로 쌓이는 자원, 대상에게 붙는 것이라 대상 집합을 다시 구해서 걸음
void UNUBattleGameplayAbility::TriggerJobResourceGainOnSkillUse()
{
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
	
	// 대기 슬롯은 발동 순간 비므로 지금은 늘 여기서 돌아감 — 일부러 안 고침
	// ActionContext 로 고치면 정령술사의 모든 스킬이 대상(아군 대상 스킬이면 아군)에게 문양 +1 을 새기는데
	// 표 설명은 [각인] 계열만 새긴다고 함 (3207 · 3239 · 3242), 문양을 무엇이 쌓는지 기획 답을 받은 뒤 정할 것
	if (!IsValid(SourceASC) || !SourceASC->PeekActionContext().bValid)
	{
		return;
	}
	
	TArray<AActor*> Targets;
	ResolveEffectTargets(Targets);
	
	for (AActor* Target : Targets)
	{
		UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));
		
		if (IsValid(TargetASC))
		{
			SourceASC->TriggerJobResourceGain(ENUJobResourceGainTrigger::OnSkillUse, TargetASC);
		}
	}
}

void UNUBattleGameplayAbility::ApplyJobResourceRowsOnce()
{
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
	
	if (!IsValid(SourceASC) || !ActionContext.bValid)
	{
		return;
	}
	
	SourceASC->ApplyActionJobResourceRows(ActionContext.SkillId);
}

// 자해 스킬 HP 소모, SP와 달리 제출 시점이 아니라 타격 시점에 소모함
// 표에서 사용 후 / 타격마다 로 명시됨
void UNUBattleGameplayAbility::PaySelfHPCost()
{
	if (SelfHPCostRatio <= 0.f)
	{
		return;
	}

	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(SourceASC))
	{
		return;
	}

	const float MaxHealth = SourceASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
	const float Current = SourceASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute());
	const float Cost = MaxHealth * SelfHPCostRatio;

	// HP 소모로 죽지는 않고, 1만 남게 함
	const float NewHealth = FMath::Max(1.f, Current - Cost);

	SourceASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(), NewHealth);
}

// 피해 실행기를 거는 효과 항목이 대상에게 가는 행동만 공격으로 셈
// 기본 공격 BP 가 어떤 GE 클래스를 쓰는지에 기대지 않으려고 GE 의 실행기를 봄
bool UNUBattleGameplayAbility::IsAttackAction() const
{
	for (const FNUAbilityEffectEntry& Entry : AbilityEffects)
	{
		if (Entry.ApplyTo == ENUEffectApplyTo::Self || !Entry.EffectClass)
		{
			continue;
		}
		
		const UGameplayEffect* Effect = Entry.EffectClass.GetDefaultObject();
		if (!Effect)
		{
			continue;
		}
		
		const bool bDealsDamage = Effect->Executions.ContainsByPredicate(
			[](const FGameplayEffectExecutionDefinition& Execution)
			{
				return Execution.CalculationClass
					&& Execution.CalculationClass->IsChildOf(UNUDamageExecution::StaticClass());
			});
		
		if (bDealsDamage)
		{
			return true;
		}
	}
	
	return false;
}

void UNUBattleGameplayAbility::OnHitApplied()
{
	// 기본 Ability는 1타로 끝
	ScheduleFinish();
}

void UNUBattleGameplayAbility::ScheduleFinish()
{
	float PostHit = GetActionTime() * (1.f - GetHitTimeNormalized()) + ReturnTime;
	UAbilityTask_WaitDelay* WaitEnd = UAbilityTask_WaitDelay::WaitDelay(this, PostHit);
	WaitEnd->OnFinish.AddDynamic(this, &UNUBattleGameplayAbility::HandleSequenceFinished);
	WaitEnd->ReadyForActivation();
}

void UNUBattleGameplayAbility::HandleSequenceFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UNUBattleGameplayAbility::ResolvePassiveEvade()
{
	// 반응 창을 여는 행동, 즉 이쪽이 맞는 행동에서만 처리함
	// 아군 대상 스킬까지 회피시키면 회복이 빗나가게 됨
	if (!ActionContext.bReactable)
	{
		return;
	}

	TArray<AActor*> Targets;
	ResolveEffectTargets(Targets);

	for (AActor* Target : Targets)
	{
		UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));

		if (!IsValid(TargetASC))
		{
			continue;
		}

		// 회피 창으로 이미 피한 대상은 다시 굴리지 않음
		if (TargetASC->HasMatchingGameplayTag(NU::State::Dodging))
		{
			continue;
		}

		if (!TargetASC->RollPassiveEvade())
		{
			continue;
		}

		// 태그를 걷는 자리가 회피 창과 같아야 하므로 같은 목록에 넣음
		TargetASC->AddLooseGameplayTag(NU::State::Dodging);
		DodgedASCs.Add(TargetASC);

		if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Target))
		{
			Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::Dodge));
		}
	}
}

void UNUBattleGameplayAbility::ResolveOnAttackPassives()
{
	// 적을 때리는 행동에서만 처리됨, 회복·버프의 대상에게 상태이상을 걸면 안 됨
	if (TargetPolicy != ENUTargetPolicy::SingleEnemy && TargetPolicy != ENUTargetPolicy::AllEnemies)
	{
		return;
	}

	UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());

	if (!IsValid(SourceASC))
	{
		return;
	}

	TArray<AActor*> Targets;
	ResolveEffectTargets(Targets);

	for (AActor* Target : Targets)
	{
		UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));

		if (!IsValid(TargetASC) || TargetASC == SourceASC)
		{
			continue;
		}

		// 회피한 대상은 TryApplyStatusEffect 가 안에서 걸러낸다
		SourceASC->TriggerOnAttackPassives(TargetASC);
		
		// 스킬이 걸어둔 공격시 부여도 같은 자리에서 굴림 (2007 · 2008)
		// 기본 공격은 SkillId 가 0이라 그걸로 스킬인지 확인함, 표의 Normal 이 이것을 가리킴
		SourceASC->TriggerOnAttackGrants(TargetASC, AttackElement, ActionContext.SkillId > 0);
	}
}

void UNUBattleGameplayAbility::ApplyEffectsByTiming(ENUEffectTiming Timing, const TArray<AActor*>& Targets)
{
	UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(SourceASC))
	{
		return;
	}
	
	AActor* SelfActor = GetAvatarActorFromActorInfo();
	
	for (const FNUAbilityEffectEntry& Entry : AbilityEffects)
	{
		// 등록된 타이밍이랑 다르면 X
		if (Entry.Timing != Timing) continue;
		
		// 자기 자신 대상은 대상 수와 무관하게 1회 적용
		if (Entry.ApplyTo == ENUEffectApplyTo::Self)
		{
			ApplyEffectEntry(Entry, SourceASC, SelfActor);
			continue;
		}
		
		for (AActor* Target : Targets)
		{
			ApplyEffectEntry(Entry, SourceASC, Target);
		}
	}
}

void UNUBattleGameplayAbility::ApplyEffectEntry(const FNUAbilityEffectEntry& Entry,
	UNUAbilitySystemComponent* SourceASC, AActor* TargetActor)
{
	if (!IsValid(SourceASC) || !IsValid(TargetActor))
	{
		return;
	}
	
	UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor));
	if (!IsValid(TargetASC))
	{
		return;
	}
	
	// 회피 성공은 공격 자체를 피한 판정이라 이 공격에 딸린 것을 전부 무효로 함
	// 피해는 NUDamageExecution 이 0으로 만들지만 상태이상·도트는 GE 로 따로 붙어서,
	// 여기서 막지 않으면 회피했는데 화상만 걸리는 경우가 생김
	// 흡혈은 피해 모디파이어가 안 붙으면 같이 사라지므로 별도 처리가 필요 없음
	if (TargetASC->HasMatchingGameplayTag(NU::State::Dodging))
	{
		return;
	}
	
	// 대상 조건, 이 대상이 그 상태여야 켜지는 항목이 있음 (2410 - 감전·마비면 2배 + 기절)
	// 조건 없는 항목은 언제나 만족으로 봄
	bool bConditionMet = true;
	
	if (Entry.ConditionStatusIds.Num() > 0)
	{
		bConditionMet = TargetASC->HasStatusEffectMatching(
			[&Entry](const FNUStatusEffectDefinition& Definition)
			{
				return Entry.ConditionStatusIds.Contains(Definition.StatusEffectId);
			});
	}
	
	// 조건이 적용 여부를 가르는 항목은 여기서 끊김
	if (!bConditionMet && Entry.bSkipWhenConditionFails)
	{
		return;
	}
	
	// 상태이상은 GE를 정의에서 선택하므로 별도 처리
	// EffectClass 가 비어있으므로 미리 처리해야 함
	if (Entry.StatusEffectId > 0)
	{
		SourceASC->TryApplyStatusEffect(
			TargetASC,
			Entry.StatusEffectId, 
			Entry.StatusEffectChance, 
			Entry.DurationTurns
			);
	}
	
	// 데미지/회복 GE는 기존 그대로
	if (!Entry.EffectClass)
	{
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = 
		MakeOutgoingGameplayEffectSpec(Entry.EffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	// 대상 체력 조건 계수 (5직업 처형기 - 35% 이하면 225% 대신 380%), 맞기 전 체력으로 가름
	// 기본 계수와 배타라 곱하지 않고 바꿈, 발동할 때 더한 계수 가산(자원 비례)은 바꾼 계수에도 그대로 얹음
	float Coefficient = DamageCoefficient;
	if (Entry.ConditionTargetHPBelow > 0.f)
	{
		const float MaxHealth = TargetASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
		const float HealthPercent = MaxHealth > 0.f
			? TargetASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute()) / MaxHealth * 100.f
			: 100.f;

		if (HealthPercent <= Entry.ConditionTargetHPBelow)
		{
			const float CoefficientAdd = DamageCoefficient - ActionContext.DamageCoefficient;
			Coefficient = Entry.ConditionalCoefficient + CoefficientAdd;

			// 피해 숫자만으로는 계수가 바뀌었는지 가리기 어려워 남김
			UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 대상 체력 %.0f%% <= %.0f%% 라 계수 %.2f -> %.2f"),
				ActionContext.SkillId, HealthPercent, Entry.ConditionTargetHPBelow, DamageCoefficient, Coefficient);
		}
	}

	// 대상 중첩 조건 계수 (3216 연쇄 폭발 - 약점 5중첩이면 320% 대신 440%), 체력 조건과 같은 교체 계수를 씀
	// 이 타격이 쌓는 약점은 피해가 들어간 뒤에 붙으므로 여기서는 맞기 전에 있던 것만 셈
	if (Entry.ConditionTargetStackCount > 0 && !Entry.ConditionTargetStack.IsNone())
	{
		int32 Stacks = 0;
		TargetASC->TryGetJobResource(Entry.ConditionTargetStack, Stacks);

		if (Stacks >= Entry.ConditionTargetStackCount)
		{
			const float CoefficientAdd = DamageCoefficient - ActionContext.DamageCoefficient;
			Coefficient = Entry.ConditionalCoefficient + CoefficientAdd;

			// 피해 숫자만으로는 계수가 바뀌었는지 가리기 어려워 남김
			UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 대상의 %s %d 개 >= %d 라 계수 %.2f -> %.2f"),
				ActionContext.SkillId, *Entry.ConditionTargetStack.ToString(), Stacks,
				Entry.ConditionTargetStackCount, DamageCoefficient, Coefficient);
		}
	}

	// 대상에게 붙은 자원에 비례하는 계수 가산 (3140 마크 1당 +150%p), 대상마다 행동당 한 번 셈
	// 처형 계수로 바뀐 경우에도 그대로 얹음
	const bool bTargetEntry = Entry.ApplyTo == ENUEffectApplyTo::Target;
	if (bTargetEntry)
	{
		Coefficient += ResolveTargetCoefficientAdd(SourceASC, TargetASC);
	}

	// 데미지는 어빌리티 전체 속성이므로 항상 주입, 사용하지 않는 GE는 무시됨
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::Coefficient, Coefficient);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::DefenseIgnore, DefenseIgnore);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::Lifesteal, Lifesteal);
	
	// 합연산과 곱연산은 층이 달라서 한 칸에 합칠 수 없음, 실행기가 각각 다른 자리에서 씀
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::AdditiveBonus, AdditiveDamageBonus);
	// 대상 조건을 채운 대상에게만 붙는 곱연산 항을 여기서 얹음
	// 실행기는 칸 하나만 읽으므로 두 배수를 곱해서 보냄
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::Multiplier,
		DamageMultiplier * (bConditionMet ? Entry.ConditionalDamageMultiplier : 1.f));

	// 반드시 치명타인 항목 (3121 나이아의 유도), 실행기가 치명타를 굴리지 않음
	if (Entry.bForceCritical)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Damage::ForceCritical, 1.f);
	}

	// 물리, 마법 타입 태그 주입, 태그에 따라 공격력, 마법공격력 다르게 사용함
	// 공격 속성 주입, 해당 공격이 무슨 속성인지로 상성 적용
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(DamageType.IsValid() ? DamageType : NU::Damage::Type::Physical);

	// 계수의 기준 축, 안 붙이면 실행기에서 공격력 or 마법 공격력 사용
	if (DamageBase.IsValid())
	{
		TagContainer.AddTag(DamageBase);
	}
	if (AttackElement.IsValid())
	{
		TagContainer.AddTag(AttackElement);
	}
	SpecHandle.Data->AppendDynamicAssetTags(TagContainer);
	
	// 적용할 GameplayEffect가 부여할 태그,수치 적용 (디버프 강도, 회복, 도트 피해 등)
	for (const TPair<FGameplayTag, float>& Pair : Entry.Magnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
	}
	
	if (Entry.DurationTurns > 0 && Entry.ReplaceStat != ENUStatusAffectedStat::None && ActionContext.SkillId > 0)
	{
		// 같은 스킬이 다시 걸면 겹치지 않고 새로 건 것으로 바뀜 (3111 견제 화살 · 3411 포효), 다타여도 한 번분만 남음
		SourceASC->ApplyReplacingStatModifier(SpecHandle, TargetASC, Entry.DurationTurns,
			ActionContext.SkillId, Entry.ReplaceStat);
	}
	else if (Entry.DurationTurns > 0)
	{
		SourceASC->ApplyTurnBasedEffect(SpecHandle, TargetASC, Entry.DurationTurns, 0);
	}
	else
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
	
	// 소모는 피해가 들어간 뒤 — 같은 타격의 다른 판정(3248 문양 상한)이 소모 전 값을 봐야 함
	if (bTargetEntry)
	{
		ConsumeTargetCoefficientResources(SourceASC, TargetASC);
	}
}

float UNUBattleGameplayAbility::ResolveTargetCoefficientAdd(UNUAbilitySystemComponent* SourceASC,
	UNUAbilitySystemComponent* TargetASC)
{
	// 직업 표에만 있는 줄이라 스킬이 아니면 볼 것이 없음
	if (ActionContext.SkillId <= 0)
	{
		return 0.f;
	}
	
	const TWeakObjectPtr<UNUAbilitySystemComponent> Key(TargetASC);
	
	if (const float* Found = TargetCoefficientAdds.Find(Key))
	{
		return *Found;
	}
	
	const float Add = SourceASC->SumTargetJobCoefficientAdd(ActionContext.SkillId, TargetASC);
	TargetCoefficientAdds.Add(Key, Add);
	
	return Add;
}

void UNUBattleGameplayAbility::ConsumeTargetCoefficientResources(UNUAbilitySystemComponent* SourceASC,
	UNUAbilitySystemComponent* TargetASC)
{
	if (ActionContext.SkillId <= 0)
	{
		return;
	}
	
	// 다타여도 대상마다 한 번만 씀
	bool bAlreadyConsumed = false;
	CoefficientConsumedTargets.Add(TWeakObjectPtr<UNUAbilitySystemComponent>(TargetASC), &bAlreadyConsumed);
	
	if (bAlreadyConsumed)
	{
		return;
	}
	
	SourceASC->ConsumeTargetJobCoefficientResources(ActionContext.SkillId, TargetASC);
}

void UNUBattleGameplayAbility::BuildActionMotionSpec(FNUBattleActionMotionSpec& OutSpec) const
{
	OutSpec.Motion = Motion;
	OutSpec.ActionAnimation = ActionAnimation;
	OutSpec.ApproachTime = ApproachTime;
	OutSpec.ActionTime = GetActionTime();
	OutSpec.ReturnTime = ReturnTime;
	OutSpec.TelegraphTime = TelegraphTime;
	OutSpec.HitTimeNormalized = GetHitTimeNormalized();
	OutSpec.CastTimeNormalized = EffectiveCastTimeNormalized;

	// 탄환류 연출의 목적지, 타겟이 없으면(자기 자신 대상 스킬 등) 원점으로 남음
	// 해당 스킬은 ProjectileVFX 를 안 쓸 것이므로 문제 없음
	// 대상의 진형 자리 - 지금 위치는 앞 행동에서 아직 돌아오는 중일 수 있음
	OutSpec.ProjectileTargetLocation = IsValid(CachedTargetActor)
		? NUBattlePresentation::GetFormationLocation(CachedTargetActor)
		: FVector::ZeroVector;

	// 어빌리티의 기본 연출을 먼저 채움 (기본 공격)
	OutSpec.CastVFX = CastVFX;
	OutSpec.ProjectileVFX = ProjectileVFX;
	OutSpec.ImpactVFX = ImpactVFX;
	OutSpec.EffectColor = EffectColor;

	// 스킬별 연출 프로필(DT_SkillVFX) 이 있으면 통째로 덮어씀
	// 액티브 스킬이 아니면 SkillId가 0이라 조회되지 않음
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* DataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
	const FNUSkillVFXProfile* Profile =
		IsValid(DataSubsystem) ? DataSubsystem->FindSkillVFXProfile(ActionContext.SkillId) : nullptr;

	if (!Profile)
	{
		// 액티브 스킬인데 행이 없으면 어빌리티 기본값(근접 슬래시)으로 재생됨
		// 표 번호가 어긋났을 때 화면에서는 그냥 평타처럼 보여서 원인을 못 찾음
		if (ActionContext.SkillId > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("스킬 %d 의 연출 프로필이 DT_SkillVFX 에 없음, 기본 연출로 재생함"),
				ActionContext.SkillId);
		}

		return;
	}

	OutSpec.CastVFX = Profile->CastVFX;
	OutSpec.ProjectileVFX = Profile->ProjectileVFX;
	OutSpec.ImpactVFX = Profile->ImpactVFX;
	OutSpec.bHasSkillVFXProfile = true;	// 직업 기본 공격 연출보다 이김

	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();

	OutSpec.EffectColor = Profile->bUseElementColor && IsValid(Settings)
		? Settings->ResolveElementColor(AttackElement)
		: Profile->FixedColor;
	OutSpec.AttackElement = AttackElement;
}

void UNUBattleGameplayAbility::ApplyPresentationDefaults(FNUBattleActionMotionSpec& OutSpec) const
{
	OutSpec.ActionAnimation = ActionAnimation;
	OutSpec.CastVFX = CastVFX;
	OutSpec.ProjectileVFX = ProjectileVFX;
	OutSpec.ImpactVFX = ImpactVFX;
	OutSpec.EffectColor = EffectColor;
}

float UNUBattleGameplayAbility::GetPreHitDelay() const
{
	const float ToImpact = GetImpactDelay();
	
	if (!ActionContext.bReactable)
	{
		return ToImpact;
	}
	
	// 반응은 있지만 패링은 안하면 회피
	if (!ActionContext.bParryable)
	{
		return ToImpact + ParryNetworkSlack;
	}
	
	// 도전 구간이 판정 시각 이후까지 기다려야 그 구간 입력을 받을 수 있음
	return ToImpact + ActionContext.ParryTiming.GetTailTime() + ParryNetworkSlack;
}

float UNUBattleGameplayAbility::GetImpactDelay() const
{
	// 예고 시간만큼 모션 전체가 뒤로 밀림
	// 패링/닷지 가 없는 어빌리티(회복 등)는 TelegraphTime이 0 이라 상관 X
	// 제자리 행동은 아바타가 접근 단계를 건너뛰므로(ANUBattleAvatar::BeginActionMotion) 접근 시간을 더하지 않음
	const float EffectiveApproachTime = Motion == ENUBattleActionMotion::InPlace ? 0.f : ApproachTime;
	return TelegraphTime + EffectiveApproachTime + GetActionTime() * GetHitTimeNormalized();
}

float UNUBattleGameplayAbility::GetActionTime() const
{
	return EffectiveActionTime > KINDA_SMALL_NUMBER ? EffectiveActionTime : ActionTime;
}

float UNUBattleGameplayAbility::GetHitTimeNormalized() const
{
	return EffectiveHitTimeNormalized >= 0.f ? EffectiveHitTimeNormalized : HitTimeNormalized;
}

ANUBattleEncounter* UNUBattleGameplayAbility::GetBattleEncounter() const
{
	UNUAbilitySystemComponent* ASC = 
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	
	return ASC ? ASC->GetBattleEncounter() : nullptr;
}

void UNUBattleGameplayAbility::ResolveEffectTargets(TArray<AActor*>& OutTargets) const
{
	ResolveTeamTargets(TargetPolicy, OutTargets);
}

void UNUBattleGameplayAbility::ResolveTeamTargets(ENUTargetPolicy Policy, TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();
	
	if (Policy == ENUTargetPolicy::Self)
	{
		if (AActor* SelfActor = GetAvatarActorFromActorInfo())
		{
			OutTargets.Add(SelfActor);
		}
		return;
	}
	
	ANUBattleAvatar* SourceAvatar = Cast<ANUBattleAvatar>(GetAvatarActorFromActorInfo());
	ANUBattleEncounter* Encounter = GetBattleEncounter();
	
	// 전투 밖이거나, 정보가 없다면 단일 대상으로 전환
	if (!SourceAvatar || !IsValid(Encounter))
	{
		if(IsValid(CachedTargetActor))
		{
			OutTargets.Add(CachedTargetActor);
		}
		return;
	}
	
	ANUBattleAvatar* TargetAvatar = Cast<ANUBattleAvatar>(CachedTargetActor);
	Encounter->ResolveAbilityTargetActors(
		SourceAvatar->GetCombatantId(),
		Policy,
		TargetAvatar ? TargetAvatar->GetCombatantId() : INDEX_NONE,
		OutTargets
		);
}

void UNUBattleGameplayAbility::OpenInputWindows()
{
	ANUBattleEncounter* Encounter = GetBattleEncounter();
	if (!IsValid(Encounter))
	{
		return;
	}

	int32 CurrentActionSequence = Encounter->GetActionSequence();

	// 캐릭터와 정령이 둘 다 표적이면 소유자가 같으므로 한 번만 보냄
	TSet<const APlayerState*> Notified;

	for (int32 CombatantId : TargetCombatantIds)
	{
		FNUBattleParticipant Participant;
		if (!Encounter->GetParticipant(CombatantId, Participant)) continue;

		// 적 전투원은 소유자가 없어 자연히 걸러짐
		// 정령은 플레이어와 소유자가 같아서 TSet에 한번만 들어감
		APlayerState* OwningPlayerState = Participant.OwningPlayerState;
		if (!OwningPlayerState || Notified.Contains(OwningPlayerState)) continue;

		ANUPlayerController* NUPC = Cast<ANUPlayerController>(OwningPlayerState->GetPlayerController());
		if (!IsValid(NUPC))
		{
			continue;
		}

		Notified.Add(OwningPlayerState);

		// 창은 플레이어당 하나라 SP 보정 기준도 플레이어 본인의 상태이상으로 통일함
		// 정령만 젖은 경우 표시와 정령의 실제 차감이 다를 수 있지만 차감 자체는 전투원별로 정확함
		ANUPlayerState* NUPlayerState = Cast<ANUPlayerState>(OwningPlayerState);
		const UNUAbilitySystemComponent* OwnerASC =
			IsValid(NUPlayerState) ? NUPlayerState->GetNUASC() : nullptr;

		if (ActionContext.bParryable)
		{
			const int32 ParryCost = IsValid(OwnerASC)
				? OwnerASC->ApplyStatusSPCost(ParrySPCost)
				: ParrySPCost;

			NUPC->ClientOpenParryWindow(CurrentActionSequence, ParryImpactOffset,
				ActionContext.ParryTiming, ParryCost);
		}
		else if (DodgeSPCost > 0)
		{
			const int32 DodgeCost = IsValid(OwnerASC)
				? OwnerASC->ApplyStatusSPCost(DodgeSPCost)
				: DodgeSPCost;

			// 회피는 예고부터 접근이 끝날 때까지, 타격 전에 이미 닫힘
			NUPC->ClientOpenDodgeWindow(CurrentActionSequence, DodgeCost,
				TelegraphTime + ApproachTime);
		}
	}
}

void UNUBattleGameplayAbility::CacheTargetCombatantIds()
{
	TargetCombatantIds.Reset();

	// 여기서 타겟 확정
	TArray<AActor*> Targets;
	ResolveEffectTargets(Targets);

	for (AActor* Target : Targets)
	{
		if (const ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Target))
		{
			TargetCombatantIds.Add(Avatar->GetCombatantId());
		}
	}
}

void UNUBattleGameplayAbility::ResolveParryWindow()
{
	ANUBattleEncounter* Encounter = GetBattleEncounter();
	if (!IsValid(Encounter)) 
	{
		return;
	}
	
	for (const TPair<int32, float>& Press : ParryPresses)
	{
		FNUBattleParticipant Participant;
		if (!Encounter->GetParticipant(Press.Key, Participant))
		{
			continue;
		}
		
		UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner));
		
		// SP는 타이밍 판정보다 먼저 소모, 이르거나 늦게 눌러도 시도한 값은 사용
		// SP가 모자라면 시도가 성립하지 않아 차감도 없고 그대로 맞음
		const bool bPaid = TryConsumeParryCost(TargetASC, Participant, Press.Key);
		
		// 값을 낸 경우에만 타이밍을 본다
		const bool bSuccess = bPaid
			&& ActionContext.ParryTiming.Judge(Press.Value, ParryImpactOffset);
		
		// 당사자에게 패링 결과 전송
		if (APlayerController* OwnerPC = Participant.OwningPlayerState 
			? Participant.OwningPlayerState->GetPlayerController() : nullptr)
		{
			if (ANUPlayerController* NUPC = Cast<ANUPlayerController>(OwnerPC))
			{
				NUPC->ClientParryResult(bSuccess);
			}
		}
		
		if (!bSuccess) 
		{
			// 값을 내고 타이밍만 놓친 경우에 띄움
			// SP 부족은 TryConsumeParryCost 가 NotEnoughSP 로 이미 띄웠으므로 두 번 뜨면 안 됨
			if (bPaid)
			{
				if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
				{
					Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::ParryFail));
				}
			}
			continue;
		}
		
		TargetASC->AddLooseGameplayTag(NU::State::Parried);
		ParriedASCs.Add(TargetASC);
		if (Encounter->GetBattleEventHandler() != nullptr) { Encounter->GetBattleEventHandler()->NotifyReaction(Encounter, TargetASC, true); }

		// ClientParryResult는 패링 큐 연출용이라 그대로 두고, 팝업은 남들도 보이게 멀티캐스트
		if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
		{
			Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::Parry));
		}
		
		UE_LOG(LogTemp, Warning, TEXT("패링 %s : 전투원 %d (오차 %.3f초)"),
			bSuccess ? TEXT("성공") : TEXT("실패"),
			Press.Key,
			Press.Value - ParryImpactOffset
			);
	}
}

bool UNUBattleGameplayAbility::TryConsumeParryCost(UNUAbilitySystemComponent* TargetASC,
	const FNUBattleParticipant& Participant, int32 CombatantId)
{
	if (!IsValid(TargetASC))
	{
		return false;
	}
	
	// 패링 비용이 0이면 true 반환
	const int32 ParryCost = TargetASC->ApplyStatusSPCost(ParrySPCost);
	if (ParryCost <= 0)
	{
		return true;
	}
	
	const float CurrentSP = TargetASC->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute());
	if (CurrentSP < static_cast<float>(ParryCost))
	{
		UE_LOG(LogTemp, Warning, TEXT("패링 실패 : 전투원 %d SP 부족 (%.0f < %d)"),
			CombatantId, CurrentSP, ParryCost);
		
		// 결과는 당사자만 보므로 실패 사유는 남들도 보이게 아바타 위에 띄움
		if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
		{
			Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::NotEnoughSP));
		}
		
		return false;
	}
	
	TargetASC->SetNumericAttributeBase(
		UNUVitalAttributeSet::GetSPAttribute(), CurrentSP - static_cast<float>(ParryCost));
	
	return true;
}

void UNUBattleGameplayAbility::ResolveDodgeWindow()
{
	ANUBattleEncounter* Encounter = GetBattleEncounter();
	if (!IsValid(Encounter))
	{
		return;
	}

	for (int32 CombatantId : DodgeRequests)
	{
		FNUBattleParticipant Participant;
		if (!Encounter->GetParticipant(CombatantId, Participant)) 
		{
			continue;
		}

		UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner));
		if (!IsValid(TargetASC)) 
		{
			continue;
		}

		// 회피는 남은 SP로 내는 것이라 부족하면 그냥 맞음
		// 보정은 전투원별로 구함, 젖음 상태이상은 개별 처리
		const int32 DodgeCost = TargetASC->ApplyStatusSPCost(DodgeSPCost);
		float CurrentSP = TargetASC->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute());
		if (CurrentSP < static_cast<float>(DodgeCost))
		{
			UE_LOG(LogTemp, Warning, TEXT("회피 실패 : 전투원 %d SP 부족 (%.0f < %d)"),
				CombatantId, CurrentSP, DodgeCost);

			// 회피 결과는 전투원별로 다르므로 각자의 아바타 위에 띄워야 함
			// 캐릭터는 SP가 있어 회피하고 정령은 없어 맞는 경우가 있음
			// SP 부족은 확률 실패와 다른 팝업이다 - 하나는 자원 관리, 하나는 운의 문제라서
			if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
			{
				Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::NotEnoughSP));
			}
			continue;
		}

		// SP 는 확률 판정보다 먼저 소모한다 - 실패해도 돌려주지 않는다
		// 회피는 "시도" 에 값을 내는 것이고, 실패하면 피해를 그대로 받는다
		TargetASC->SetNumericAttributeBase(
			UNUVitalAttributeSet::GetSPAttribute(), CurrentSP - static_cast<float>(DodgeCost));

		// 패링은 타이밍 판정이라 난수가 없지만 회피는 확률이다
		const float DodgeChance = ResolveDodgeChance(TargetASC, CombatantId);
		if (FMath::FRand() >= DodgeChance)
		{
			UE_LOG(LogTemp, Warning, TEXT("회피 실패 : 전투원 %d 확률 미달 (확률 %.0f%%, SP %d 소모)"),
				CombatantId, DodgeChance * 100.f, DodgeCost);

			// SP 는 이미 소모했고 피해는 그대로 받는다
			if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
			{
				Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::DodgeFail));
			}
			continue;
		}

		TargetASC->AddLooseGameplayTag(NU::State::Dodging);
		DodgedASCs.Add(TargetASC);
		if (Encounter->GetBattleEventHandler() != nullptr) { Encounter->GetBattleEventHandler()->NotifyReaction(Encounter, TargetASC, false); }

		UE_LOG(LogTemp, Warning, TEXT("회피 성공 : 전투원 %d (확률 %.0f%%, SP %d 소모)"),
			CombatantId, DodgeChance * 100.f, DodgeCost);

		if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
		{
			Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::Dodge));
		}
	}
}

void UNUBattleGameplayAbility::ClearStateTags()
{
	// 패링 상태 부여한 ASC에서 패링 상태 태그 제거
	for (const TWeakObjectPtr<UNUAbilitySystemComponent>& WeakASC : ParriedASCs)
	{
		if (!WeakASC.IsValid()) 
		{
			continue;
		}

		WeakASC->RemoveLooseGameplayTag(NU::State::Parried);
	}
	ParriedASCs.Reset();

	// 회피 상태 부여한 ASC에서 회피 상태 태그 제거
	for (const TWeakObjectPtr<UNUAbilitySystemComponent>& WeakASC : DodgedASCs)
	{
		if (!WeakASC.IsValid()) 
		{
			continue;
		}

		WeakASC->RemoveLooseGameplayTag(NU::State::Dodging);
	}
	DodgedASCs.Reset();
	
	ParryPresses.Reset();
	DodgeRequests.Reset();
}

FVector UNUBattleGameplayAbility::ResolveActionLocation(ANUBattleAvatar* Avatar, AActor* TargetActor) const
{
	if (!IsValid(Avatar))
	{
		return FVector::ZeroVector;
	}
	
	if (Motion == ENUBattleActionMotion::InPlace)
	{
		return Avatar->GetActorLocation();
	}
	
	UNUAbilitySystemComponent* ASC = Avatar->GetNUASC();
	ANUBattleEncounter* Encounter = ASC ? ASC->GetBattleEncounter() : nullptr;
	
	// 전투 진입안했다면 제자리에서
	if (!IsValid(Encounter))
	{
		return Avatar->GetActorLocation();
	}
	
	ANUBattleAvatar* TargetAvatar = Cast<ANUBattleAvatar>(TargetActor);
	int32 TargetId = TargetAvatar ? TargetAvatar->GetCombatantId() : INDEX_NONE;
	
	return Encounter->GetActionLocation(Avatar->GetCombatantId(), TargetId, Motion);
}

bool UNUBattleGameplayAbility::IsTargetedCombatant(int32 CombatantId) const
{
	return TargetCombatantIds.Contains(CombatantId);
}
