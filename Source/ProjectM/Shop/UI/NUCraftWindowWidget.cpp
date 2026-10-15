#include "Shop/UI/NUCraftWindowWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Shop/NUCraftTypes.h"
#include "Shop/NUShopTypes.h"
#include "Shop/UI/NUCraftCategoryWidget.h"
#include "Shop/UI/NUCraftDetailWidget.h"
#include "Shop/UI/NUCraftEntryWidget.h"

void UNUCraftWindowWidget::Show(APlayerController* OwningPlayer, int32 InShopId)
{
	const APlayerState* PlayerState = OwningPlayer != nullptr ? OwningPlayer->PlayerState : nullptr;
	UNUInventoryComponent* InventoryComponent = PlayerState != nullptr
		? PlayerState->FindComponentByClass<UNUInventoryComponent>() : nullptr;
	if (InventoryComponent != nullptr)
	{
		// 창의 수명·ESC 스택은 인벤 컴포넌트가 쥔다 (상점 창과 같은 방식)
		InventoryComponent->OpenCraftUI(InShopId);
	}
}

void UNUCraftWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	DetailPanel->OnCraftRequested.BindUObject(this, &UNUCraftWindowWidget::HandleCraftRequested);
	DetailPanel->OnBackRequested.BindUObject(this, &UNUCraftWindowWidget::HandleDetailBack);
	DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UNUCraftWindowWidget::SetShop(int32 InShopId)
{
	if (ShopId != InShopId)
	{
		SelectedRecipeId = 0;
		CollapsedCategories.Reset();
	}
	ShopId = InShopId;
	RefreshData();
}

void UNUCraftWindowWidget::RefreshData()
{
	const FNUShopRow* Shop = ShopId > 0 ? NUShop::FindShopRow(ShopId) : nullptr;
	if (Shop == nullptr || !ResolveComponents())
	{
		return; // SetShop 전(창 생성 직후)이거나 인벤이 아직 없다
	}

	if (UTextBlock* Title = FindTextByName(TEXT("TitleText")))
	{
		Title->SetText(Shop->DisplayName);
	}
	if (UTextBlock* GoldText = FindTextByName(TEXT("GoldText")))
	{
		GoldText->SetText(FText::AsNumber(Inventory->GetGold()));
	}

	PopulateList();
	RefreshDetail();
}

FText UNUCraftWindowWidget::GetHardLockReason(const FNUCraftRecipeRow& Recipe) const
{
	const int32 MyLevel = GetMyLevel();
	if (MyLevel > 0 && MyLevel < Recipe.RequiredLevel)
	{
		return FText::Format(NSLOCTEXT("NUCraft", "RequiredLevel", "Lv. {0} 이상"), FText::AsNumber(Recipe.RequiredLevel));
	}
	if (Recipe.RequiredBlueprintId > 0 && Inventory.IsValid() && Inventory->CountItemsByNumericId(Recipe.RequiredBlueprintId) <= 0)
	{
		return NSLOCTEXT("NUCraft", "NoBlueprint", "도면이 없습니다.");
	}
	return FText::GetEmpty();
}

void UNUCraftWindowWidget::PopulateList()
{
	UPanelWidget* Panel = FindPanelByName(TEXT("ListBox"));
	if (Panel == nullptr || !Inventory.IsValid())
	{
		return;
	}

	// 다시 지으면 줄 위젯이 새것이라 패드 포커스가 사라진다 --- 포커스가 서 있던 줄(레시피)·머리줄(분류)을 기억했다 되돌린다
	int32 FocusedRecipeId = 0;
	FString FocusedCategoryKey;
	for (UWidget* Child : Panel->GetAllChildren())
	{
		if (Child->HasAnyUserFocus())
		{
			if (const UNUCraftEntryWidget* FocusedEntry = Cast<UNUCraftEntryWidget>(Child))
			{
				FocusedRecipeId = FocusedEntry->GetRecipeId();
			}
			else if (const UNUCraftCategoryWidget* FocusedHeader = Cast<UNUCraftCategoryWidget>(Child))
			{
				FocusedCategoryKey = FocusedHeader->GetKey();
			}
			break;
		}
	}
	HideFocusTooltip(); // 옛 줄에 붙은 툴팁

	Panel->ClearChildren();
	EntryByRecipe.Reset();

	if (EntryWidgetClass == nullptr || CategoryWidgetClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[제작] EntryWidgetClass/CategoryWidgetClass 미지정 --- 목록을 그릴 수 없습니다 (WBP_CraftWindow 기본값 확인)"));
		return;
	}

	// 분류별로 묶는다. 분류 순서는 표(SlotIndex)에서 처음 나온 순
	TArray<FString> Order;
	TMap<FString, TArray<const FNUCraftRecipeRow*>> ByCategory;
	for (const FNUCraftRecipeRow* Recipe : NUCraft::GetRecipes(ShopId))
	{
		if (Inventory->FindRowNameByNumericId(Recipe->ResultId).IsNone())
		{
			continue; // 표에 없는 결과물 --- 레시피가 낡았다
		}
		const FString Key = Recipe->Category.ToString();
		if (!ByCategory.Contains(Key))
		{
			Order.Add(Key);
		}
		ByCategory.FindOrAdd(Key).Add(Recipe);
	}

	for (const FString& Key : Order)
	{
		const TArray<const FNUCraftRecipeRow*>& Recipes = ByCategory[Key];
		const bool bExpanded = !CollapsedCategories.Contains(Key);

		// 분류 글자가 비어 있으면 머리줄 없이 줄만 늘어놓는다
		if (!Key.IsEmpty())
		{
			UNUCraftCategoryWidget* Header = CreateWidget<UNUCraftCategoryWidget>(this, CategoryWidgetClass);
			Header->SetCategory(Key, Recipes[0]->Category, Recipes.Num(), bExpanded);
			Header->OnToggled.BindUObject(this, &UNUCraftWindowWidget::HandleCategoryToggled);
			Panel->AddChild(Header);
			if (Key == FocusedCategoryKey)
			{
				Header->SetFocus();
			}
			if (!bExpanded)
			{
				continue;
			}
		}

		for (const FNUCraftRecipeRow* Recipe : Recipes)
		{
			const FName ResultRowName = Inventory->FindRowNameByNumericId(Recipe->ResultId);
			const FNUItemDefRow* Def = Inventory->FindItemDef(ResultRowName);

			UNUCraftEntryWidget* Entry = CreateWidget<UNUCraftEntryWidget>(this, EntryWidgetClass);
			Entry->SetRecipe(Recipe->RecipeId,
				Def != nullptr ? Def->DisplayName : FText::FromName(ResultRowName),
				Recipe->ResultCount,
				LoadItemIconTexture(ResultRowName),
				BuildMaterialText(*Recipe),
				Recipe->Gold,
				GetHardLockReason(*Recipe));
			Entry->SetSelected(Recipe->RecipeId == SelectedRecipeId);
			Entry->OnSelected.BindUObject(this, &UNUCraftWindowWidget::HandleEntrySelected);
			Entry->OnActivated.BindUObject(this, &UNUCraftWindowWidget::HandleEntryActivated);
			Entry->OnEntryFocusChanged.BindUObject(this, &UNUCraftWindowWidget::HandleEntryFocusChanged, ResultRowName);

			// 결과물 툴팁은 인벤 칸과 같은 내용 (무엇이 나오는지 스탯까지 보여준다)
			if (UWidget* Tooltip = BuildItemTooltip(ResultRowName))
			{
				Entry->SetToolTip(Tooltip);
			}
			Panel->AddChild(Entry);
			EntryByRecipe.Add(Recipe->RecipeId, Entry);
			if (Recipe->RecipeId == FocusedRecipeId)
			{
				Entry->SetFocus();
			}
		}
	}

	// 목록 끝에서 아래로 가면 창 공용 규칙(그 방향 가장 가까운 조작부)이 오른쪽 아래 [만들기]로 넘겼다 --- 끝에서는 제자리
	if (const int32 Count = Panel->GetChildrenCount(); Count > 0)
	{
		Panel->GetChildAt(Count - 1)->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
	}
}

FText UNUCraftWindowWidget::BuildMaterialText(const FNUCraftRecipeRow& Recipe) const
{
	if (!Inventory.IsValid())
	{
		return FText::GetEmpty();
	}

	TArray<FText> Parts;
	for (const FNUCraftMaterial& Material : Recipe.GetMaterials())
	{
		const FName RowName = Inventory->FindRowNameByNumericId(Material.RefId);
		const FNUItemDefRow* Def = RowName.IsNone() ? nullptr : Inventory->FindItemDef(RowName);
		// 가진 수를 같이 보여야 무엇이 모자란지 바로 안다
		Parts.Add(FText::Format(NSLOCTEXT("NUCraft", "MaterialPart", "{0} {1}/{2}"),
			Def != nullptr ? Def->DisplayName : FText::FromName(RowName),
			FText::AsNumber(Inventory->CountItemsByNumericId(Material.RefId)),
			FText::AsNumber(Material.Count)));
	}

	if (Recipe.RequiredBlueprintId > 0)
	{
		const FName BlueprintRowName = Inventory->FindRowNameByNumericId(Recipe.RequiredBlueprintId);
		const FNUItemDefRow* Def = BlueprintRowName.IsNone() ? nullptr : Inventory->FindItemDef(BlueprintRowName);
		// 도면은 소모하지 않는다 --- 가지고만 있으면 된다
		Parts.Add(FText::Format(NSLOCTEXT("NUCraft", "MaterialBlueprint", "{0} 필요"),
			Def != nullptr ? Def->DisplayName : FText::FromName(BlueprintRowName)));
	}

	return FText::Join(NSLOCTEXT("NUCraft", "MaterialSeparator", " · "), Parts);
}

void UNUCraftWindowWidget::RefreshDetail()
{
	const FNUCraftRecipeRow* Recipe = SelectedRecipeId > 0 ? NUCraft::FindRecipe(SelectedRecipeId) : nullptr;
	if (Recipe == nullptr || !Inventory.IsValid())
	{
		SelectedRecipeId = 0;
		DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 상세 패널이 쓸 값은 여기서 인벤을 보고 다 계산해 넘긴다 --- 패널은 인벤을 모른다
	FNUCraftDetailData Data;
	Data.RecipeId = Recipe->RecipeId;
	const FName ResultRowName = Inventory->FindRowNameByNumericId(Recipe->ResultId);
	const FNUItemDefRow* ResultDef = Inventory->FindItemDef(ResultRowName);
	Data.ResultName = ResultDef != nullptr ? ResultDef->DisplayName : FText::FromName(ResultRowName);
	Data.ResultIcon = LoadItemIconTexture(ResultRowName);
	Data.ResultCount = Recipe->ResultCount;
	Data.GoldPerCraft = Recipe->Gold;
	Data.OwnedGold = Inventory->GetGold();
	Data.MaxCount = Inventory->GetMaxCraftCount(*Recipe);

	for (const FNUCraftMaterial& Material : Recipe->GetMaterials())
	{
		const FName RowName = Inventory->FindRowNameByNumericId(Material.RefId);
		const FNUItemDefRow* Def = RowName.IsNone() ? nullptr : Inventory->FindItemDef(RowName);
		FNUCraftMaterialView& View = Data.Materials.AddDefaulted_GetRef();
		View.Name = Def != nullptr ? Def->DisplayName : FText::FromName(RowName);
		View.Icon = LoadItemIconTexture(RowName);
		View.Have = Inventory->CountItemsByNumericId(Material.RefId);
		View.Need = Material.Count;
	}

	// 레벨·도면은 개수와 무관하게 막고, 그게 아니면 한 번도 못 만드는 이유(재료·골드)를 보여준다
	Data.Reason = GetHardLockReason(*Recipe);
	if (Data.Reason.IsEmpty() && Data.MaxCount <= 0)
	{
		Inventory->CanCraft(*Recipe, 1, Data.Reason);
	}

	DetailPanel->SetData(Data);
	DetailPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UNUCraftWindowWidget::HandleCategoryToggled(const FString& CategoryKey)
{
	if (CollapsedCategories.Contains(CategoryKey))
	{
		CollapsedCategories.Remove(CategoryKey);
	}
	else
	{
		CollapsedCategories.Add(CategoryKey);
	}
	PopulateList();
}

void UNUCraftWindowWidget::HandleEntrySelected(int32 RecipeId)
{
	if (SelectedRecipeId == RecipeId)
	{
		return;
	}
	// 고른 줄 표시만 바꾼다 --- 목록을 다시 그리면 스크롤이 튄다
	if (TObjectPtr<UNUCraftEntryWidget>* Previous = EntryByRecipe.Find(SelectedRecipeId))
	{
		(*Previous)->SetSelected(false);
	}
	SelectedRecipeId = RecipeId;
	if (TObjectPtr<UNUCraftEntryWidget>* Current = EntryByRecipe.Find(SelectedRecipeId))
	{
		(*Current)->SetSelected(true);
	}
	RefreshDetail();
}

void UNUCraftWindowWidget::HandleCraftRequested(int32 RecipeId, int32 Times)
{
	const FNUCraftRecipeRow* Recipe = NUCraft::FindRecipe(RecipeId);
	if (Recipe == nullptr || !Inventory.IsValid())
	{
		return;
	}
	// 막힌 이유는 컴포넌트가 배너로 알린다. 성공하면 인벤 변경 통지로 목록과 상세가 다시 그려진다
	Inventory->CraftRecipe(*Recipe, Times);
}

void UNUCraftWindowWidget::HandleEntryFocusChanged(UNUCraftEntryWidget* Entry, bool bFocused, FName ResultRowName)
{
	HideFocusTooltip();
	if (bFocused)
	{
		// 줄에 서기만 해도 고른다 --- 훑어보는 동안 오른쪽 상세가 따라 바뀐다
		HandleEntrySelected(Entry->GetRecipeId());
		// 오른쪽은 상세 패널이라 툴팁은 창 왼쪽에 띄운다
		ShowFocusTooltip(Entry, ResultRowName, true);
	}
}

void UNUCraftWindowWidget::HandleEntryActivated(int32 RecipeId)
{
	HandleEntrySelected(RecipeId);
	DetailPanel->FocusCraftButton();
}

void UNUCraftWindowWidget::HandleDetailBack()
{
	if (TObjectPtr<UNUCraftEntryWidget>* Selected = EntryByRecipe.Find(SelectedRecipeId))
	{
		(*Selected)->SetFocus();
	}
}

void UNUCraftWindowWidget::FocusForPad()
{
	if (TObjectPtr<UNUCraftEntryWidget>* Selected = EntryByRecipe.Find(SelectedRecipeId))
	{
		(*Selected)->SetFocus();
		return;
	}
	if (const UPanelWidget* Panel = FindPanelByName(TEXT("ListBox")))
	{
		for (UWidget* Child : Panel->GetAllChildren())
		{
			if (UNUCraftEntryWidget* Entry = Cast<UNUCraftEntryWidget>(Child))
			{
				Entry->SetFocus();
				return;
			}
		}
	}
	Super::FocusForPad(); // 빈 목록 --- 닫기 버튼으로
}

void UNUCraftWindowWidget::CloseSelf()
{
	if (Inventory.IsValid())
	{
		Inventory->CloseCraftUI();
	}
}
