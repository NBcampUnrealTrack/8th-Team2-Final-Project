// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "NUShopTypes.generated.h"

struct FNUItemDefRow;

// 상점 시스템 값 타입 --- 기획 시트(상점 / 상점 품목)와 컬럼을 1:1로 맞춘다. CSV는 Scripts/ConvertShopTable.ps1이 시트에서 굽는다.
// 서버 인벤이 켜져 있으면 ShopBuy(180)/ShopSell(181)로 보내고 서버가 품목표로 금액을 계산·검증한다 (gRPC 가이드 §17, 9/7). 서버 없이 PIE면 로컬 더미

// 상점 유형 (기획 표 ShopType 컬럼과 1:1). 판매 탭은 일반 상점에만 있다 (기획 5절)
UENUM(BlueprintType)
enum class ENUShopType : uint8
{
	Shop,      // 일반 상점 --- 구매 + 판매
	Workshop,  // 공방 --- 제작·수리·강화 (별도 시스템, 1차 미구현)
	Facility,  // 시설 --- 창고·숙박·회복 (별도 시스템, 1차 미구현)
};

// 상점 1곳. 팀 CSV(Scripts/Data/ShopTable.csv)와 컬럼 1:1 --- 표가 갱신되면 재임포트만 한다.
// RowName은 CSV 규약대로 ShopId 숫자 문자열("8003")이다
USTRUCT(BlueprintType)
struct FNUShopRow : public FTableRowBase
{
	GENERATED_BODY()

	// 기획 표 Shop_ID --- 품목 표·NPC 표가 이 번호로 상점을 지목하고, 서버 패킷이 생기면 통신 키가 된다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 ShopId = 0;

	// 기획 키 (ST_SHOP_LI_GEN 등) --- 사람이 표를 대조할 때 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	FName ShopKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	FText DisplayName;

	// 지역 번호 (0 리베르그로브, 1 신데하스) --- 컬럼 대응, 아직 소비처 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 AreaId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	ENUShopType ShopType = ENUShopType::Shop;

	// 이 상점을 여는 NPC (기획 NPC_ID). NPC 상호작용 연결에 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 NpcId = 0;

	// 위치 --- 컬럼 대응, 표가 채워지면 NPC 배치 쪽에서 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 LocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	FString LocationName;

	// 지역 물가 (리베르그로브 1.0, 신데하스 1.2). 품목표 Price에 이미 곱해져 있어 런타임에서는 다시 곱하지 않는다 (9/6 민성 님 확인) --- 표 대조용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	float PriceMultiplier = 1.0f;

	// 매각 환율 (기획 0.25) --- 0이면 이 상점에서는 팔 수 없다 (시트 빈칸을 폴백으로 가리지 않는다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	float BuybackRate = 0.0f;

	// 해금 퀘스트 (0 = 항상 열림). 1차에서는 검사하지 않는다 --- 퀘스트 연동 때 붙인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 UnlockQuestId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	FString Description;
};

// 품목이 가리키는 표 (기획 품목 표 RefType 컬럼과 1:1)
UENUM(BlueprintType)
enum class ENUShopRefType : uint8
{
	Item,   // 아이템 표(DT_ItemTable)의 NumericId
	Equip,  // 장비 표(DT_EquipTable)의 NumericId
};

// 상점 품목 1줄. 팀 CSV(Scripts/Data/ShopItemTable.csv)와 컬럼 1:1. RowName은 ShopItemId 숫자 문자열("13001").
// 아이템 표에 가격이 없어(9/4 결정) 되팔기 기준가로도 이 표를 쓴다 (9/6 확정)
USTRUCT(BlueprintType)
struct FNUShopItemRow : public FTableRowBase
{
	GENERATED_BODY()

	// 기획 표 ShopItem_ID --- 목록 버튼 페이로드·수량 창 콜백이 이 번호로 행을 다시 찾는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 ShopItemId = 0;

	// 어느 상점 품목인지 (FNUShopRow.ShopId)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 ShopId = 0;

	// 상점 안 진열 순서 (1부터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 SlotIndex = 0;

	// RefId가 어느 표 번호인지 --- 컬럼 대응. 조회는 NumericId가 두 표에 걸쳐 유일해 구분 없이 한다 (FindRowNameByNumericId)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	ENUShopRefType RefType = ENUShopRefType::Item;

	// 파는 아이템·장비의 NumericId --- 서버 ShopBuy의 item_id와 같은 번호
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 RefId = 0;

	// 사람이 표를 대조할 때 쓴다 --- 화면에는 아이템 표 DisplayName을 쓴다 (인벤·툴팁과 같은 이름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	FString DisplayName;

	// 구매가 그대로 --- 기획서 9-3 "최종 판매가 (배율 반영 완료)": 등급계수와 상점 PriceMultiplier까지 민성 님이 표에 반영한다 (9/6 확인).
	// 매각가 = 이 값 x 상점 BuybackRate (NUShop::GetSellPrice)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 Price = 0;

	// 재고 (-1 무한). 차감은 서버 몫 --- 클라는 표시만 한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 Stock = -1;

	// 구매 가능 최소 레벨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 RequiredLevel = 1;

	// 해금 퀘스트 (0 = 항상). 1차에서는 검사하지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	int32 UnlockQuestId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop")
	FString Description;
};

// 상점 데이터 지정 (프로젝트 세팅 > 게임 > NU Shop)
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "NU Shop"))
class PROJECTM_API UNUShopSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 상점 목록 (FNUShopRow)
	UPROPERTY(config, EditAnywhere, Category = "NU|Shop")
	TSoftObjectPtr<UDataTable> ShopTable;

	// 상점별 품목 (FNUShopItemRow)
	UPROPERTY(config, EditAnywhere, Category = "NU|Shop")
	TSoftObjectPtr<UDataTable> ShopItemTable;

	// 상점 창 WBP (UNUShopWindowWidget 파생). PlayerState BP마다 지정하지 않고 여기 하나로 둔다 ---
	// 공용 테스트 맵(/Game/Maps)처럼 남의 PlayerState를 쓰는 곳에서도 같은 창이 뜬다 (거래 창 TradeWindowClass와 같은 방식)
	UPROPERTY(config, EditAnywhere, Category = "NU|Shop")
	TSoftClassPtr<class UNUShopWindowWidget> ShopWindowClass;

	// 제작(공방) 레시피 표 (FNUCraftRecipeRow). 기획 표가 오기 전까지는 임시 데이터 ---
	// 구조는 상점 품목표와 같은 RefType/RefId 방식이라 표만 갈아끼우면 된다
	UPROPERTY(config, EditAnywhere, Category = "NU|Shop")
	TSoftObjectPtr<UDataTable> CraftRecipeTable;

	// 제작 창 WBP (UNUCraftWindowWidget 파생). 공방 선택지에서 연다
	UPROPERTY(config, EditAnywhere, Category = "NU|Shop")
	TSoftClassPtr<class UNUCraftWindowWidget> CraftWindowClass;
};

namespace NUShop
{
	// 개당 매각가 = 품목 표 Price x 상점 BuybackRate, 소수점 버림 (기획서 7-1 "구매가 x 되팔기율"). 구매가는 함수 없이 FNUShopItemRow::Price 그대로다.
	// 기준 행: 이 상점이 그 아이템(NumericId)을 팔면 그 행, 아니면 가장 싼 행(= 지역 물가 1.0의 원본 가격, 기획서 9-4).
	// 어느 상점 품목 표에도 없는 아이템은 0 = 팔 수 없음 (서버도 같은 규칙으로 막아 달라고 요청)
	PROJECTM_API int32 GetSellPrice(const FNUItemDefRow& Def, const FNUShopRow& Shop);

	// 설정 테이블에서 상점 행을 찾는다 (없으면 nullptr). RowName = ShopId 숫자 문자열 (CSV 규약)
	PROJECTM_API const FNUShopRow* FindShopRow(int32 ShopId);

	// 품목 행 (없으면 nullptr). RowName = ShopItemId 숫자 문자열
	PROJECTM_API const FNUShopItemRow* FindShopItemRow(int32 ShopItemId);

	// 그 상점이 파는 품목들, SlotIndex 순. 포인터는 품목 테이블의 행이라 그 자리에서 바로 쓴다
	PROJECTM_API TArray<const FNUShopItemRow*> GetShopItems(int32 ShopId);
}
