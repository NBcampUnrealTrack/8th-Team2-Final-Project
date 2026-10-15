// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NUGE_ItemManaRestore.generated.h"

// 소모품 SP 회복 GE (Instant, SP += SetByCaller NU.Data.Item.Heal).
// 클래스 이름은 ItemTable.csv 가 경로로 참조하고 있어 유지, 테이블 정리 시점에 같이 바꾼다.
// 기획 테이블의 "MP 회복" 분류(C_MP_*)가 사용. SetByCaller 키는 ItemHeal과 공유(수치 주입 키로만 쓰임).
UCLASS()
class PROJECTM_API UNUGE_ItemManaRestore : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNUGE_ItemManaRestore();
};
