// Fill out your copyright notice in the Description page of Project Settings.


#include "Battle/UI/NUActionBannerWidget.h"

#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UNUActionBannerWidget::Setup(const FNUBattleActionBanner& InBanner)
{
	Banner = InBanner;
	
	// 배너는 입력을 받지 않으므로 커맨드 UI를 가리지 않게 HitTestInvisible
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (BannerText != nullptr)
	{
		BannerText->SetText(GetBannerText());
	}
	if (BannerRoot != nullptr)
	{
		bLeaving = false;
		BannerMotion.Add(BannerRoot, 0.f, 0.3f, FVector2D(0.f, -12.f), 0.f, 0.96f);
		BannerMotion.Start();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
		World->GetTimerManager().SetTimer(
			HideTimer, 
			this,
			&ThisClass::HideBanner,
			Lifetime, 
			false
			);
	}
	
	OnBannerShown();
}

FText UNUActionBannerWidget::GetBannerText() const
{
	FText Result = Banner.DisplayName;

	// 레벨이 다른 스킬이 아직 없어 지금은 항상 건너뜀
	// 나중에 스킬 레벨이 달라지면 사용됨
	if (Banner.Level >= 2)
	{
		Result = FText::Format(
			NSLOCTEXT("NUBattle", "ActionBannerLevel", "{0} Lv.{1}"),
			Result,
			FText::AsNumber(Banner.Level));
	}

	// 1타면 뒤에 아무것도 붙이지 않음
	if (Banner.HitCount >= 2)
	{
		Result = FText::Format(
			NSLOCTEXT("NUBattle", "ActionBannerHitCount", "{0} ×{1}"),
			Result,
			FText::AsNumber(Banner.HitCount));
	}

	return Result;
}

void UNUActionBannerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
	
	Super::NativeDestruct();
}

void UNUActionBannerWidget::HideBanner()
{
	if (BannerRoot != nullptr && GetVisibility() != ESlateVisibility::Collapsed)
	{
		bLeaving = true;
		BannerMotion.AddOut(BannerRoot, 0.f, 0.22f, FVector2D(0.f, -6.f));
		BannerMotion.Start();
		return;
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UNUActionBannerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BannerMotion.Advance(InDeltaTime) && bLeaving)
	{
		bLeaving = false;
		SetVisibility(ESlateVisibility::Collapsed);
		BannerRoot->SetRenderOpacity(1.f);
		BannerRoot->SetRenderTransform(FWidgetTransform());
	}
}
