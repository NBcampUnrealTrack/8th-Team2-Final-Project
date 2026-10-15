// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NUGE_EquipmentStats.generated.h"

// 장비 장착 스탯 GE (Infinite, Combat 6종 + MaxHealth/MaxSP를 SetByCaller AddBase로 부여).
// - Infinite 필수: Instant로 바꾸면 BaseValue가 영구 변경되어 해제 시 원복이 깨진다.
// - 스택 정책 None 유지 필수: AggregateBySource로 바꾸면 슬롯 하나 해제가 모든 슬롯의 modifier를 제거한다.
//   슬롯마다 독립 Active 인스턴스로 적용하고 NUEquipmentComponent가 슬롯별 핸들로 개별 제거한다.
UCLASS()
class PROJECTM_API UNUGE_EquipmentStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_EquipmentStats();
};
