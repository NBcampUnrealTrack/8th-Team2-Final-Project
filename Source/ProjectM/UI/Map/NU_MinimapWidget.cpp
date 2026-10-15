// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Map/NU_MinimapWidget.h"

#include "UI/Map/NU_MapImageViewWidget.h"

void UNU_MinimapWidget::InitializeWidget()
{
	// 베이스가 초기 전투 상태를 통지하며 ApplyFieldVisibility 를 부르므로 그 전에 구독해 둔다
	if (MapView != nullptr)
	{
		MapChangedHandle = MapView->OnMapImageChanged.AddUObject(this, &ThisClass::ApplyFieldVisibility);
	}
	Super::InitializeWidget();
}

void UNU_MinimapWidget::ReleaseWidget()
{
	if (MapView != nullptr)
	{
		MapView->OnMapImageChanged.Remove(MapChangedHandle);
	}
	Super::ReleaseWidget();
}

void UNU_MinimapWidget::ApplyFieldVisibility()
{
	const bool bVisible = ShouldShowFieldUI() && MapView != nullptr && MapView->HasMapImage();
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	BP_OnFieldVisibilityChanged(bVisible);
}
