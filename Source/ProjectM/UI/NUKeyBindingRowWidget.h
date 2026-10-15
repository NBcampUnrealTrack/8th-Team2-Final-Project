// 키 설정 창의 한 줄 --- 동작 하나의 이름과 지금 키(키보드 칸·패드 칸)를 보여주고 [키보드]/[패드]를 받는다.
// 모양은 WBP_KeyBindingRow가 갖고, C++는 이름 계약과 상태만 가진다. 실제 키 변경은 창(UNUKeyBindingsWindow)이 한다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   NameText    (TextBlock)              동작 이름 ("소지품 열기")
//   KeyText     (TextBlock)              키보드 칸 키 ("I") --- 비면 EmptyText 쪽만 보인다
//   ChangeButton(WBP_CommonButton)       [키보드] 바꾸기
//   EmptyText   (TextBlock, 선택)        키보드 칸이 비었을 때만 보인다 ("지정 안 됨"). 색은 WBP
//   WaitingBox  (아무 위젯, 선택)        키보드 키 입력을 기다리는 동안만 보인다 ("새 키를 누르세요")
//   ChangedBox  (아무 위젯, 선택)        키보드 칸이 기본값에서 바뀐 줄에만 보인다 (적용 전 표시)
//   패드 칸 (9/15) --- 없는 WBP는 키보드 칸만 보인다
//   PadKeyText      (TextBlock, 선택)    패드 칸 키 ("A"). 고정 표기(이동 = "왼쪽 스틱")도 여기
//   PadEmptyText    (TextBlock, 선택)    패드 칸이 비었을 때만
//   PadWaitingBox   (아무 위젯, 선택)    패드 버튼 입력을 기다리는 동안만
//   PadChangedBox   (아무 위젯, 선택)    패드 칸이 바뀐 줄에만
//   PadChangeButton (WBP_CommonButton, 선택) [패드] 바꾸기. 고정 표기 줄에선 접는다
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUKeyBindingRowWidget.generated.h"

class UNU_CommonButtonWidget;
class UTextBlock;

DECLARE_DELEGATE_TwoParams(FOnNUKeyBindingChangeRequested, FName /*MappingName*/, bool /*bGamepad*/);

UCLASS(Abstract)
class PROJECTM_API UNUKeyBindingRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 창이 줄을 만들고 바로 부른다. 키가 비어 있으면 "지정 안 됨"으로 보인다.
	// PadFixedText가 있으면 패드 칸은 그 글자로 고정하고 [패드] 버튼을 접는다 (이동처럼 스틱에 묶인 동작)
	void SetBinding(FName InMappingName, const FText& InDisplayName,
		const FKey& InKey, bool bChanged,
		const FKey& InPadKey, bool bPadChanged, const FText& PadFixedText);

	// 키 입력을 기다리는 동안 켠다 (창이 한 줄·한 장치만 켠다)
	void SetWaiting(bool bWaiting, bool bGamepad);

	// 그 장치의 바꾸기 버튼에 포커스 (패드로 키를 지정한 뒤 목록을 다시 그려도 자리를 잃지 않게)
	void FocusChangeButton(bool bGamepad);

	FName GetMappingName() const { return MappingName; }

	FOnNUKeyBindingChangeRequested OnChangeRequested;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleChangeClicked();

	UFUNCTION()
	void HandlePadChangeClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> ChangeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> WaitingBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ChangedBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PadKeyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PadEmptyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PadWaitingBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PadChangedBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNU_CommonButtonWidget> PadChangeButton;

	FName MappingName;
};
