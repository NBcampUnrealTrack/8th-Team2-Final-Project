// Fill out your copyright notice in the Description page of Project Settings.


#include "NUParryComponent.h"

#include "Battle/UI/NUParryCueWidget.h"
#include "Engine/World.h"
#include "PlayerController/NUPlayerController.h"


UNUParryComponent::UNUParryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicated(false);
}

void UNUParryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (WindowActionSequence == INDEX_NONE)
	{
		SetComponentTickEnabled(false);
		return;
	}

	float Elapsed = GetWorld()->GetTimeSeconds() - WindowStartTime;
	float ChallengeStart = ImpactOffset - Timing.GetLeadTime();

	if (Elapsed < ChallengeStart) return;   // 아직 도전 구간 전

	float Progress = (Elapsed - ChallengeStart) / Timing.ChallengeDuration;

	// 위젯이 없어도 입력 창은 유지, 표시만 생략
	if (IsValid(CueWidget))
	{
		if (CueWidget->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			CueWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		CueWidget->SetProgress(Progress);
	}

	if (Progress >= 1.f)
	{
		CloseWindow();
	}
}

void UNUParryComponent::SetCueWidget(UNUParryCueWidget* InWidget)
{
	CueWidget = InWidget;
}

void UNUParryComponent::OpenWindow(int32 InActionSequence, float InImpactOffset,
	const FNUParryTimingSpec& InTiming, int32 InSPCost)
{
	WindowActionSequence = InActionSequence;
	ImpactOffset = InImpactOffset;
	Timing = InTiming;
	WindowStartTime = GetWorld()->GetTimeSeconds();
	bPressSent = false;
	
	if (IsValid(CueWidget))
	{
		CueWidget->InitializeZones(Timing, InSPCost);
		CueWidget->SetVisibility(ESlateVisibility::Collapsed);   // 도전 구간 시작까지 숨김
	}
	
	SetComponentTickEnabled(true);
}

void UNUParryComponent::CloseWindow()
{
	WindowActionSequence = INDEX_NONE;
	WindowStartTime = 0.f;
	ImpactOffset = 0.f;
	bPressSent = false;
	
	SetComponentTickEnabled(false);
	
	if (IsValid(CueWidget))
	{
		CueWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNUParryComponent::RegisterPress()
{
	// 이미 눌렀거나 잘못된 행동 순서라면 X
	if (WindowActionSequence == INDEX_NONE || bPressSent) return;
	
	float PressOffset = GetWorld()->GetTimeSeconds() - WindowStartTime;
	
	// 도전 구간 밖이면 입력을 소모 X
	if (PressOffset < ImpactOffset - Timing.GetLeadTime()
		|| PressOffset > ImpactOffset + Timing.GetTailTime())
	{
		return;
	}
	
	bPressSent = true;

	// 누른 즉시 창 닫기
	int32 SentActionSequence = WindowActionSequence;
	CloseWindow();

	if (ANUPlayerController* PC = Cast<ANUPlayerController>(GetOwner()))
	{
		// 로컬 전투는 인카운터로, 넷 전투는 재생 큐로 간다
		PC->RouteParryPress(SentActionSequence, PressOffset);
	}
}
