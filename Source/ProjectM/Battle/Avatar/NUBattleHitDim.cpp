#include "Battle/Avatar/NUBattleHitDim.h"

#include "Battle/Avatar/NUBattleParryFX.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	int32 GNUHitDimEnabled = 1;
	float GNUHitDimAlpha = 0.35f;
	float GNUHitDimSeconds = 0.16f;

	FAutoConsoleVariableRef CVarNUHitDimEnabled(
		TEXT("nu.VFX.HitDim.Enabled"), GNUHitDimEnabled,
		TEXT("피격 순간 화면 암전 켜기 (1) / 끄기 (0)"));
	FAutoConsoleVariableRef CVarNUHitDimAlpha(
		TEXT("nu.VFX.HitDim.Alpha"), GNUHitDimAlpha,
		TEXT("피격 암전의 최대 어둡기 (0 - 1, 기본 0.35)"));
	FAutoConsoleVariableRef CVarNUHitDimSeconds(
		TEXT("nu.VFX.HitDim.Seconds"), GNUHitDimSeconds,
		TEXT("피격 암전이 올라갔다 내려오는 전체 시간 (초, 기본 0.16)"));

	FTimerHandle GNUHitDimReturnHandle;
}

void NUBattleHitDim::Play(UWorld* World)
{
	if (GNUHitDimEnabled == 0 || !IsValid(World))
	{
		return;
	}

	// the parry flash owns the camera fade for its few frames
	if (NUBattleParryFX::IsFlashing(World))
	{
		return;
	}

	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	APlayerCameraManager* Camera = IsValid(Controller) ? Controller->PlayerCameraManager : nullptr;
	if (!IsValid(Camera))
	{
		return;
	}

	// 전환 페이드가 걸려 있으면(검은 화면 유지 중) 겹쳐서 건드리지 않는다
	if (Camera->FadeAmount > 0.01f && !World->GetTimerManager().IsTimerActive(GNUHitDimReturnHandle))
	{
		return;
	}

	const float Alpha = FMath::Clamp(GNUHitDimAlpha, 0.f, 1.f);
	const float Total = FMath::Max(0.04f, GNUHitDimSeconds);
	const float InTime = Total * 0.35f;
	const float OutTime = Total - InTime;

	Camera->StartCameraFade(0.f, Alpha, InTime, FLinearColor::Black, false, true);

	TWeakObjectPtr<APlayerCameraManager> WeakCamera(Camera);
	World->GetTimerManager().SetTimer(GNUHitDimReturnHandle, [WeakCamera, Alpha, OutTime]()
	{
		if (WeakCamera.IsValid())
		{
			WeakCamera->StartCameraFade(Alpha, 0.f, OutTime, FLinearColor::Black, false, false);
		}
	}, InTime, false);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GNUHitDimCmd(
	TEXT("nu.VFX.HitDim"),
	TEXT("피격 암전을 한 번 재생해 본다 (nu.VFX.HitDim.Alpha / Seconds 로 조절)"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		NUBattleHitDim::Play(World);
	}));
#endif
