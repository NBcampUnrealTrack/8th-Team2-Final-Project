#include "UI/HUD/NU_HudButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"

void UNU_HudButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(Btn_Common))
	{
		Btn_Common->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleHovered);
		Btn_Common->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleUnhovered);
		Btn_Common->OnPressed.AddUniqueDynamic(this, &ThisClass::HandlePressed);
		Btn_Common->OnReleased.AddUniqueDynamic(this, &ThisClass::HandleReleased);
	}
	Hover = HoverTarget = Press = PressTarget = 0.f;
	ApplyHover();
	RefreshShortcutKey();
}

void UNU_HudButtonWidget::NativeDestruct()
{
	if (IsValid(Btn_Common))
	{
		Btn_Common->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
		Btn_Common->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleUnhovered);
		Btn_Common->OnPressed.RemoveDynamic(this, &ThisClass::HandlePressed);
		Btn_Common->OnReleased.RemoveDynamic(this, &ThisClass::HandleReleased);
	}
	Super::NativeDestruct();
}

void UNU_HudButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (FMath::IsNearlyEqual(Hover, HoverTarget, 0.001f) && FMath::IsNearlyEqual(Press, PressTarget, 0.001f))
	{
		return;
	}
	// 들어갈 때는 빨리, 나올 때는 조금 천천히
	const float HoverSpeed = HoverTarget > Hover ? 16.f : 10.f;
	Hover = FMath::FInterpTo(Hover, HoverTarget, InDeltaTime, HoverSpeed);
	Press = FMath::FInterpTo(Press, PressTarget, InDeltaTime, 28.f);
	if (FMath::IsNearlyEqual(Hover, HoverTarget, 0.002f)) { Hover = HoverTarget; }
	if (FMath::IsNearlyEqual(Press, PressTarget, 0.002f)) { Press = PressTarget; }
	ApplyHover();
}

void UNU_HudButtonWidget::ApplyHover()
{
	if (HoverLift != nullptr)
	{
		FWidgetTransform Transform;
		Transform.Translation = FVector2D(0.f, -LiftPx * Hover + Press);
		Transform.Scale = FVector2D(1.f - 0.06f * Press);
		HoverLift->SetRenderTransform(Transform);
	}
	if (HoverMark != nullptr)
	{
		HoverMark->SetRenderOpacity(Hover);
	}
	if (Tooltip != nullptr)
	{
		Tooltip->SetRenderOpacity(Hover);
		FWidgetTransform Transform;
		Transform.Translation = FVector2D(0.f, 6.f * (1.f - Hover));
		Tooltip->SetRenderTransform(Transform);
		Tooltip->SetVisibility(Hover > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNU_HudButtonWidget::RefreshShortcutKey()
{
	if (KeyCap == nullptr)
	{
		return;
	}
	FText KeyText;
	const APlayerController* PC = GetOwningPlayer();
	const ULocalPlayer* LocalPlayer = PC != nullptr ? PC->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Input != nullptr && ShortcutAction != nullptr)
	{
		// 패드 버튼은 이름표에 안 맞으니 키보드 키 하나만
		for (const FKey& Key : Input->QueryKeysMappedToAction(ShortcutAction))
		{
			if (Key.IsValid() && !Key.IsGamepadKey() && !Key.IsMouseButton())
			{
				KeyText = Key.GetDisplayName(false);
				break;
			}
		}
	}
	if (Txt_Key != nullptr)
	{
		Txt_Key->SetText(KeyText);
	}
	KeyCap->SetVisibility(KeyText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UNU_HudButtonWidget::HandleHovered()
{
	HoverTarget = 1.f;
	RefreshShortcutKey();     // 키 설정이 바뀌었을 수 있어 뜰 때마다 다시 읽는다
}
void UNU_HudButtonWidget::HandleUnhovered() { HoverTarget = 0.f; PressTarget = 0.f; }
void UNU_HudButtonWidget::HandlePressed() { PressTarget = 1.f; }
void UNU_HudButtonWidget::HandleReleased() { PressTarget = 0.f; }
