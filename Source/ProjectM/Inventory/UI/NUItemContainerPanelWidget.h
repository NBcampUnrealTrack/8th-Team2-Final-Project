// 저장소 패널 하나(가방 / 캐릭터 창고 / 계정 창고) --- 모양은 WBP_ItemContainerPanel이 갖고, C++는 머리글 글자와 칸 배치만 한다.
// 인벤 창이 PanelWidgetClass로 만들어 PanelRow에 붙인다. 폭은 창이 정한다(칸 수 x 칸 크기) --- 머리글 글자 폭에 창이 흔들리지 않게.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   PanelSizeBox (SizeBox)                 루트
//   HeaderText   (TextBlock)               "{저장소}  {사용} / {칸 수}" --- 로딩 중에는 저장소 이름만
//   FullText     (TextBlock, 선택)         같은 글자, 가득 찼을 때만 보인다 --- 색은 WBP (둘 중 하나만 보인다)
//   SortButton   (WBP_CommonButton, 선택)  이 저장소만 정렬 --- 창고 창에서만 보인다 (가방은 창 머리의 정렬 버튼을 쓴다)
//   Grid         (UniformGridPanel)        칸(WBP_ItemCell)이 채워진다. 칸 사이 간격은 WBP의 SlotPadding
//   LoadingText  (TextBlock, 선택)         서버 목록을 아직 못 받았을 때 --- 그동안 Grid는 접는다
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUItemContainerPanelWidget.generated.h"

class UNU_CommonButtonWidget;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;

DECLARE_DELEGATE_OneParam(FOnNUContainerSort, int32 /*Container*/);

UCLASS(Abstract)
class PROJECTM_API UNUItemContainerPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetContainer(int32 InContainer) { Container = InContainer; }
	void SetPanelWidth(float Width);

	// 머리글. 가득 찼으면(사용 >= 칸 수) FullText 쪽만 보인다
	void SetHeader(const FText& ContainerName, int32 Used, int32 SlotCount);

	// 서버 목록을 아직 못 받은 창고 --- 이름과 안내만 보이고 칸은 접는다
	void SetLoading(const FText& ContainerName);

	void SetSortButtonVisible(bool bVisible);

	// 칸을 Index 순으로 격자에 놓는다 (Columns마다 줄바꿈). 칸은 균등 격자 안에서 늘어나지 않고 가운데에 둔다
	void AddCell(UWidget* Cell, int32 Index, int32 Columns);

	FOnNUContainerSort OnSortClicked;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleSortClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> PanelSizeBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FullText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNU_CommonButtonWidget> SortButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> Grid;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadingText;

	int32 Container = 0;
};
