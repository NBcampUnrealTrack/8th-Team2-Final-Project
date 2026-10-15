#pragma once

#include "CoreMinimal.h"
#include "Field/NPC/NUQuestNPCBase.h"
#include "NURecoveryNPCBase.generated.h"

class UNURecoveryProviderComponent;

// 회복 NPC에 퀘스트 기능과 즉시 회복 Provider를 기본으로 제공한다.
UCLASS(Abstract, Blueprintable)
class PROJECTM_API ANURecoveryNPCBase : public ANUQuestNPCBase
{
	GENERATED_BODY()

public:
	ANURecoveryNPCBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Recovery", meta = (DisplayName = "Recovery Provider"))
	TObjectPtr<UNURecoveryProviderComponent> RecoveryProvider;

};
