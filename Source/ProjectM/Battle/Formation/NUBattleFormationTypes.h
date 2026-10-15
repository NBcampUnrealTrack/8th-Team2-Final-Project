// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleFormationTypes.generated.h"


// 전투 진형의 열 구분
UENUM(BlueprintType)
enum class ENUBattleFormationRow : uint8
{
	Player		UMETA(DisplayName = "플레이어 열"),
	Naia		UMETA(DisplayName = "정령 열"),
	Enemy		UMETA(DisplayName = "적 열")
};
