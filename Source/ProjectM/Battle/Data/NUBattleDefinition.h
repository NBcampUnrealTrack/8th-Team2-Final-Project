#pragma once

#include "CoreMinimal.h"
#include "Battle/Interface/NUBattleEventInterface.h"
#include "Engine/DataAsset.h"
#include "NUBattleDefinition.generated.h"

// 전투 요청에 연결하는 공유 설정. 진행 중 상태와 어빌리티 핸들은 보관하지 않는다.
// 필요한 외부 처리자를 제공하며 공용 전투는 구체적인 퀘스트 타입을 알지 않는다.
UCLASS(Abstract, BlueprintType)
class PROJECTM_API UNUBattleDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual TScriptInterface<INUBattleEventInterface> GetEventHandler(ANUBattleEncounter* Encounter) { return {}; }
};
