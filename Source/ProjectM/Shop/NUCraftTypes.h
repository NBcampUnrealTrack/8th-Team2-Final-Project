// 제작(공방) 표 --- 기획서 「상점 시스템」 4·6절의 공방을 클라에서 먼저 돌려보기 위한 구조.
// 레시피 수치는 아직 기획이 없어 임시다(민성 님 표가 오면 CSV만 갈아끼운다).
// 컬럼은 상점 품목표와 같은 방식이다: 결과물·재료를 RefType + RefId(NumericId)로 가리킨다.
//
// 서버 패킷이 아직 없어 제작은 로컬에서만 돈다 --- 상점 1차 때와 같은 자리(bServerInventoryActive)를 쓴다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shop/NUShopTypes.h" // ENUShopRefType
#include "NUCraftTypes.generated.h"

// 레시피 재료 한 줄. 표에서는 Mat1_Type/Mat1_Id/Mat1_Count 처럼 펴서 적는다
USTRUCT(BlueprintType)
struct FNUCraftMaterial
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	ENUShopRefType RefType = ENUShopRefType::Item;

	// 재료의 NumericId (아이템 6xxx / 장비 7xxx)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 RefId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 Count = 0;

	bool IsValid() const { return RefId > 0 && Count > 0; }
};

// 레시피 한 줄. 행 이름은 RecipeId 문자열
USTRUCT(BlueprintType)
struct FNUCraftRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	// 기본키. 상점 품목표(13xxx)와 겹치지 않게 14001부터 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 RecipeId = 0;

	// 어느 공방인지 (FNUShopRow.ShopId --- 8002 정비공 / 8006 조제 / 8007 조리 / 8008 직조 / 8009 목공)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 ShopId = 0;

	// 목록 순서 (1부터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 SlotIndex = 0;

	// 결과물
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	ENUShopRefType ResultType = ENUShopRefType::Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 ResultId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 ResultCount = 1;

	// 사람이 표를 대조할 때 쓴다 --- 화면에는 아이템 표 DisplayName을 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	FString DisplayName;

	// 창에서 묶어 보여줄 분류 ("낡은 세트", "체력 포션"). 같은 글자끼리 접었다 펼 수 있는 한 묶음이 된다. 비우면 묶음 없이 나열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	FText Category;

	// 재료 (최대 4). 빈 칸은 RefId 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	FNUCraftMaterial Material1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	FNUCraftMaterial Material2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	FNUCraftMaterial Material3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	FNUCraftMaterial Material4;

	// 제작 수수료 (골드). 0이면 무료
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 Gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 RequiredLevel = 1;

	// 이 도면을 가지고 있어야 목록에 뜬다 (0 = 항상). 기획서 8절 "영구 해금"은 서버 몫이라 지금은 소지 검사만
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	int32 RequiredBlueprintId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft")
	FString Description;

	// 빈 칸을 걸러 실제 재료만 돌려준다
	TArray<FNUCraftMaterial> GetMaterials() const;
};

namespace NUCraft
{
	// 프로젝트 세팅(NU Shop)의 레시피 표에서 찾는다. 없으면 nullptr
	PROJECTM_API const FNUCraftRecipeRow* FindRecipe(int32 RecipeId);

	// 그 공방의 레시피를 SlotIndex 순으로 (표가 없으면 빈 배열)
	PROJECTM_API TArray<const FNUCraftRecipeRow*> GetRecipes(int32 ShopId);
}
