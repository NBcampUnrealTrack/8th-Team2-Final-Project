// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Inventory/NUEquipmentTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Templates/SubclassOf.h"
#include "NUInventoryTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNUInventory, Log, All);

class UGameplayEffect;
class UNUInventoryComponent;

namespace NUInventory
{
	// 서버 ResultCode를 사용자에게 보일 문구로 바꾼다 (NUParty::GetResultMessage 선례).
	// 표의 원본은 리버브 "gRPC 연동 가이드 (클라이언트용)" 결과 코드표다 --- 값을 여기서 지어내지 말 것
	PROJECTM_API FText GetResultMessage(int32 ResultCode);
}

// 소모품 효과 종류 --- 기획 시트 "아이템 데이터 테이블"의 EItemEffectType 탭과 값이 같다 (Effect1_Type / Effect2_Type 컬럼).
// 값의 뜻은 Effect_Value 가 정한다: 회복량 / 해제할 상태이상 ID (0이면 전부) / 포획 확률 등. IsPercent 면 최대치 대비 %
UENUM(BlueprintType)
enum class ENUItemEffectType : uint8
{
	None = 0,
	HealHP = 1,
	HealSP = 2,
	CureStatus = 3,
	Revive = 4,
	Damage = 5,
	Affection = 6,
	Capture = 7
};

// 저장소 정렬 기준 (UNUInventoryComponent::SortContainer). 가방에 분류 구간이 있으면 구간 안에서만 순서가 바뀐다
UENUM(BlueprintType)
enum class ENUItemSortMode : uint8
{
	Kind,	// 종류 => 아이템 번호 => 개수 많은 순
	Grade	// 등급 높은 순 (등급 없는 것은 뒤) => 종류 => 아이템 번호 => 개수 많은 순
};

// 아이템 대분류 (아이템/인벤토리/장비)
UENUM(BlueprintType)
enum class ENUItemType : uint8
{
	Consumable,	// 소모품 — 사용 시 GE 적용
	Equipment, // 장비 (장착은 추후)
	Quest, // 퀘스트 아이템
	Commune, // 교감(포획) 아이템
	Material, // 생산 재료
	Blueprint, // 제작 도면 — 영구 해금이라 팔면 재구매 악용이 되므로 판매 불가 (기획 5절)
	KeyItem, // 열쇠 아이템 (증표·배정권 등) — 퀘스트템처럼 버리기·판매 불가
	Keepsake // 기념품 --- 스탯도 기능도 없는 서사적 소지품 (기획서 분류 "기념품/증표"). 버리기/판매 규칙은 기획 확인 전까지 열쇠 아이템과 같게 둔다
};

namespace NUItem
{
	// 버리기 가능 여부 — 퀘스트템·열쇠템은 잃어버리면 진행이 막힌다. 기념품은 기획 확인 전까지 같이 막는다
	inline bool IsDroppable(ENUItemType Type)
	{
		return Type != ENUItemType::Quest && Type != ENUItemType::KeyItem && Type != ENUItemType::Keepsake;
	}

	// 상점 판매 가능 여부 — 버리기 불가 품목에 더해 도면도 막는다 (판매 후 재구매 악용 방지)
	inline bool IsSellable(ENUItemType Type)
	{
		return IsDroppable(Type) && Type != ENUItemType::Blueprint;
	}
}

// 아이템 정적 정의. RowName이 아이템 ID (FUnitStatsRow/FSkillRow 선례)
USTRUCT(BlueprintType)
struct FNUItemDefRow : public FTableRowBase
{
	GENERATED_BODY()

	// 기획 테이블의 Index — 서버/기획 참조용 숫자 ID. RowName(Item_ID)이 실제 키
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	int32 NumericId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	ENUItemType ItemType = ENUItemType::Consumable;

	// GAS 연동/이벤트 페이로드용 식별 태그 (NU.Item.*)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item", meta = (ClampMin = 1))
	int32 MaxStack = 99;

	// 사용 시 적용할 GE. null이면 Consumable 한정 UNUGE_ItemHeal 폴백.
	// 커스텀 GE도 SetByCaller 키는 NU.Data.Item.Heal 을 사용해야 Effect1_Value가 전달된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	TSubclassOf<UGameplayEffect> UseEffect;

	// 효과 2칸 --- 기획 시트 컬럼 그대로 (서버도 같은 컬럼을 읽는다). 값의 뜻은 ENUItemEffectType 참고.
	// Effect1_Value 는 SetByCaller(NU.Data.Item.Heal)로 GE에 들어가고, 교감석이면 포획 확률이다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	ENUItemEffectType Effect1_Type = ENUItemEffectType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	int32 Effect1_Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	bool Effect1_IsPercent = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	ENUItemEffectType Effect2_Type = ENUItemEffectType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	int32 Effect2_Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	bool Effect2_IsPercent = false;

	// 장착 시 캐릭터 위에 겹쳐 그릴 외형 스킨 (PaperZD AnimationSkin — 장비 레이어에 적용).
	// 비우면 장착해도 외형 변화 없음. 시퀀스별 장비 플립북 매핑은 스킨 에셋이 가진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	TSoftObjectPtr<class UPaperZDAnimationSkin> EquipSkin;

	// 팔보다 앞에 그려야 하는 부분을 따로 받는 보조 스킨 (갑옷의 어깨 등).
	// 한 레이어는 깊이가 하나뿐이라, 팔 뒤로 갈 몸통과 팔 앞으로 나올 어깨를 나눠 그린다.
	// 비우면 그 부위는 레이어 하나로만 그려진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	TSoftObjectPtr<class UPaperZDAnimationSkin> EquipSkinOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	bool bUsableInField = true;

	// 전투 커맨드(ENUBattleCommandType::Item) 연동 시 CanUseInBattle 판정에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	bool bUsableInBattle = false;

	// 전투에서 아이템 사용 시 타겟 선택 정책 (포션·치료제=아군, 교감석=적)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	ENUTargetPolicy BattleTargetPolicy = ENUTargetPolicy::SingleAlly;

	// ItemType == Equipment 전용: 장착 부위 (None이면 장착 불가)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item|Equip")
	ENUEquipType EquipType = ENUEquipType::None;

	// ItemType == Equipment 전용: 장착 시 부여 스탯
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item|Equip")
	FNUEquipStats EquipStats;

	// 등급 (시트 Grade를 변환 스크립트가 enum 이름으로 바꾼다). 표시용 --- 장비 수치는 이미 EquipStats에 반영돼 있다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item")
	ENUItemGrade Grade = ENUItemGrade::None;

	// ItemType == Equipment 전용: 장착 요구 레벨 (시트 RequiredLevel).
	// 판정은 서버가 한다(1308 거절) --- 클라는 툴팁 표시와 사전 안내에만 쓴다. 0이면 제한 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item|Equip")
	int32 RequiredLevel = 0;

	// ItemType == Equipment 전용: 직업 전용 표시 문구 (시트 JobRestrict의 한국어, All이면 빈 값).
	// 장비 설명문을 툴팁에서 뺐으므로 무기의 직업 정보는 이 줄이 대신한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item|Equip")
	FText RequiredJobText;

	// ItemType == Equipment 전용: 전용 직업 번호 (직업 표 JobId, 시트 JobRestrict 키를 변환 스크립트가 번호로 바꾼다. 0 = 제한 없음).
	// 서버는 직업 제한을 모르므로 판정은 클라가 한다 --- 다른 직업이면 착용을 안 보내고 툴팁을 붉게 (9/14 유민)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Item|Equip")
	int32 RequiredJobId = 0;
};

// 가방(저장소 1)의 분류 --- 통합 메뉴 인벤토리의 탭 순서이자 서버에 요청한 칸 구간의 순서 (9/30 회의 "탭별 상한")
namespace NUBagCategory
{
	constexpr int32 Equipment = 0;
	constexpr int32 Consumable = 1;
	constexpr int32 Material = 2;
	constexpr int32 Other = 3;
	constexpr int32 Num = 4;
}

namespace NUItem
{
	// 아이템이 드는 가방 분류. 교감석은 쓰는 물건이라 소모품에, 도면은 제작에 쓰여 재료에 넣는다. 표에 없는 아이템은 기타
	inline int32 GetBagCategory(const FNUItemDefRow* Def)
	{
		if (Def == nullptr)
		{
			return NUBagCategory::Other;
		}
		switch (Def->ItemType)
		{
		case ENUItemType::Equipment:  return NUBagCategory::Equipment;
		case ENUItemType::Consumable:
		case ENUItemType::Commune:    return NUBagCategory::Consumable;
		case ENUItemType::Material:
		case ENUItemType::Blueprint:  return NUBagCategory::Material;
		default:                      return NUBagCategory::Other;
		}
	}
}

// 분류 하나가 쓰는 가방 칸 번호 [Start, Start + Count)
struct FNUBagSection
{
	int32 Start = 0;
	int32 Count = 0;

	bool Contains(int32 SlotNo) const { return SlotNo >= Start && SlotNo < Start + Count; }
};

// 인벤토리 한 칸(스택). InstanceId는 전투 커맨드(FNUBattleCommandRequest.InstanceId)와 공유하는 식별자.
// 서버 저장 계약은 칸 단위 (container, slot_no, item_id, item_count) — InstanceId는 클라 로컬 식별자다
USTRUCT(BlueprintType)
struct FNUItemInstance : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Item")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Item")
	FName ItemRowName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Item")
	int32 Count = 0;

	// 그리드 칸 번호 (0부터, 서버 slot_no와 같은 체계). INDEX_NONE = 미배정
	UPROPERTY(BlueprintReadOnly, Category = "NU|Item")
	int32 SlotNo = INDEX_NONE;
};

// 소유자 전용(COND_OwnerOnly)으로 복제되는 인벤토리 리스트 (FastArray 델타 직렬화)
USTRUCT()
struct FNUInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FNUItemInstance> Items;

	// 수신 콜백을 컴포넌트로 전달하는 백포인터 (컴포넌트 생성자에서 세팅, 복제 대상 아님)
	UPROPERTY(NotReplicated)
	TObjectPtr<UNUInventoryComponent> OwnerComponent;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FastArrayDeltaSerialize<FNUItemInstance, FNUInventoryList>(Items, DeltaParms, *this);
	}

	// 항목별 콜백은 같은 배치의 다른 변경이 미적용 상태일 수 있어 로그 전용.
	// 외부 알림은 배치당 1회 불리는 PostReplicatedReceive에서만 한다.
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template<>
struct TStructOpsTypeTraits<FNUInventoryList> : public TStructOpsTypeTraitsBase2<FNUInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};
