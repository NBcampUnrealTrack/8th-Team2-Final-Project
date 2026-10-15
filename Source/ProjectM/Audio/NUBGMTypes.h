// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBGMTypes.generated.h"

class USoundBase;

// BGM 레이어 우선순위 - 숫자가 클수록 우선순위가 높음, Resolve()가 위에서부터 첫 유효 값을 고름
enum class ENUBGMLayer : uint8
{
	Level,
	Area,
	Battle,
	Count,
};

// ANUBGMZone 하나가 어느 범위의 BGM을 지정하는지
UENUM()
enum class ENUBGMZoneScope : uint8
{
	// 지금 있는 레벨의 기본 BGM - 레벨마다 하나
	WholeLevel,

	// 이 액터가 들어있는 전투 배경 서브레벨(세트)이 활성 상태일 때 쓸 Battle BGM - 전투 세트마다 하나
	BattleStage,

	// 박스 오버랩으로 갈아끼우는 구역 내 BGM
	Area,
};

// BGM 한 곡의 재생 정보
USTRUCT(BlueprintType)
struct FNUBGMEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Audio")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Audio", meta = (ClampMin = "0.0"))
	float Volume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Audio", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Audio", meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 1.f;
};
