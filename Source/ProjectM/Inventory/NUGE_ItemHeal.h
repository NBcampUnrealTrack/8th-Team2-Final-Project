// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NUGE_ItemHeal.generated.h"

// 소모품 기본 회복 GE (Instant, Health += SetByCaller NU.Data.Item.Heal). (폴백)
UCLASS()
class PROJECTM_API UNUGE_ItemHeal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_ItemHeal();
};
