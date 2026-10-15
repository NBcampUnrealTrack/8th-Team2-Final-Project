// 제작 창 --- 공방 NPC의 [제작] 선택지가 연다. 레시피를 분류(표의 Category)별로 묶어 접었다 펼치고,
// 줄을 고르면 오른쪽 상세 패널(UNUCraftDetailWidget)이 펼쳐진다. 개수를 정해 [만들기]를 누르면 인벤 컴포넌트가 로컬에서 만든다.
// 서버 제작 패킷이 아직 없고 레시피 수치도 임시다 --- 표만 갈아끼우면 이 창은 그대로 쓴다.
// 패드: 줄·분류 머리줄에 포커스가 선다. 줄에 서면 그 레시피를 바로 고르고(상세가 따라 바뀜), A는 상세의 [만들기]로,
// 머리줄의 A는 접기·펼치기. 상세에서 B를 누르면 고른 줄로 돌아온다. 목록을 다시 지어도 같은 줄로 포커스를 되돌린다.
//
// 명명 계약(베이스가 이름으로 찾는다): TitleText(공방 이름) / GoldText(소지금) / ListBox(분류·줄이 붙는 패널)
// 위젯 계약(BindWidget): DetailPanel (WBP_CraftDetail, UNUCraftDetailWidget) --- 창 WBP 안에 접혀 있다가 줄을 고르면 펼친다
// 분류 머리줄은 CategoryWidgetClass(WBP_CraftCategory), 줄은 EntryWidgetClass(WBP_CraftEntry)로 만든다
#pragma once

#include "CoreMinimal.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUCraftWindowWidget.generated.h"

class UNUCraftCategoryWidget;
class UNUCraftDetailWidget;
class UNUCraftEntryWidget;
struct FNUCraftRecipeRow;

UCLASS(Abstract)
class PROJECTM_API UNUCraftWindowWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()

public:
	// 공방 선택지가 부른다. 창의 수명은 인벤 컴포넌트가 쥐므로(OpenCraftUI) 여기서는 창을 돌려주지 않는다
	static void Show(APlayerController* OwningPlayer, int32 InShopId);

	// 열려 있는 창에 다시 부르면 다른 공방으로 갈아탄다 (고른 줄과 접힘 상태는 버린다)
	void SetShop(int32 InShopId);

	// 고른 줄, 없으면 첫 레시피 줄 (분류 머리줄은 건너뛴다)
	virtual void FocusForPad() override;

protected:
	virtual void NativeConstruct() override;
	virtual void RefreshData() override; // 인벤·골드가 바뀌면 재료 개수와 상세 패널을 다시 그린다
	virtual void CloseSelf() override;

private:
	void PopulateList();

	// "회복 물약 재료 1/3 · 광석 조각 0/1" 처럼 재료를 한 줄로. 모자란 것도 그대로 보인다
	FText BuildMaterialText(const FNUCraftRecipeRow& Recipe) const;

	// 재료를 모아도 못 만드는 이유 (레벨 미달 / 도면 없음). 없으면 빈 글자
	FText GetHardLockReason(const FNUCraftRecipeRow& Recipe) const;

	// 고른 레시피로 상세 패널을 채운다. 고른 것이 없으면 접는다
	void RefreshDetail();

	void HandleCategoryToggled(const FString& CategoryKey);
	void HandleEntrySelected(int32 RecipeId);
	void HandleCraftRequested(int32 RecipeId, int32 Times);

	// 패드: 줄에 서면 고르고 결과물 툴팁을 띄운다 / 줄의 A는 [만들기]로 / 상세의 B는 고른 줄로
	void HandleEntryFocusChanged(UNUCraftEntryWidget* Entry, bool bFocused, FName ResultRowName);
	void HandleEntryActivated(int32 RecipeId);
	void HandleDetailBack();

	// 기획 표 Shop_ID (0 = 미지정)
	int32 ShopId = 0;
	int32 SelectedRecipeId = 0;

	// 접어 둔 분류 (Category 글자). 기본은 전부 펼침
	TSet<FString> CollapsedCategories;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNUCraftDetailWidget> DetailPanel;

	// 줄·분류 머리줄 WBP. WBP_CraftWindow 기본값에서 지정한다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Craft")
	TSubclassOf<UNUCraftEntryWidget> EntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Craft")
	TSubclassOf<UNUCraftCategoryWidget> CategoryWidgetClass;

	// 고른 줄 표시를 바꿀 때 다시 그리지 않으려고 줄을 기억한다 (populate마다 재구성)
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNUCraftEntryWidget>> EntryByRecipe;
};
