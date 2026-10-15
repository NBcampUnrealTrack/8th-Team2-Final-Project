// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleTargetIndicator.h"

#include "PaperSpriteComponent.h"


ANUBattleTargetIndicator::ANUBattleTargetIndicator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComponent);

	IndicatorSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("IndicatorSprite"));
	IndicatorSprite->SetupAttachment(RootComponent);
	IndicatorSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
