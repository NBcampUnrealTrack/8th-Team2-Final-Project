// Fill out your copyright notice in the Description page of Project Settings.


#include "NUCombatantVitalWidget.h"

namespace
{
	float NUSafeRatio(float Current, float Max)
	{
		return Max > KINDA_SMALL_NUMBER ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;
	}
}

void UNUCombatantVitalWidget::SetVitals(float InHealth, float InMaxHealth, float InSP, float InMaxSP, bool bInShowSP)
{
	Health = InHealth;
	MaxHealth = InMaxHealth;
	ActualSP = InSP;
	MaxSP = InMaxSP;
	bShowSP = bInShowSP;

	HealthPercent = NUSafeRatio(Health, MaxHealth);

	RefreshDisplay();
}

void UNUCombatantVitalWidget::SetPendingSpend(float InPendingSpend)
{
	PendingSpend = InPendingSpend;

	RefreshDisplay();
}

void UNUCombatantVitalWidget::RefreshDisplay()
{
	// 예정 소모를 미리 깎아서 보여줌
	// ←/→ 로 투입량을 조절하면 실제로 줄었다 차는 것처럼 보이고, WBP는 평소대로 SP Percent만 그리면 됨
	SP = FMath::Max(0.f, ActualSP - PendingSpend);
	SPPercent = NUSafeRatio(SP, MaxSP);

	OnVitalsChanged();
}
