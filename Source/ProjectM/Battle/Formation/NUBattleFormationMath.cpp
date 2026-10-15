// Fill out your copyright notice in the Description page of Project Settings.

#include "NUBattleFormationMath.h"

bool NUBattleFormation::ResolveAdvanceDirection(const FVector& AllyCenter, const FVector& EnemyCenter,
	bool bIsAlly, FVector& OutDirection)
{
	const FVector From = bIsAlly ? AllyCenter : EnemyCenter;
	const FVector To = bIsAlly ? EnemyCenter : AllyCenter;

	OutDirection = (To - From).GetSafeNormal2D();

	return !OutDirection.IsNearlyZero();
}

FVector NUBattleFormation::ResolveActionLocation(ENUBattleActionMotion Motion, const FVector& SourceWorld,
	const FVector& TargetWorld, bool bHasTarget, const FVector& ArenaOrigin, const FVector& Advance,
	float ActionLineOffset, float TargetApproachDistance)
{
	switch (Motion)
	{
	case ENUBattleActionMotion::InPlace:
		return SourceWorld;

	case ENUBattleActionMotion::MoveToCenter:
		{
			const float AlongAxis = FVector::DotProduct(ArenaOrigin - SourceWorld, Advance);

			return SourceWorld + Advance * (AlongAxis - ActionLineOffset);
		}

	case ENUBattleActionMotion::MoveToTarget:
		if (bHasTarget)
		{
			return TargetWorld - Advance * TargetApproachDistance;
		}
		break;
	}

	return SourceWorld;
}
