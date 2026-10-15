#include "Quest/Component/NUQuestComponent.h"

#include "Inventory/NUInventoryComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "Quest/Data/NUQuestEvent.h"

// 인벤토리의 일반 이벤트를 퀘스트 이벤트로 변환하는 연결부.
// 실제 변경 결과만 구독한다. 목록 동기화와 창 갱신은 목표 진행으로 취급하지 않는다.
void UNUQuestComponent::InitializeInventoryTracking()
{
	UnbindInventoryTracking();
	InventoryComponent = GetOwner()->FindComponentByClass<UNUInventoryComponent>();
	if (!InventoryComponent.IsValid()) { return; }

	InventoryComponent->OnItemAcquired.AddUObject(this, &ThisClass::HandleItemAcquired);
	InventoryComponent->OnPurchaseSucceeded.AddUObject(this, &ThisClass::HandlePurchaseSucceeded);
	InventoryComponent->OnStorageAccessed.AddUObject(this, &ThisClass::HandleStorageAccessed);
}

void UNUQuestComponent::UnbindInventoryTracking()
{
	if (InventoryComponent.IsValid())
	{
		InventoryComponent->OnItemAcquired.RemoveAll(this);
		InventoryComponent->OnPurchaseSucceeded.RemoveAll(this);
		InventoryComponent->OnStorageAccessed.RemoveAll(this);
	}
	InventoryComponent.Reset();
}

// 실패하거나 실제로 증가한 수량이 없으면 수집 목표를 진행시키지 않는다.
void UNUQuestComponent::HandleItemAcquired(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0) { return; }

	FNUQuestEvent Event;
	Event.ObjectiveType = NU::Quest::Objective::CollectItem;
	Event.TargetId = ItemId;
	Event.Amount = Count;
	HandleQuestEvent(Event);
}

// 구매와 창고 목표는 아이템이 아닌 서비스를 제공한 NPC를 대상으로 판정한다.
void UNUQuestComponent::HandlePurchaseSucceeded(int32 NpcId)
{
	if (NpcId <= 0) { return; }

	FNUQuestEvent Event;
	Event.ObjectiveType = NU::Quest::Objective::PurchaseItem;
	Event.TargetId = FName(*FString::FromInt(NpcId));
	Event.Amount = 1;
	HandleQuestEvent(Event);
}

void UNUQuestComponent::HandleStorageAccessed(int32 NpcId)
{
	if (NpcId <= 0) { return; }

	FNUQuestEvent Event;
	Event.ObjectiveType = NU::Quest::Objective::UseStorage;
	Event.TargetId = FName(*FString::FromInt(NpcId));
	Event.Amount = 1;
	HandleQuestEvent(Event);
}
