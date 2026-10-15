// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Common/NU_CloseButtonWidget.h"

#include "Components/Button.h"

void UNU_CloseButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(Btn_Close))
	{
		Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
		Btn_Close->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}
}

void UNU_CloseButtonWidget::HandleClicked()
{
	OnCloseClicked.Broadcast();
}
