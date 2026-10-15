// 제작 상세 패널의 재료 칸 하나 --- 아이콘 위, 그 아래 "보유/필요". 칸 사이의 "+"는 각 칸이 앞에 달고 있다(첫 칸만 접는다).
// 모양은 WBP_CraftMaterial이 갖고, C++는 이름 계약과 상태만 가진다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   IconImage  (Image)              재료 아이콘 --- 없는 아이템은 LabelText로 대신한다
//   CountText  (TextBlock)          "3/6" (보유/필요). 필요는 만들 개수만큼 곱해진 값
//   LabelText  (TextBlock, 선택)    아이콘이 없을 때 이름
//   ShortBox   (아무 위젯, 선택)    보유가 필요보다 적을 때만 보인다 (붉은 테 등)
//   PlusBox    (아무 위젯, 선택)    앞 칸과 잇는 "+" --- 첫 칸에서는 접는다
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCraftMaterialWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Abstract)
class PROJECTM_API UNUCraftMaterialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMaterial(UTexture2D* Icon, const FText& Name, int32 Have, int32 Need, bool bFirst);

	// 만들 개수가 바뀔 때 필요 수만 다시 --- 아이콘은 그대로
	void SetCounts(int32 Have, int32 Need);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ShortBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PlusBox;
};
