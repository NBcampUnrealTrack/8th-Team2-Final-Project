// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Common/NU_JobResourceGaugeWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UNU_JobResourceGaugeWidget::SetJobResource(const FNUJobResourceView& InView)
{
	View = InView;

	if (IsValid(Txt_Name))
	{
		Txt_Name->SetText(View.DisplayName);
	}

	if (IsValid(Txt_Value))
	{
		Txt_Value->SetText(View.MaxAmount > 0
			? FText::FromString(FString::Printf(TEXT("%d / %d"), View.Amount, View.MaxAmount))
			: FText::AsNumber(View.Amount));
	}

	if (IsValid(Bar_Amount))
	{
		Bar_Amount->SetPercent(View.MaxAmount > 0
			? FMath::Clamp(static_cast<float>(View.Amount) / View.MaxAmount, 0.f, 1.f)
			: 0.f);
	}

	OnJobResourceChanged();
}
