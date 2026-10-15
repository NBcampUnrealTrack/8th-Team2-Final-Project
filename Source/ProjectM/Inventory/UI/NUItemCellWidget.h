// 아이템 칸 하나 --- 모양은 WBP_ItemCell이 갖고, C++는 아이콘·개수·상태만 넣는다 (인벤·창고·장비창 공용).
// 클래스는 프로젝트 세팅(NU UI)의 ItemCellClass. 크기는 창이 정한다(SetCellSize) --- 창이 칸 수로 자기 크기를 계산하기 때문.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   CellSizeBox (SizeBox)          루트. 창이 준 크기로 정사각형을 만든다
//   IconImage   (Image)            아이템 아이콘. 없으면 접는다
//   CountText   (TextBlock, 선택)  "xN" 배지 --- 2개 이상일 때만
//   LabelText   (TextBlock, 선택)  아이콘 없는 아이템의 이름 / 빈 장비 칸의 부위 이름. 그 외에는 접는다
//   EmptyBox    (아무 위젯, 선택)  빈 칸일 때만 보인다 (빈 칸 색·무늬)
//   OfferedBox  (아무 위젯, 선택)  거래에 올린 칸일 때만 보인다 (흐리게 덮는 층)
//   HeldBox     (아무 위젯, 선택)  패드로 집은 칸일 때만 보인다 (놓을 곳을 고르는 동안 출발 칸 표시)
//
// 입력: 마우스 다운을 (저장소, 칸)과 함께 창에 넘긴다 --- 드래그 시작 여부와 우클릭의 뜻은 창이 정한다.
// 포커스도 받는다(패드 D패드 이동) --- 포커스가 들고 나는 것을 창에 알려 툴팁과 집기·메뉴의 대상이 되게 한다. 키의 뜻은 창이 정한다.
// 안쪽 위젯은 전부 HitTestInvisible이어야 이 위젯이 마우스를 받는다 (WBP 규칙)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUItemCellWidget.generated.h"

class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;

DECLARE_DELEGATE_RetVal_ThreeParams(FReply, FOnNUItemCellMouseDown, int32 /*Container*/, int32 /*SlotNo*/, const FPointerEvent& /*MouseEvent*/);
DECLARE_DELEGATE_TwoParams(FOnNUItemCellFocus, class UNUItemCellWidget* /*Cell*/, bool /*bFocused*/);

UCLASS(Abstract)
class PROJECTM_API UNUItemCellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// (저장소, 칸) 좌표 --- 인벤·창고 패널이 한 창에 섞여도 칸이 자기 출처를 안다. 장비창은 (NUContainer::Equipment, 부위 번호)
	void SetCoords(int32 InContainer, int32 InSlotNo);
	int32 GetContainer() const { return Container; }
	int32 GetSlotNo() const { return SlotNo; }

	void SetCellSize(float Size);

	// 아이템이 든 칸. Icon이 없으면 Label(이름)을 글자로 보여 준다. bOffered = 거래에 올려 둔 칸
	void SetItem(UTexture2D* Icon, int32 Count, const FText& Label, bool bOffered);

	// 빈 칸. Label은 빈 장비 칸의 부위 이름 (인벤 빈 칸은 비워서 넘긴다)
	void SetEmpty(const FText& Label);

	// 패드로 집은 칸 표시 (SetItem/SetEmpty가 지운다 --- 다시 그린 뒤 창이 다시 칠한다)
	void SetHeld(bool bHeld);

	FOnNUItemCellMouseDown OnCellMouseDown;
	FOnNUItemCellFocus OnCellFocusChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> CellSizeBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> OfferedBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HeldBox;

	int32 Container = 0;
	int32 SlotNo = INDEX_NONE;
};
