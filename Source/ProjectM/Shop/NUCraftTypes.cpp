#include "Shop/NUCraftTypes.h"

TArray<FNUCraftMaterial> FNUCraftRecipeRow::GetMaterials() const
{
	TArray<FNUCraftMaterial> Result;
	for (const FNUCraftMaterial& Material : { Material1, Material2, Material3, Material4 })
	{
		if (Material.IsValid())
		{
			Result.Add(Material);
		}
	}
	return Result;
}

namespace NUCraft
{

static const UDataTable* GetRecipeTable()
{
	return GetDefault<UNUShopSettings>()->CraftRecipeTable.LoadSynchronous();
}

const FNUCraftRecipeRow* FindRecipe(int32 RecipeId)
{
	const UDataTable* Table = GetRecipeTable();
	if (Table == nullptr || RecipeId <= 0)
	{
		return nullptr;
	}
	// 행 이름은 RecipeId 숫자 문자열 (상점 표와 같은 규약)
	return Table->FindRow<FNUCraftRecipeRow>(FName(*FString::FromInt(RecipeId)), TEXT("NUCraft::FindRecipe"), false);
}

TArray<const FNUCraftRecipeRow*> GetRecipes(int32 ShopId)
{
	TArray<const FNUCraftRecipeRow*> Result;
	const UDataTable* Table = GetRecipeTable();
	if (Table == nullptr || ShopId <= 0)
	{
		return Result;
	}

	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FNUCraftRecipeRow* Recipe = reinterpret_cast<const FNUCraftRecipeRow*>(Row.Value);
		if (Recipe->ShopId == ShopId && Recipe->ResultId > 0)
		{
			Result.Add(Recipe);
		}
	}

	Result.Sort([](const FNUCraftRecipeRow& A, const FNUCraftRecipeRow& B)
	{
		return A.SlotIndex != B.SlotIndex ? A.SlotIndex < B.SlotIndex : A.RecipeId < B.RecipeId;
	});
	return Result;
}

} // namespace NUCraft
