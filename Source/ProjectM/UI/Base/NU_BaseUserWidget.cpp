// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Base/NU_BaseUserWidget.h"


void UNU_BaseUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheNUPlayerController();
	CacheNUPawn();

	InitializeWidget();
}

void UNU_BaseUserWidget::NativeDestruct()
{
	ReleaseWidget();
	Super::NativeDestruct();
}

void UNU_BaseUserWidget::CacheNUPlayerController()
{
	CachedNUPlayerController = GetOwningPlayer();
}

void UNU_BaseUserWidget::CacheNUPawn()
{
	//PC가 올바르지 않을 경우 다시 PC 캐싱 수행
	if (!CachedNUPlayerController.IsValid())
	{
		CacheNUPlayerController();
	}

	CachedNUPawn = CachedNUPlayerController.IsValid()
		? CachedNUPlayerController->GetPawn()
		: nullptr;
}

void UNU_BaseUserWidget::RefreshNUPawn()
{
	if (CachedNUPlayerController.IsValid())
	{
		CachedNUPawn = CachedNUPlayerController->GetPawn();
	}
}

void UNU_BaseUserWidget::InitializeWidget()
{

}

void UNU_BaseUserWidget::ReleaseWidget()
{

}

APlayerController* UNU_BaseUserWidget::GetNUPlayerController() const
{
	return CachedNUPlayerController.Get();
}

APawn* UNU_BaseUserWidget::GetNUPawn() const
{
	return CachedNUPawn.Get();
}

