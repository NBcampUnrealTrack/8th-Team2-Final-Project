// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUInventoryComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUGE_ItemHeal.h"
#include "Inventory/NUInventoryTags.h"
#include "Inventory/UI/NUCharacterInfoWidget.h"
#include "Inventory/UI/NUEquipmentWidget.h"
#include "Inventory/UI/NUInventoryWindowWidget.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "PlayerController/NUPlayerController.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Recovery/Component/NURecoveryComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shop/NUCraftTypes.h"
#include "Shop/UI/NUCraftWindowWidget.h"
#include "Shop/NUShopTypes.h"
#include "Shop/UI/NUShopWindowWidget.h"
#include "TimerManager.h"
#include "Trade/NUTradeSubsystem.h"
#include "UI/CharacterMenu/NUCharacterMenuBase.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

#if !UE_BUILD_SHIPPING
#include "Inventory/NUInventoryDebugOverlay.h"
#endif

UNUInventoryComponent::UNUInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	InventoryList.OwnerComponent = this;
}

void UNUInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, InventoryList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, Gold, COND_OwnerOnly);
}

void UNUInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// 서버/클라 양쪽에서 준비 — 클라도 List 표시명 등 정의 조회가 필요
	EnsureItemDefTable();

	// 전투 진입 시 열려 있는 시트 자동 닫기 (로컬 클라에서만 의미) --- 로컬 전투는 ASC가, 서버 전투는 넷 전투 서브시스템과 전투 무대가 알린다
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	if (UNUAbilitySystemComponent* ASC = OwnerState ? OwnerState->GetNUASC() : nullptr)
	{
		ASC->OnBattleEncounterChanged.AddWeakLambda(this, [this](ANUBattleEncounter*) { RefreshOwnerBattleState(); });
	}
	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnBattleStateChanged.AddUObject(this, &UNUInventoryComponent::RefreshOwnerBattleState);
	}
	if (UNUBattleNetStageSubsystem* Stage = GetWorld()->GetSubsystem<UNUBattleNetStageSubsystem>())
	{
		Stage->OnNetStageReadyChanged.AddWeakLambda(this, [this](bool) { RefreshOwnerBattleState(); });
	}

#if !UE_BUILD_SHIPPING
	// 개발 편의: 시작 아이템 자동 지급 (콘솔로 매번 nu.Inv.Add 칠 필요 없게).
	// 서버 인벤에 아이템이 있으면 목록 응답이 이걸 덮어쓴다 (서버가 원천)
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		for (const TPair<FName, int32>& Entry : DebugStartingItems)
		{
			AddItem(Entry.Key, Entry.Value);
		}
	}
#endif

	// 게임서버 인벤 로드 — 내 PlayerState일 때만 (필드 진입은 로그인 뒤라 보통 즉시 성립,
	// 필드 직행 PIE의 늦은 로그인은 OnStateChanged로 받는다)
	const APlayerController* LocalPC = GetLocalPlayerController();
	if (LocalPC != nullptr && LocalPC->PlayerState == GetOwner())
	{
		if (UEmberServerManager* Server = GetServerManager())
		{
			Server->OnStateChanged.AddDynamic(this, &UNUInventoryComponent::HandleEmberStateChanged);
			if (Server->IsLoggedIn())
			{
				RequestServerInventory();
			}
		}
	}
}

void UNUInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.RemoveDynamic(this, &UNUInventoryComponent::HandleEmberStateChanged);
	}
	// 넷 전투 서브시스템은 게임 인스턴스 수명이라 월드보다 오래 산다
	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnBattleStateChanged.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

/////////////////////////////////////////////////////////////////
// 서버 권위 API
/////////////////////////////////////////////////////////////////

bool UNUInventoryComponent::CanReceiveItems(const TMap<FName, int32>& Items) const
{
	// 새로 필요한 칸은 아이템이 드는 범위(분류 구간)마다 센다 --- 구간이 없는 가방은 전체가 한 범위
	TArray<TPair<FNUBagSection, int64>, TInlineAllocator<NUBagCategory::Num>> RequiredSlots;
	for (const TPair<FName, int32>& Entry : Items)
	{
		const FNUItemDefRow* Definition = FindItemDef(Entry.Key);
		if (Definition == nullptr || Definition->MaxStack <= 0 || Entry.Value <= 0) { return false; }
		int64 Remaining = Entry.Value;
		for (const FNUItemInstance& Item : InventoryList.Items)
		{
			if (Item.ItemRowName == Entry.Key) { Remaining -= FMath::Max(0, Definition->MaxStack - Item.Count); }
		}
		const FNUBagSection Range = GetBagRange(Entry.Key);
		TPair<FNUBagSection, int64>* Required = RequiredSlots.FindByPredicate([&Range](const TPair<FNUBagSection, int64>& Pair) { return Pair.Key.Start == Range.Start; });
		if (Required == nullptr) { Required = &RequiredSlots.Emplace_GetRef(Range, 0); }
		Required->Value += (FMath::Max<int64>(0, Remaining) + Definition->MaxStack - 1) / Definition->MaxStack;
	}
	return !RequiredSlots.ContainsByPredicate([this](const TPair<FNUBagSection, int64>& Pair) { return Pair.Value > CountFreeSlots(Pair.Key); });
}

bool UNUInventoryComponent::AddItemsAtomic(const TMap<FName, int32>& Items, TFunction<bool()> BeforeNotify)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || IsServerInventoryActive() || !CanReceiveItems(Items)) { return false; }
	const TArray<FNUItemInstance> PreviousItems = InventoryList.Items;
	// 전체 용량을 검사한 뒤 알림 없이 모두 반영한다.
	for (const TPair<FName, int32>& Entry : Items)
	{
		const FNUItemDefRow* Definition = FindItemDef(Entry.Key);
		int32 Remaining = Entry.Value;
		for (FNUItemInstance& Item : InventoryList.Items)
		{
			if (Item.ItemRowName != Entry.Key || Remaining == 0) { continue; }
			const int32 Fill = FMath::Min(Remaining, FMath::Max(0, Definition->MaxStack - Item.Count));
			Item.Count += Fill;
			Remaining -= Fill;
			if (Fill > 0) { InventoryList.MarkItemDirty(Item); }
		}
		while (Remaining > 0)
		{
			const int32 Slot = FindFreeSlotNo(Entry.Key);
			FNUItemInstance& Item = InventoryList.Items.AddDefaulted_GetRef();
			Item.InstanceId = FGuid::NewGuid();
			Item.ItemRowName = Entry.Key;
			Item.SlotNo = Slot;
			Item.Count = FMath::Min(Remaining, Definition->MaxStack);
			Remaining -= Item.Count;
			InventoryList.MarkItemDirty(Item);
		}
	}
	// 다른 보상 검증이 실패하면 알림을 보내기 전에 원상 복구한다.
	if (BeforeNotify && !BeforeNotify()) { InventoryList.Items = PreviousItems; InventoryList.MarkArrayDirty(); return false; }
	BroadcastChangedAndPush();
	for (const TPair<FName, int32>& Entry : Items) { OnItemAcquired.Broadcast(Entry.Key, Entry.Value); }
	return true;
}

bool UNUInventoryComponent::AddItem(FName ItemRowName, int32 Count)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || Count <= 0)
	{
		return false;
	}

	EnsureItemDefTable();
	const FNUItemDefRow* Def = FindItemDef(ItemRowName);
	if (Def == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("AddItem 실패 — 정의 없음 [%s]"), *ItemRowName.ToString());
		return false;
	}

	int32 Remaining = Count;

	// 기존 스택부터 채운다
	for (FNUItemInstance& Item : InventoryList.Items)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Item.ItemRowName != ItemRowName || Item.Count >= Def->MaxStack)
		{
			continue;
		}
		const int32 Fill = FMath::Min(Def->MaxStack - Item.Count, Remaining);
		Item.Count += Fill;
		Remaining -= Fill;
		InventoryList.MarkItemDirty(Item);
	}

	// 남으면 새 스택 — 기존 항목 복사 금지(FastArray ReplicationID 오염), 반드시 기본 생성 후 채운다.
	// 칸이 모자라면 들어가는 만큼만 넣고 나머지는 버린다 (부분 지급)
	while (Remaining > 0)
	{
		const int32 FreeSlotNo = FindFreeSlotNo(ItemRowName);
		if (FreeSlotNo == INDEX_NONE)
		{
			break;
		}
		const int32 Fill = FMath::Min(Def->MaxStack, Remaining);
		FNUItemInstance& NewItem = InventoryList.Items.AddDefaulted_GetRef();
		NewItem.InstanceId = FGuid::NewGuid();
		NewItem.ItemRowName = ItemRowName;
		NewItem.Count = Fill;
		NewItem.SlotNo = FreeSlotNo;
		Remaining -= Fill;
		InventoryList.MarkItemDirty(NewItem);
	}

	const int32 Added = Count - Remaining;
	if (Remaining > 0)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("가방이 가득 참 — [%s] %d개 중 %d개만 받음 (%d/%d칸)"),
			*ItemRowName.ToString(), Count, Added, GetUsedSlots(), MaxSlots);
	}
	if (Added <= 0)
	{
		return false;
	}

	UE_LOG(LogNUInventory, Log, TEXT("AddItem [%s] x%d (%d/%d칸)"),
		*ItemRowName.ToString(), Added, GetUsedSlots(), MaxSlots);
	BroadcastChangedAndPush();

	// 부분 지급된 경우 실제로 받은 수량만 통지
	OnItemAcquired.Broadcast(ItemRowName, Added);

	return true;
}

bool UNUInventoryComponent::RemoveItem(const FGuid& InstanceId, int32 Count)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || Count <= 0)
	{
		return false;
	}

	const int32 Index = InventoryList.Items.IndexOfByPredicate(
		[&InstanceId](const FNUItemInstance& Item) { return Item.InstanceId == InstanceId; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	FNUItemInstance& Item = InventoryList.Items[Index];
	if (Item.Count > Count)
	{
		Item.Count -= Count;
		InventoryList.MarkItemDirty(Item);
	}
	else
	{
		InventoryList.Items.RemoveAt(Index);
		InventoryList.MarkArrayDirty();
	}

	BroadcastChangedAndPush();
	return true;
}

bool UNUInventoryComponent::RemoveInstanceForEquip(const FGuid& InstanceId, FNUItemInstance& OutItem)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	const int32 Index = InventoryList.Items.IndexOfByPredicate(
		[&InstanceId](const FNUItemInstance& Item) { return Item.InstanceId == InstanceId; });
	if (Index == INDEX_NONE || InventoryList.Items[Index].Count < 1)
	{
		return false;
	}

	FNUItemInstance& Item = InventoryList.Items[Index];
	OutItem = FNUItemInstance();
	OutItem.InstanceId = Item.InstanceId;
	OutItem.ItemRowName = Item.ItemRowName;
	OutItem.Count = 1;
	OutItem.SlotNo = Item.SlotNo; // 해제로 돌아올 때 원래 칸 우선 복귀용

	// 장비는 MaxStack=1 전제지만, 스택돼 있어도 1개만 분리해 안전하게 처리
	if (Item.Count > 1)
	{
		Item.Count -= 1;
		InventoryList.MarkItemDirty(Item);
	}
	else
	{
		InventoryList.Items.RemoveAt(Index);
		InventoryList.MarkArrayDirty();
	}

	BroadcastChangedAndPush();
	return true;
}

bool UNUInventoryComponent::AddItemInstance(const FNUItemInstance& Item)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || !Item.InstanceId.IsValid() || Item.Count < 1)
	{
		return false;
	}
	if (FindInstance(Item.InstanceId) != nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("AddItemInstance 실패 — 중복 InstanceId [%s]"), *Item.ItemRowName.ToString());
		return false;
	}
	// InstanceId 보존이 목적이라 기존 스택에 합치지 않는다 => 항상 새 칸이 필요.
	// 원래 있던 칸이 아직 비어 있으면 그 칸으로, 아니면 빈 칸에 배정 (장착 해제 복귀)
	const bool bKeepSlot = GetBagRange(Item.ItemRowName).Contains(Item.SlotNo) && FindBySlotNo(Item.SlotNo) == nullptr;
	const int32 SlotNo = bKeepSlot ? Item.SlotNo : FindFreeSlotNo(Item.ItemRowName);
	if (SlotNo == INDEX_NONE)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("AddItemInstance 실패 — 가방이 가득 참 (%d/%d칸)"), GetUsedSlots(), MaxSlots);
		return false;
	}

	// 기존 항목 복사 금지(FastArray ReplicationID 오염) — 기본 생성 후 필드만 채운다
	FNUItemInstance& NewItem = InventoryList.Items.AddDefaulted_GetRef();
	NewItem.InstanceId = Item.InstanceId;
	NewItem.ItemRowName = Item.ItemRowName;
	NewItem.Count = Item.Count;
	NewItem.SlotNo = SlotNo;
	InventoryList.MarkItemDirty(NewItem);

	BroadcastChangedAndPush();
	return true;
}

/////////////////////////////////////////////////////////////////
// 조회
/////////////////////////////////////////////////////////////////

const FNUItemInstance* UNUInventoryComponent::FindInstance(const FGuid& InstanceId) const
{
	return InventoryList.Items.FindByPredicate(
		[&InstanceId](const FNUItemInstance& Item) { return Item.InstanceId == InstanceId; });
}

const FNUItemInstance* UNUInventoryComponent::FindFirstByRowName(FName ItemRowName) const
{
	return InventoryList.Items.FindByPredicate(
		[ItemRowName](const FNUItemInstance& Item) { return Item.ItemRowName == ItemRowName; });
}

const FNUItemInstance* UNUInventoryComponent::FindBySlotNo(int32 SlotNo) const
{
	if (SlotNo < 0)
	{
		return nullptr;
	}
	return InventoryList.Items.FindByPredicate(
		[SlotNo](const FNUItemInstance& Item) { return Item.SlotNo == SlotNo; });
}

FNUItemInstance* UNUInventoryComponent::FindMutableBySlotNo(int32 SlotNo)
{
	return const_cast<FNUItemInstance*>(FindBySlotNo(SlotNo));
}

bool UNUInventoryComponent::HasFreeSlot(FName ItemRowName) const
{
	return FindFreeSlotNo(ItemRowName) != INDEX_NONE;
}

const FNUBagSection* UNUInventoryComponent::GetBagSection(int32 Category) const
{
	return BagSections.IsValidIndex(Category) ? &BagSections[Category] : nullptr;
}

void UNUInventoryComponent::SetBagSections(const TArray<FNUBagSection>& Sections)
{
	BagSections = Sections;
	if (!BagSections.IsEmpty())
	{
		MaxSlots = 0;
		for (const FNUBagSection& Section : BagSections)
		{
			MaxSlots += Section.Count;
		}
	}
	BroadcastChangedAndPush();
}

int32 UNUInventoryComponent::GetBagCategory(const FNUItemDefRow* Def) const
{
	if (Def == nullptr || BagCategoryByType.IsEmpty())
	{
		return NUItem::GetBagCategory(Def);
	}
	// 교감석 종류(Commune)는 클라 표에만 있다 --- 시트에서는 소모품이라 서버 표에도 소모품으로 들어 있다
	const int32* Category = BagCategoryByType.Find(Def->ItemType == ENUItemType::Commune ? ENUItemType::Consumable : Def->ItemType);
	return Category != nullptr ? *Category : NUBagCategory::Other; // 표에 없는 종류는 기타 (서버와 같은 규칙)
}

FNUBagSection UNUInventoryComponent::GetBagRange(FName ItemRowName) const
{
	const FNUBagSection* Section = GetBagSection(GetBagCategory(FindItemDef(ItemRowName)));
	return Section != nullptr ? *Section : FNUBagSection{ 0, MaxSlots };
}

int32 UNUInventoryComponent::FindFreeSlotNo(FName ItemRowName) const
{
	const FNUBagSection Range = GetBagRange(ItemRowName);
	for (int32 SlotNo = Range.Start; SlotNo < Range.Start + Range.Count; ++SlotNo)
	{
		if (FindBySlotNo(SlotNo) == nullptr)
		{
			return SlotNo;
		}
	}
	return INDEX_NONE;
}

int32 UNUInventoryComponent::CountFreeSlots(const FNUBagSection& Range) const
{
	int32 Used = 0;
	for (const FNUItemInstance& Item : InventoryList.Items)
	{
		Used += Range.Contains(Item.SlotNo) ? 1 : 0;
	}
	return FMath::Max(0, Range.Count - Used);
}

int32 UNUInventoryComponent::FindFreeSlotInContainer(int32 Container, FName ItemRowName) const
{
	if (Container == NUContainer::Inventory)
	{
		return FindFreeSlotNo(ItemRowName);
	}

	const TArray<FNUItemInstance>* Items = GetContainerItems(Container);
	const int32 SlotCount = GetContainerSlotCount(Container);
	if (Items == nullptr || SlotCount <= 0)
	{
		return INDEX_NONE;
	}

	TBitArray<> Used(false, SlotCount);
	for (const FNUItemInstance& Item : *Items)
	{
		if (Item.SlotNo >= 0 && Item.SlotNo < SlotCount)
		{
			Used[Item.SlotNo] = true;
		}
	}
	for (int32 SlotNo = 0; SlotNo < SlotCount; ++SlotNo)
	{
		if (!Used[SlotNo])
		{
			return SlotNo;
		}
	}
	return INDEX_NONE;
}

int32 UNUInventoryComponent::GetCountByItemId(FName ItemRowName) const
{
	int32 Total = 0;
	for (const FNUItemInstance& Item : InventoryList.Items)
	{
		if (Item.ItemRowName == ItemRowName)
		{
			Total += Item.Count;
		}
	}
	return Total;
}

const FNUItemDefRow* UNUInventoryComponent::FindItemDef(FName ItemRowName) const
{
	// 소모품 => 장비 => 런타임 더미 순 (RowName은 테이블 간 중복 금지)
	if (const UDataTable* Table = GetItemDefTable())
	{
		if (const FNUItemDefRow* Row = Table->FindRow<FNUItemDefRow>(ItemRowName, TEXT("NUInventory"), false))
		{
			return Row;
		}
	}
	if (EquipDefTable != nullptr)
	{
		if (const FNUItemDefRow* Row = EquipDefTable->FindRow<FNUItemDefRow>(ItemRowName, TEXT("NUInventory"), false))
		{
			return Row;
		}
	}
	return nullptr;
}

bool UNUInventoryComponent::CanUseInBattle(const FGuid& InstanceId) const
{
	const FNUItemInstance* Instance = FindInstance(InstanceId);
	if (Instance == nullptr || Instance->Count < 1)
	{
		return false;
	}
	const FNUItemDefRow* Def = FindItemDef(Instance->ItemRowName);
	return Def != nullptr && Def->bUsableInBattle;
}


/////////////////////////////////////////////////////////////////
// 사용
/////////////////////////////////////////////////////////////////

void UNUInventoryComponent::ServerUseItem_Implementation(FGuid InstanceId)
{
	const FNUItemInstance* Instance = nullptr;
	const FNUItemDefRow* Def = nullptr;
	FText BlockedReason; // 플레이어에게 알릴 거절 사유 --- 사유 없는 거절(경합·잘못된 호출)은 로그만 남는다
	const bool bValid = ValidateUseItem(InstanceId, Instance, Def, BlockedReason);

	// 서버에 접속해 있으면 쓰지 않는다 --- 필드 아이템 사용 패킷이 서버에 아직 없다 (가이드 10/2판).
	// 로컬로 쓰면 클라 목록만 줄고 회복도 클라에만 적용돼, 가방과 필드 체력을 다시 받으면 되돌아온다. 패킷이 생기면 여기서 보낸다.
	// 서버 가방을 받은 뒤 접속이 끊긴 동안도 같이 막는다 (다시 붙으면 서버 값으로 돌아간다).
	// 서버가 없을 때는 HP·SP 회복만 흉내 낸다 --- 부활·애정도·효과 없는 아이템은 엉뚱한 HP 회복으로 소모되므로 쓰지 않는다
	const UEmberServerManager* Server = GetServerManager();
	if (!bValid)
	{
		// 사유와 로그는 ValidateUseItem이 남겼다
	}
	else if (bServerInventoryActive || (Server != nullptr && Server->IsLoggedIn()))
	{
		BlockedReason = NSLOCTEXT("NUInventory", "FieldUseNotReady", "필드에서는 아직 아이템을 사용할 수 없습니다.");
	}
	else if (Def->Effect1_Type != ENUItemEffectType::HealHP && Def->Effect1_Type != ENUItemEffectType::HealSP)
	{
		BlockedReason = NSLOCTEXT("NUInventory", "ItemUseNotReady", "아직 사용할 수 없는 아이템입니다.");
	}
	if (!bValid || !BlockedReason.IsEmpty())
	{
		if (bValid)
		{
			UE_LOG(LogNUInventory, Log, TEXT("UseItem 거절 — %s [%s]"), *BlockedReason.ToString(), *Instance->ItemRowName.ToString());
		}
		if (UNUUIManager* UI = BlockedReason.IsEmpty() ? nullptr : UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(BlockedReason);
		}
		return;
	}

	if (!ApplyUseEffect(*Def))
	{
		return;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::Item::Use);
	const int32 UsedValue = Def->Effect1_Value;
	const FName UsedRowName = Instance->ItemRowName;
	RemoveItem(InstanceId, 1); // 내부에서 Broadcast + ForceNetUpdate

	UE_LOG(LogNUInventory, Log, TEXT("아이템 사용 [%s] (+%d)"), *UsedRowName.ToString(), UsedValue);

	// 사용 이벤트 발신 (AttributeSet의 Dead 이벤트 선례) — 퀘스트/UI 등 추후 리스너용
	FGameplayEventData Payload;
	Payload.EventTag = NU::Event::Item::Used;
	Payload.EventMagnitude = UsedValue;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), NU::Event::Item::Used, Payload);
}

void UNUInventoryComponent::ServerDropItem_Implementation(FGuid InstanceId, int32 Count)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || Count <= 0)
	{
		return;
	}

	const FNUItemInstance* Instance = FindInstance(InstanceId);
	if (Instance == nullptr)
	{
		return;
	}

	const FNUItemDefRow* Def = FindItemDef(Instance->ItemRowName);
	if (Def != nullptr && !NUItem::IsDroppable(Def->ItemType))
	{
		UE_LOG(LogNUInventory, Log, TEXT("버리기 거절 — 퀘스트·열쇠 아이템 [%s]"), *Instance->ItemRowName.ToString());
		return;
	}

	const FName DroppedRowName = Instance->ItemRowName;
	const int32 DropCount = FMath::Min(Count, Instance->Count);

	// 서버 모드면 로컬을 건드리지 않는다 --- ItemDiscard(66)를 보내고 SVR_Item_Slot 푸시로 반영된다.
	// 로컬로 지우면 총량이 어긋나 정렬이 거절되고 재로그인 시 아이템이 부활한다 (예전 버그)
	if (bServerInventoryActive)
	{
		UEmberServerManager* Server = GetServerManager();
		if (Server != nullptr && Server->IsLoggedIn() && Instance->SlotNo >= 0)
		{
			UE_LOG(LogNUInventory, Log, TEXT("버리기 요청 [%s] x%d --- 칸 %d"),
				*DroppedRowName.ToString(), DropCount, Instance->SlotNo);
			Server->SendItemDiscard(NUContainer::Inventory, Instance->SlotNo, DropCount);
			UNUSFXSubsystem::Play(this, NU::SFX::Item::Discard);
		}
		else
		{
			UE_LOG(LogNUInventory, Warning, TEXT("버리기 실패 --- 서버 미접속이거나 칸 번호 없음 [%s]"),
				*DroppedRowName.ToString());
		}
		return;
	}

	if (RemoveItem(InstanceId, DropCount)) // 내부에서 Broadcast + ForceNetUpdate
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Item::Discard);
		UE_LOG(LogNUInventory, Log, TEXT("아이템 버림 [%s] x%d (%d/%d칸)"),
			*DroppedRowName.ToString(), DropCount, GetUsedSlots(), MaxSlots);
	}
}

void UNUInventoryComponent::ServerMoveItemBetween_Implementation(int32 FromContainer, int32 FromSlotNo, int32 ToContainer, int32 ToSlotNo)
{
	// ItemMove(61)와 같은 판정: 빈칸=이동, 같은 아이템(스택 가능)=합치기, 그 밖의 찬칸=교환. 컨테이너가 달라도 된다
	const bool bValidContainers =
		FromContainer >= NUContainer::Inventory && FromContainer <= NUContainer::AccountStorage &&
		ToContainer >= NUContainer::Inventory && ToContainer <= NUContainer::AccountStorage;
	if (!bValidContainers || FromSlotNo < 0 || ToSlotNo < 0
		|| (FromContainer == ToContainer && FromSlotNo == ToSlotNo))
	{
		return;
	}

	// 가방 칸에는 아이템이 제 범위(분류 구간) 안일 때만 놓인다 (서버에도 같은 거절을 요청했다).
	// 맞바꾸면 목적지 아이템도 출발 칸으로 오므로 양쪽을 다 본다. 창고는 구간이 없다
	const auto FitsBagSlot = [this](int32 ItemContainer, int32 ItemSlotNo, int32 Container, int32 SlotNo)
	{
		const TArray<FNUItemInstance>* Items = Container == NUContainer::Inventory ? GetContainerItems(ItemContainer) : nullptr;
		const FNUItemInstance* Item = Items != nullptr
			? Items->FindByPredicate([ItemSlotNo](const FNUItemInstance& It) { return It.SlotNo == ItemSlotNo; }) : nullptr;
		return Item == nullptr || GetBagRange(Item->ItemRowName).Contains(SlotNo);
	};
	if (!FitsBagSlot(FromContainer, FromSlotNo, ToContainer, ToSlotNo) || !FitsBagSlot(ToContainer, ToSlotNo, FromContainer, FromSlotNo))
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUInventory", "WrongBagSection", "그 칸에는 둘 수 없는 아이템입니다."));
		}
		return;
	}

	// 창고가 끼거나 서버 인벤이 원천이면 서버로 — 실제 결과는 SVR_Item_Slot(70) 푸시로 반영
	const bool bNeedsServer = FromContainer != NUContainer::Inventory
		|| ToContainer != NUContainer::Inventory
		|| bServerInventoryActive;
	if (bNeedsServer)
	{
		UEmberServerManager* Server = GetServerManager();
		if (Server != nullptr && Server->IsLoggedIn())
		{
			Server->SendItemMove(FromContainer, FromSlotNo, ToContainer, ToSlotNo);
		}
		else
		{
			UE_LOG(LogNUInventory, Log, TEXT("창고 이동은 서버 로그인이 필요합니다 (%d:%d → %d:%d)"),
				FromContainer, FromSlotNo, ToContainer, ToSlotNo);
		}
		return;
	}

	// 순수 로컬(미로그인/서버 인벤 비활성) — 인벤 내부 이동만
	FNUItemInstance* From = FindMutableBySlotNo(FromSlotNo);
	if (From == nullptr)
	{
		return;
	}

	if (FNUItemInstance* To = FindMutableBySlotNo(ToSlotNo))
	{
		// 같은 아이템이고 목적지에 자리가 남았으면 합친다 --- 상한까지 채우고 남은 개수는 출발 칸에 둔다.
		// 스택이 안 되거나(상한 1) 목적지가 이미 상한이면 옮길 것이 없어 교환으로 넘어간다 (서버와 같은 순서)
		const FNUItemDefRow* Def = To->ItemRowName == From->ItemRowName ? FindItemDef(From->ItemRowName) : nullptr;
		const int32 Moved = Def != nullptr ? FMath::Min(From->Count, Def->MaxStack - To->Count) : 0;
		if (Moved > 0)
		{
			To->Count += Moved;
			InventoryList.MarkItemDirty(*To);
			From->Count -= Moved;
			if (From->Count > 0)
			{
				InventoryList.MarkItemDirty(*From);
			}
			else
			{
				InventoryList.Items.RemoveAt(UE_PTRDIFF_TO_INT32(From - InventoryList.Items.GetData()));
				InventoryList.MarkArrayDirty();
			}
			UE_LOG(LogNUInventory, Log, TEXT("칸 합치기 %d → %d (%d개)"), FromSlotNo, ToSlotNo, Moved);
			BroadcastChangedAndPush();
			return;
		}
		To->SlotNo = FromSlotNo;
		InventoryList.MarkItemDirty(*To);
	}
	From->SlotNo = ToSlotNo;
	InventoryList.MarkItemDirty(*From);

	UE_LOG(LogNUInventory, Log, TEXT("칸 이동 %d → %d"), FromSlotNo, ToSlotNo);
	BroadcastChangedAndPush();
}

/////////////////////////////////////////////////////////////////
// 게임서버(gRPC) 인벤토리 연동 — InventoryList(60)/ItemMove(61)/SVR_Item_Slot(70)
/////////////////////////////////////////////////////////////////

void UNUInventoryComponent::RequestServerInventory()
{
	RequestContainerList(NUContainer::Inventory);

	// 장착 상태도 같이 받는다 — 장비는 컨테이너 4이고 칸 번호가 곧 부위라 별도 복원 패킷이 없다
	RequestContainerList(NUContainer::Equipment);
}

void UNUInventoryComponent::RequestContainerList(int32 Container)
{
	UEmberServerManager* Server = GetServerManager();
	if (Server != nullptr && Server->IsLoggedIn())
	{
		Server->SendInventoryList(Container);
		PendingListContainers.Add(Container);
	}
}

FText UNUInventoryComponent::GetContainerDisplayName(int32 Container)
{
	switch (Container)
	{
	case NUContainer::CharStorage:    return NSLOCTEXT("NUInventory", "ContainerCharStorage", "캐릭터 창고");
	case NUContainer::AccountStorage: return NSLOCTEXT("NUInventory", "ContainerAccountStorage", "계정 창고");
	case NUContainer::Equipment:      return NSLOCTEXT("NUInventory", "ContainerEquipment", "장비");
	default:                          return NSLOCTEXT("NUInventory", "ContainerInventory", "가방");
	}
}

void UNUInventoryComponent::HandleServerListResponse(int32 ResultCode, const FString& ResponseData)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}
	// 요청한 순서대로 답이 오므로 앞에서부터 꺼내 어느 저장소의 응답인지 맞춘다.
	// 성공이면 아래에서 JSON의 Container를 쓰므로 여기서는 큐만 비운다
	const int32 RequestedContainer = PendingListContainers.IsEmpty()
		? NUContainer::Inventory
		: PendingListContainers[0];
	if (!PendingListContainers.IsEmpty())
	{
		PendingListContainers.RemoveAt(0);
	}

	if (ResultCode != 200)
	{
		UE_LOG(LogNUInventory, Error, TEXT("인벤토리 목록 실패 (%d) --- 저장소 %d"), ResultCode, RequestedContainer);

		// 창고는 이 응답이 유일한 내용물 출처라, 실패하면 "불러오는 중"에서 멈춘 채 이유를 알 수 없다.
		// 인벤·장비는 로그인 때 자동으로 받는 것이라 실패해도 사용자가 할 일이 없어 로그만 남긴다
		if (RequestedContainer == NUContainer::CharStorage || RequestedContainer == NUContainer::AccountStorage)
		{
			if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
			{
				UI->ShowNotice(FText::Format(
					NSLOCTEXT("NUInventory", "ContainerLoadFailed", "{0}를 불러오지 못했습니다. {1}"),
					GetContainerDisplayName(RequestedContainer),
					NUInventory::GetResultMessage(ResultCode)));
			}
		}
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNUInventory, Error, TEXT("인벤토리 목록 JSON 파싱 실패: %s"), *ResponseData);
		return;
	}

	// JSON 키는 PascalCase. 어느 저장소의 응답인지 Container로 구분한다
	int32 Container = NUContainer::Inventory;
	Root->TryGetNumberField(TEXT("Container"), Container);

	// 칸 수는 서버가 권위 (gRPC_help — 클라 하드코딩 금지)
	int32 ServerSlotCount = 0;
	Root->TryGetNumberField(TEXT("SlotCount"), ServerSlotCount);

	const TArray<TSharedPtr<FJsonValue>>* JsonSlots = nullptr;
	Root->TryGetArrayField(TEXT("Slots"), JsonSlots);

	// --- 장비 (4): 칸 번호가 곧 부위라 장비 컴포넌트가 받는다 ---
	if (Container == NUContainer::Equipment)
	{
		ApplyEquipmentSlotsFromJson(JsonSlots);
		return;
	}

	// --- 창고 (2/3): 캐시에 통째로 적용 — 서버 전용이라 빈 목록도 그대로 진실 ---
	if (Container != NUContainer::Inventory)
	{
		EnsureItemDefTable();
		FNUContainerCache& Cache = StorageCaches.FindOrAdd(Container);
		Cache.SlotCount = ServerSlotCount;
		Cache.bLoaded = true;
		Cache.Items.Reset();
		UnknownSlotsByContainer.Remove(Container); // 목록을 새로 받는다 --- 아래 루프가 다시 판단한다
		if (JsonSlots != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Value : *JsonSlots)
			{
				const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Obj.IsValid())
				{
					continue;
				}
				int32 SlotNo = INDEX_NONE;
				int32 ItemId = 0;
				int32 ItemCount = 0;
				Obj->TryGetNumberField(TEXT("SlotNo"), SlotNo);
				Obj->TryGetNumberField(TEXT("ItemId"), ItemId);
				Obj->TryGetNumberField(TEXT("ItemCount"), ItemCount);
				ApplySlotToArray(Cache.Items, SlotNo, ItemId, ItemCount, Container);
			}
		}
		UE_LOG(LogNUInventory, Log, TEXT("창고 %d 적용 — %d칸 사용 (최대 %d)"), Container, Cache.Items.Num(), Cache.SlotCount);
		OnStorageChanged.Broadcast(Container);
		return;
	}

	// --- 인벤 (1) ---
	if (ServerSlotCount > 0)
	{
		MaxSlots = ServerSlotCount;
	}
	ReadBagSections(*Root);

	// 아직 아이템 획득 패킷이 없다(62~69 예약) — 서버 인벤이 비어 있으면 개발용 로컬
	// 지급을 유지하고, 아이템이 오는 순간부터 서버를 원천으로 전환한다
	if (JsonSlots == nullptr || JsonSlots->Num() == 0)
	{
		// 로컬로 받은 아이템은 구간을 모르고 앞 칸부터 놓였다 --- 구간이 왔으면 제 구간으로 다시 놓는다
		TArray<FNUItemInstance> Reslotted;
		if (!BagSections.IsEmpty() && BuildSortedItems(NUContainer::Inventory, ENUItemSortMode::Kind, Reslotted))
		{
			InventoryList.Items = MoveTemp(Reslotted);
			InventoryList.MarkArrayDirty();
		}
		UE_LOG(LogNUInventory, Log, TEXT("서버 인벤 비어 있음 — 로컬 흐름 유지 (칸 수만 %d 적용)"), MaxSlots);
		BroadcastChangedAndPush();
		return;
	}

	EnsureItemDefTable();
	bServerInventoryActive = true;

	// 소지금도 서버 값으로 --- 목록 응답(CharacterList)의 잔액이 정본이다 (더미 초기값을 덮는다)
	const UGameInstance* GI = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUCharacterManager* Characters = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	FNUCharacterSummary SelectedCharacter;
	if (Characters != nullptr && Characters->GetSelectedCharacter(SelectedCharacter))
	{
		Gold = SelectedCharacter.Gold;
	}

	UnknownSlotsByContainer.Remove(NUContainer::Inventory);
	InventoryList.Items.Reset();
	InventoryList.MarkArrayDirty();
	for (const TSharedPtr<FJsonValue>& Value : *JsonSlots)
	{
		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Obj.IsValid())
		{
			continue;
		}
		int32 SlotNo = INDEX_NONE;
		int32 ItemId = 0;
		int32 ItemCount = 0;
		Obj->TryGetNumberField(TEXT("SlotNo"), SlotNo);
		Obj->TryGetNumberField(TEXT("ItemId"), ItemId);
		Obj->TryGetNumberField(TEXT("ItemCount"), ItemCount);
		ApplySlotToArray(InventoryList.Items, SlotNo, ItemId, ItemCount, NUContainer::Inventory);
	}

	UE_LOG(LogNUInventory, Log, TEXT("서버 인벤 적용 — %d칸 사용 (최대 %d)"), GetUsedSlots(), MaxSlots);
	BroadcastChangedAndPush();
}

void UNUInventoryComponent::ReadBagSections(const FJsonObject& Root)
{
	// 서버가 주는 구간 이름 (번호 = NUBagCategory)
	static const TCHAR* const CategoryNames[NUBagCategory::Num] = { TEXT("Equipment"), TEXT("Consumable"), TEXT("Material"), TEXT("Other") };

	TArray<FNUBagSection> Sections;
	Sections.SetNum(NUBagCategory::Num);
	TMap<ENUItemType, int32> CategoryByType;
	const TArray<TSharedPtr<FJsonValue>>* JsonSections = nullptr;
	if (Root.TryGetArrayField(TEXT("Sections"), JsonSections))
	{
		for (const TSharedPtr<FJsonValue>& Value : *JsonSections)
		{
			const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Name;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("Category"), Name))
			{
				continue;
			}
			for (int32 Category = 0; Category < NUBagCategory::Num; ++Category)
			{
				if (Name == CategoryNames[Category])
				{
					Obj->TryGetNumberField(TEXT("Start"), Sections[Category].Start);
					Obj->TryGetNumberField(TEXT("Count"), Sections[Category].Count);

					// 이 구간에 드는 아이템 종류 (시트의 ItemType 이름). 클라가 모르는 이름은 건너뛴다
					TArray<FString> TypeNames;
					Obj->TryGetStringArrayField(TEXT("Types"), TypeNames);
					for (const FString& TypeName : TypeNames)
					{
						const int64 TypeValue = StaticEnum<ENUItemType>()->GetValueByNameString(TypeName);
						if (TypeValue != INDEX_NONE)
						{
							CategoryByType.Add(static_cast<ENUItemType>(TypeValue), Category);
						}
					}
				}
			}
		}
	}

	// 네 분류의 칸 수가 다 와야 구간 가방이다 --- 하나라도 빠지면 한 통으로 둔다 (구간을 안 주는 응답 포함)
	const bool bComplete = !Sections.ContainsByPredicate([](const FNUBagSection& Section) { return Section.Count <= 0; });
	BagSections = bComplete ? MoveTemp(Sections) : TArray<FNUBagSection>(); // 알림은 목록을 다 적용한 뒤 한 번에 나간다
	BagCategoryByType = bComplete ? MoveTemp(CategoryByType) : TMap<ENUItemType, int32>();

	TArray<FString> Keys;
	for (const auto& Field : Root.Values)
	{
		Keys.Add(FString(Field.Key));
	}
	FString SectionsText;
	if (JsonSections != nullptr)
	{
		FJsonSerializer::Serialize(*JsonSections, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SectionsText));
	}
	UE_LOG(LogNUInventory, Log, TEXT("가방 구간 %s --- 응답 키 [%s], Sections %s"),
		bComplete ? TEXT("적용") : TEXT("없음(한 통)"), *FString::Join(Keys, TEXT(", ")), JsonSections != nullptr ? *SectionsText : TEXT("없음"));
}

void UNUInventoryComponent::HandleServerMoveResponse(int32 ResultCode)
{
	// 200 = 접수 (실제 결과는 SVR_Item_Slot 푸시로 온다)
	if (ResultCode != 200)
	{
		// 1300 없는 저장소 / 1301 칸 범위 밖 / 1302 출발 칸 빔 / 1303 출발=도착 / 1204 캐릭터 미선택
		UE_LOG(LogNUInventory, Warning, TEXT("칸 이동 거절 (%d)"), ResultCode);

		// 거절되면 아이콘이 원래 칸으로 돌아갈 뿐이라 화면만 봐서는 이유를 알 수 없다
		ShowItemRejectNotice(ResultCode);
	}
}

void UNUInventoryComponent::HandleServerDiscardResponse(int32 ResultCode)
{
	// 200 = 버렸다 (바뀐 칸은 SVR_Item_Slot 푸시로 온다)
	if (ResultCode != 200)
	{
		// 1300 없는 저장소(장비 포함) / 1301 칸 범위 밖 / 1302 빈 칸 / 1310 개수 1 미만 / 1204 미선택
		UE_LOG(LogNUInventory, Warning, TEXT("버리기 거절 (%d)"), ResultCode);
		ShowItemRejectNotice(ResultCode);
	}
}

void UNUInventoryComponent::HandleServerSplitResponse(int32 ResultCode)
{
	// 200 = 접수 (나뉜 두 칸은 SVR_Item_Slot 푸시로 온다)
	if (ResultCode != 200)
	{
		// 1309 스택 불가 / 1310 개수 범위 밖 / 1311 목적지 참 / 1305 빈 칸 없음 / 1302 출발 칸 빔
		UE_LOG(LogNUInventory, Warning, TEXT("나누기 거절 (%d)"), ResultCode);
		ShowItemRejectNotice(ResultCode);
	}
}

void UNUInventoryComponent::HandleServerShopResponse(bool bBuy, int32 ResultCode, const FString& ResultMessage, const FString& ResponseData)
{
	// 구매 응답은 보낸 순서대로 오므로 앞에서부터 꺼내 어느 상점(NPC) 것인지 맞춘다
	int32 NpcId = 0;
	if (bBuy && !PendingShopBuyNpcIds.IsEmpty())
	{
		NpcId = PendingShopBuyNpcIds[0];
		PendingShopBuyNpcIds.RemoveAt(0);
	}

	if (ResultCode != 200)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("%s 거절 (%d) %s"), bBuy ? TEXT("구매") : TEXT("판매"), ResultCode, *ResultMessage);
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			// 2208은 서버가 계산한 금액이 result_message에 실려 온다 --- 클라 표가 낡았다는 신호라 그대로 보여 준다
			UI->ShowNotice(ResultCode == 2208 && !ResultMessage.IsEmpty()
				? FText::Format(NSLOCTEXT("NUShop", "PriceMismatchDetail", "가격 정보가 서버와 다릅니다. ({0})"), FText::FromString(ResultMessage))
				: NUInventory::GetResultMessage(ResultCode));
		}
		return;
	}

	// {"Gold": 잔액} --- 바뀐 칸은 SVR_Item_Slot(70)으로 따로 와서 목록을 갱신하므로 여기서는 골드만 맞춘다
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
	int64 ServerGold = 0;
	if (FJsonSerializer::Deserialize(Reader, Root, FJsonSerializer::EFlags::StoreNumbersAsStrings) && Root.IsValid() && Root->TryGetNumberField(TEXT("Gold"), ServerGold))
	{
		Gold = ServerGold;
	}
	else
	{
		UE_LOG(LogNUInventory, Warning, TEXT("상점 응답에 Gold가 없음: %s"), *ResponseData);
	}
	if (NpcId > 0)
	{
		OnPurchaseSucceeded.Broadcast(NpcId);
	}

	UE_LOG(LogNUInventory, Log, TEXT("%s 완료 --- 잔액 %lld골드"), bBuy ? TEXT("구매") : TEXT("판매"), Gold);
	UNUSFXSubsystem::Play(this, bBuy ? NU::SFX::Shop::Buy : NU::SFX::Shop::Sell);
	OnInventoryChanged.Broadcast(); // 골드 표시 갱신 (상점 창·인벤 창이 구독한다)
}

void UNUInventoryComponent::HandleServerSlotPush(int32 Container, int32 SlotNo, int32 ItemId, int32 ItemCount)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	// --- 장비 (4) ---
	if (Container == NUContainer::Equipment)
	{
		EnsureItemDefTable();
		if (UNUEquipmentComponent* Equipment = GetEquipmentComponent())
		{
			Equipment->ApplyServerEquipSlot(SlotNo, ItemId, ItemCount);
		}
		return;
	}

	// --- 창고 (2/3) ---
	if (Container != NUContainer::Inventory)
	{
		FNUContainerCache* Cache = StorageCaches.Find(Container);
		if (Cache == nullptr || !Cache->bLoaded)
		{
			RequestContainerList(Container); // 아직 안 받은 창고의 통지 => 전체 로드로 동기화
			return;
		}
		EnsureItemDefTable();
		ApplySlotToArray(Cache->Items, SlotNo, ItemId, ItemCount, Container);
		OnStorageChanged.Broadcast(Container);
		return;
	}

	// --- 인벤 (1) ---
	if (!bServerInventoryActive)
	{
		// 로컬 모드 중 칸 통지가 왔다 = 서버 인벤에 아이템이 생겼다 — 전체를 다시 받아 동기화
		RequestServerInventory();
		return;
	}

	ApplySlotToArray(InventoryList.Items, SlotNo, ItemId, ItemCount, NUContainer::Inventory);
	BroadcastChangedAndPush();
}

void UNUInventoryComponent::ApplyEquipmentSlotsFromJson(const TArray<TSharedPtr<FJsonValue>>* JsonSlots)
{
	UNUEquipmentComponent* Equipment = GetEquipmentComponent();
	if (Equipment == nullptr)
	{
		return;
	}
	EnsureItemDefTable();

	// 응답에 없는 칸은 빈 부위다 — 먼저 전부 비우고 온 것만 채운다
	for (int32 SlotNo = 0; SlotNo < static_cast<int32>(ENUEquipSlot::MAX); ++SlotNo)
	{
		Equipment->ApplyServerEquipSlot(SlotNo, 0, 0);
	}

	if (JsonSlots == nullptr)
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& Value : *JsonSlots)
	{
		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Obj.IsValid())
		{
			continue;
		}
		int32 SlotNo = INDEX_NONE;
		int32 ItemId = 0;
		int32 ItemCount = 0;
		Obj->TryGetNumberField(TEXT("SlotNo"), SlotNo);
		Obj->TryGetNumberField(TEXT("ItemId"), ItemId);
		Obj->TryGetNumberField(TEXT("ItemCount"), ItemCount);
		Equipment->ApplyServerEquipSlot(SlotNo, ItemId, ItemCount);
	}
}

void UNUInventoryComponent::ShowEquipRejectNotice(int32 ResultCode, bool bUnequip)
{
	UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController());
	if (UI == nullptr)
	{
		return;
	}

	// 실패 코드 => 사람 말. 1306/1307은 클라가 미리 걸러 보내므로 보통 오지 않는다 (혹시 오면 표시).
	// 해제할 때의 1305는 "인벤이 가득"이라 공용 문구("빈 칸이 없습니다")보다 구체적으로 쓴다
	FText Reason;
	switch (ResultCode)
	{
	case 1305: Reason = NSLOCTEXT("NUInventory", "EquipRejectFull", "인벤토리가 가득 차 있습니다."); break;
	case 1306:
	case 1307:
	case 1308:
		Reason = NUInventory::GetResultMessage(ResultCode);
		break;
	default:
		Reason = FText::Format(bUnequip
			? NSLOCTEXT("NUInventory", "UnequipRejectEtc", "해제에 실패했습니다. ({0})")
			: NSLOCTEXT("NUInventory", "EquipRejectEtc", "장착에 실패했습니다. ({0})"),
			FText::AsNumber(ResultCode));
		break;
	}
	UI->ShowNotice(Reason);
}

void UNUInventoryComponent::ShowItemRejectNotice(int32 ResultCode)
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
	{
		UI->ShowNotice(NUInventory::GetResultMessage(ResultCode));
	}
}

void UNUInventoryComponent::ShowRewardMissingNotice(const FString& ResponseData)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* Reward = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Missing = nullptr;
	if (!Root->TryGetObjectField(TEXT("Reward"), Reward) || !(*Reward)->TryGetArrayField(TEXT("MissingItems"), Missing) || Missing->IsEmpty())
	{
		return;
	}

	// 항목은 "6615x1" 꼴 글자다 (가이드 13.6). 이름을 아는 것만 적는다 --- 꼴이 달라 하나도 못 읽어도 못 받았다는 것은 알린다
	TArray<FString> Raw, Names;
	for (const TSharedPtr<FJsonValue>& Value : *Missing)
	{
		FString Entry, IdText, CountText;
		if (!Value.IsValid() || !Value->TryGetString(Entry))
		{
			continue;
		}
		Raw.Add(Entry);
		if (!Entry.Split(TEXT("x"), &IdText, &CountText))
		{
			IdText = Entry;
		}
		const FNUItemDefRow* Def = FindItemDefByNumericId(FCString::Atoi(*IdText));
		if (Def != nullptr && !Def->DisplayName.IsEmpty())
		{
			const int32 Count = FCString::Atoi(*CountText);
			Names.Add(Count > 1 ? FString::Printf(TEXT("%s x%d"), *Def->DisplayName.ToString(), Count) : Def->DisplayName.ToString());
		}
	}
	UE_LOG(LogNUInventory, Warning, TEXT("가방 구간이 가득 차 못 받은 보상 %d종 --- %s"), Missing->Num(), *FString::Join(Raw, TEXT(", ")));

	if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
	{
		UI->ShowNotice(Names.IsEmpty()
			? NSLOCTEXT("NUInventory", "RewardMissing", "가방이 가득 차 받지 못한 보상이 있습니다.")
			: FText::Format(NSLOCTEXT("NUInventory", "RewardMissingNamed", "가방이 가득 차 받지 못한 보상이 있습니다. ({0})"),
				FText::FromString(FString::Join(Names, TEXT(", ")))));
	}
}

UNUEquipmentComponent* UNUInventoryComponent::GetEquipmentComponent() const
{
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	return OwnerState != nullptr ? OwnerState->GetEquipmentComponent() : nullptr;
}

void UNUInventoryComponent::ApplySlotToArray(TArray<FNUItemInstance>& Items, int32 SlotNo, int32 ItemId, int32 ItemCount, int32 Container)
{
	if (SlotNo < 0)
	{
		return;
	}

	// 인벤만 복제된다 --- 창고는 각 클라가 자기 세션에서 받으므로 FastArray 마킹이 없다
	const bool bNetDirty = Container == NUContainer::Inventory;

	// 이 칸을 다시 판단한다 --- 모르는 아이템이 빠지거나 아는 것으로 바뀌면 여기서 풀린다
	if (TMap<int32, int32>* Unknown = UnknownSlotsByContainer.Find(Container))
	{
		Unknown->Remove(SlotNo);
		if (Unknown->IsEmpty())
		{
			UnknownSlotsByContainer.Remove(Container);
		}
	}

	const int32 Index = Items.IndexOfByPredicate(
		[SlotNo](const FNUItemInstance& Item) { return Item.SlotNo == SlotNo; });

	// 빈 칸 통지 => 그 칸 비우기
	if (ItemId == 0 || ItemCount <= 0)
	{
		if (Index != INDEX_NONE)
		{
			Items.RemoveAt(Index);
			if (bNetDirty)
			{
				InventoryList.MarkArrayDirty();
			}
		}
		return;
	}

	const FName RowName = FindRowNameByNumericId(ItemId);
	if (RowName.IsNone())
	{
		UE_LOG(LogNUInventory, Warning, TEXT("서버 ItemId %d의 정의를 못 찾음 — 칸 %d 무시 (테이블 NumericId 확인)"), ItemId, SlotNo);
		// 이 저장소에 클라가 모르는 아이템이 있다 --- 그 칸이 비어 보이므로 재배치를 보내면
		// 서버 총량 검증에서 걸린다. 그 저장소의 정렬을 막아 어긋난 배치가 나가지 않게 한다
		UnknownSlotsByContainer.FindOrAdd(Container).Add(SlotNo, ItemId);
		return;
	}

	if (Index != INDEX_NONE)
	{
		FNUItemInstance& Item = Items[Index];
		if (Item.ItemRowName != RowName)
		{
			Item.InstanceId = FGuid::NewGuid(); // 다른 아이템이 들어옴 (교환) — 전투 참조용 새 식별자
			Item.ItemRowName = RowName;
		}
		Item.Count = ItemCount;
		if (bNetDirty)
		{
			InventoryList.MarkItemDirty(Item);
		}
	}
	else
	{
		FNUItemInstance& NewItem = Items.AddDefaulted_GetRef();
		NewItem.InstanceId = FGuid::NewGuid(); // 서버엔 인스턴스 개념이 없다 — 클라 세션 로컬 식별자
		NewItem.ItemRowName = RowName;
		NewItem.Count = ItemCount;
		NewItem.SlotNo = SlotNo;
		if (bNetDirty)
		{
			InventoryList.MarkItemDirty(NewItem);
		}
	}
}

const TArray<FNUItemInstance>* UNUInventoryComponent::GetContainerItems(int32 Container) const
{
	if (Container == NUContainer::Inventory)
	{
		return &InventoryList.Items;
	}
	const FNUContainerCache* Cache = StorageCaches.Find(Container);
	return Cache != nullptr ? &Cache->Items : nullptr;
}

int32 UNUInventoryComponent::GetContainerSlotCount(int32 Container) const
{
	if (Container == NUContainer::Inventory)
	{
		return MaxSlots;
	}
	const FNUContainerCache* Cache = StorageCaches.Find(Container);
	return Cache != nullptr ? Cache->SlotCount : 0;
}

bool UNUInventoryComponent::IsContainerLoaded(int32 Container) const
{
	if (Container == NUContainer::Inventory)
	{
		return true;
	}
	const FNUContainerCache* Cache = StorageCaches.Find(Container);
	return Cache != nullptr && Cache->bLoaded;
}

const FNUItemDefRow* UNUInventoryComponent::FindDefRowByNumericId(int32 NumericId, FName& OutRowName) const
{
	OutRowName = NAME_None;

	// 0은 "칸이 비었다"는 뜻으로도 오고, 기획 표가 안 붙은 런타임 더미 행의 값도 0이다
	// 걸러내지 않으면 더미 포션이 서버가 안 보낸 아이템의 정의로 잡힌다
	if (NumericId <= 0)
	{
		return nullptr;
	}

	// RowName(Item_ID)이 실제 키, NumericId는 서버/기획 참조용 — 역매핑은 순회 (테이블이 작아 충분)
	const UDataTable* Tables[] = { GetItemDefTable(), EquipDefTable.Get() };
	for (const UDataTable* Table : Tables)
	{
		if (Table == nullptr)
		{
			continue;
		}
		for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
		{
			const FNUItemDefRow* Def = reinterpret_cast<const FNUItemDefRow*>(Row.Value);
			if (Def != nullptr && Def->NumericId == NumericId)
			{
				OutRowName = Row.Key;
				return Def;
			}
		}
	}
	return nullptr;
}

FName UNUInventoryComponent::FindRowNameByNumericId(int32 NumericId) const
{
	FName RowName = NAME_None;
	FindDefRowByNumericId(NumericId, RowName);
	return RowName;
}

const FNUItemDefRow* UNUInventoryComponent::FindItemDefByNumericId(int32 NumericId) const
{
	FName RowName = NAME_None;
	return FindDefRowByNumericId(NumericId, RowName);
}

UEmberServerManager* UNUInventoryComponent::GetServerManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

void UNUInventoryComponent::HandleEmberStateChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn)
	{
		// 끊기기 전에 보낸 요청은 답이 오지 않는다 --- 남겨두면 저장소 짝이 밀린다
		PendingListContainers.Reset();

		RequestServerInventory();
		// 재로그인 — 이미 로드했던 창고도 다시 받아 동기화
		for (const TPair<int32, FNUContainerCache>& Pair : StorageCaches)
		{
			if (Pair.Value.bLoaded)
			{
				RequestContainerList(Pair.Key);
			}
		}
	}
}

void UNUInventoryComponent::ServerDebugAddItem_Implementation(FName ItemRowName, int32 Count)
{
#if !UE_BUILD_SHIPPING
	AddItem(ItemRowName, Count);
#endif
}

bool UNUInventoryComponent::ValidateUseItem(const FGuid& InstanceId, const FNUItemInstance*& OutInstance, const FNUItemDefRow*& OutDef, FText& OutReason) const
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(Owner);
	UNUAbilitySystemComponent* ASC = OwnerState ? OwnerState->GetNUASC() : nullptr;
	if (ASC == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("UseItem 거절 — ASC 없음"));
		return false;
	}
	if (!ASC->IsAlive())
	{
		UE_LOG(LogNUInventory, Log, TEXT("UseItem 거절 — 사망 상태"));
		OutReason = NSLOCTEXT("NUInventory", "UseBlockedDead", "전투불능 상태에서는 아이템을 사용할 수 없습니다.");
		return false;
	}
	// 전투 중 필드 사용 차단. NU.Context.* 태그는 아직 어디서도 부여되지 않아 신호로 쓸 수 없고,
	// 복제되는 Encounter 참조(로컬 전투)와 넷 전투 상태(서버 전투)가 실제 동작하는 판정. 전투 중 사용은 전투 커맨드 경로로만.
	if (IsOwnerInBattle())
	{
		UE_LOG(LogNUInventory, Log, TEXT("UseItem 거절 — 전투 중 (전투 커맨드 경로 사용)"));
		OutReason = NSLOCTEXT("NUInventory", "UseBlockedBattle", "전투 중에는 사용할 수 없습니다.");
		return false;
	}

	OutInstance = FindInstance(InstanceId);
	if (OutInstance == nullptr || OutInstance->Count < 1)
	{
		UE_LOG(LogNUInventory, Log, TEXT("UseItem 거절 — 인스턴스 없음/수량 부족"));
		return false;
	}

	OutDef = FindItemDef(OutInstance->ItemRowName);
	if (OutDef == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("UseItem 거절 — 정의 없음 [%s]"), *OutInstance->ItemRowName.ToString());
		return false;
	}
	if (OutDef->ItemType != ENUItemType::Consumable || !OutDef->bUsableInField)
	{
		UE_LOG(LogNUInventory, Log, TEXT("UseItem 거절 — 필드 사용 불가 타입 [%s]"), *OutInstance->ItemRowName.ToString());
		return false;
	}
	return true;
}

bool UNUInventoryComponent::ApplyUseEffect(const FNUItemDefRow& Def)
{
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	UNUAbilitySystemComponent* ASC = OwnerState ? OwnerState->GetNUASC() : nullptr;
	if (ASC == nullptr)
	{
		return false;
	}

	const TSubclassOf<UGameplayEffect> EffectClass =
		Def.UseEffect ? Def.UseEffect : TSubclassOf<UGameplayEffect>(UNUGE_ItemHeal::StaticClass());

	// ApplyInitialVitals와 동일한 SetByCaller 적용 패턴
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Item::Heal, static_cast<float>(Def.Effect1_Value));

	// Instant GE는 Active로 남지 않아 반환 핸들 유효성으로 성공 판정 불가 — 적용 호출까지를 성공으로 본다
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	return true;
}

// ---------------------------------------------------------------
// 인벤 정렬 — 최종 배치를 ItemSort 한 발로 보낸다
// ---------------------------------------------------------------

bool UNUInventoryComponent::SortInventory(ENUItemSortMode Mode)
{
	return SortContainer(NUContainer::Inventory, Mode);
}

bool UNUInventoryComponent::SortContainer(int32 Container, ENUItemSortMode Mode)
{
	if (bSorting)
	{
		return false;
	}

	const TArray<FNUItemInstance>* Items = GetContainerItems(Container);
	if (Items == nullptr)
	{
		return false; // 아직 안 받은 창고
	}

	UNUTradeSubsystem* Trade = GetTradeSubsystem();
	if (Trade != nullptr && Trade->IsTrading())
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUInventory", "SortBlockedByTrade", "거래 중에는 정렬할 수 없습니다."));
		}
		return false;
	}

	if (const TMap<int32, int32>* Unknown = UnknownSlotsByContainer.Find(Container))
	{
		// 클라 테이블에 없는 아이템이 있으면 그 칸이 배치에서 빠져 총량이 어긋난다.
		// 그 칸은 화면에도 안 그려져 비어 보이므로, 칸과 번호를 알려주지 않으면 원인을 알 수 없다
		TArray<FString> Details;
		for (const TPair<int32, int32>& Pair : *Unknown)
		{
			Details.Add(FString::Printf(TEXT("%d칸=%d"), Pair.Key, Pair.Value));
		}
		Details.Sort();
		const FString DetailText = FString::Join(Details, TEXT(", "));

		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(FText::Format(
				NSLOCTEXT("NUInventory", "SortBlockedByUnknownItem",
					"등록되지 않은 아이템이 있어 정렬할 수 없습니다. ({0})"),
				FText::FromString(DetailText)));
		}
		UE_LOG(LogNUInventory, Warning, TEXT("정렬 거부 --- 저장소 %d에 클라 테이블이 모르는 아이템: %s"),
			Container, *DetailText);
		return false;
	}

	if (Items->Num() <= 1)
	{
		return true; // 옮길 것이 없다 --- 이미 그 순서다
	}

	TArray<FNUItemInstance> Sorted;
	if (!BuildSortedItems(Container, Mode, Sorted))
	{
		// 한 분류의 아이템이 그 구간 칸 수보다 많다 --- 서버가 구간을 지키면 생기지 않는다
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUInventory", "SortNoRoom", "분류 칸이 모자라 정렬할 수 없습니다."));
		}
		UE_LOG(LogNUInventory, Warning, TEXT("정렬 거부 --- 분류 구간보다 아이템이 많다"));
		return false;
	}

	UEmberServerManager* Server = GetServerManager();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		if (Container != NUContainer::Inventory)
		{
			// 창고는 복제도 로컬 지급도 없다 --- 서버가 원천이라 오프라인에서는 손댈 수 없다
			return false;
		}
		// 서버 없이 도는 개발 경로 --- 로컬에서 바로 재배치한다
		ApplySortLocally(Sorted);
		return true;
	}

	// 배치를 칸 목록으로. 서버는 아이템을 번호로만 알아서 정의가 없으면 보낼 수 없다
	TArray<FNUServerSlotEntry> Slots;
	Slots.Reserve(Sorted.Num());
	for (int32 Index = 0; Index < Sorted.Num(); ++Index)
	{
		const FNUItemDefRow* Def = FindItemDef(Sorted[Index].ItemRowName);
		if (Def == nullptr)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("정렬 취소 — 정의가 없는 아이템 [%s]"),
				*Sorted[Index].ItemRowName.ToString());
			return false;
		}

		FNUServerSlotEntry& Entry = Slots.AddDefaulted_GetRef();
		Entry.SlotNo = Sorted[Index].SlotNo;
		Entry.ItemId = Def->NumericId;
		Entry.ItemCount = Sorted[Index].Count;
	}

	bSorting = true;
	SortingContainer = Container;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SortTimeoutTimer, this, &UNUInventoryComponent::HandleSortTimeout, 3.0f, false);
	}

	UE_LOG(LogNUInventory, Log, TEXT("정렬 전송 --- 저장소 %d, %d칸"), Container, Slots.Num());
	Server->SendItemSort(Container, Slots);
	return true;
}

void UNUInventoryComponent::SplitStack(int32 Container, int32 SlotNo, int32 Count)
{
	const TArray<FNUItemInstance>* Items = GetContainerItems(Container);
	if (Items == nullptr)
	{
		return;
	}
	const FNUItemInstance* Item = Items->FindByPredicate(
		[SlotNo](const FNUItemInstance& It) { return It.SlotNo == SlotNo; });
	if (Item == nullptr)
	{
		return;
	}

	// 스택이 안 되는 아이템은 나눌 것이 없다 (장비는 MaxStack 1)
	const FNUItemDefRow* Def = FindItemDef(Item->ItemRowName);
	if (Def == nullptr || Def->MaxStack <= 1)
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUInventory", "SplitNotStackable", "나눌 수 없는 아이템입니다."));
		}
		return;
	}

	// 전부 떼면 그냥 이동이다 — 서버도 1310으로 거부한다
	if (Count < 1 || Count >= Item->Count)
	{
		return;
	}

	UEmberServerManager* Server = GetServerManager();
	if (Server != nullptr && Server->IsLoggedIn())
	{
		// 목적지 -1 = 서버가 빈 칸 중 가장 앞을 고른다.
		// 클라가 빈 칸을 계산해 보내면 그 사이에 칸이 차서 남의 아이템을 덮어쓸 수 있다
		Server->SendItemSplit(Container, SlotNo, INDEX_NONE, Count);
		return;
	}

	// --- 서버 없이 도는 개발 경로 (인벤만) ---
	if (Container != NUContainer::Inventory)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	const int32 FreeSlot = FindFreeSlotNo(Item->ItemRowName);
	if (FreeSlot == INDEX_NONE)
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUInventory", "SplitNoRoom", "가방에 빈 칸이 없습니다."));
		}
		return;
	}

	const int32 Index = InventoryList.Items.IndexOfByPredicate(
		[SlotNo](const FNUItemInstance& It) { return It.SlotNo == SlotNo; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	FNUItemInstance NewItem;
	NewItem.InstanceId = FGuid::NewGuid(); // 나뉜 쪽은 다른 인스턴스다
	NewItem.ItemRowName = InventoryList.Items[Index].ItemRowName;
	NewItem.Count = Count;
	NewItem.SlotNo = FreeSlot;

	InventoryList.Items[Index].Count -= Count;
	InventoryList.MarkItemDirty(InventoryList.Items[Index]);
	InventoryList.Items.Add(NewItem);
	InventoryList.MarkArrayDirty();

	UE_LOG(LogNUInventory, Log, TEXT("나누기(로컬) [%s] %d개 => 칸 %d"),
		*NewItem.ItemRowName.ToString(), Count, FreeSlot);
	BroadcastChangedAndPush();
}

void UNUInventoryComponent::ApplySortLocally(const TArray<FNUItemInstance>& Sorted)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	InventoryList.Items = Sorted; // 칸 번호는 BuildSortedItems가 매겼다
	InventoryList.MarkArrayDirty();
	BroadcastChangedAndPush();

	if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
	{
		UI->ShowNotice(NSLOCTEXT("NUInventory", "SortDone", "정렬이 완료되었습니다."));
	}
	UE_LOG(LogNUInventory, Log, TEXT("정렬 완료 (로컬)"));
}

bool UNUInventoryComponent::BuildSortedItems(int32 Container, ENUItemSortMode Mode, TArray<FNUItemInstance>& OutSorted) const
{
	const TArray<FNUItemInstance>* Items = GetContainerItems(Container);
	OutSorted = Items != nullptr ? *Items : TArray<FNUItemInstance>();
	OutSorted.StableSort([this, Mode](const FNUItemInstance& A, const FNUItemInstance& B)
	{
		const FNUItemDefRow* DefA = FindItemDef(A.ItemRowName);
		const FNUItemDefRow* DefB = FindItemDef(B.ItemRowName);

		// 등급 순이면 높은 등급 먼저 --- 등급 없는 것(None = 0)과 정의 없는 아이템은 뒤
		const ENUItemGrade GradeA = DefA != nullptr ? DefA->Grade : ENUItemGrade::None;
		const ENUItemGrade GradeB = DefB != nullptr ? DefB->Grade : ENUItemGrade::None;
		if (Mode == ENUItemSortMode::Grade && GradeA != GradeB)
		{
			return GradeA > GradeB;
		}

		// 정의 없는 아이템은 맨 뒤
		const uint8 TypeA = DefA != nullptr ? static_cast<uint8>(DefA->ItemType) : MAX_uint8;
		const uint8 TypeB = DefB != nullptr ? static_cast<uint8>(DefB->ItemType) : MAX_uint8;
		if (TypeA != TypeB)
		{
			return TypeA < TypeB;
		}

		const int32 IdA = DefA != nullptr ? DefA->NumericId : MAX_int32;
		const int32 IdB = DefB != nullptr ? DefB->NumericId : MAX_int32;
		if (IdA != IdB)
		{
			return IdA < IdB;
		}

		return A.Count > B.Count; // 같은 아이템은 큰 스택 먼저
	});

	// 칸 번호 --- 아이템이 드는 범위마다 앞부터 (창고와 구간 없는 가방은 범위가 하나라 0부터 차례로)
	TMap<int32, int32, TInlineSetAllocator<NUBagCategory::Num>> UsedByRangeStart;
	for (int32 Index = 0; Index < OutSorted.Num(); ++Index)
	{
		FNUItemInstance& Item = OutSorted[Index];
		if (Container != NUContainer::Inventory)
		{
			Item.SlotNo = Index;
			continue;
		}
		const FNUBagSection Range = GetBagRange(Item.ItemRowName);
		int32& Used = UsedByRangeStart.FindOrAdd(Range.Start);
		if (Used >= Range.Count)
		{
			return false;
		}
		Item.SlotNo = Range.Start + Used++;
	}
	return true;
}

void UNUInventoryComponent::HandleServerSortResponse(int32 ResultCode)
{
	// 200 = 접수 (바뀐 칸은 SVR_Item_Slot 푸시로 온다)
	if (ResultCode != 200)
	{
		// 1312 총량 불일치 / 1313 스택 상한 초과 / 1314 구간이 다른 칸 / 1300 없는 저장소 / 1301 칸 번호 문제 / 1310 개수 0 이하
		UE_LOG(LogNUInventory, Warning, TEXT("정렬 거절 (%d)"), ResultCode);
		FinishSort(false, NUInventory::GetResultMessage(ResultCode));
		return;
	}
	FinishSort(true);
}

void UNUInventoryComponent::HandleSortTimeout()
{
	if (bSorting)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("정렬 중단 — 응답이 오지 않음"));
		FinishSort(false, NSLOCTEXT("NUInventory", "SortNoResponse", "서버 응답이 오지 않았습니다."));
	}
}

void UNUInventoryComponent::FinishSort(bool bCompleted, const FText& FailReason)
{
	if (!bSorting)
	{
		return;
	}
	bSorting = false;
	const int32 Container = SortingContainer;
	SortingContainer = INDEX_NONE;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SortTimeoutTimer);
	}

	if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
	{
		// 왜 중단됐는지가 핵심이다 --- 총량 불일치와 응답 없음은 사용자가 할 일이 다르다
		UI->ShowNotice(bCompleted
			? NSLOCTEXT("NUInventory", "SortDone", "정렬이 완료되었습니다.")
			: FailReason.IsEmpty()
				? NSLOCTEXT("NUInventory", "SortAborted", "정렬이 중단되었습니다.")
				: FText::Format(NSLOCTEXT("NUInventory", "SortAbortedReason", "정렬이 중단되었습니다. {0}"), FailReason));
	}
	UE_LOG(LogNUInventory, Log, TEXT("정렬 %s --- 저장소 %d"), bCompleted ? TEXT("완료") : TEXT("중단"), Container);
	if (bCompleted)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Item::Sort);
	}
}

UNUTradeSubsystem* UNUInventoryComponent::GetTradeSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World != nullptr ? World->GetGameInstance() : nullptr;
	return GI != nullptr ? GI->GetSubsystem<UNUTradeSubsystem>() : nullptr;
}

namespace
{
	// 떠 있는 통합 캐릭터 메뉴 (없으면 nullptr). 누가 열었든(키·전체 메뉴·미리보기 명령) 매니저의 창 목록에서 찾는다
	UNUCharacterMenuBase* FindOpenCharacterMenu(const UNUUIManager* UI)
	{
		if (UI != nullptr)
		{
			for (UUserWidget* Window : UI->GetShownWindows(NUUILayer::Window))
			{
				if (UNUCharacterMenuBase* Menu = Cast<UNUCharacterMenuBase>(Window))
				{
					return Menu;
				}
			}
		}
		return nullptr;
	}
}

bool UNUInventoryComponent::ToggleCharacterMenu(ENUCharacterMenuSection Section, bool bCloseFromAnyPage)
{
	APlayerController* PC = GetLocalPlayerController();
	if (PC == nullptr)
	{
		return false;
	}
	const UNUUIManager* UI = UNUUIManager::Get(PC);
	UNUCharacterMenuBase* Menu = FindOpenCharacterMenu(UI);

	// 전투 중에는 열지 않는다 --- 화면을 다 덮어 전투를 가린다 (떠 있던 메뉴도 전투가 시작되면 닫힌다).
	// false 를 돌려주면 호출부가 옛 소지품·정보창을 열어 전투 중에 창이 뜨므로, 키를 받아서 버린 것으로 true.
	// 이미 떠 있는 메뉴는 전투 중이어도 같은 키로 닫을 수 있게 둔다
	if (Menu == nullptr && IsOwnerInBattle())
	{
		return true;
	}

	const TSubclassOf<UNUCharacterMenuBase> MenuClass = GetDefault<UNUUISettings>()->CharacterMenuClass.LoadSynchronous();
	if (MenuClass == nullptr)
	{
		return false;
	}

	if (Menu != nullptr)
	{
		// 메뉴 위에 확인·수량 창이 떠 있으면 키를 받지 않는다 --- 답을 기다리는 창을 두고 메뉴만 닫히면 그 창이 주인 없이 남는다
		if (UI->IsAnyWindowOpen(NUUILayer::Modal))
		{
			return true;
		}
		if (bCloseFromAnyPage || Menu->GetSelectedSection() == Section)
		{
			Menu->RequestClose();
		}
		else
		{
			Menu->SelectSection(Section);
		}
	}
	else
	{
		UNUCharacterMenuBase::Show(PC, MenuClass, Section);
	}
	return true;
}

void UNUInventoryComponent::CloseCharacterMenu()
{
	if (UNUCharacterMenuBase* Menu = FindOpenCharacterMenu(UNUUIManager::Get(GetLocalPlayerController())))
	{
		// 전투 시작·거래 창이 닫는 경로 --- 닫기 연출을 기다리면 그동안 메뉴가 입력을 막는다
		Menu->CloseImmediately();
	}
}

bool UNUInventoryComponent::UsesLegacyInventoryWindow() const
{
	const UNUTradeSubsystem* Trade = GetTradeSubsystem();
	return bInventoryOpenedByStorage || bInventoryOpenedByShop
		|| (Trade != nullptr && Trade->GetState() >= ENUTradeState::Active)
		|| (InventoryWindowWidget != nullptr && InventoryWindowWidget->IsInViewport());
}

void UNUInventoryComponent::ToggleCharacterMenuUI()
{
	if (UsesLegacyInventoryWindow() || !ToggleCharacterMenu(ENUCharacterMenuSection::Character, true))
	{
		ToggleInventoryUI();
	}
}

void UNUInventoryComponent::ToggleInventoryUI()
{
	// 혼자 여는 소지품은 통합 캐릭터 메뉴의 인벤토리 페이지다 (9/29). 옛 소지품 창은 다른 창 옆에서 주고받을 때만 쓴다 ---
	// 창고·상점·공방이 연 것과 거래 창 옆에 여는 것. 떠 있는 옛 창을 닫는 것도 이 아래 길이다
	if (!UsesLegacyInventoryWindow() && ToggleCharacterMenu(ENUCharacterMenuSection::Inventory))
	{
		return;
	}

	// WBP 미지정 시 디버그 오버레이 폴백 (개발 빌드 한정)
	if (InventoryWindowWidgetClass == nullptr)
	{
#if !UE_BUILD_SHIPPING
		NUInventoryDebugUI::Toggle(GetWorld());
#endif
		return;
	}
	// 창고·상점·공방이 주고받으려고 연 소지품은 그 창의 동반 창이다 (패드 첫 포커스는 그 창이 받는다)
	ToggleSheetWidget(InventoryWindowWidgetClass, InventoryWindowWidget,
		FSimpleDelegate::CreateUObject(this, &UNUInventoryComponent::ToggleInventoryUI),
		bInventoryOpenedByStorage || bInventoryOpenedByShop);

	// 8/25 회의 결정: 인벤을 열면 장비창이 함께 뜬다.
	// 창고·상점이 주고받으려고 연 인벤에는 딸려 보내지 않는다 --- 창이 셋이 되어 화면을 덮는다
	if (!bInventoryOpenedByStorage && !bInventoryOpenedByShop && EquipmentWidgetClass != nullptr)
	{
		const bool bInventoryOpen = InventoryWindowWidget != nullptr && InventoryWindowWidget->IsInViewport();
		const bool bEquipmentOpen = EquipmentWidget != nullptr && EquipmentWidget->IsInViewport();
		if (bInventoryOpen != bEquipmentOpen)
		{
			ToggleEquipmentUI();
		}
	}
}

void UNUInventoryComponent::ToggleCharacterInfoUI()
{
	if (ToggleCharacterMenu(ENUCharacterMenuSection::Character))
	{
		return;
	}
	if (CharacterInfoWidgetClass == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("캐릭터 정보창 WBP 미지정 — PlayerState BP의 InventoryComponent에서 지정하세요"));
		return;
	}
	ToggleSheetWidget(CharacterInfoWidgetClass, CharacterInfoWidget,
		FSimpleDelegate::CreateUObject(this, &UNUInventoryComponent::ToggleCharacterInfoUI));
}

void UNUInventoryComponent::ToggleEquipmentUI()
{
	if (EquipmentWidgetClass == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("장비창 WBP 미지정 — PlayerState BP의 InventoryComponent에서 지정하세요"));
		return;
	}
	// 소지품을 열면 따라 뜨는 창이라 소지품이 떠 있으면 동반 창이다 (패드 첫 포커스는 소지품이 받는다)
	ToggleSheetWidget(EquipmentWidgetClass, EquipmentWidget,
		FSimpleDelegate::CreateUObject(this, &UNUInventoryComponent::ToggleEquipmentUI),
		InventoryWindowWidget != nullptr && InventoryWindowWidget->IsInViewport());
}

void UNUInventoryComponent::ToggleEquipmentMenuUI()
{
	if (!ToggleCharacterMenu(ENUCharacterMenuSection::Equipment)) ToggleEquipmentUI();
}

void UNUInventoryComponent::OpenShopUI(int32 ShopId)
{
	const TSubclassOf<UNUSheetWidgetBase> WindowClass = GetDefault<UNUShopSettings>()->ShopWindowClass.LoadSynchronous();
	if (WindowClass == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("상점 창 WBP 미지정 — 프로젝트 세팅(NU Shop)의 ShopWindowClass를 지정하세요"));
		return;
	}
	const FNUShopRow* Shop = NUShop::FindShopRow(ShopId);
	if (Shop == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("상점 표에 없는 상점 [%d] — 프로젝트 세팅(NU Shop)의 ShopTable 확인"), ShopId);
		return;
	}

	// 상점은 인벤과 주고받는 창이라 같이 띄운다 --- 창고와 같은 규칙.
	// 인벤을 먼저 열어야 ESC가 더 최근인 상점을 집고, 상점 닫기가 이 인벤까지 함께 닫는다.
	// 판매를 상점 창 안(판매 탭)에서 하는 WBP는 소지품 창을 띄우지 않는다 (창 클래스 기본값 bOpenInventoryWithShop)
	const UNUShopWindowWidget* ShopDefaults = Cast<UNUShopWindowWidget>(WindowClass->GetDefaultObject());
	if ((ShopDefaults == nullptr || ShopDefaults->ShouldOpenInventory())
		&& (InventoryWindowWidget == nullptr || !InventoryWindowWidget->IsInViewport()))
	{
		bInventoryOpenedByShop = true;
		ToggleInventoryUI();
	}

	if (ShopWindow == nullptr || !ShopWindow->IsInViewport())
	{
		ToggleSheetWidget(WindowClass, ShopWindow,
			FSimpleDelegate::CreateUObject(this, &UNUInventoryComponent::CloseShopUI), false,
			ShopDefaults != nullptr && ShopDefaults->ShouldHideFieldHUD(),
			ShopDefaults != nullptr && ShopDefaults->ShouldBlockGameInput());
	}
	if (UNUShopWindowWidget* Window = Cast<UNUShopWindowWidget>(ShopWindow))
	{
		Window->SetShop(ShopId); // 이미 열려 있으면 내용만 갈린다 (다른 NPC 상점으로 전환)
	}

	// 인벤 창의 우클릭 뜻·안내 문구가 상점 모드로 바뀌므로 다시 그리게 한다
	OnInventoryChanged.Broadcast();
}

void UNUInventoryComponent::OpenCraftUI(int32 ShopId)
{
	const TSubclassOf<UNUSheetWidgetBase> WindowClass = GetDefault<UNUShopSettings>()->CraftWindowClass.LoadSynchronous();
	if (WindowClass == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("제작 창 WBP 미지정 --- 프로젝트 세팅(NU Shop)의 CraftWindowClass를 지정하세요"));
		return;
	}
	if (NUShop::FindShopRow(ShopId) == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("상점 표에 없는 공방 [%d] --- 프로젝트 세팅(NU Shop)의 ShopTable 확인"), ShopId);
		return;
	}

	// 재료가 가방에 있어야 하므로 인벤을 같이 띄운다 (상점과 같은 규칙)
	if (InventoryWindowWidget == nullptr || !InventoryWindowWidget->IsInViewport())
	{
		bInventoryOpenedByShop = true;
		ToggleInventoryUI();
	}

	if (CraftWindow == nullptr || !CraftWindow->IsInViewport())
	{
		ToggleSheetWidget(WindowClass, CraftWindow,
			FSimpleDelegate::CreateUObject(this, &UNUInventoryComponent::CloseCraftUI));
	}
	if (UNUCraftWindowWidget* Window = Cast<UNUCraftWindowWidget>(CraftWindow))
	{
		Window->SetShop(ShopId); // 이미 열려 있으면 내용만 갈린다 (다른 공방으로 전환)
	}
	OnInventoryChanged.Broadcast();
}

void UNUInventoryComponent::CloseCraftUI()
{
	if (CraftWindow != nullptr && CraftWindow->IsInViewport())
	{
		ToggleSheetWidget(CraftWindow->GetClass(), CraftWindow, FSimpleDelegate()); // 열림 상태 토글 = 닫기
	}

	// 제작이 열면서 켠 인벤이면 같이 닫는다 (상점과 같은 깃발을 쓴다 --- 둘이 동시에 열리지는 않는다)
	if (bInventoryOpenedByShop)
	{
		bInventoryOpenedByShop = false;
		if (InventoryWindowWidget != nullptr && InventoryWindowWidget->IsInViewport())
		{
			ToggleInventoryUI();
		}
	}
}

void UNUInventoryComponent::CloseShopUI()
{
	if (ShopWindow != nullptr && ShopWindow->IsInViewport())
	{
		ToggleSheetWidget(ShopWindow->GetClass(), ShopWindow, FSimpleDelegate()); // 열림 상태 토글 = 닫기
	}

	// 상점이 열면서 켠 인벤이면 같이 닫는다
	if (bInventoryOpenedByShop)
	{
		bInventoryOpenedByShop = false;
		if (InventoryWindowWidget != nullptr && InventoryWindowWidget->IsInViewport())
		{
			ToggleInventoryUI();
		}
	}
	else
	{
		OnInventoryChanged.Broadcast(); // 남아 있는 인벤 창의 우클릭 뜻을 원래대로 되돌린다
	}
}

int32 UNUInventoryComponent::GetOpenShopId() const
{
	const UNUShopWindowWidget* Window = Cast<UNUShopWindowWidget>(ShopWindow);
	return Window != nullptr && ShopWindow->IsInViewport() ? Window->GetShopId() : 0;
}

int32 UNUInventoryComponent::GetReceivableCount(FName ItemRowName) const
{
	const FNUItemDefRow* Def = FindItemDef(ItemRowName);
	if (Def == nullptr)
	{
		return 0;
	}

	// 빈 칸(분류 구간이 있으면 그 구간의) x 최대 스택 + 같은 아이템 스택의 남은 자리
	int64 Receivable = static_cast<int64>(CountFreeSlots(GetBagRange(ItemRowName))) * Def->MaxStack;
	for (const FNUItemInstance& Item : InventoryList.Items)
	{
		if (Item.ItemRowName == ItemRowName)
		{
			Receivable += FMath::Max(0, Def->MaxStack - Item.Count);
		}
	}
	return static_cast<int32>(FMath::Min<int64>(Receivable, MAX_int32));
}

bool UNUInventoryComponent::ShopBuyItem(const FNUShopItemRow& Item, int32 Count, const FNUShopRow& Shop)
{
	const FName ItemRowName = FindRowNameByNumericId(Item.RefId);
	const int32 UnitPrice = Item.Price; // 표의 값이 곧 구매가 (등급·지역 배수 반영 완료)
	if (ItemRowName.IsNone() || Count < 1 || UnitPrice <= 0)
	{
		return false;
	}

	const int64 TotalPrice = static_cast<int64>(UnitPrice) * Count;
	if (TotalPrice > Gold)
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUShop", "NotEnoughGold", "골드가 부족합니다."));
		}
		return false;
	}

	// AddItem은 자리가 모자라면 일부만 지급하고도 성공한다 --- 구매는 전량 아니면 취소라 미리 재본다
	if (Count > GetReceivableCount(ItemRowName))
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUShop", "NotEnoughRoom", "가방에 자리가 부족합니다."));
		}
		return false;
	}

	if (bServerInventoryActive)
	{
		// 서버가 품목표로 다시 계산해 검증한다 (가이드 §17). 총액은 클라 표가 낡았는지 대조하는 용도 --- 다르면 2208.
		// 지급된 칸은 SVR_Item_Slot(70), 잔액은 응답 JSON으로 온다 (HandleServerShopResponse)
		UEmberServerManager* Server = GetServerManager();
		if (Server == nullptr)
		{
			return false;
		}
		PendingShopBuyNpcIds.Add(Shop.NpcId);
		Server->SendShopBuy(Shop.ShopId, Item.RefId, Count, static_cast<int32>(TotalPrice));
		return true;
	}

	// 서버 없이 PIE --- 로컬 더미
	if (!AddItem(ItemRowName, Count))
	{
		return false;
	}
	Gold -= TotalPrice;
	OnPurchaseSucceeded.Broadcast(Shop.NpcId);
	UNUSFXSubsystem::Play(this, NU::SFX::Shop::Buy);

	UE_LOG(LogNUInventory, Log, TEXT("구매 [%s] x%d --- %lld골드 (잔액 %lld)"),
		*ItemRowName.ToString(), Count, TotalPrice, Gold);
	return true;
}

int32 UNUInventoryComponent::CountItemsByNumericId(int32 NumericId) const
{
	const FName RowName = FindRowNameByNumericId(NumericId);
	if (RowName.IsNone())
	{
		return 0;
	}

	int32 Total = 0;
	for (const FNUItemInstance& Item : GetItems())
	{
		if (Item.ItemRowName == RowName)
		{
			Total += Item.Count;
		}
	}
	return Total;
}

bool UNUInventoryComponent::CanCraft(const FNUCraftRecipeRow& Recipe, int32 Times, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	const FName ResultRowName = FindRowNameByNumericId(Recipe.ResultId);
	if (ResultRowName.IsNone() || Recipe.ResultCount < 1 || Times < 1)
	{
		OutReason = NSLOCTEXT("NUCraft", "BadRecipe", "레시피가 올바르지 않습니다.");
		return false;
	}

	const TArray<FNUCraftMaterial> Materials = Recipe.GetMaterials();
	if (Materials.IsEmpty())
	{
		OutReason = NSLOCTEXT("NUCraft", "NoMaterial", "레시피에 재료가 없습니다.");
		return false;
	}

	if (static_cast<int64>(Recipe.Gold) * Times > Gold)
	{
		OutReason = NSLOCTEXT("NUCraft", "NotEnoughGold", "골드가 부족합니다.");
		return false;
	}

	// 도면은 소모하지 않고 가지고만 있으면 된다 (기획서 8절 "영구 해금"의 임시 형태)
	if (Recipe.RequiredBlueprintId > 0 && CountItemsByNumericId(Recipe.RequiredBlueprintId) <= 0)
	{
		OutReason = NSLOCTEXT("NUCraft", "NoBlueprint", "도면이 없습니다.");
		return false;
	}

	for (const FNUCraftMaterial& Material : Materials)
	{
		if (CountItemsByNumericId(Material.RefId) < Material.Count * Times)
		{
			OutReason = NSLOCTEXT("NUCraft", "NotEnoughMaterial", "재료가 부족합니다.");
			return false;
		}
	}

	// 재료를 뺀 뒤 결과물이 들어갈 자리가 있는지까지 봐야 하지만, 여기서는 넉넉하게 지금 자리로 본다.
	// (재료가 칸을 비우므로 실제로는 이보다 여유가 생긴다). 단 분류 구간이 있으면 재료가 비우는 칸은 재료 구간이라
	// 결과물 자리가 늘지 않는다 --- 지금 자리가 모자라면 막는다
	if (Recipe.ResultCount * Times > GetReceivableCount(ResultRowName)
		&& (!BagSections.IsEmpty() || FindFreeSlotNo(ResultRowName) == INDEX_NONE))
	{
		OutReason = NSLOCTEXT("NUCraft", "NotEnoughRoom", "가방에 자리가 부족합니다.");
		return false;
	}
	return true;
}

int32 UNUInventoryComponent::GetMaxCraftCount(const FNUCraftRecipeRow& Recipe) const
{
	const TArray<FNUCraftMaterial> Materials = Recipe.GetMaterials();
	if (Materials.IsEmpty() || Recipe.ResultCount < 1
		|| (Recipe.RequiredBlueprintId > 0 && CountItemsByNumericId(Recipe.RequiredBlueprintId) <= 0))
	{
		return 0;
	}

	int32 Max = 99; // 한 번에 만드는 상한 --- 스택 상한과 같은 자릿수면 충분하다
	for (const FNUCraftMaterial& Material : Materials)
	{
		Max = FMath::Min(Max, CountItemsByNumericId(Material.RefId) / Material.Count);
	}
	if (Recipe.Gold > 0)
	{
		Max = static_cast<int32>(FMath::Min<int64>(Max, Gold / Recipe.Gold));
	}
	return FMath::Max(0, Max);
}

bool UNUInventoryComponent::CraftRecipe(const FNUCraftRecipeRow& Recipe, int32 Times)
{
	FText Reason;
	if (!CanCraft(Recipe, Times, Reason))
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(Reason);
		}
		return false;
	}

	// 재료를 먼저 뺀다 --- 결과물이 들어갈 자리를 재료가 비워 주는 경우가 있다
	for (const FNUCraftMaterial& Material : Recipe.GetMaterials())
	{
		int32 Remaining = Material.Count * Times;
		const FName MaterialRowName = FindRowNameByNumericId(Material.RefId);
		// 뒤에서부터 --- 앞 칸(정렬된 자리)을 남겨 두면 가방이 덜 흐트러진다.
		// RemoveItem이 배열을 건드리므로 지울 대상을 먼저 모아 둔다
		TArray<TPair<FGuid, int32>> Takes;
		const TArray<FNUItemInstance>& Bag = GetItems();
		for (int32 Index = Bag.Num() - 1; Index >= 0 && Remaining > 0; --Index)
		{
			if (Bag[Index].ItemRowName != MaterialRowName)
			{
				continue;
			}
			const int32 Take = FMath::Min(Remaining, Bag[Index].Count);
			Takes.Emplace(Bag[Index].InstanceId, Take);
			Remaining -= Take;
		}
		for (const TPair<FGuid, int32>& Take : Takes)
		{
			RemoveItem(Take.Key, Take.Value);
		}
	}

	const FName ResultRowName = FindRowNameByNumericId(Recipe.ResultId);
	const int32 ResultTotal = Recipe.ResultCount * Times;
	AddItem(ResultRowName, ResultTotal);
	Gold -= static_cast<int64>(Recipe.Gold) * Times;

	UE_LOG(LogNUInventory, Log, TEXT("제작 [%s] x%d --- %d골드 (잔액 %lld)"),
		*ResultRowName.ToString(), ResultTotal, Recipe.Gold * Times, Gold);

	if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
	{
		const FNUItemDefRow* Def = FindItemDef(ResultRowName);
		const FText Name = Def != nullptr ? Def->DisplayName : FText::FromName(ResultRowName);
		UI->ShowNotice(ResultTotal > 1
			? FText::Format(NSLOCTEXT("NUCraft", "CraftedN", "{0} {1}개를 만들었습니다."), Name, FText::AsNumber(ResultTotal))
			: FText::Format(NSLOCTEXT("NUCraft", "Crafted", "{0}을(를) 만들었습니다."), Name));
	}
	UNUSFXSubsystem::Play(this, NU::SFX::Shop::Craft);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UNUInventoryComponent::ShopSellItem(const FGuid& InstanceId, int32 Count, const FNUShopRow& Shop)
{
	const FNUItemInstance* Instance = FindInstance(InstanceId);
	const FNUItemDefRow* Def = Instance != nullptr ? FindItemDef(Instance->ItemRowName) : nullptr;
	if (Instance == nullptr || Def == nullptr || Count < 1)
	{
		return false;
	}

	// 퀘스트템·열쇠템·도면은 못 팔고, 가격 없는 아이템(0)은 매각가가 없다
	const int32 UnitPrice = NUShop::GetSellPrice(*Def, Shop);
	if (!NUItem::IsSellable(Def->ItemType) || UnitPrice <= 0)
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController()))
		{
			UI->ShowNotice(NSLOCTEXT("NUShop", "CannotSell", "판매할 수 없는 아이템입니다."));
		}
		return false;
	}

	const FName SoldRowName = Instance->ItemRowName;
	const int32 SellCount = FMath::Min(Count, Instance->Count);
	if (bServerInventoryActive)
	{
		// 서버가 같은 식(기준 행 Price x BuybackRate, 개당 버림)으로 다시 계산한다 --- 다르면 2208. 칸 갱신은 SVR_Item_Slot(70)
		UEmberServerManager* Server = GetServerManager();
		if (Server == nullptr)
		{
			return false;
		}
		Server->SendShopSell(Instance->SlotNo, SellCount, UnitPrice * SellCount, Shop.ShopId);
		return true;
	}

	// 서버 없이 PIE --- 로컬 더미
	if (!RemoveItem(InstanceId, SellCount))
	{
		return false;
	}
	Gold += static_cast<int64>(UnitPrice) * SellCount;
	UNUSFXSubsystem::Play(this, NU::SFX::Shop::Sell);

	UE_LOG(LogNUInventory, Log, TEXT("판매 [%s] x%d --- +%lld골드 (잔액 %lld)"),
		*SoldRowName.ToString(), SellCount, static_cast<int64>(UnitPrice) * SellCount, Gold);
	return true;
}

void UNUInventoryComponent::ToggleStorageUI()
{
	APlayerController* PC = GetLocalPlayerController();
	if (PC == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("창고 창 토글 실패 — 로컬 PlayerController 없음"));
		return;
	}

	if (IsStorageOpen())
	{
		if (UNUUIManager* UI = UNUUIManager::Get(PC))
		{
			UI->CloseWindow(StorageWindowWidget);
		}
		else
		{
			StorageWindowWidget->RemoveFromParent();
		}

		// 창고가 열면서 켠 인벤이면 같이 닫는다
		if (bInventoryOpenedByStorage)
		{
			bInventoryOpenedByStorage = false;
			if (InventoryWindowWidget != nullptr && InventoryWindowWidget->IsInViewport())
			{
				ToggleInventoryUI();
			}
		}
		return;
	}

	// 창고 전용 WBP (두 패널 배치·제목·닫기 버튼은 WBP가 갖는다). 어느 PlayerState든 같은 창
	const TSubclassOf<UNUSheetWidgetBase> ClassToUse = GetDefault<UNUUISettings>()->StorageWindowClass.LoadSynchronous();
	if (ClassToUse == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("창고 창 WBP 미지정 — 프로젝트 세팅(NU UI)의 StorageWindowClass를 지정하세요"));
		return;
	}

	if (StorageWindowWidget == nullptr)
	{
		StorageWindowWidget = CreateWidget<UNUSheetWidgetBase>(PC, ClassToUse);
		if (StorageWindowWidget == nullptr)
		{
			return;
		}
	}
	// 창고는 소지품과 주고받는 창이라 인벤을 같이 띄운다.
	// 창고보다 먼저 열어야 ESC가 더 최근인 창고를 집고, 창고 닫기가 이 인벤까지 함께 닫는다
	if (InventoryWindowWidget == nullptr || !InventoryWindowWidget->IsInViewport())
	{
		bInventoryOpenedByStorage = true;
		ToggleInventoryUI();
	}

	// 딤은 매니저가 창고 밴드 바로 아래에 깐다
	{
		FNUUIWindowParams StorageParams;
		StorageParams.bDimBackground = true;
		StorageParams.bCloseOnSessionEnd = true; // 창고는 서버 전용이라 세션이 끊기면 의미가 없다
		StorageParams.OpenSFX = NU::SFX::UI::ChestOpen; // 나무 상자를 여닫는 소리
		StorageParams.CloseSFX = NU::SFX::UI::ChestClose;
		if (UNUUIManager* UI = UNUUIManager::Get(PC))
		{
			UI->OpenWindow(StorageWindowWidget, StorageParams,
				FSimpleDelegate::CreateUObject(this, &UNUInventoryComponent::ToggleStorageUI));
		}
		else
		{
			StorageWindowWidget->AddToViewport(NUUILayer::Window);
		}
	}

	if (UNUInventoryWindowWidget* Window = Cast<UNUInventoryWindowWidget>(StorageWindowWidget))
	{
		// 좌 = 계정 창고, 우 = 캐릭터 창고 (회의 결정 배치)
		Window->SetDisplayContainers({ NUContainer::AccountStorage, NUContainer::CharStorage });
	}
}

bool UNUInventoryComponent::IsStorageOpen() const
{
	return StorageWindowWidget != nullptr && StorageWindowWidget->IsInViewport();
}

void UNUInventoryComponent::ToggleSheetWidget(TSubclassOf<UNUSheetWidgetBase> WidgetClass, TObjectPtr<UNUSheetWidgetBase>& WidgetRef, FSimpleDelegate OnCloseRequested, bool bCompanion, bool bHideFieldHUD, bool bBlockGameInput)
{
	APlayerController* PC = GetLocalPlayerController();
	if (PC == nullptr)
	{
		return;
	}
	UNUUIManager* UI = UNUUIManager::Get(PC);

	if (WidgetRef != nullptr && WidgetRef->IsInViewport())
	{
		if (UI != nullptr)
		{
			UI->CloseWindow(WidgetRef);
		}
		else
		{
			WidgetRef->RemoveFromParent();
		}
	}
	else
	{
		if (WidgetRef == nullptr)
		{
			WidgetRef = CreateWidget<UNUSheetWidgetBase>(PC, WidgetClass);
			if (WidgetRef == nullptr)
			{
				return;
			}
		}
		if (UI != nullptr)
		{
			FNUUIWindowParams Params;
			Params.bCloseOnSessionEnd = true; // 서버 아이템을 그리는 창이라 세션이 끊기면 닫는다
			Params.bCompanion = bCompanion;
			Params.bHideFieldHUD = bHideFieldHUD;
			Params.bBlockGameInput = bBlockGameInput;
			UI->OpenWindow(WidgetRef, Params, MoveTemp(OnCloseRequested));
		}
		else
		{
			WidgetRef->AddToViewport(NUUILayer::Window);
		}
	}
}

bool UNUInventoryComponent::IsOwnerInBattle() const
{
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	const UNUAbilitySystemComponent* ASC = OwnerState != nullptr ? OwnerState->GetNUASC() : nullptr;
	if (ASC != nullptr && ASC->GetBattleEncounter() != nullptr)
	{
		return true;
	}

	// 서버 전투는 이 클라이언트의 상태다 --- 내 플레이어가 아니면 알 수 없으므로 보지 않는다
	if (GetLocalPlayerController() == nullptr)
	{
		return false;
	}
#if !UE_BUILD_SHIPPING
	if (bDebugFakeNetBattle)
	{
		return true;
	}
#endif
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (BattleNet != nullptr && BattleNet->IsInBattle())
	{
		return true;
	}
	// 서버의 전투 종료는 처치 연출이 끝나기 전에 먼저 온다 --- 전투 무대가 내려가기 시작할 때까지는 전투로 본다 (필드 HUD 와 같은 기준, UNU_BaseInGameWidget)
	const UWorld* World = GetWorld();
	const UNUBattleNetStageSubsystem* Stage = World ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;
	return Stage != nullptr && Stage->IsNetStageReady();
}

UNUBattleNetSubsystem* UNUInventoryComponent::GetBattleNet() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
}

#if !UE_BUILD_SHIPPING
void UNUInventoryComponent::DebugSetFakeNetBattle(bool bFake)
{
	bDebugFakeNetBattle = bFake;
	RefreshOwnerBattleState();
}
#endif

void UNUInventoryComponent::RefreshOwnerBattleState()
{
	// 창은 내 플레이어에게만 있다
	if (GetLocalPlayerController() == nullptr)
	{
		return;
	}
	const bool bNowInBattle = IsOwnerInBattle();
	if (bNowInBattle == bOwnerInBattle)
	{
		return;
	}
	bOwnerInBattle = bNowInBattle;
	if (!bNowInBattle)
	{
		OnOwnerBattleChanged.Broadcast();
		return;
	}

	// 전투 진입 시 열린 창 전부 닫기 — 커서 요청 해제는 매니저가 한다
	UNUUIManager* UI = UNUUIManager::Get(GetLocalPlayerController());
	const auto CloseIfOpen = [UI](UNUSheetWidgetBase* Widget)
	{
		if (Widget == nullptr || !Widget->IsInViewport())
		{
			return;
		}
		if (UI != nullptr)
		{
			UI->CloseWindow(Widget);
		}
		else
		{
			Widget->RemoveFromParent();
		}
	};
	CloseIfOpen(InventoryWindowWidget);
	CloseIfOpen(CharacterInfoWidget);
	CloseIfOpen(EquipmentWidget);
	CloseIfOpen(StorageWindowWidget);
	CloseIfOpen(ShopWindow);
	CloseIfOpen(CraftWindow);
	CloseCharacterMenu();

	// 창고·상점·공방이 연 인벤도 여기서 닫혔으므로 리셋 --- 안 하면 전투 후 수동으로 연 인벤이 그 창 닫기에 딸려 닫힌다.
	// CloseWindow는 닫기 요청 콜백(CloseShopUI 등)을 부르지 않으므로 깃발을 직접 푼다
	bInventoryOpenedByStorage = false;
	bInventoryOpenedByShop = false;
	OnOwnerBattleChanged.Broadcast();
}

APlayerController* UNUInventoryComponent::GetLocalPlayerController() const
{
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	APlayerController* PC = OwnerState ? OwnerState->GetPlayerController() : nullptr;
	return (PC != nullptr && PC->IsLocalController()) ? PC : nullptr;
}

/////////////////////////////////////////////////////////////////
// 변경 알림 / 정의 테이블
/////////////////////////////////////////////////////////////////

void UNUInventoryComponent::HandleInventoryReplicated()
{
	OnInventoryChanged.Broadcast();
}

void UNUInventoryComponent::BroadcastChangedAndPush()
{
	// 서버(리슨 호스트 포함)는 FastArray 수신 콜백이 불리지 않으므로 직접 알림
	OnInventoryChanged.Broadcast();
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UNUInventoryComponent::ApplyServerGold(int64 ServerGold)
{
	Gold = ServerGold;
	OnInventoryChanged.Broadcast();
}

void UNUInventoryComponent::AddGold(int64 Delta)
{
	// 음수 잔액은 사망 손실로만 만든다. 수입은 기존 빚부터 자연스럽게 상환한다.
	if (Delta > 0)
	{
		Gold = Gold > MAX_int64 - Delta ? MAX_int64 : Gold + Delta;
	}
	else if (Gold >= 0)
	{
		Gold = Delta < -Gold ? 0 : Gold + Delta;
	}

	// 골드 표시(상점 창 등)는 인벤 변경 통지로 갱신된다 --- 배열은 안 바뀌었으니 넷 푸시는 없다
	OnInventoryChanged.Broadcast();
}

const UDataTable* UNUInventoryComponent::GetItemDefTable() const
{
	return ItemDefTable != nullptr ? ItemDefTable.Get() : RuntimeItemDefTable.Get();
}

void UNUInventoryComponent::EnsureItemDefTable()
{
	if (ItemDefTable != nullptr || RuntimeItemDefTable != nullptr)
	{
		return;
	}

	// 기획 데이터 테이블이 나오기 전까지의 더미 정의 2종
	UDataTable* Table = NewObject<UDataTable>(this, TEXT("NUInventoryDummyItemTable"));
	Table->RowStruct = FNUItemDefRow::StaticStruct();

	FNUItemDefRow Potion;
	Potion.DisplayName = FText::FromString(TEXT("작은 회복 물약"));
	Potion.ItemType = ENUItemType::Consumable;
	Potion.ItemTag = NU::Item::Potion::SmallHeal;
	Potion.MaxStack = 99;
	Potion.Effect1_Type = ENUItemEffectType::HealHP;
	Potion.Effect1_Value = 30;
	Potion.bUsableInField = true;
	Potion.bUsableInBattle = true;
	Table->AddRow(TEXT("Potion_SmallHeal"), Potion);

	FNUItemDefRow Stone;
	Stone.DisplayName = FText::FromString(TEXT("공명석"));
	Stone.ItemType = ENUItemType::Commune;
	Stone.ItemTag = NU::Item::CommuneStone;
	Stone.MaxStack = 10;
	Stone.bUsableInField = false;
	Stone.bUsableInBattle = true;
	Stone.BattleTargetPolicy = ENUTargetPolicy::SingleEnemy;
	Table->AddRow(TEXT("CommuneStone"), Stone);

	RuntimeItemDefTable = Table;
}

#if !UE_BUILD_SHIPPING
// 임시 상점 열기 --- NPC 상호작용 연결 전까지의 테스트 경로.
// 사용: nu.Shop.Open [Shop_ID] (생략 시 8003 = 시안의 잡화 좌판)
// 임시 공방 열기 --- 공방 NPC(루디·엘라라·메리스·브란)에 아직 컴포넌트가 안 붙어 있어 쓰는 테스트 경로.
// 사용: nu.Craft.Open [Shop_ID] (생략 시 8006 = 조제 공방)
static FAutoConsoleCommandWithWorldAndArgs GNUCraftOpenCommand(
	TEXT("nu.Craft.Open"),
	TEXT("공방 제작 창 열기 (인자: 기획 표 Shop_ID, 생략 시 8006)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* PC = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		APlayerState* PS = PC != nullptr ? PC->PlayerState.Get() : nullptr;
		UNUInventoryComponent* Inventory = PS != nullptr ? PS->FindComponentByClass<UNUInventoryComponent>() : nullptr;
		if (Inventory != nullptr)
		{
			Inventory->OpenCraftUI(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 8006);
		}
	}));

// 제작 재료를 한 번에 채운다 --- 표에 있는 재료(6501~6510)를 개수만큼.
// 사용: nu.Craft.GiveMaterials [개수] (생략 시 10)
static FAutoConsoleCommandWithWorldAndArgs GNUCraftGiveCommand(
	TEXT("nu.Craft.GiveMaterials"),
	TEXT("제작 재료를 개수만큼 지급 (생략 시 10개씩)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* PC = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		APlayerState* PS = PC != nullptr ? PC->PlayerState.Get() : nullptr;
		UNUInventoryComponent* Inventory = PS != nullptr ? PS->FindComponentByClass<UNUInventoryComponent>() : nullptr;
		if (Inventory == nullptr)
		{
			return;
		}
		const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10;
		for (int32 NumericId = 6501; NumericId <= 6510; ++NumericId)
		{
			const FName RowName = Inventory->FindRowNameByNumericId(NumericId);
			if (!RowName.IsNone())
			{
				Inventory->ServerDebugAddItem(RowName, Count);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GNUShopOpenCommand(
	TEXT("nu.Shop.Open"),
	TEXT("상점 창 열기 (인자: 기획 표 Shop_ID, 생략 시 8003)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* PC = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		APlayerState* PS = PC != nullptr ? PC->PlayerState.Get() : nullptr;
		UNUInventoryComponent* Inventory = PS != nullptr ? PS->FindComponentByClass<UNUInventoryComponent>() : nullptr;
		if (Inventory != nullptr)
		{
			Inventory->OpenShopUI(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 8003);
		}
	}));
#endif


bool UNUInventoryComponent::ApplyDeathGoldLoss(int64 Loss)
{
	const UNURecoveryComponent* Recovery = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UNURecoveryComponent>() : nullptr;
	if (Recovery == nullptr || !Recovery->CanExecuteAuthorityRecovery() || Loss < 0 || Gold < MIN_int64 + Loss)
	{
		return false;
	}
	Gold -= Loss;
	GetOwner()->ForceNetUpdate();
	OnInventoryChanged.Broadcast();
	return true;
}

void UNUInventoryComponent::OnRep_Gold()
{
	OnInventoryChanged.Broadcast();
}

void UNUInventoryComponent::SetPadHeldItem(int32 Container, int32 SlotNo, const FGuid& InstanceId)
{
	PadHeldItem.Container = Container;
	PadHeldItem.SlotNo = SlotNo;
	PadHeldItem.InstanceId = InstanceId;
	OnPadHeldItemChanged.Broadcast();
}

void UNUInventoryComponent::ClearPadHeldItem()
{
	if (!PadHeldItem.IsSet())
	{
		return;
	}
	PadHeldItem = FNUPadHeldItem();
	OnPadHeldItemChanged.Broadcast();
}
