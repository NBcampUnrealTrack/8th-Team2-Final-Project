// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUSkillVFXProfile.generated.h"

class UNiagaraSystem;

// 스킬 하나에 붙는 연출 이펙트 묶음
// 기획 시트가 아니라 클라 전용 표(DT_SkillVFX), 표에 없는 스킬은
// UNUBattleDataSubsystem::FindSkillVFXProfile 이 nullptr 을 돌려주고
// 호출부가 기본 프로필(근접 슬래시) 사용
USTRUCT(BlueprintType)
struct PROJECTM_API FNUSkillVFXProfile : public FTableRowBase
{
	GENERATED_BODY()

	// 이 행이 어느 스킬 것인지, 조회 키임
	// 행 이름은 안 보므로 알아보기 쉬운 이름으로 지어도 되고, 비워두면 그 행은 무시됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|VFX")
	int32 SkillId = INDEX_NONE;

	// 시전 연출, 없으면 안 뜸
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|VFX")
	TSoftObjectPtr<UNiagaraSystem> CastVFX;

	// 있으면 시전자 -> 대상으로 투사체 연출, 없으면 즉시 명중 처리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|VFX")
	TSoftObjectPtr<UNiagaraSystem> ProjectileVFX;

	// 대상에게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|VFX")
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	// true면 스킬의 AttackElement 로 색을 정함, false면 FixedColor 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|VFX")
	bool bUseElementColor = false;

	// bUseElementColor가 false일 때 쓰는 고정 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|VFX", meta = (EditCondition = "!bUseElementColor"))
	FLinearColor FixedColor = FLinearColor::White;
};
