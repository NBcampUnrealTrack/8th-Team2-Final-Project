// 패드 디버그 콘솔 명령 (nu.Pad.*) --- 개발 전용, Shipping 제외.
// 실제 패드 없이 패드 입력을 흉내 낸다: Slate에 패드 버튼·축 이벤트를 넣고, 엔진 장치 서브시스템이 "패드를 썼다"로
// 인식하도록 가짜 XInput 장치를 하나 등록해 두되(XInputDevice 플러그인이 실제 패드에 하는 등록과 같다),
// 기본 사용자에게는 흉내 내는 동안만 붙인다 --- 실제 패드가 다시 연결될 때 기본 사용자 자리를 막지 않게.
// 등록 방식은 self-registering static (FAutoConsoleCommandWithWorldAndArgs).

#if !UE_BUILD_SHIPPING

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerController/NUPlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/InputDeviceRegistry.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

namespace NUGamepadDebug
{

static TOptional<FInputDeviceId> GFakePadId;

// 에디터 콘솔에 명령을 치면 포커스가 콘솔에 남아 흉내 낸 패드 키가 게임으로 안 간다.
// 포커스가 게임 뷰포트 안(창 버튼 포함)에 없을 때만 뷰포트로 옮긴다 --- 창 안 버튼에 있는 포커스는 그대로 둬야 D패드 이동을 볼 수 있다
static void EnsureGameViewportFocus()
{
	const TSharedPtr<SWidget> Viewport = GEngine != nullptr && GEngine->GameViewport != nullptr ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
	if (Viewport.IsValid() && !FSlateApplication::Get().HasFocusedDescendants(Viewport.ToSharedRef()))
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

// 가짜 패드 장치를 한 번만 만든다 (사용자에게 붙이고 떼는 건 SetFakePadConnected)
static FInputDeviceId GetFakePad()
{
	if (!GFakePadId.IsSet())
	{
		const FInputDeviceId NewId = IPlatformInputDeviceMapper::Get().AllocateNewInputDeviceId();
		FInputDeviceDescriptor Desc;
		Desc.InputDeviceName = TEXT("XInputInterface");
		Desc.HardwareDeviceHandle = NewId;
		Desc.HardwareDeviceIdentifier = TEXT("XInputController");
		FInputDeviceRegistry::RegisterDevice(Desc);
		GFakePadId = NewId;
	}
	return GFakePadId.GetValue();
}

// 흉내 내는 동안만 가짜 패드를 기본 사용자에게 붙이고, 끝나면 실제 패드가 끊길 때처럼 사용자 없음으로 뗀다.
// 붙여 둔 채로 두면 엔진이 기본 사용자에게 패드가 이미 있다고 보고(기본 사용자는 키보드와 첫 패드만 나눠 가진다),
// 절전에서 깨어 다시 연결된 실제 패드를 새 사용자(1번)에게 붙여 게임이 그 패드를 못 받았다 (9/28)
static void SetFakePadConnected(bool bConnected)
{
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	Mapper.Internal_MapInputDeviceToUser(GetFakePad(),
		bConnected ? Mapper.GetPrimaryPlatformUser() : PLATFORMUSERID_NONE,
		bConnected ? EInputDeviceConnectionState::Connected : EInputDeviceConnectionState::Disconnected);
}

static FAutoConsoleCommandWithWorldAndArgs GKeyCmd(
	TEXT("nu.Pad.Key"),
	TEXT("패드 버튼 흉내: nu.Pad.Key <키 이름> [down|up]  (예: nu.Pad.Key Gamepad_FaceButton_Bottom). 생략하면 누르고 다음 틱에 뗀다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !FSlateApplication::IsInitialized())
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Pad.Key] 사용법: nu.Pad.Key <키 이름> [down|up]"));
			return;
		}
		const FName KeyName(*Args[0]);
		if (!FKey(KeyName).IsGamepadKey())
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Pad.Key] 패드 키가 아님: %s"), *Args[0]);
			return;
		}
		const FInputDeviceId Device = GetFakePad();
		const FPlatformUserId User = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
		const FString Mode = Args.Num() >= 2 ? Args[1].ToLower() : FString();
		if (Mode == TEXT("up"))
		{
			FSlateApplication::Get().OnControllerButtonReleased(KeyName, User, Device, false);
			SetFakePadConnected(false);
			return;
		}
		SetFakePadConnected(true); // down이면 up까지 붙여 둔다
		EnsureGameViewportFocus();
		FSlateApplication::Get().OnControllerButtonPressed(KeyName, User, Device, false);
		if (Mode != TEXT("down") && World != nullptr)
		{
			// 같은 프레임에 떼면 눌림이 한 프레임도 안 남아 Started 트리거가 안 올 수 있어 다음 틱에 뗀다
			World->GetTimerManager().SetTimerForNextTick([KeyName, User, Device]()
			{
				if (FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().OnControllerButtonReleased(KeyName, User, Device, false);
				}
				SetFakePadConnected(false);
			});
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GAxisCmd(
	TEXT("nu.Pad.Axis"),
	TEXT("패드 축 흉내: nu.Pad.Axis <축 이름> <값>  (예: nu.Pad.Axis Gamepad_LeftY 1). 0을 넣을 때까지 유지된다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2 || !FSlateApplication::IsInitialized())
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Pad.Axis] 사용법: nu.Pad.Axis <축 이름> <값>"));
			return;
		}
		const FName KeyName(*Args[0]);
		if (!FKey(KeyName).IsGamepadKey())
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Pad.Axis] 패드 축이 아님: %s"), *Args[0]);
			return;
		}
		const float Value = FCString::Atof(*Args[1]);
		const FInputDeviceId Device = GetFakePad();
		const FPlatformUserId User = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
		SetFakePadConnected(true); // 기울인 동안만 붙여 두고 0을 넣으면 뗀다
		EnsureGameViewportFocus();
		FSlateApplication::Get().OnControllerAnalog(KeyName, User, Device, Value);
		if (FMath::IsNearlyZero(Value))
		{
			SetFakePadConnected(false);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GModeCmd(
	TEXT("nu.Pad.Mode"),
	TEXT("엔진이 보는 마지막 입력 장치를 로그로 남긴다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		const UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get();
		if (Devices == nullptr)
		{
			return;
		}
		const FHardwareDeviceIdentifier Id = Devices->GetMostRecentlyUsedHardwareDevice(IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser());
		const ANUPlayerController* PC = World != nullptr ? Cast<ANUPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
		UE_LOG(LogTemp, Log, TEXT("[nu.Pad.Mode] 엔진 마지막 장치 = %s (종류 %d) / 우리 판정 = %s"), *Id.ToString(), static_cast<int32>(Id.PrimaryDeviceType),
			PC == nullptr ? TEXT("PC 없음") : PC->IsGamepadMode() ? TEXT("패드") : TEXT("키보드·마우스"));
	}));

} // namespace NUGamepadDebug

#endif // !UE_BUILD_SHIPPING
