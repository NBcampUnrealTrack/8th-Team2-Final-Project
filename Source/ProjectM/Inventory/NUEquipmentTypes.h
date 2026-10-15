// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUEquipmentTypes.generated.h"

// 아이템 정의의 장비 부위 (FNUItemDefRow.EquipType) — 기획 "장비 종류" 9종 (8/20 하의 추가 반영)
// 방패는 부위가 아니라 무기 1종으로 묶인다.
// 반지는 자유 2칸이 아니라 반지 1/반지 2 전용 아이템이 따로 있다 (기획 테이블의 Ring1/Ring2 슬롯 컬럼)
UENUM(BlueprintType)
enum class ENUEquipType : uint8
{
	None,
	Head,		// 투구
	Chest,		// 갑옷
	Gloves,		// 장갑
	Boots,		// 신발
	Weapon,		// 무기
	Ring1,		// 반지 1
	Ring2,		// 반지 2
	Earring,	// 귀걸이
	Legs		// 하의 (8/20 기획 추가 --- 기존 번호가 밀리지 않게 맨 뒤에 붙인다)
};

// 아이템 등급 (FNUItemDefRow.Grade) --- 기획 장비 시트·아이템 시트의 Grade 5종.
// 장비는 등급이 옵션 수치의 배율과 요구 레벨을 정한다 (일반 1 / 고급 20 / 희귀 35 / 영웅 50 / 전설 60).
// 장비가 아닌 아이템은 표시에만 쓴다. 표에 등급이 없는 행과 개발용 더미는 None
UENUM(BlueprintType)
enum class ENUItemGrade : uint8
{
	None,
	Common,		// 일반
	Uncommon,	// 고급
	Rare,		// 희귀
	Heroic,		// 영웅
	Legendary	// 전설
};

// 장착 슬롯 — 부위당 1칸. 컴포넌트 배열 인덱스와 1:1이므로 값 순서 변경 금지
UENUM(BlueprintType)
enum class ENUEquipSlot : uint8
{
	Head,
	Chest,
	Gloves,
	Boots,
	Weapon,
	Ring1,
	Ring2,
	Earring,
	Legs,
	MAX UMETA(Hidden)
};

// 장비 스탯 블록. GE 적용 시 8필드 전부 SetByCaller로 주입.
USTRUCT(BlueprintType)
struct FNUEquipStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float AttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float MagicAttackPower = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float Defense = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float Speed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float CriticalRate = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float CriticalDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float MaxHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float MaxMana = 0.f;

	// 아래 둘은 대응 어트리뷰트가 아직 없다 (진영 님 GAS 협의 대기) --- 지금은 툴팁 표시 전용이고
	// UNUGE_EquipmentStats가 읽지 않으므로 실제 전투 수치에는 반영되지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float DamageReduce = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Equip")
	float HealBonus = 0.f;
};

// 장착 항목 (배열 인덱스 = ENUEquipSlot). 빈 슬롯 = !InstanceId.IsValid()
USTRUCT(BlueprintType)
struct FNUEquippedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Equip")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Equip")
	FName ItemRowName = NAME_None;
};

namespace NUEquip
{
	// 부위 타입 => 장착 대상 슬롯. 장착 불가 타입(None 등)은 ENUEquipSlot::MAX 반환
	PROJECTM_API ENUEquipSlot ResolveTargetSlot(ENUEquipType Type, const TArray<FNUEquippedItem>& Equipped);

	// 게임서버 부위 번호 <=> 클라 슬롯. 서버는 Weapon=0, Helmet=1, Armor=2, Gloves=3,
	// Boots=4, Ring1=5, Ring2=6, Earring=7 (gRPC_help §4.7) — 클라 enum 순서와 다르다.
	// 패킷 경계에서만 변환하고 내부는 전부 ENUEquipSlot을 쓴다
	PROJECTM_API int32 ToServerEquipSlot(ENUEquipSlot Slot);           // 없으면 INDEX_NONE
	PROJECTM_API ENUEquipSlot FromServerEquipSlot(int32 ServerSlot);  // 없으면 ENUEquipSlot::MAX

	// UI 표시용 슬롯 이름 (임시 UI용)
	PROJECTM_API FText GetSlotDisplayName(ENUEquipSlot Slot);

	// UI 표시용 부위 이름 (툴팁 — 아이템 정의의 EquipType 기준)
	PROJECTM_API FText GetEquipTypeDisplayName(ENUEquipType Type);

	// UI 표시용 등급 이름 (None이면 빈 글자)
	PROJECTM_API FText GetGradeDisplayName(ENUItemGrade Grade);
}
