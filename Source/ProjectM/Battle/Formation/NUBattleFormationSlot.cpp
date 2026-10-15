// Fill out your copyright notice in the Description page of Project Settings.


#include "Battle/Formation/NUBattleFormationSlot.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"


ANUBattleFormationSlot::ANUBattleFormationSlot()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Icon = CreateDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	SetRootComponent(Icon);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(Icon);
}

ENUBattleFormationRow ANUBattleFormationSlot::GetRow() const
{
	return Row;
}

int32 ANUBattleFormationSlot::GetSlotIndex() const
{
	return SlotIndex;
}
