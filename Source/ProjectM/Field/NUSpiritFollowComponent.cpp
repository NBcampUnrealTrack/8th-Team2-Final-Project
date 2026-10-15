// Fill out your copyright notice in the Description page of Project Settings.

#include "Field/NUSpiritFollowComponent.h"

#include "GameFramework/Actor.h"

namespace NUSpiritFollow
{
	FNUSpiritFollowSettings MakeDefaults()
	{
		// 기본값은 구조체 선언에 있음
		// 종류가 늘면 여기서 추가해서 사용
		return FNUSpiritFollowSettings();
	}
}

UNUSpiritFollowComponent::UNUSpiritFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNUSpiritFollowComponent::Setup(const FNUSpiritFollowSettings& InSettings)
{
	Settings = InSettings;
}

FVector UNUSpiritFollowComponent::GetDesiredLocation(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return GetOwner() != nullptr ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	}

	return Target->GetActorTransform().TransformPosition(Settings.FollowOffset);
}

void UNUSpiritFollowComponent::UpdateFollow(const AActor* Target, float DeltaSeconds)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(Target)) return;

	FVector Desired = GetDesiredLocation(Target);
	FVector NewLocation = FMath::VInterpTo(OwnerActor->GetActorLocation(), Desired, DeltaSeconds, Settings.InterpSpeed);

	if (GetNetMode() != NM_DedicatedServer)
	{
		BobTime += DeltaSeconds;
		NewLocation.Z = Desired.Z + FMath::Sin(BobTime * Settings.BobFrequency * 2.0f * PI) * Settings.BobAmplitude;
	}

	OwnerActor->SetActorLocation(NewLocation);
}

void UNUSpiritFollowComponent::ResetBob()
{
	BobTime = 0.f;
}
