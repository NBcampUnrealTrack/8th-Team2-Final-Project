// Fill out your copyright notice in the Description page of Project Settings.


#include "NUParryCueWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

void UNUParryCueWidget::InitializeZones(const FNUParryTimingSpec& InTiming, int32 InSPCost)
{
	CachedTiming = InTiming;
	ParrySPCost = InSPCost;
	
	float SuccessRatio = InTiming.GetSuccessRatio();
	SuccessStart = FMath::Clamp(InTiming.SuccessAnchor - SuccessRatio, 0.f, 1.f);
	SuccessEnd   = FMath::Clamp(InTiming.SuccessAnchor + SuccessRatio, 0.f, 1.f);
	
	Progress = 0.f;
	bSuccess = false;
	ApplyNativeCue();
	
	OnZonesInitialized();
}

void UNUParryCueWidget::SetProgress(float InProgress)
{
	Progress = FMath::Clamp(InProgress, 0.f, 1.f);
	
	bSuccess = CachedTiming.Judge(
		Progress * CachedTiming.ChallengeDuration,
		CachedTiming.GetLeadTime());
	ApplyNativeCue();
	
	OnProgressChanged();
}

void UNUParryCueWidget::SetDodgePrompt(bool bInOpen, int32 InSPCost)
{
	bDodgePromptOpen = bInOpen;
	DodgeSPCost = InSPCost;

	// 패링은 UNUParryComponent가 루트 Visibility를 토글해 주지만 회피는 따로 처리가 없음
	// 여기서 Hidden / Visible 처리
	SetVisibility(bDodgePromptOpen
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	ApplyNativeCue();

	OnDodgePromptChanged();
}

void UNUParryCueWidget::ApplyNativeCue()
{
	// 성공 구간과 바늘을 막대 안에 놓는다 (CueBarCanvas 기준 왼쪽에서 비율 x 폭)
	if (CueZone != nullptr)
	{
		if (UCanvasPanelSlot* ZoneSlot = Cast<UCanvasPanelSlot>(CueZone->Slot))
		{
			ZoneSlot->SetPosition(FVector2D(SuccessStart * CueBarWidth, ZoneSlot->GetPosition().Y));
			ZoneSlot->SetSize(FVector2D(FMath::Max((SuccessEnd - SuccessStart) * CueBarWidth, 2.f), ZoneSlot->GetSize().Y));
		}
	}
	if (CueNeedle != nullptr)
	{
		if (UCanvasPanelSlot* NeedleSlot = Cast<UCanvasPanelSlot>(CueNeedle->Slot))
		{
			NeedleSlot->SetPosition(FVector2D(Progress * CueBarWidth, NeedleSlot->GetPosition().Y));
		}
	}
	if (CueSuccessGlow != nullptr)
	{
		CueSuccessGlow->SetRenderOpacity(bSuccess ? 1.f : 0.f);
	}
	if (ParryRow != nullptr)
	{
		ParryRow->SetVisibility(bDodgePromptOpen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (DodgeRow != nullptr)
	{
		DodgeRow->SetVisibility(bDodgePromptOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ParryCostText != nullptr)
	{
		ParryCostText->SetText(ParrySPCost > 0 ? FText::Format(NSLOCTEXT("NUBattle", "CueSP", "SP {0}"), FText::AsNumber(ParrySPCost)) : FText::GetEmpty());
	}
	if (DodgeCostText != nullptr)
	{
		DodgeCostText->SetText(DodgeSPCost > 0 ? FText::Format(NSLOCTEXT("NUBattle", "CueSP", "SP {0}"), FText::AsNumber(DodgeSPCost)) : FText::GetEmpty());
	}
}
