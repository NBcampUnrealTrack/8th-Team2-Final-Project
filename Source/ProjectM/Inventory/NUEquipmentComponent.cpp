// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUEquipmentComponent.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/GameInstance.h"
#include "GameplayEffect.h"
#include "Net/EmberServerManager.h"
#include "Inventory/NUGE_EquipmentStats.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTags.h"
#include "Inventory/NUInventoryTypes.h"
#include "Net/UnrealNetwork.h"
#include "NUGAS/NUAbilitySystemComponent.h"

UNUEquipmentComponent::UNUEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	EquippedItems.SetNum(static_cast<int32>(ENUEquipSlot::MAX));
}

void UNUEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, EquippedItems, COND_OwnerOnly);
}

bool UNUEquipmentComponent::IsJobAllowed(const FNUItemDefRow& Def) const
{
	if (Def.RequiredJobId == 0)
	{
		return true;
	}
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	const UNUJobComponent* Job = OwnerState != nullptr ? OwnerState->GetJobComponent() : nullptr;
	const int32 MyJobId = Job != nullptr ? Job->GetJobId() : 0;
	return MyJobId == 0 || MyJobId == Def.RequiredJobId;
}

bool UNUEquipmentComponent::IsSlotOccupied(ENUEquipSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return EquippedItems.IsValidIndex(Index) && EquippedItems[Index].InstanceId.IsValid();
}

/////////////////////////////////////////////////////////////////
// 장착 / 해제 (서버 권위)
/////////////////////////////////////////////////////////////////

void UNUEquipmentComponent::ServerEquipItem_Implementation(FGuid InstanceId)
{
	UNUInventoryComponent* Inventory = GetInventory();
	UNUAbilitySystemComponent* ASC = GetNUASC();
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || Inventory == nullptr || ASC == nullptr)
	{
		return;
	}
	if (Inventory->IsOwnerInBattle())
	{
		UE_LOG(LogNUInventory, Log, TEXT("장착 거절 — 전투 중"));
		return;
	}

	const FNUItemInstance* Instance = Inventory->FindInstance(InstanceId);
	if (Instance == nullptr || Instance->Count < 1)
	{
		UE_LOG(LogNUInventory, Log, TEXT("장착 거절 — 인스턴스 없음"));
		return;
	}
	const FNUItemDefRow* Def = Inventory->FindItemDef(Instance->ItemRowName);
	if (Def == nullptr || Def->ItemType != ENUItemType::Equipment || Def->EquipType == ENUEquipType::None)
	{
		UE_LOG(LogNUInventory, Log, TEXT("장착 거절 — 장비 아님 [%s]"), *Instance->ItemRowName.ToString());
		return;
	}

	if (!IsJobAllowed(*Def))
	{
		UE_LOG(LogNUInventory, Log, TEXT("장착 거절 --- 직업 전용 [%s] 직업 %d"), *Instance->ItemRowName.ToString(), Def->RequiredJobId);
		return;
	}

	const ENUEquipSlot TargetSlot = NUEquip::ResolveTargetSlot(Def->EquipType, EquippedItems);
	if (TargetSlot == ENUEquipSlot::MAX)
	{
		return;
	}

	// 게임서버가 붙어 있으면 요청만 보낸다 — 결과는 컨테이너 4 칸 푸시로 돌아온다.
	// 여기서 미리 배열을 건드리면 서버 응답과 두 번 반영돼 어긋난다
	if (UEmberServerManager* Server = GetServerManager())
	{
		if (Server->IsLoggedIn())
		{
			// 부위를 클라 테이블의 EquipType에서 뽑으므로, 1307(부위 불일치)이 오면
			// 서버가 같은 칸에서 다른 아이템을 보고 있다는 뜻이다 --- 아이템까지 남겨야 구분된다
			UE_LOG(LogNUInventory, Log, TEXT("장착 요청 --- 인벤 칸 %d = [%s] 아이템 %d => 부위 %d"),
				Instance->SlotNo, *Instance->ItemRowName.ToString(), Def->NumericId,
				NUEquip::ToServerEquipSlot(TargetSlot));

			Server->SendItemEquip(Instance->SlotNo, NUEquip::ToServerEquipSlot(TargetSlot));
			return;
		}
	}

	const FNUEquipStats StatsToApply = Def->EquipStats;

	// --- 여기부터는 실패 불가 변이 (검증 후 변이 원칙 — 롤백 없음) ---

	FNUItemInstance NewItem;
	if (!Inventory->RemoveInstanceForEquip(InstanceId, NewItem))
	{
		return; // 유일한 실패 가능 지점 — 아직 아무것도 변하지 않음
	}

	const int32 SlotIndex = static_cast<int32>(TargetSlot);

	// 점유 슬롯이면 기존 장비를 먼저 내리고(GE 제거) 마지막에 인벤토리로 복귀시킨다
	FNUItemInstance PrevItem;
	bool bHadPrev = false;
	if (EquippedItems[SlotIndex].InstanceId.IsValid())
	{
		bHadPrev = true;
		PrevItem.InstanceId = EquippedItems[SlotIndex].InstanceId;
		PrevItem.ItemRowName = EquippedItems[SlotIndex].ItemRowName;
		PrevItem.Count = 1;
		RemoveEquipEffect(TargetSlot);
	}

	EquippedItems[SlotIndex].InstanceId = NewItem.InstanceId;
	EquippedItems[SlotIndex].ItemRowName = NewItem.ItemRowName;
	ActiveEffectHandles[SlotIndex] = ApplyEquipEffect(StatsToApply);

	if (bHadPrev)
	{
		Inventory->AddItemInstance(PrevItem);
	}

	UE_LOG(LogNUInventory, Log, TEXT("장착 [%s] → %s"),
		*NewItem.ItemRowName.ToString(), *NUEquip::GetSlotDisplayName(TargetSlot).ToString());
	BroadcastChangedAndPush();
}

void UNUEquipmentComponent::ServerUnequipSlot_Implementation(ENUEquipSlot Slot)
{
	UNUInventoryComponent* Inventory = GetInventory();
	UNUAbilitySystemComponent* ASC = GetNUASC();
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || Inventory == nullptr || ASC == nullptr)
	{
		return;
	}
	if (Inventory->IsOwnerInBattle())
	{
		UE_LOG(LogNUInventory, Log, TEXT("해제 거절 — 전투 중"));
		return;
	}

	const int32 SlotIndex = static_cast<int32>(Slot);
	if (!EquippedItems.IsValidIndex(SlotIndex) || !EquippedItems[SlotIndex].InstanceId.IsValid())
	{
		return;
	}

	// 검증 후 변이 원칙 — GE를 걷어내기 전에 인벤토리가 받아줄 수 있는지 먼저 확인한다.
	// (먼저 해제했다가 자리가 없으면 장비가 증발한다)
	if (!Inventory->HasFreeSlot(EquippedItems[SlotIndex].ItemRowName))
	{
		UE_LOG(LogNUInventory, Log, TEXT("해제 거절 — 가방이 가득 참 (%d/%d칸)"),
			Inventory->GetUsedSlots(), Inventory->GetMaxSlots());
		Inventory->ShowEquipRejectNotice(1305, true); // 서버가 같은 이유로 거절할 때와 같은 문구
		return;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::Item::Unequip);

	// 게임서버가 붙어 있으면 요청만 보낸다 — 벗은 장비가 들어갈 인벤 칸은 서버가 고르고,
	// 결과는 인벤·장비 양쪽 칸 푸시로 돌아온다
	if (UEmberServerManager* Server = GetServerManager())
	{
		if (Server->IsLoggedIn())
		{
			Server->SendItemUnequip(NUEquip::ToServerEquipSlot(Slot));
			return;
		}
	}

	RemoveEquipEffect(Slot);

	FNUItemInstance ReturnItem;
	ReturnItem.InstanceId = EquippedItems[SlotIndex].InstanceId;
	ReturnItem.ItemRowName = EquippedItems[SlotIndex].ItemRowName;
	ReturnItem.Count = 1;

	EquippedItems[SlotIndex] = FNUEquippedItem();
	Inventory->AddItemInstance(ReturnItem);

	UE_LOG(LogNUInventory, Log, TEXT("해제 [%s] ← %s"),
		*ReturnItem.ItemRowName.ToString(), *NUEquip::GetSlotDisplayName(Slot).ToString());
	BroadcastChangedAndPush();
}

void UNUEquipmentComponent::ApplyServerEquipSlot(int32 ServerSlotNo, int32 ItemId, int32 ItemCount)
{
	UNUInventoryComponent* Inventory = GetInventory();
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || Inventory == nullptr)
	{
		return;
	}

	// 서버 부위 번호는 클라 enum과 순서가 다르다 — 경계에서 변환
	const ENUEquipSlot Slot = NUEquip::FromServerEquipSlot(ServerSlotNo);
	if (Slot == ENUEquipSlot::MAX)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("장비 칸 %d은 부위 범위 밖 — 무시"), ServerSlotNo);
		return;
	}
	const int32 SlotIndex = static_cast<int32>(Slot);

	const FName RowName = (ItemId != 0 && ItemCount > 0)
		? Inventory->FindRowNameByNumericId(ItemId)
		: NAME_None;
	if (ItemId != 0 && ItemCount > 0 && RowName.IsNone())
	{
		UE_LOG(LogNUInventory, Warning, TEXT("서버 ItemId %d의 정의를 못 찾음 — 장비 칸 %d 무시"), ItemId, ServerSlotNo);
		return;
	}

	if (EquippedItems[SlotIndex].ItemRowName == RowName)
	{
		return; // 변화 없음 — GE를 건드리지 않는다
	}

	RemoveEquipEffect(Slot);

	if (RowName.IsNone())
	{
		EquippedItems[SlotIndex] = FNUEquippedItem();
	}
	else
	{
		// 서버엔 인스턴스 개념이 없다 — 전투 참조용 식별자는 클라가 새로 만든다
		EquippedItems[SlotIndex].InstanceId = FGuid::NewGuid();
		EquippedItems[SlotIndex].ItemRowName = RowName;
		if (const FNUItemDefRow* Def = Inventory->FindItemDef(RowName))
		{
			ActiveEffectHandles[SlotIndex] = ApplyEquipEffect(Def->EquipStats);
		}
	}

	UE_LOG(LogNUInventory, Log, TEXT("장비 %s ← [%s]"),
		*NUEquip::GetSlotDisplayName(Slot).ToString(),
		RowName.IsNone() ? TEXT("비움") : *RowName.ToString());
	BroadcastChangedAndPush();
}

void UNUEquipmentComponent::ReapplyAllEquipmentEffects()
{
	UNUInventoryComponent* Inventory = GetInventory();
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || Inventory == nullptr)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < EquippedItems.Num(); ++SlotIndex)
	{
		RemoveEquipEffect(static_cast<ENUEquipSlot>(SlotIndex));
		if (!EquippedItems[SlotIndex].InstanceId.IsValid())
		{
			continue;
		}
		if (const FNUItemDefRow* Def = Inventory->FindItemDef(EquippedItems[SlotIndex].ItemRowName))
		{
			ActiveEffectHandles[SlotIndex] = ApplyEquipEffect(Def->EquipStats);
		}
	}
}

/////////////////////////////////////////////////////////////////
// 내부
/////////////////////////////////////////////////////////////////

UNUInventoryComponent* UNUEquipmentComponent::GetInventory() const
{
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	return OwnerState ? OwnerState->GetInventoryComponent() : nullptr;
}

UEmberServerManager* UNUEquipmentComponent::GetServerManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World != nullptr ? World->GetGameInstance() : nullptr;
	return GI != nullptr ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

UNUAbilitySystemComponent* UNUEquipmentComponent::GetNUASC() const
{
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(GetOwner());
	return OwnerState ? OwnerState->GetNUASC() : nullptr;
}

FActiveGameplayEffectHandle UNUEquipmentComponent::ApplyEquipEffect(const FNUEquipStats& Stats)
{
	UNUAbilitySystemComponent* ASC = GetNUASC();
	if (ASC == nullptr)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(UNUGE_EquipmentStats::StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	// 8키 전부 세팅 (미세팅 키가 있으면 GAS가 에러 로그를 낸다)
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::AttackPower, Stats.AttackPower);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::MagicAttackPower, Stats.MagicAttackPower);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::Defense, Stats.Defense);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::Speed, Stats.Speed);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::CriticalRate, Stats.CriticalRate);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::CriticalDamage, Stats.CriticalDamage);
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::MaxHealth, Stats.MaxHealth);
	// 장비 테이블 컬럼이 아직 MaxMana라 그대로 읽어 SP로 넣는다
	// 컬럼이 SP로 바뀌면 이 줄의 출처만 갈아끼우면 됨
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Equip::MaxSP, Stats.MaxMana);

	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

void UNUEquipmentComponent::RemoveEquipEffect(ENUEquipSlot Slot)
{
	const int32 SlotIndex = static_cast<int32>(Slot);
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(ENUEquipSlot::MAX))
	{
		return;
	}
	if (ActiveEffectHandles[SlotIndex].IsValid())
	{
		if (UNUAbilitySystemComponent* ASC = GetNUASC())
		{
			ASC->RemoveActiveGameplayEffect(ActiveEffectHandles[SlotIndex]);
		}
		ActiveEffectHandles[SlotIndex] = FActiveGameplayEffectHandle();
	}
}

void UNUEquipmentComponent::BroadcastChangedAndPush()
{
	// 서버(리슨 호스트 포함)는 OnRep이 불리지 않으므로 직접 알림
	OnEquipmentChanged.Broadcast();
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UNUEquipmentComponent::OnRep_EquippedItems()
{
	OnEquipmentChanged.Broadcast();
}
