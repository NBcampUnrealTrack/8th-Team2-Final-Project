// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Inventory/NUEquipmentTypes.h"
#include "NUEquipmentComponent.generated.h"

class UNUAbilitySystemComponent;
class UNUInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUEquipmentChanged);

// 장비 장착 (NUPlayerState 부착).
// - 슬롯 11개 고정 배열 (인덱스 = ENUEquipSlot), COND_OwnerOnly 복제
// - 장착 = 인벤토리에서 인스턴스 이동(InstanceId 보존), 해제 = 인벤토리 복귀
// - 스탯은 슬롯별 Infinite GE(UNUGE_EquipmentStats)로 부여, 서버 전용 핸들로 해제 시 원복
// - 전투 중 장착/해제 금지
struct FNUItemDefRow;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<FNUEquippedItem>& GetEquippedItems() const { return EquippedItems; }
	bool IsSlotOccupied(ENUEquipSlot Slot) const;

	// 이 장비를 내 직업이 낄 수 있는지 --- 전용 직업이 없거나(0) 아직 직업이 없으면(0) 된다. 다른 직업이면 안 된다.
	// 서버는 직업 제한을 모르므로 요청 전에 여기서 막는다 (인벤 창 안내와 ServerEquipItem이 같이 쓴다)
	bool IsJobAllowed(const FNUItemDefRow& Def) const;

	// 장착: 인벤토리의 장비 아이템(InstanceId)을 부위에 맞는 슬롯에 장착 (점유 시 기존 장비는 인벤토리 복귀)
	UFUNCTION(Server, Reliable)
	void ServerEquipItem(FGuid InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerUnequipSlot(ENUEquipSlot Slot);

	// 서버 전용: 전 슬롯 GE 제거 후 현재 장착 목록 기준 재적용 (복구·데이터 갱신용)
	void ReapplyAllEquipmentEffects();

	// 게임서버 장비 저장소(컨테이너 4)의 한 칸을 반영 — 칸 번호가 곧 부위다.
	// 칸 번호는 서버 부위 번호(Weapon=0...)라 내부에서 ENUEquipSlot으로 변환한다.
	// ItemId 0(또는 개수 0)이면 그 부위를 비운다. UNUInventoryComponent가 호출한다
	void ApplyServerEquipSlot(int32 ServerSlotNo, int32 ItemId, int32 ItemCount);

public:
	UPROPERTY(BlueprintAssignable, Category = "NU|Equip")
	FOnNUEquipmentChanged OnEquipmentChanged;

private:
	UNUInventoryComponent* GetInventory() const;
	UNUAbilitySystemComponent* GetNUASC() const;
	class UEmberServerManager* GetServerManager() const;

	FActiveGameplayEffectHandle ApplyEquipEffect(const FNUEquipStats& Stats);
	void RemoveEquipEffect(ENUEquipSlot Slot);
	void BroadcastChangedAndPush();

	UFUNCTION()
	void OnRep_EquippedItems();

	// 크기 고정 ENUEquipSlot::MAX(8) — 생성자에서 확정, 이후 SetNum 금지
	UPROPERTY(ReplicatedUsing = OnRep_EquippedItems)
	TArray<FNUEquippedItem> EquippedItems;

	// 슬롯별 장착 GE 핸들 — 서버 전용, 복제 안 함 (핸들은 서버 로컬 식별자)
	FActiveGameplayEffectHandle ActiveEffectHandles[static_cast<int32>(ENUEquipSlot::MAX)];
};
