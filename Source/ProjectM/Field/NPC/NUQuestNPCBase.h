#pragma once

#include "CoreMinimal.h"
#include "Field/NPC/NUNPCBase.h"
#include "NUQuestNPCBase.generated.h"

class UNUQuestProviderComponent;
class UNUQuestMarkerComponent;

// 퀘스트를 제공하거나 퀘스트 목표가 되는 NPC의 공통 부모
// 일반 NPC 기능은 ANUNPCBase가, 퀘스트 Provider와 마커는 이 클래스가 담당
UCLASS(Abstract, Blueprintable)
class PROJECTM_API ANUQuestNPCBase : public ANUNPCBase
{
	GENERATED_BODY()

public:
	ANUQuestNPCBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Quest", meta = (DisplayName = "Quest Provider"))
	TObjectPtr<UNUQuestProviderComponent> QuestProvider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Quest", meta = (DisplayName = "Quest Marker"))
	TObjectPtr<UNUQuestMarkerComponent> QuestMarker;
};
