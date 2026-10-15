#include "CharacterProgression/UI/NUProgressionWindowBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"

void UNUProgressionWindowBase::SetCloseRequested(FSimpleDelegate InCloseRequested)
{
	CloseRequested = MoveTemp(InCloseRequested);
}

void UNUProgressionWindowBase::RequestClose()
{
	CloseRequested.ExecuteIfBound();
}

void UNUProgressionWindowBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 닫았다 다시 열어도 마지막 창 위치 유지
	if (WindowFrame != nullptr)
	{
		WindowFrame->SetRenderTranslation(WindowTranslation);
	}
	BindCloseButton();
}

void UNUProgressionWindowBase::NativeDestruct()
{
	UnbindCloseButton();
	bDraggingWindow = false;
	Super::NativeDestruct();
}

void UNUProgressionWindowBase::BindCloseButton()
{
	// 명명 계약: WBP에 "CloseButton"이 있으면 공통 닫기 동작 연결
	UWidget* CloseWidget = GetWidgetFromName(TEXT("CloseButton"));
	if (UButton* CloseButtonNative = Cast<UButton>(CloseWidget))
	{
		CloseButtonNative->OnClicked.RemoveDynamic(this, &ThisClass::RequestClose);
		CloseButtonNative->OnClicked.AddDynamic(this, &ThisClass::RequestClose);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &ThisClass::RequestClose);
		CloseButtonWidget->OnCloseClicked.AddDynamic(this, &ThisClass::RequestClose);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(CloseWidget))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &ThisClass::RequestClose);
		CommonButtonWidget->OnCommonClicked.AddDynamic(this, &ThisClass::RequestClose);
	}
}

void UNUProgressionWindowBase::UnbindCloseButton()
{
	UWidget* CloseWidget = GetWidgetFromName(TEXT("CloseButton"));
	if (UButton* CloseButtonNative = Cast<UButton>(CloseWidget))
	{
		CloseButtonNative->OnClicked.RemoveDynamic(this, &ThisClass::RequestClose);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &ThisClass::RequestClose);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(CloseWidget))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &ThisClass::RequestClose);
	}
}

FReply UNUProgressionWindowBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// DragHandle 영역을 좌클릭한 경우에만 창 이동 시작
	if (CanDragWindow()
		&& InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& DragHandle != nullptr
		&& DragHandle->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		// 시작 마우스 위치와 현재 창 위치를 같이 기억
		bDraggingWindow = true;
		DragStartScreenPosition = InMouseEvent.GetScreenSpacePosition();
		DragStartTranslation = WindowTranslation;
		// 커서가 창 밖으로 나가도 MouseMove를 계속 받도록 캡처
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

bool UNUProgressionWindowBase::CanDragWindow() const
{
	return true;
}

FReply UNUProgressionWindowBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingWindow && WindowFrame != nullptr)
	{
		// 마우스 이동량과 창 이동량이 맞도록 보정
		const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
		
		WindowTranslation = 
			DragStartTranslation 
			+ (InMouseEvent.GetScreenSpacePosition() - DragStartScreenPosition) 
			/ ViewportScale;
		WindowFrame->SetRenderTranslation(WindowTranslation);
		
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UNUProgressionWindowBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 좌클릭을 놓으면 드래그 종료 후 마우스 캡처 해제
	if (bDraggingWindow && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingWindow = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UNUProgressionWindowBase::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// 알트탭 등으로 캡처를 잃어도 드래그 상태가 남지 않게 처리
	bDraggingWindow = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
