// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "NUBattleProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

// 스킬 탄환 연출 전용 액터, 판정에는 관여하지 않는 순수 시각 요소
// 클라이언트마다 로컬로 스폰되고, 도착하면 임팩트를 띄우고 사라짐
UCLASS()
class PROJECTM_API ANUBattleProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANUBattleProjectile();

	virtual void Tick(float DeltaSeconds) override;

	// TrailSystem 은 필수, InImpactVFX 는 없으면 도착 연출을 생략함
	// InImpactSFX 는 도착할 때 내는 효과음 태그, 비우면 소리 없음
	void Launch(UNiagaraSystem* TrailSystem,
		TSoftObjectPtr<UNiagaraSystem> InImpactVFX,
		const FVector& StartLocation,
		const FVector& EndLocation, 
		float FlightTime,
		const FLinearColor& Color,
		const FGameplayTag& InImpactSFX = FGameplayTag());

protected:
	UPROPERTY(VisibleAnywhere, Category = "NU|VFX")
	TObjectPtr<UNiagaraComponent> TrailComponent;

private:
	FVector Origin = FVector::ZeroVector;
	FVector Destination = FVector::ZeroVector;
	
	float Duration = 0.f;
	float Elapsed = 0.f;
	
	FLinearColor EffectColor = FLinearColor::White;
	
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	FGameplayTag ImpactSFX;
};
