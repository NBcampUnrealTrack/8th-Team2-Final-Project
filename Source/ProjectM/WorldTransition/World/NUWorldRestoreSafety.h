#pragma once

#include "CoreMinimal.h"

class ACharacter;
class ANUWorldEntryPoint;
class UNUWalkableInteriorComponent;

// 이동 전에 목적 크기의 캡슐, 월드 범위, 보행 가능한 바닥을 확인한다.
namespace NUWorldRestoreSafety
{
	PROJECTM_API bool IsSafe(ACharacter* Character, UNUWalkableInteriorComponent* Interior, const FVector& Location);
	PROJECTM_API bool FindFloorArrival(ACharacter* Character, UNUWalkableInteriorComponent* Interior, const FVector& Center, FVector& OutLocation);
	PROJECTM_API bool FindEntryArrival(ACharacter* Character, UNUWalkableInteriorComponent* Interior, const ANUWorldEntryPoint* Entry, FVector& OutLocation);
	PROJECTM_API bool FindInteriorArrival(ACharacter* Character, UNUWalkableInteriorComponent* Interior, FVector& OutLocation);
}
