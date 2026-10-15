// 제작 창의 분류 머리줄 ("낡은 세트", "체력 포션") --- 누르면(패드는 A) 아래 줄들을 접었다 펼친다.
// 모양은 WBP_CraftCategory가 갖고, C++는 이름 계약과 상태만 가진다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   TitleText    (TextBlock)          분류 이름
//   CountText    (TextBlock, 선택)    "(3)" 처럼 안에 몇 줄인지
//   ExpandedBox  (아무 위젯, 선택)    펼쳐진 상태 표시 (아래 화살표 등)
//   CollapsedBox (아무 위젯, 선택)    접힌 상태 표시 (오른쪽 화살표 등)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCraftCategoryWidget.generated.h"

class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnNUCraftCategoryToggled, const FString& /*CategoryKey*/);

UCLASS(Abstract)
class PROJECTM_API UNUCraftCategoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCategory(const FString& InKey, const FText& InTitle, int32 InCount, bool bExpanded);

	// 목록을 다시 지은 뒤 같은 머리줄로 패드 포커스를 되돌릴 때 찾는 열쇠
	const FString& GetKey() const { return Key; }

	FOnNUCraftCategoryToggled OnToggled;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExpandedBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CollapsedBox;

	FString Key;
};
