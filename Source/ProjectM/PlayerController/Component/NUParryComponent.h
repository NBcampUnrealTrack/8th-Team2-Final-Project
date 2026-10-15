// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Battle/NUBattleTypes.h"
#include "NUParryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUParryJudged, bool, bSuccess);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUParryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUParryComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	
	void SetCueWidget(class UNUParryCueWidget* InWidget);
	
	void OpenWindow(int32 InActionSequence, float InImpactOffset, const FNUParryTimingSpec& InTiming,
		int32 InSPCost);
	void CloseWindow();
	
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Parry")
	void RegisterPress();

public:
	// 델리게이트
	
	UPROPERTY(BlueprintAssignable, Category = "NU|Parry")
	FOnNUParryJudged OnParryJudged;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UNUParryCueWidget> CueWidget;
	
	FNUParryTimingSpec Timing;

	int32 WindowActionSequence = INDEX_NONE;
	float WindowStartTime = 0.f;
	float ImpactOffset = 0.f;
	bool bPressSent = false;
};
