#include "Field/NPC/NUQuestNPCBase.h"

#include "Quest/Component/NUQuestMarkerComponent.h"
#include "Quest/Component/NUQuestProviderComponent.h"

#include "Components/SphereComponent.h"

ANUQuestNPCBase::ANUQuestNPCBase()
{
	// 기존 Quest NPC Blueprint와 레벨 인스턴스의 설정을 보존하기 위해 내부 Subobject 이름은 유지한다.
	QuestProvider = CreateDefaultSubobject<UNUQuestProviderComponent>(TEXT("NPCInteraction"));

	QuestMarker = CreateDefaultSubobject<UNUQuestMarkerComponent>(TEXT("QuestMarker"));
	QuestMarker->SetupAttachment(InteractionVolume);
}
