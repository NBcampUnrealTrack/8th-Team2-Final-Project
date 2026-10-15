// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NUSpiritFollowComponent.generated.h"


USTRUCT(BlueprintType)
struct FNUSpiritFollowSettings
{
	GENERATED_BODY()

	// 주인 기준 상대 위치
	UPROPERTY(VisibleAnywhere, Category = "NU|Follow")
	FVector FollowOffset = FVector(-30.0f, 30.0f, 0.0f);

	UPROPERTY(VisibleAnywhere, Category = "NU|Follow")
	float InterpSpeed = 4.0f;

	UPROPERTY(VisibleAnywhere, Category = "NU|Follow")
	float BobAmplitude = 8.0f;

	UPROPERTY(VisibleAnywhere, Category = "NU|Follow")
	float BobFrequency = 1.0f;
};

namespace NUSpiritFollow
{
	// 로컬/원격 정령이 공유하는 기본값
	PROJECTM_API FNUSpiritFollowSettings MakeDefaults();
}

// 정령이 주인을 따라다니는 이동을 담당하는 컴포넌트.
UCLASS(meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUSpiritFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUSpiritFollowComponent();

	void Setup(const FNUSpiritFollowSettings& InSettings);

	// 주인 기준 목표 위치, 스폰 직후 즉시 배치할 때 사용
	FVector GetDesiredLocation(const AActor* Target) const;

	// 소유 액터를 목표 위치로 보간해 옮김
	void UpdateFollow(const AActor* Target, float DeltaSeconds);

	// 순간이동 후 부유 위치 초기화
	void ResetBob();

private:
	UPROPERTY(VisibleAnywhere, Category = "NU|Follow", meta = (AllowPrivateAccess = "true"))
	FNUSpiritFollowSettings Settings;

	float BobTime = 0.f;
};
