// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "UI/NUScreenMotion.h"
#include "NUConfirmDialog.generated.h"

// 확인 버튼이 어떤 성격의 동작인지. 색이 뜻을 거스르면(생성이 빨강) 사용자가 취소로 오해한다
UENUM(BlueprintType)
enum class ENUConfirmIntent : uint8
{
	// 되돌릴 수 없는 파괴 --- 삭제·버리기 (빨강)
	Destructive,

	// 만들거나 받아들이는 것 --- 생성·수락 (초록)
	Positive
};

// 되돌릴 수 없는 동작(버리기·판매·해체 등) 앞에 세우는 공용 확인 창.
// 취소 콜백/라벨을 주면 수락/거절 창(거래 신청 등)으로도 쓴다.
//
// 팀 패턴: C++는 로직/계약만, 모양은 WBP가 담당한다.
//   - 데이터: Message / ConfirmLabel / CancelLabel (BlueprintReadOnly)
//   - 통지: OnMessageSet (BlueprintImplementableEvent)
//   - 위젯 계약(BindWidget): MessageText / ConfirmButton / CancelButton
//     (ConfirmLabelText / CancelLabelText는 Optional --- 버튼이 글자를 스스로 들고 있는 WBP도 있다)
//
// 정식 UI 제작 시 이 클래스를 부모로 WBP만 새로 만들면 되고, 호출부는 바뀌지 않는다.
UCLASS(Abstract)
class PROJECTM_API UNUConfirmDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	// 확인 창을 띄운다. DialogClass가 없으면 nullptr 반환 (호출부가 직접 처리할지 정한다)
	// InOnCancelled: 거절/취소가 응답인 경우에만 넘긴다 (ESC·취소 버튼에서 호출)
	// InCancelLabel: 비우면 WBP 기본 문구("취소")를 그대로 둔다
	// InIntent: 확인 버튼 색을 가른다 (기본은 파괴 --- 이 창의 원래 용도)
	// bNoticeOnly: 고를 것이 없는 안내 (취소 버튼을 접는다). 확인을 눌러도 InOnConfirmed만 부르고 닫는다
	static UNUConfirmDialog* Show(
		APlayerController* OwningPlayer,
		TSubclassOf<UNUConfirmDialog> DialogClass,
		const FText& InMessage,
		const FText& InConfirmLabel,
		FSimpleDelegate InOnConfirmed,
		FSimpleDelegate InOnCancelled = FSimpleDelegate(),
		const FText& InCancelLabel = FText(),
		ENUConfirmIntent InIntent = ENUConfirmIntent::Destructive,
		bool bNoticeOnly = false);

	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void Confirm();

	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void Cancel();

	// 콜백 없이 닫는다 --- 요청이 무효가 된 정리 경로용 (거절 응답을 보내면 안 되는 경우)
	void Dismiss();

	// 떠 있는 동안 문구를 바꾼다 (남은 시간 표시 등). 이름 계약 위젯과 OnMessageSet을 다시 태운다
	void SetMessage(const FText& InMessage);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ESC = 취소. 모달이라 포커스를 잡아도 무방하다
	// (창 위젯은 같은 이유로 포커스를 잡으면 이동 중 키가 풀려서 쓰지 않는다)
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// WBP가 텍스트를 직접 그리고 싶을 때 쓰는 통지 (이름 계약을 안 쓰는 경우)
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnMessageSet();

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText ConfirmLabel;

	// 비어 있으면 WBP 기본 문구 유지
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText CancelLabel;

	// WBP가 자기 방식으로 칠하고 싶으면 이 값을 본다 (C++는 이름 계약 버튼만 칠한다)
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	ENUConfirmIntent Intent = ENUConfirmIntent::Destructive;

	// 고를 것이 없는 안내 --- 취소 버튼을 접는다 (ESC로도 닫히지만 취소 콜백은 없다)
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	bool bNoticeOnly = false;

	// 열릴 때 Dim(뒤를 덮는 막)이 깔리고 Card(창 본체)가 살짝 커지며 나타난다. 이름이 없는 WBP는 그냥 뜬다 (기본 끔)
	UPROPERTY(EditAnywhere, Category = "NU|UI")
	bool bPlayOpenMotion = false;

	// 켜면 확인 버튼(순정 Button)에 색 틴트를 곱하지 않고 스타일을 통째로 고른다 --- 틴트는 테두리·글자까지 같은 색으로 물들인다.
	// 파괴 = WBP에 그려 둔 ConfirmButton 스타일 그대로, 생성·수락·안내 = 아래 스타일과 글자색.
	// 이름 계약(선택): IntentIconDestructive / IntentIconPositive(Image) 중 맞는 쪽만 보인다
	UPROPERTY(EditAnywhere, Category = "NU|UI")
	bool bUseIntentStyles = false;

	UPROPERTY(EditAnywhere, Category = "NU|UI", meta = (EditCondition = "bUseIntentStyles"))
	FButtonStyle PositiveConfirmStyle;

	UPROPERTY(EditAnywhere, Category = "NU|UI", meta = (EditCondition = "bUseIntentStyles"))
	FSlateColor PositiveLabelColor;

private:
	// 버튼은 WBP마다 타입이 다르다(순정 Button / WBP_CloseButton / WBP_CommonButton) --- UWidget으로 받아 캐스팅한다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CancelButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> ConfirmLabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> CancelLabelText;

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	void Close();

	// 이름 계약 위젯(MessageText)에 현재 Message를 그린다
	void ApplyMessageToWidget();

	FSimpleDelegate OnConfirmed;
	FSimpleDelegate OnCancelled;

	FNUScreenMotion OpenMotion;
};
