// Fill out your copyright notice in the Description page of Project Settings.


#include "NUActiveSkillAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"


namespace
{
	// 이 줄이 대상 집합 밖의 아군에게 가는지
	// 스킬 자체가 아군 대상이면 대상 목록이 이미 아군이라 여기로 오지 않음
	bool IsCrossSideAllyRow(const FNUActiveEffectRow& Row, ENUTargetPolicy Policy)
	{
		if (Row.TargetScope != ENUSkillTargetScope::Ally)
		{
			return false;
		}
		
		return Policy != ENUTargetPolicy::SingleAlly && Policy != ENUTargetPolicy::AllAllies;
	}
	
	// 직업 행의 편 판정, 나이아 행과 열거형이 달라 함수가 따로임
	bool IsJobSelfRow(const FNUJobSkillEffectRow& Row)
	{
		// 적 대상 줄(마크 · 약점 중첩)은 대상 쪽으로 가야함
		return Row.TargetScope == ENUJobTargetScope::Self
			|| Row.TargetScope == ENUJobTargetScope::None;
	}

	// 아군 쪽 범위를 뜻하는 직업 줄인지, 누구에게 닿는지는 ASC 의 DoesJobScopeReach 가 가름
	bool IsJobAllySideRow(const FNUJobSkillEffectRow& Row)
	{
		return Row.TargetScope == ENUJobTargetScope::Ally
			|| Row.TargetScope == ENUJobTargetScope::AllyAndNaia
			|| Row.TargetScope == ENUJobTargetScope::Party;
	}

	bool IsJobCrossSideAllyRow(const FNUJobSkillEffectRow& Row, ENUTargetPolicy Policy)
	{
		if (!IsJobAllySideRow(Row))
		{
			return false;
		}

		return Policy != ENUTargetPolicy::SingleAlly && Policy != ENUTargetPolicy::AllAllies;
	}

	// 직업 줄의 회복, SP 는 회복 GE 가 체력만 받아 나이아 줄(2007)과 같은 GainResource 로 보냄
	// 받는 쪽은 행동이 끝날 때 SP 가 상한까지 차서, 그 자리에서 더하기만 하면 사라짐 (3240 지맥 개방 — 아군 SP 8)
	void SendJobRestore(const FNUJobSkillEffectRow& Row, UNUAbilitySystemComponent* SourceASC,
		UNUAbilitySystemComponent* TargetASC)
	{
		if (Row.AffectedStat == ENUStatusAffectedStat::SP)
		{
			const int32 Turns = Row.DurationScope == ENUEffectDurationScope::Turns ? Row.DurationTurns : 0;
			TargetASC->GainResource(SourceASC, Row.AffectedStat, Row.GetScaledValue(), Row.bIsPercent, Turns);
			return;
		}

		// 표가 백분율로 보내므로 다른 줄과 같은 비율을 씀 (12 -> 0.12)
		SourceASC->SendHeal(TargetASC, SourceASC, Row.AffectedStat, Row.GetScaledValue(), Row.bIsPercent);
	}

	bool IsSelfRow(const FNUActiveEffectRow& Row)
	{
		// 예약 피해 줄은 대상이 적이지만 예약은 시전자에게 걸림
		// 터질 때 누구를 때릴지는 그 시점에 정의표의 대상 정책으로 다시 정함
		if (Row.EffectType == ENUSkillEffectType::Damage)
		{
			return true;
		}
		
		// 공격시 부여도 예약 — 걸릴 대상은 적이지만 몇 턴 동안 들고 있는 것은 시전자
		// 반사는 걸린 쪽이 들고 있어야 해서 여기 오면 안 됨, 그래서 종류까지 봄
		if (Row.EffectType == ENUSkillEffectType::StatusApply
			&& Row.ConditionType == ENUEffectCondition::OnAttack)
		{
			return true;
		}

		return Row.TargetScope == ENUSkillTargetScope::Self;
	}
}

UNUActiveSkillAbility::UNUActiveSkillAbility()
{
	// 스킬 전체가 공유하는 태그, 조회는 SkillId로 함, 태그는 로그, 배너 폴백용도
	AbilityId = NU::Ability::Battle::ActiveSkill;

	// HitCount가 스킬 60행 전부 1이라 반복 타격 X
	SPCostPerHit = 0;

	// 아래 값들은 ApplyActionContext가 정의 값으로 덮어씀
	// 컨텍스트를 못 받으면 발동 자체를 막으므로 이 값이 실제로 쓰이는 일은 없음
	SPCost = 0;
	DamageCoefficient = 0.f;
	TargetPolicy = ENUTargetPolicy::SingleEnemy;
	Motion = ENUBattleActionMotion::InPlace;
}

void UNUActiveSkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 스킬 전체가 스펙 하나를 공유하므로 인스턴스 필드에 지난 발동의 값이 남아 있음
	// 컨텍스트 없이 발동하면 이전에 쓴 스킬로 나가버리므로 여기서 끊는다
	// Super가 컨텍스트를 소비하기 전이라 Peek으로 확인해야 함
	const UNUAbilitySystemComponent* ASC =
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());

	if (!IsValid(ASC) || !ASC->PeekActionContext().bValid)
	{
		UE_LOG(LogTemp, Error, TEXT("컨텍스트 없이 액티브 스킬 실행기가 발동했습니다"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UNUActiveSkillAbility::ApplySelfRuntimeEffects()
{
	UNUAbilitySystemComponent* SourceASC = 
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());

	if (!IsValid(SourceASC))
	{
		return;
	}

	for (const FNUActiveEffectRow& Row : ActionContext.RuntimeEffects)
	{
		if (IsSelfRow(Row))
		{
			ApplyRuntimeRow(Row, SourceASC, SourceASC);
		}
	}

	for (const FNUJobSkillEffectRow& Row : ActionContext.JobRuntimeEffects)
	{
		if (IsJobSelfRow(Row))
		{
			ApplyJobRuntimeRow(Row, SourceASC, SourceASC);
		}
	}
}

void UNUActiveSkillAbility::ApplyCrossSideRuntimeEffects()
{
	UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	
	if (!IsValid(SourceASC))
	{
		return;
	}
	
	const bool bNeedsAllies = ActionContext.RuntimeEffects.ContainsByPredicate(
		[this](const FNUActiveEffectRow& Row)
		{
			return IsCrossSideAllyRow(Row, TargetPolicy);
		})
		|| ActionContext.JobRuntimeEffects.ContainsByPredicate(
		[this](const FNUJobSkillEffectRow& Row)
		{
			return IsJobCrossSideAllyRow(Row, TargetPolicy);
		});
	
	if (!bNeedsAllies)
	{
		return;
	}
	
	TArray<AActor*> Allies;
	ResolveTeamTargets(ENUTargetPolicy::AllAllies, Allies);
	
	for (AActor* Ally : Allies)
	{
		UNUAbilitySystemComponent* AllyASC = Cast<UNUAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Ally));
		
		if (!IsValid(AllyASC))
		{
			continue;
		}
		
		for (const FNUActiveEffectRow& Row : ActionContext.RuntimeEffects)
		{
			if (IsCrossSideAllyRow(Row, TargetPolicy))
			{
				ApplyRuntimeRow(Row, SourceASC, AllyASC);
			}
		}

		for (const FNUJobSkillEffectRow& Row : ActionContext.JobRuntimeEffects)
		{
			// 아군 범위 줄은 그 범위에 닿는 사람에게만 (Ally 는 자신 · 나이아 제외)
			if (IsJobCrossSideAllyRow(Row, TargetPolicy)
				&& SourceASC->DoesJobScopeReach(Row.TargetScope, AllyASC))
			{
				ApplyJobRuntimeRow(Row, SourceASC, AllyASC);
			}
		}
	}
}

// 직업 표의 런타임 효과 적용, 효과 항목(GE)으로 못 담아 ASC 로 직접 걸음
void UNUActiveSkillAbility::ApplyJobRuntimeRow(const FNUJobSkillEffectRow& Row,
	UNUAbilitySystemComponent* SourceASC, UNUAbilitySystemComponent* TargetASC)
{
	switch (Row.EffectType)
	{
	case ENUJobEffectType::Heal:
		// 소모한 양 1당은 행동 문맥 축이라 아직 못 셈, 쓸 수는 있게 두고 1회분만 회복함 (3006 정령 해방 · 3308 성배 방출)
		if (!Row.ValuePer.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[직업] 스킬 %d #%d : %s 를 못 세어 1회분(%g%s)만 회복함"),
				Row.JobSkillId, Row.EffectIndex, *Row.ValuePer.ToString(),
				Row.Value, Row.bIsPercent ? TEXT("%") : TEXT(""));
		}

		SendJobRestore(Row, SourceASC, TargetASC);
		break;

	case ENUJobEffectType::ResourceGain:
		// 표가 스탯 칸을 채웠으면 어트리뷰트(SP), 비워 두면 직업 자원
		if (!Row.ValuePer.IsNone())
		{
			// 몇 배인지 셀 수 없는 축이 아직 있음 (3125 흡수 사격 — 준 피해 1당)
			UE_LOG(LogTemp, Verbose, TEXT("직업 스킬 %d #%d : %s 를 못 세어 자원을 건너뜁니다"),
				Row.JobSkillId, Row.EffectIndex, *Row.ValuePer.ToString());
		}
		else if (Row.AffectedStat != ENUStatusAffectedStat::None)
		{
			SendJobRestore(Row, SourceASC, TargetASC);
		}
		else
		{
			// 자원은 개수라 나누지 않음, 음수면 소모 (3205 명상 — 마력 응축 -3)
			TargetASC->AddJobResource(Row.ResourceName, FMath::RoundToInt(Row.Value));
		}
		break;

	case ENUJobEffectType::DebuffStack:
		// 대상에게 쌓이는 중첩, 자원표가 OwnerScope=Target 이라고 말하는 것들
		TargetASC->AddJobResource(Row.ResourceName, FMath::RoundToInt(Row.Value));
		break;

	case ENUJobEffectType::DamageTakenMod:
		// 값이 곧 배수(x0.65), 기간이 있는 경우만 여기까지 옴
		TargetASC->ApplyDamageTakenMod(Row.JobSkillId, Row.GetScaledValue(), Row.DurationTurns);
		break;

	case ENUJobEffectType::Shield:
		// 비율이면 받는 쪽 최대 체력 기준, 나이아 스킬 보호막과 같은 자리로 걸어 "아군 보호막 x2"(3444)도 같이 먹음
		// 기간이 있는 경우만 여기까지 옴
		TargetASC->ApplyShield(SourceASC, Row.JobSkillId, Row.GetScaledValue(), Row.bIsPercent, Row.DurationTurns);
		break;

	case ENUJobEffectType::ResourceTimerReset:
		// 자원은 대상에게 있고 안 쌓인 턴 셈만 0 으로 돌림 (3137 약점 관측 — 마크 3턴을 처음부터)
		TargetASC->ResetJobResourceIdleTurns(Row.ResourceName);
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("직업 스킬 %d #%d : 런타임으로 왔는데 처리가 없음 : %s"),
			Row.JobSkillId, Row.EffectIndex, *UEnum::GetValueAsString(Row.EffectType));
		break;
	}
}

void UNUActiveSkillAbility::ApplyRuntimeEffect(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());

	UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor));

	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		return;
	}

	for (const FNUActiveEffectRow& Row : ActionContext.RuntimeEffects)
	{
		if (!IsSelfRow(Row) && !IsCrossSideAllyRow(Row, TargetPolicy))
		{
			ApplyRuntimeRow(Row, SourceASC, TargetASC);
		}
	}

	for (const FNUJobSkillEffectRow& Row : ActionContext.JobRuntimeEffects)
	{
		if (IsJobSelfRow(Row) || IsJobCrossSideAllyRow(Row, TargetPolicy))
		{
			continue;
		}

		// 대상 목록이 아군인 스킬의 아군 범위 줄도 그 범위에 닿는 사람에게만
		if (IsJobAllySideRow(Row) && !SourceASC->DoesJobScopeReach(Row.TargetScope, TargetASC))
		{
			continue;
		}

		ApplyJobRuntimeRow(Row, SourceASC, TargetASC);
	}
}

// 일반적인 효과 항목으로 담을 수 없는 특수한 효과들
void UNUActiveSkillAbility::ApplyRuntimeRow(const FNUActiveEffectRow& Row, UNUAbilitySystemComponent* SourceASC,
	UNUAbilitySystemComponent* TargetASC)
{
	switch (Row.EffectType)
	{
	case ENUSkillEffectType::Heal:
		// 지속 턴이 있으면 턴마다, 없으면 그 자리에서 한 번 회복
		if (Row.GetDurationTurns() > 0)
		{
			SourceASC->ApplyTurnBasedHeal(TargetASC, Row);
		}
		else
		{
			SourceASC->SendHeal(TargetASC, SourceASC, Row.AffectedStat, Row.GetScaledValue(), Row.bIsPercent);
		}
		break;
	
	case ENUSkillEffectType::StatusCure:
		// 표의 Value 가 해제 개수, 비율이 아니라 개수라 나누지 않음
		TargetASC->CureStatusEffects(FMath::RoundToInt(Row.Value));
		break;
	
	case ENUSkillEffectType::Shield:
		// 지속 턴이 0이면 전투가 끝날 때까지 유지됨
		TargetASC->ApplyShield(SourceASC, Row.SkillId, Row.GetScaledValue(), Row.bIsPercent, Row.GetDurationTurns());
		break;
	
	case ENUSkillEffectType::Reflect:
		// 적에게 거는 디버프, 걸린 쪽이 공격하면 그 공격으로 가한 만큼 자신이 돌려받음
		// 되받는 비율, 표가 백분율로 보내므로 다른 줄과 같은 비율을 사용함 (50 -> 0.5)
		TargetASC->ApplyReflect(SourceASC, Row.SkillId, Row.GetScaledValue(), Row.GetDurationTurns());
		break;
	
	case ENUSkillEffectType::ActionBlock:
		// 스스로를 묶는 상태를 검, 같은 스킬의 아래 줄들이 이 상태에 얹힘
		TargetASC->ApplySelfBoundState(Row.SkillId, Row.GetDurationTurns(), true);
		break;
	
	case ENUSkillEffectType::Damage:
		// 여기 오는 피해 줄은 예약 공격뿐, 그 자리에서 때리는 줄은 효과 항목에서 처리함
		TargetASC->SetSelfBoundRelease(Row.SkillId);
		break;
	
	case ENUSkillEffectType::Lifesteal:
		TargetASC->SetSelfBoundLifesteal(Row.SkillId, Row.GetScaledValue());
		break;
	
	case ENUSkillEffectType::ResourceGain:
		// SP 는 행동당 예산이라 상한을 올리는 쪽으로 처리
		TargetASC->GainResource(SourceASC, Row.AffectedStat, Row.GetScaledValue(),
			Row.bIsPercent, Row.GetDurationTurns());
		break;
	
	case ENUSkillEffectType::StatusApply:
		// 여기 오는 상태 줄은 공격시 부여뿐임, 그 자리에서 거는 줄은 효과 항목으로 갔음
		{
			FGameplayTagContainer Elements;
			bool bIncludeBasicAttack = false;
			Row.GetConditionAttackKinds(Elements, bIncludeBasicAttack);
			
			TargetASC->ApplyOnAttackGrant(Row.SkillId, Row.StatusEffectId, Row.Chance,
				Elements, bIncludeBasicAttack, Row.GetDurationTurns());
		}
		break;
	
	case ENUSkillEffectType::ElementDamageTakenMod:
		// 그 속성으로 맞을 때만 켜지는 배수, 속성 위력과 방향이 반대라 대상 쪽에 붙음
		TargetASC->ApplyElementAffliction(
			SourceASC, 
			Row.SkillId,
			Row.Element,
			Row.StackType == ENUEffectStackType::Multiplicative ? Row.GetScaledValue() : 1.f + Row.GetScaledValue(),
			false,
			Row.GetDurationTurns()
			);
		break;
	
	case ENUSkillEffectType::ElementSkillBlock:
		// 그 속성 액티브 스킬을 못 쓰게 함, 피해 배수는 건드리지 않으므로 1
		TargetASC->ApplyElementAffliction(
			SourceASC,
			Row.SkillId,
			Row.Element,
			1.f, 
			true, 
			Row.GetDurationTurns()
			);
		break;
	
	case ENUSkillEffectType::ElementPowerMod:
		{
			// 지속이 그 상태인 동안이면 상태의 남은 턴을 그대로 씀 (2210 개화)
			// 표의 Permanent 는 턴 수가 0이라 그냥 쓰면 전투 끝까지가 됨
			const int32 Turns = Row.ConditionType == ENUEffectCondition::DuringState
					? TargetASC->GetSelfBoundRemainingTurns(Row.SkillId)
					: Row.GetDurationTurns();
			
			// AfterElementUse 면 그 속성 스킬을 그만큼 더 쓴 뒤에 켜짐 (2407 과전류)
			const int32 RequiredUses = Row.ConditionType == ENUEffectCondition::AfterElementUse 
				? Row.GetRequiredUses() 
				: 0;
			
			// 그 속성으로 공격할 때만 켜지는 배수
			TargetASC->ApplyElementPowerMod(
				SourceASC, 
				Row.SkillId, 
				Row.Element,
				Row.StackType == ENUEffectStackType::Multiplicative ? Row.GetScaledValue() : 1.f + Row.GetScaledValue(),
				Turns,
				RequiredUses
				);
		}
	
	default:
		break;
	}
}
