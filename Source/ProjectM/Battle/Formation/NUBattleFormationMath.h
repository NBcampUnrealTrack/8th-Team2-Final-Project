// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"

namespace NUBattleFormation
{
	// 두 진형 중심이 수평으로 겹치면 전진 방향을 만들 수 없음
	PROJECTM_API bool ResolveAdvanceDirection(
		const FVector& AllyCenter,
		const FVector& EnemyCenter,
		bool bIsAlly,
		FVector& OutDirection
		);

	// MoveToTarget시 대상이 유효하지 않으면 폴백은 호출부에서 MoveToCenter로 다시 요청해야 함
	PROJECTM_API FVector ResolveActionLocation(
		ENUBattleActionMotion Motion,
		const FVector& SourceWorld,
		const FVector& TargetWorld,
		bool bHasTarget,
		const FVector& ArenaOrigin,
		const FVector& Advance,
		float ActionLineOffset,
		float TargetApproachDistance
		);
}
