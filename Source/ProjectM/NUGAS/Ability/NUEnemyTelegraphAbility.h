// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleGameplayAbility.h"
#include "NUEnemyTelegraphAbility.generated.h"


// 큰 기술 앞에 한 턴 쉬며 경고만 하는 적 행동, 피해 · SP 소모 없이 행동 한 번만 씀
// 적 패턴 페이즈 표의 OnEnterAbilityId 로 부름 (브램블코어 — 언덕 무너뜨리기 앞 경고 턴)
// 배너가 이름을 띄우는 것이 곧 경고 문구, BP 없이 C++ 로만 쓰므로 적 아바타가 직접 줌
UCLASS()
class PROJECTM_API UNUEnemyTelegraphAbility : public UNUBattleGameplayAbility
{
	GENERATED_BODY()

public:
	UNUEnemyTelegraphAbility();
};
