#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUSplitStackDialog.generated.h"

class UButton;
class UEditableTextBox;
class UNU_CommonButtonWidget;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnNUSplitConfirmed, int32 /*Count*/);

// 수량 입력 창 --- 나누기가 원조지만 상점 구매·판매도 같은 창을 쓴다 (문구·기본값·단가 표시만 다르다).
// 나누기 상한이 (보유 - 1)인 이유: 전부 떼는 건 나누기가 아니라 이동이라 서버도 거부한다.
// 모양은 WBP_CountDialog가 갖고(프로젝트 세팅 NU UI의 CountDialogClass), C++는 값·검사·확정만 한다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다.
//   MessageText   (TextBlock)         물음 문구
//   CountTextBox  (EditableTextBox)   수량 입력 --- 숫자만, 1~상한으로 자른다
//   PriceText     (TextBlock, 선택)   "금액: N골드" --- 단가가 있을 때만 보인다 (상점)
//   ConfirmButton (WBP_CommonButton)  글자는 호출부마다 다르다 (나누기/구매/판매) --- C++가 LabelText를 바꾼다
//   CancelButton  (WBP_CommonButton)
UCLASS(Abstract)
class PROJECTM_API UNUSplitStackDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	// 나누기 전용 --- MaxSplit(보유 - 1), 기본값 절반. 인벤 창이 부른다
	// OwnerWindow = 이 창을 연 창. 그 창이 닫히면 수량 창도 같이 닫힌다
	static UNUSplitStackDialog* Show(UUserWidget* OwnerWindow, int32 MaxSplit, FOnNUSplitConfirmed InOnConfirmed);

	// 범용 수량 입력 --- 상점 구매·판매가 부른다 (기본값 1, 절반·상한-1 규칙 없음).
	// InUnitPrice > 0이면 입력할 때마다 "금액: 수량 x 단가"를 같이 보여준다
	static UNUSplitStackDialog* ShowCount(UUserWidget* OwnerWindow, int32 MaxCount, int32 InDefaultCount,
		const FText& InMessage, const FText& InConfirmLabel, int32 InUnitPrice, FOnNUSplitConfirmed InOnConfirmed);

	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void Confirm();

	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void Cancel();

protected:
	virtual void NativeConstruct() override;

	// ESC·패드 B = 취소, Enter·패드 A = 확인
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 패드 D패드 = 수량 조절 (위아래 1, 좌우 10). 미리보기 단계에서 잡는다 --- 포커스된 버튼이 D패드를 포커스 이동으로 먼저 삼켜 창의 OnKeyDown까지 안 올라온다
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// 입력 도중 상한을 넘으면 그 자리에서 상한값으로 되돌린다 (숫자가 아닌 글자도 걸러낸다)
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleDecreaseClicked();

	UFUNCTION()
	void HandleIncreaseClicked();

	UFUNCTION()
	void HandleMaxClicked();

	void Close();

	// 1~상한으로 잘라 입력칸에 넣는다 (−/+/최대 버튼·D패드)
	void SetEnteredCount(int32 Count);

	// 현재 입력값 (없거나 이상하면 1)
	int32 GetEnteredCount() const;

	// 단가가 있으면 "금액: 수량 x 단가"를 다시 쓴다 (입력이 바뀔 때마다)
	void UpdatePriceText();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> CountTextBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> CancelButton;

	// 선택: 수량 -1 / +1 / 상한 버튼. 없는 WBP는 입력칸만 쓴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DecreaseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> IncreaseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MaxButton;

	int32 MaxSplit = 1;
	int32 DefaultCount = 1;
	int32 UnitPrice = 0;      // 0 = 금액 줄 없음 (나누기)
	FText DialogMessage;
	FText ConfirmLabel;
	FOnNUSplitConfirmed OnConfirmed;

	// 클램프하느라 텍스트를 되돌릴 때 OnTextChanged가 다시 도는 것을 막는다
	bool bApplyingClamp = false;
};
