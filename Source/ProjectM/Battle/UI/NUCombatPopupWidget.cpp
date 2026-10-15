// Fill out your copyright notice in the Description page of Project Settings.


#include "NUCombatPopupWidget.h"

#include "Engine/World.h"


void UNUCombatPopupWidget::Setup(AActor* InAnchorActor, const FNUCombatPopupData& InPopupData, int32 StackIndex)
{
	AnchorActor = InAnchorActor;
	PopupData = InPopupData;

	StackOffset = StackStep * static_cast<float>(FMath::Max(0, StackIndex));
	SpawnWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	OnPopupSetup();
}

AActor* UNUCombatPopupWidget::GetAnchorActor() const
{
	return AnchorActor.Get();
}

const FVector2D& UNUCombatPopupWidget::GetStackOffset() const
{
	return StackOffset;
}

bool UNUCombatPopupWidget::IsExpired(float WorldTime) const
{
	return WorldTime - SpawnWorldTime >= Lifetime;
}
