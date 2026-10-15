// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Battle/Enemy/NUEnemyPatternTypes.h"
#include "NUEnemyPatternAsset.generated.h"

// 적의 행동 패턴
// 행동이 비슷한 적은 같은 에셋을 공유해도 됨
UCLASS(BlueprintType)
class PROJECTM_API UNUEnemyPatternAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 위에서부터 확인해서 진입 조건을 만족하는 마지막이 현재 페이즈
	// 일반 몬스터는 비워둠
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Enemy Pattern")
	TArray<FNUEnemyPhase> Phases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Enemy Pattern")
	TArray<FNUEnemyPatternRule> Rules;

	// 후보가 하나도 없을 때 낼 행동, 비우면 인카운터가 기본 공격으로 채움
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Enemy Pattern",
		meta = (Categories = "NU.Ability.Battle"))
	FGameplayTag FallbackAbilityId;
};

// NaiaId -> 패턴 에셋
// NaiaTable.csv 와 FNUNaiaDefinition 을 건드리지 않기 위해 매핑을 여기로 뺐음
UCLASS(BlueprintType)
class PROJECTM_API UNUEnemyPatternRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	// 등록되지 않은 몬스터는 지금까지처럼 기본 공격만 함
	const UNUEnemyPatternAsset* FindPattern(FName NaiaId) const;

public:
	// 키는 DT_NaiaDefinition 의 RowName (MOB_001 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Enemy Pattern")
	TMap<FName, TSoftObjectPtr<UNUEnemyPatternAsset>> Patterns;
};
