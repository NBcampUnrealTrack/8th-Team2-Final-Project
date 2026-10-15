#pragma once

#include "CoreMinimal.h"

class AActor;

namespace NUInteractionSpatialRules
{
	// 같은 공간에서 허용하며, 문은 연결된 실내와 실외 양쪽에서 허용한다. 출입 전환 중에는 거부한다.
	PROJECTM_API bool CanInteract(AActor* Interactor, AActor* Target);
}
