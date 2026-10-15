// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUBattleTargetIndicator.generated.h"

UCLASS()
class PROJECTM_API ANUBattleTargetIndicator : public AActor
{
	GENERATED_BODY()

public:
	ANUBattleTargetIndicator();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UPaperSpriteComponent> IndicatorSprite;
};
