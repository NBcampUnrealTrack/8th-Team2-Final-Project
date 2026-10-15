#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUProgressionWindowBase.generated.h"

class UWidget;

// 성장 UI 창 닫기/드래그 이동을 처리
UCLASS(Abstract)
class PROJECTM_API UNUProgressionWindowBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCloseRequested(FSimpleDelegate InCloseRequested);

	UFUNCTION(BlueprintCallable, Category = "NU|Progression|UI")
	void RequestClose();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual bool CanDragWindow() const;

	// WBP에서 이동할 창 본체와 드래그 영역을 같은 이름으로 배치
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> WindowFrame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DragHandle;

private:
	// 명명 계약: WBP에 "CloseButton"이 있으면 닫기 동작을 C++가 바인딩한다.
	// 순정 Button(레거시)과 UNU_CloseButtonWidget(WBP_CloseButton, 신규 공용 부품) 둘 다 지원하려면
	// BindWidgetOptional로 타입을 하나로 고정할 수 없어 이름으로 찾아 캐스팅한다 (NUSheetWidgetBase와 동일 패턴).
	void BindCloseButton();
	void UnbindCloseButton();

	FSimpleDelegate CloseRequested;

	// 드래그 시작 위치를 기준으로 WindowFrame의 이동량을 계산
	FVector2D WindowTranslation = FVector2D::ZeroVector;
	FVector2D DragStartScreenPosition = FVector2D::ZeroVector;
	FVector2D DragStartTranslation = FVector2D::ZeroVector;
	bool bDraggingWindow = false;
};
