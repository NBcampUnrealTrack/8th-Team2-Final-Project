// Ember Chronicle - shared PaperZD locomotion state
#pragma once

#include "CoreMinimal.h"
#include "PaperZDAnimInstance.h"
#include "NUPaperZDAnimInstance.generated.h"

/** Runtime state consumed by the generated PaperZD animation graphs. */
UCLASS(Abstract, Blueprintable)
class PROJECTM_API UNUPaperZDAnimInstance : public UPaperZDAnimInstance
{
	GENERATED_BODY()

public:
	/** Selects the walk branch when true and the idle branch when false. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "NU|Animation")
	bool bIsMoving = false;

	/** PaperZD directional input: Up=(0,1), Right=(1,0), Down=(0,-1), Left=(-1,0). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "NU|Animation")
	FVector2D DirectionInput = FVector2D(0.0f, -1.0f);

	UFUNCTION(BlueprintCallable, Category = "NU|Animation")
	void SetLocomotionState(bool bInIsMoving, FVector2D InDirectionInput);

	/** 전투 전용, 사망시 사망 애니메이션으로 전환하는 용도 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "NU|Animation")
	bool bIsDead = false;

	UFUNCTION(BlueprintCallable, Category = "NU|Animation")
	void SetDeadState(bool bInIsDead);
};
