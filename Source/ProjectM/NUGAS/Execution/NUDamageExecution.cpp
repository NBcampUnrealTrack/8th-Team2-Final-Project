// Fill out your copyright notice in the Description page of Project Settings.


#include "NUDamageExecution.h"

#include "Battle/NUBattleTypes.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"


namespace
{
	// 속성 상성
	struct FNUElementMatchup
	{
		FGameplayTag AttackElement;		// 공격속성
		FGameplayTag StrongElement;		// 상성 우위
		FGameplayTag WeakElement;		// 상성 열위
	};
	
	const TArray<FNUElementMatchup>& GetElementMatchups()
	{
		static const TArray<FNUElementMatchup> Matchups
		{
			{ NU::Element::Fire, NU::Element::Grass, NU::Element::Water },
			{ NU::Element::Water, NU::Element::Fire, NU::Element::Lightning },
			{ NU::Element::Grass, NU::Element::Wind, NU::Element::Fire },
			{ NU::Element::Lightning, NU::Element::Water, NU::Element::Earth },
			{ NU::Element::Earth, NU::Element::Lightning, NU::Element::Wind },
			{ NU::Element::Wind, NU::Element::Earth, NU::Element::Grass }
		};
		
		return Matchups;
	}
	
	// 계산에 사용할 속성값들을 가져올 수 있게 하는 매크로
	struct FNUDamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
		DECLARE_ATTRIBUTE_CAPTUREDEF(MagicAttackPower);
		DECLARE_ATTRIBUTE_CAPTUREDEF(Defense);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalRate);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalDamage);
		
		// MaxHealth 는 어트리뷰트가 protected라 매크로로 못가져옴
		// 접근자로 만들어 쓰므로 Property 는 두지 않음
		FGameplayEffectAttributeCaptureDefinition MaxHealthDef;

		FNUDamageStatics()
		{
			// Attribute Set 클래스, 어트리뷰트, Source/Target, bSnapshot
			// Snapshot 이 true면 캡처 시점이 Spec 생성 시점
			// Snapshot 이 false면 캡처 시점이 GE가 실제로 적용되는 시점
			DEFINE_ATTRIBUTE_CAPTUREDEF(UNUCombatAttributeSet, AttackPower, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UNUCombatAttributeSet, MagicAttackPower, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UNUCombatAttributeSet, Defense, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UNUCombatAttributeSet, CriticalRate, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UNUCombatAttributeSet, CriticalDamage, Source, true);

			// 드루이드 계수의 기준, 시전자 쪽이다
			MaxHealthDef = FGameplayEffectAttributeCaptureDefinition(
				UNUVitalAttributeSet::GetMaxHealthAttribute(),
				EGameplayEffectAttributeCaptureSource::Source,
				true);
		}
	};
	
	const FNUDamageStatics& DamageStatics()
	{
		static FNUDamageStatics Statics;
		return Statics;
	}
	
	// 합연산 풀 상한
	// 넘긴 만큼은 버림, 아래로는 피해가 음수가 되지 않게 -100% 에서 막음
	constexpr float AdditivePoolMax = 2.f;
	constexpr float AdditivePoolMin = -1.f;
}


UNUDamageExecution::UNUDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().MagicAttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalRateDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().MaxHealthDef);
}

void UNUDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	
	// AttemptCalculateCapturedAttributeMagnitude에 넘기는 인자
	FAggregatorEvaluateParameters Params;
	Params.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Params.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	
	// 공격자는 공격 어빌리티에 실어 보낸 태그만 확인
	// 타겟은 타겟 ASC가 가진 태그를 확인
	const FGameplayTagContainer& AttackTags = Spec.CapturedSourceTags.GetSpecTags();
	const FGameplayTagContainer* TargetTags = Params.TargetTags;
	
	
	// ----------------------------------------
	// 0. 회피 성공, 완전 무효화
	// 성공하면 계산할 필요 없이 데미지 0 처리
	{
		if (TargetTags && TargetTags->HasTagExact(NU::State::Dodging))
		{
			return;
		}
	}

	
	// 계산 순서
	//   1. 기본 공격력    ATK 또는 MATK
	//   2. 기본 피해      기본 공격력 x (위력 / 100)
	//   3. 합연산 적용    기본 피해 x (1 + Σ 합연산)   <- 풀에 전부 더한 뒤 한 번만 곱함
	//   4. 곱연산 적용    3번 x Π(곱연산)              <- 항목마다 독립으로 곱함
	
	float ActualDamage = 0.f;

	// ----------------------------------------
	// 1~2. 기본 데미지 = 기준 스탯 * 계수
	{
		// Damage::Base가 없으면 기본값은 공격력, 마법이면 마법 공격력, 드루이드 스킬만 시전자의 최대 HP,
		// 어느 쪽인지는 컨텍스트를 만든 곳이 태그로 실어 보냄
		const bool bOnMaxHealth = AttackTags.HasTagExact(NU::Damage::Base::MaxHealth);
		const bool bMagical = AttackTags.HasTagExact(NU::Damage::Type::Magical);
	
		const FGameplayEffectAttributeCaptureDefinition& BaseDef = bOnMaxHealth
			? DamageStatics().MaxHealthDef
			: bMagical ? DamageStatics().MagicAttackPowerDef : DamageStatics().AttackPowerDef;
	
		float BaseStat = 0.f;
		const bool bCaptured = ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
			BaseDef,
			Params,
			BaseStat
			);
		
		if (!bCaptured)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s 캡쳐 실패"),
				bOnMaxHealth ? TEXT("MaxHealth")
				: (bMagical ? TEXT("MagicAttackPower") : TEXT("AttackPower")));
		}
	
		// 계수, 어빌리티가 SetByCaller로 넣음. 없으면 1.0
		const float Coefficient = Spec.GetSetByCallerMagnitude(NU::Data::Damage::Coefficient, false, 1.f);
	
		// 기본 데미지
		ActualDamage = BaseStat * Coefficient;
	}
	
	// ----------------------------------------
	// 3. 합연산 풀, "주는 피해 +X%" 계열은 전부 여기 하나로 모여 한 번만 곱해짐
	{
		ActualDamage *= CalculateAdditivePool(ExecutionParams, AttackTags);
	}
	
	
	/* 여기부터 곱연산, 아래 항목들은 각각 독립적으로 한 번씩 곱해진다 */
	
	// ----------------------------------------
	// 4-1. 방어력에 의한 데미지 경감
	{
		ActualDamage *= CalculateDefenseMultiplier(ExecutionParams, Params);
	}
	
	// ----------------------------------------
	// 4-2-1. 받는 피해 감소, 대상 쪽 곱연산 항
	{
		ActualDamage *= CalculateDamageTakenMultiplier(ExecutionParams);
	}
	
	// ----------------------------------------
	// 4-2-2. 속성에 의해 더 받는 피해, 스킬이 건 곱연산 항 (2109 수중 감옥)
	{
		ActualDamage *= CalculateElementDamageTakenMultiplier(ExecutionParams, AttackTags);
	}
	
	// ----------------------------------------
	// 4-3-1. 속성 배율 
	ENUElementAffinity Affinity = ENUElementAffinity::None;
	{
		const float ElementMultiplier = CalculateElementMultiplier(&AttackTags, TargetTags);
		if (ElementMultiplier > 1.5f)
		{
			Affinity = ENUElementAffinity::Weak;
		}
		else if (ElementMultiplier < 0.8f)
		{
			Affinity = ENUElementAffinity::Resist;
		}
		
		ActualDamage *= ElementMultiplier;	
	}
	
	// ----------------------------------------
	// 4-3-2. 속성 위력 버프, 스킬이 건 곱연산 항 (2005 열기 · 2204 광합성 · 2405 대전)
	// 같은 이름의 패시브는 3번 합연산 풀에 있음, 표의 StackType 이 둘을 가름
	{
		ActualDamage *= CalculateElementPowerMultiplier(ExecutionParams, AttackTags);
	}
	
	// ----------------------------------------
	// 4-3-3. 상시 직업 스킬의 곱연산 항 (3215 확산 x1.167 · 3314 보정 x1.123)
	// 같은 스킬의 합연산 줄은 3번 풀에 있음, 표의 StackType 이 둘을 가름
	{
		ActualDamage *= CalculateJobDamageMultiplier(ExecutionParams);
	}
	
	// ----------------------------------------
	// 4-4. 치명타 배율
	// 팝업에서 Damage와 Critical을 가르는 데 쓰이므로 블록 밖에서 선언해야 함
	bool bCritical = false;
	{
		ActualDamage *= CalculateCriticalMultiplier(ExecutionParams, Params, bCritical);
	}
	
	// ----------------------------------------
	// 4-5. 패링성공 시 데미지 경감 처리
	{
		if (TargetTags && TargetTags->HasTagExact(NU::State::Parried))
		{
			ActualDamage *= 0.5f;
		}
	}
	
	// ----------------------------------------
	// 4-6. 조건부 대폭 증가, 어빌리티가 실어 보낸 곱연산 항 하나
	// 최후의 일격·뇌신의 심판처럼 "그 조건이면 2배" 인 것들이 여기로 들어옴
	{
		const float Multiplier = Spec.GetSetByCallerMagnitude(NU::Data::Damage::Multiplier, false, 1.f);
		ActualDamage *= FMath::Max(0.f, Multiplier);
	}
	
	// ----------------------------------------
	// 5. 추가 피해
	{
		ActualDamage *= CalculateIncomingDamageAmp(ExecutionParams);
	}
	
	ActualDamage = FMath::Max(0.f, ActualDamage);
	if (ActualDamage <= 0.f) return;

	// 모디파이어는 넣은 순서대로 적용되고 각각 PostGameplayEffectExecute를 호출함
	// 그래서 IncomingDamage 분기에서 읽으려면 치명타 플래그가 먼저여야 함
	// 치명타가 아닐 때도 0으로 덮어써서 이전 타격의 값이 남지 않게 함
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UNUVitalAttributeSet::GetIncomingCriticalAttribute(),
		EGameplayModOp::Override, bCritical ? 1.f : 0.f)
		);
	
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UNUVitalAttributeSet::GetIncomingElementAffinityAttribute(),
		EGameplayModOp::Override, static_cast<float>(Affinity))
		);

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UNUVitalAttributeSet::GetIncomingDamageAttribute(),
		EGameplayModOp::Additive, ActualDamage)
		);
}

float UNUDamageExecution::CalculateElementMultiplier(const FGameplayTagContainer* AttackTags,
	const FGameplayTagContainer* TargetTags) const
{
	if (!TargetTags)
	{
		return 1.f;
	}
	
	// Fatal 디버프 있으면 속성 무시하고 항상 약점 배율
	if (TargetTags->HasTagExact(NU::State::Ailment::Fatal))
	{
		return 2.f;
	}
	
	if (!AttackTags)
	{
		return 1.f;
	}
	
	float Multiplier = 1.f;
	
	// 이중속성은 두 배율을 곱하기
	for (const FNUElementMatchup& Matchup : GetElementMatchups())
	{
		// 공격 속성에 해당하지 않으면 건너뜀
		if (!AttackTags->HasTagExact(Matchup.AttackElement))
		{
			continue;
		}
		
		// 한 공격 속성의 우위 대상과 열위 대상은 서로 다른 속성이라 둘 다 걸릴 수 있음
		if (TargetTags->HasTagExact(Matchup.StrongElement))
		{
			Multiplier *= 2.f;
		}
		if (TargetTags->HasTagExact(Matchup.WeakElement))
		{
			Multiplier *= 0.5f;
		}
	}
	
	// 무속성 공격 or 공격 대상이 속성이 없으면 곱한 것이 없어 1.f 그대로 나감
	return Multiplier;
}

float UNUDamageExecution::CalculateAdditivePool(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	const FGameplayTagContainer& AttackTags) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	
	// 이 행동만의 값, 어빌리티가 실어 보냄
	// 조건부 위력 증가(속성 위력 버프 스킬 등)와 직업 스킬의 합연산 항이 여기로 들어옴
	float Pool = Spec.GetSetByCallerMagnitude(NU::Data::Damage::AdditiveBonus, false, 0.f);
	
	// 공격자에게 상시로 켜져 있는 것
	// 균열이 대상 ASC 를 읽는 것과 같은 방식, 어트리뷰트를 새로 만들지 않는 이유도 같음
	const UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetSourceAbilitySystemComponent());
	
	if (IsValid(SourceASC))
	{
		Pool += SourceASC->SumAlwaysDamageBonus();
		
		// 이번 공격의 속성이 맞을 때만 켜지는 것
		// 속성은 어빌리티가 스펙에 실어 보낸 태그로 판단함, 상성 배율이 보는 것과 같은 태그
		Pool += SourceASC->SumElementPowerBonus(AttackTags);
	
		// 이번 공격의 대상이 그 상태여야 켜지는 것 (ex) 2803 잔열 — 화상 걸린 적에게 +5%)
		Pool += SourceASC->SumConditionalDamageBonus(
			Cast<UNUAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent()));
		
		// 상시 직업 스킬의 합연산 줄 (3001 과다 수용 · 3101 선행 · 3046 정점)
		// 속도 차이를 보는 구간이 있어 대상을 같이 넘김
		Pool += SourceASC->SumJobDamageBonus(
			Cast<UNUAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent()));
	}
	
	// 능력치 자체를 올리는 버프(상시 스탯 패시브·깃털류)는 여기 없음
	// 그건 AttackPower 집계 단계에서 이미 합연산으로 들어가 있고, 능력치 창에도 그렇게 보임
	
	return 1.f + FMath::Clamp(Pool, AdditivePoolMin, AdditivePoolMax);
}

float UNUDamageExecution::CalculateElementDamageTakenMultiplier(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	const FGameplayTagContainer& AttackTags) const
{
	const UNUAbilitySystemComponent* TargetASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent());
	
	if (!IsValid(TargetASC))
	{
		return 1.f;
	}
	
	return TargetASC->GetElementDamageTakenMultiplier(AttackTags);
}

float UNUDamageExecution::CalculateJobDamageMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams) const
{
	const UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetSourceAbilitySystemComponent());
	
	if (!IsValid(SourceASC))
	{
		return 1.f;
	}
	
	return SourceASC->GetJobDamageMultiplier(
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent()));
}

float UNUDamageExecution::CalculateElementPowerMultiplier(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	const FGameplayTagContainer& AttackTags) const
{
	const UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetSourceAbilitySystemComponent());
	
	if (!IsValid(SourceASC))
	{
		return 1.f;
	}
	
	return SourceASC->GetElementPowerMultiplier(AttackTags);
}

float UNUDamageExecution::CalculateDefenseMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	const FAggregatorEvaluateParameters& Params) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	
	float Defense = 0.f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DefenseDef, Params, Defense))
	{
		UE_LOG(LogTemp, Warning, TEXT("Defense 캡쳐 실패"));
	}
	
	// 방어 무시는 경감된 피해에 곱하는 게 아니라 유효 방어력을 깎는 방식이다
	// 서버 계약이 유효 방어력 = DEF x (1 - 무시율) 이라 그대로 맞춤
	const float DefenseIgnore = FMath::Clamp(
		Spec.GetSetByCallerMagnitude(NU::Data::Damage::DefenseIgnore, false, 0.f), 0.f, 1.f);
	
	Defense *= 1.f - DefenseIgnore;
	
	return 100.f / (100.f + FMath::Max(0.f, Defense));
}

float UNUDamageExecution::CalculateDamageTakenMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams) const
{
	const UNUAbilitySystemComponent* TargetASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent());
	
	if (!IsValid(TargetASC))
	{
		return 1.f;
	}
	
	// 2838 민첩한 대응이 두 속도를 견주므로 공격자 값을 같이 넘김
	// 속도는 이 GE 가 건드리는 값이 아니라서 캡처를 늘리지 않고 ASC 에서 바로 읽어도 됨
	float AttackerSpeed = 0.f;
	const UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetSourceAbilitySystemComponent());
	if (IsValid(SourceASC))
	{
		AttackerSpeed = SourceASC->GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute());
	}
	
	// 공격자에게 붙은 자원을 보는 직업 줄이 있어 공격자도 넘김 (3148 마크 5중첩 · 3248 문양 상한인 적에게 받는 피해 x0.8)
	return TargetASC->CalcDamageTakenMultiplier(AttackerSpeed, SourceASC);
}

float UNUDamageExecution::CalculateIncomingDamageAmp(const FGameplayEffectCustomExecutionParameters& ExecutionParams) const
{
	// 정의를 읽어서 피해 증가율을 반환함
	const UNUAbilitySystemComponent* TargetASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent());
	
	if (!IsValid(TargetASC))
	{
		return 1.f;
	}
	
	// 같은 축을 켠 상태이상이 둘 이상이면 합연산, SP 소모 증가와 같은 규칙
	const float Amp = FMath::Max(0.f,
		TargetASC->SumStatusEffectAxis(
			[](const FNUStatusEffectDefinition& Definition)
			{
				return Definition.DamageAmp;
			}
			)
		);
	
	return 1.f + Amp;
}

float UNUDamageExecution::CalculateCriticalMultiplier(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	const FAggregatorEvaluateParameters& Params, bool& bOutCritical) const
{
	float Rate = 0.f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalRateDef, Params, Rate))
	{
		UE_LOG(LogTemp, Warning, TEXT("CriticalRate 캡쳐 실패"));
	}
	
	float CritDamage = 1.5f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalDamageDef, Params, CritDamage))
	{
		UE_LOG(LogTemp, Warning, TEXT("CriticalDamage 캡쳐 실패"));
	}
	
	// 공격마다 값이 갈리는 치명타 줄은 어트리뷰트에 없음, 공격 순간 더함 (3114 단발 · 3015 잃은 HP · 3039 동조율)
	if (const UNUAbilitySystemComponent* SourceASC =
		Cast<UNUAbilitySystemComponent>(ExecutionParams.GetSourceAbilitySystemComponent()))
	{
		const UNUAbilitySystemComponent* TargetASC =
			Cast<UNUAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent());
		
		Rate += SourceASC->SumJobPerAttackCriticalBonus(ENUStatusAffectedStat::CriticalRate, TargetASC);
		CritDamage += SourceASC->SumJobPerAttackCriticalBonus(ENUStatusAffectedStat::CriticalDamage, TargetASC);
	}
	
	// 반드시 치명타인 행동은 굴리지 않음 (3121 나이아의 유도), 치명타 피해 배율은 그대로 씀
	const bool bForceCritical = ExecutionParams.GetOwningSpec().GetSetByCallerMagnitude(
		NU::Data::Damage::ForceCritical, false, 0.f) > 0.5f;

	bOutCritical = bForceCritical || FMath::FRand() < Rate;
	return bOutCritical ? CritDamage : 1.f;
}
