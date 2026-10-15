// Fill out your copyright notice in the Description page of Project Settings.


#include "NUSkillGameplayEffect.h"

#include "NUGAS/Execution/NUDamageExecution.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

UNUGE_SkillDamage::UNUGE_SkillDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 계수, 물리/마법 타입, 속성은 어빌리티가 SetByCaller 와 동적 태그로 주입하므로
	// 이 GE는 Execution 만 설정하면 됨
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UNUDamageExecution::StaticClass();

	Executions.Add(Execution);
	
	// 피격 GameplayCue 사용
	FGameplayEffectCue HitCue;
	HitCue.GameplayCueTags.AddTag(GameplayCue::Battle::Hit);
	HitCue.MagnitudeAttribute = UNUVitalAttributeSet::GetIncomingDamageAttribute();
	
	GameplayCues.Add(HitCue);
}

UNUGE_SkillHeal::UNUGE_SkillHeal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 아이템 회복과 유사하게 IncomingHeal 메타 어트리뷰트 이용해서 회복함
	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UNUVitalAttributeSet::GetIncomingHealingAttribute();
	ModInfo.ModifierOp = EGameplayModOp::AddBase;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = NU::Data::Skill::Heal;
	ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModInfo);

	// 회복 GameplayCue 사용
	FGameplayEffectCue HealCue;
	HealCue.GameplayCueTags.AddTag(GameplayCue::Battle::Heal);
	HealCue.MagnitudeAttribute = UNUVitalAttributeSet::GetIncomingHealingAttribute();

	GameplayCues.Add(HealCue);
}
