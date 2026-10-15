// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "NUStatusGameplayEffect.generated.h"


// 스탯 변경에 사용하는 어트리뷰트들
namespace NUStatusEffect
{
	struct FNUStatModifierAxis
	{
		FGameplayAttribute Attribute;
		
		// 비율 변경(곱), 사용 안하면 1로 채우기
		FGameplayTag MultiplierTag;
		
		// 고정값 변경(합), 사용 안하면 0으로 채우기
		FGameplayTag AdditiveTag;
	};
	
	// 스탯 변경이 건드릴 수 있는 스탯 전부
	// 변경할 스탯을 늘리려면 여기에 항목 하나만 추가하면 GE 생성자와 아래 두 함수가 따라옴
	PROJECTM_API const TMap<ENUStatusAffectedStat, FNUStatModifierAxis>& GetStatModifierAxes();

	// 모든 스탯 변경값을 초기화함 (곱은 1 합은 0)
	// 변경할 스탯을 세팅하기 전에 반드시 먼저 불러야 함
	PROJECTM_API void FillNeutralStatModifiers(TMap<FGameplayTag, float>& OutMagnitudes);

	// 어트리뷰트 자체가 비율인 축인지 (치명타 확률 · 치명타 피해)
	// 이런 축은 백분율로 적힌 값이 배율이 아니라 합으로 들어가야 함
	PROJECTM_API bool IsRatioStat(ENUStatusAffectedStat Stat);

	// 변경할 스탯 하나를 세팅함. bIsPercent가 곱셈·덧셈을 판정
	// 표에 없는 스탯을 변경하려고 하면 false를 반환하고 아무것도 하지 않음
	PROJECTM_API bool SetStatModifier(TMap<FGameplayTag, float>& OutMagnitudes,
		ENUStatusAffectedStat Stat, float Value, bool bIsPercent);
}

// 상태이상 태그만 붙이는 공용 GE
UCLASS()
class PROJECTM_API UNUGE_StatusEffect_Marker : public UGameplayEffect
{
	GENERATED_BODY()
	
public:
	UNUGE_StatusEffect_Marker();
};

// 패시브의 상시 스탯 증감 GE
// 상태이상용과 축 표를 같이 쓰지만 클래스는 분리함 (패시브는 영구 적용 GE)
UCLASS()
class PROJECTM_API UNUGE_PassiveStatModifier : public UGameplayEffect
{
	GENERATED_BODY()
	
public:
	UNUGE_PassiveStatModifier();
};

// 스탯 변경 GE
UCLASS()
class PROJECTM_API UNUGE_StatusEffect_StatModifier : public UGameplayEffect
{
	GENERATED_BODY()
	
public:
	UNUGE_StatusEffect_StatModifier();
};

// 턴 종료 피해 GE
UCLASS()
class PROJECTM_API UNUGE_StatusEffect_TickDamage : public UGameplayEffect
{
	GENERATED_BODY()
	
public:
	UNUGE_StatusEffect_TickDamage();
};
