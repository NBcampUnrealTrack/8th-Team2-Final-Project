// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Base/NU_BaseSheetWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/World.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "PlayerController/NUPlayerController.h"
#include "TimerManager.h"
#include "UI/NUConfirmDialog.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUSheetWidget, Log, All);

void UNU_BaseSheetWidget::OpenSheet()
{
	if (IsSheetOpen())
	{
		return;
	}

	// 전투 중에는 애초에 열지 않는다.
	// 열고 나서 HandleEncounterChanged 로 닫으면 한 프레임 깜빡이므로 여기서 막는다.
	// CachedEncounter 는 아직 비어 있으니(InitializeWidget 전) ASC 에서 직접 읽는다.
	if (bCloseOnBattleStart && ResolvePlayerContext())
	{
		const UNUAbilitySystemComponent* ASC = GetNUASC();
		if (ASC != nullptr && ASC->GetBattleEncounter() != nullptr)
		{
			return;
		}
	}

	// AddToViewport 가 NativeConstruct -> InitializeWidget 을 태우므로 구독은 그쪽에서 붙는다
	AddToViewport(SheetZOrder);

	// 닫았다 다시 열었을 때 이전 드래그 위치를 유지한다
	SetRenderTranslation(WindowTranslation);

	ApplyCursorRequest(true);
	BP_OnSheetOpened();
}

void UNU_BaseSheetWidget::CloseSheet()
{
	if (!IsSheetOpen())
	{
		return;
	}

	// 커서 요청을 먼저 거둔다 — RemoveFromParent 뒤에는 PC 해석이 실패할 수 있다
	ApplyCursorRequest(false);

	// RemoveFromParent 가 NativeDestruct -> ReleaseWidget 을 태워 구독을 끊는다
	RemoveFromParent();

	BP_OnSheetClosed();
}

void UNU_BaseSheetWidget::ToggleSheet()
{
	if (IsSheetOpen())
	{
		CloseSheet();
		return;
	}

	OpenSheet();
}

bool UNU_BaseSheetWidget::IsSheetOpen() const
{
	return IsInViewport();
}

void UNU_BaseSheetWidget::InitializeWidget()
{
	// Super 가 PlayerState / ASC 를 해석하고 전투 여부를 1회 통지한다.
	// 그 통지가 HandleEncounterChanged 로 들어오므로, 전투 중에 창을 열면 곧바로 닫힌다.
	Super::InitializeWidget();

	RequestRefresh();
}

void UNU_BaseSheetWidget::ReleaseWidget()
{
	// 닫히는 경로가 CloseSheet 가 아닐 수도 있다(부모 위젯 파괴, 레벨 전환 등).
	// 커서 요청이 남으면 커서가 영구히 켜진 채로 남으므로 여기서 한 번 더 거둔다.
	// PC 의 RequestCursor 는 같은 Reason 을 두 번 해제해도 안전하다.
	ApplyCursorRequest(false);

	Super::ReleaseWidget();
}

void UNU_BaseSheetWidget::HandleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
	Super::HandleEncounterChanged(NewEncounter);

	// 전투 진입 시 닫는다. 전투 연출과 커맨드 UI 위에 창이 남으면 안 된다.
	if (bCloseOnBattleStart && NewEncounter != nullptr)
	{
		// 이 통지는 NativeConstruct 도중(초기 동기화)에도 올 수 있어서 즉시 제거하면
		// 구축 중인 위젯을 파괴하는 셈이 된다. 다음 틱으로 미뤄 안전하게 닫는다.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateWeakLambda(this, [this]() { CloseSheet(); }));
		}
		return;
	}

	RequestRefresh();
}

//-----------------------창 드래그-------------------------

FReply UNU_BaseSheetWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggable && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingWindow = true;
		DragStartScreenPos = InMouseEvent.GetScreenSpacePosition();
		DragStartTranslation = WindowTranslation;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UNU_BaseSheetWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingWindow)
	{
		// 스크린 좌표 델타를 DPI 스케일로 나눠 위젯 로컬 이동량으로 변환한다
		const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
		WindowTranslation = DragStartTranslation + (InMouseEvent.GetScreenSpacePosition() - DragStartScreenPos) / Scale;
		SetRenderTranslation(WindowTranslation);
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UNU_BaseSheetWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingWindow && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingWindow = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

//-----------------------갱신 / 확인창 / 커서-------------------------

void UNU_BaseSheetWidget::RequestRefresh()
{
	if (bRefreshQueued || GetWorld() == nullptr)
	{
		return;
	}

	bRefreshQueued = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bRefreshQueued = false;
		RefreshSheet();
	}));
}

void UNU_BaseSheetWidget::RefreshSheet()
{
}

void UNU_BaseSheetWidget::RequestConfirm(const FText& InMessage, const FText& InConfirmLabel, FSimpleDelegate OnConfirmed)
{
	if (UNUConfirmDialog::Show(GetNUPlayerController(), ConfirmDialogClass, InMessage, InConfirmLabel, OnConfirmed) != nullptr)
	{
		return;
	}

	// 확인 창 WBP 미지정 — 동작 자체는 살려두고 경고만 남긴다 (지정하면 바로 확인 창이 붙는다)
	UE_LOG(LogNUSheetWidget, Warning, TEXT("ConfirmDialogClass 미지정 — 확인 없이 실행: %s"), *InMessage.ToString());
	OnConfirmed.ExecuteIfBound();
}

void UNU_BaseSheetWidget::ApplyCursorRequest(bool bWant)
{
	if (ANUPlayerController* PC = GetNUPlayerControllerChecked())
	{
		// Owner 로 this 를 넘기면 위젯이 파괴될 때 PC 가 요청을 자동 정리한다
		PC->RequestCursor(CursorReason, bWant, this);
	}
}

ANUPlayerController* UNU_BaseSheetWidget::GetNUPlayerControllerChecked() const
{
	return Cast<ANUPlayerController>(GetNUPlayerController());
}
