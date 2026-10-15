#include "UI/HUD/NU_InteractPromptWidget.h"

#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"

void UNU_InteractPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshKey();
}

void UNU_InteractPromptWidget::RefreshKey()
{
	if (KeyText == nullptr)
	{
		return;
	}
	const UInputAction* Action = InteractAction.LoadSynchronous();
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Action == nullptr || Input == nullptr)
	{
		return;
	}
	for (const FKey& Key : Input->QueryKeysMappedToAction(Action))
	{
		if (Key.IsValid() && !Key.IsGamepadKey() && !Key.IsMouseButton())
		{
			KeyText->SetText(Key.GetDisplayName(false));
			return;
		}
	}
}

void UNU_InteractPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 접혀 있는 동안은 틱이 오지 않는다 --- 몇 프레임 건너뛰고 다시 왔다면 방금 나타난 것
	const bool bJustShown = LastTickFrame == 0 || GFrameCounter > LastTickFrame + 2;
	LastTickFrame = GFrameCounter;
	if (bJustShown && PromptRoot != nullptr)
	{
		RefreshKey();       // 키 설정을 바꿨을 수 있다
		PopMotion.Add(PromptRoot, 0.f, 0.24f, FVector2D(0.f, 8.f), 0.f, 0.88f);
		PopMotion.Start();
		PressTime = FMath::Max(0.f, PressPeriod - 0.9f);   // 첫 눌림은 나타나고 0.6초쯤 뒤
	}
	PopMotion.Advance(InDeltaTime);

	// 키 눌림: 주기의 끝쪽에서 0.08초 내려가고 0.06초 머물렀다 0.14초에 올라온다
	if (KeyCap != nullptr && PressPeriod > 0.3f)
	{
		PressTime = FMath::Fmod(PressTime + InDeltaTime, PressPeriod);
		const float Start = PressPeriod - 0.28f;
		float Press = 0.f;
		if (PressTime >= Start)
		{
			const float T = PressTime - Start;
			Press = T < 0.08f ? T / 0.08f : (T < 0.14f ? 1.f : 1.f - FMath::Clamp((T - 0.14f) / 0.14f, 0.f, 1.f));
		}
		KeyCap->SetRenderTranslation(FVector2D(0.f, PressDepth * Press));
		if (KeyLip != nullptr)
		{
			KeyLip->SetRenderOpacity(1.f - Press);
		}
	}
}
