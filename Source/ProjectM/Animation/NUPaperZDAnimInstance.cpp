#include "Animation/NUPaperZDAnimInstance.h"

void UNUPaperZDAnimInstance::SetLocomotionState(bool bInIsMoving, FVector2D InDirectionInput)
{
	bIsMoving = bInIsMoving;
	if (!InDirectionInput.IsNearlyZero())
	{
		DirectionInput = InDirectionInput.GetSafeNormal();
	}
}

void UNUPaperZDAnimInstance::SetDeadState(bool bInIsDead)
{
	bIsDead = bInIsDead;
}
