// 필드 카메라 리그 실험용 콘솔 명령 (nu.Cam.*) --- 개발 전용, Shipping 제외.
// 각도·거리·화각·심도를 PIE에서 바로 바꿔 보고, 값이 정해지면 리그(BP_LG8_FieldCharacter 등)에 반영한다.
// 컴포넌트는 종류로 찾으므로 카메라 리그 코드를 건드리지 않는다.
//
// 도트 배율 공식: 언리얼 카메라는 세로 화각 고정(AspectRatio_MaintainYFOV)이라
//   배율(px/도트) = 뷰포트 세로 / (2 x 거리 x tan(세로 반화각)),  tan(세로 반화각) = tan(화각/2) / 1.777778
// 가로·세로 모두 이 값이고, 세로 눌림(cos(피치))은 빌보드 보정(UpdateBillboard)이 상쇄한다.

#if !UE_BUILD_SHIPPING

#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace NUFieldCameraDebug
{

// FOV 프로퍼티는 기준 종횡비 16:9에서의 가로 화각이다
constexpr float ReferenceAspect = 1.777778f;

// 생략한 인자 표시 --- 이 값이면 지금 값을 그대로 둔다 (피치·거리·화각 어디에도 안 나오는 수)
constexpr float Keep = -100000.0f;

static float ArgOrKeep(const TArray<FString>& Args, int32 Index)
{
	return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Keep;
}

// 처음 만지기 전의 값 --- 되돌리기용. BP가 덮은 값은 폰 기본 객체에 안 남아서(스폰 때 적용) 여기서 기억해 둔다
struct FRigSnapshot
{
	TWeakObjectPtr<AActor> Owner;
	FRotator Rotation = FRotator::ZeroRotator;
	float ArmLength = 0.0f;
	float Fov = 0.0f;
	FVector TargetOffset = FVector::ZeroVector;
};
static FRigSnapshot GFirstSeen;

// 지금 조종 중인 폰의 스프링암과 카메라 (필드 캐릭터 리그). 처음 보는 폰이면 그때 값을 기억한다
static bool FindRig(UWorld* World, USpringArmComponent*& OutArm, UCameraComponent*& OutCamera)
{
	APawn* Pawn = World != nullptr ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	OutArm = Pawn != nullptr ? Pawn->FindComponentByClass<USpringArmComponent>() : nullptr;
	OutCamera = Pawn != nullptr ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (OutArm == nullptr || OutCamera == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[nu.Cam] 필드 카메라를 못 찾음 (필드 캐릭터를 조종 중일 때만 된다)"));
		return false;
	}
	if (GFirstSeen.Owner.Get() != Pawn)
	{
		GFirstSeen.Owner = Pawn;
		GFirstSeen.Rotation = OutArm->GetRelativeRotation();
		GFirstSeen.ArmLength = OutArm->TargetArmLength;
		GFirstSeen.Fov = OutCamera->FieldOfView;
		GFirstSeen.TargetOffset = OutArm->TargetOffset;
	}
	return true;
}

static float TanHalfY(float FovDeg)
{
	return FMath::Tan(FMath::DegreesToRadians(FovDeg) * 0.5f) / ReferenceAspect;
}

// 목표 도트 배율이 나오는 화각 (거리·뷰포트 세로 기준)
static float FovForScale(float ViewportY, float Distance, float Scale)
{
	return FMath::RadiansToDegrees(2.0f * FMath::Atan(ViewportY / (2.0f * Distance * Scale) * ReferenceAspect));
}

// 현재 값과 도트 배율을 한 줄로 찍는다 --- 시안을 고를 때 이 숫자로 얘기한다
static void DumpRig(UWorld* World, const USpringArmComponent* Arm, const UCameraComponent* Camera)
{
	const FRotator Rotation = Arm->GetRelativeRotation();
	UE_LOG(LogTemp, Log, TEXT("[nu.Cam] 피치 %.1f 요 %.1f 거리 %.0f 화각 %.2f 시선높이 %.0f"),
		Rotation.Pitch, Rotation.Yaw, Arm->TargetArmLength, Camera->FieldOfView, Arm->TargetOffset.Z);

	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (World->GetGameViewport() != nullptr)
	{
		World->GetGameViewport()->GetViewportSize(ViewportSize);
	}
	if (ViewportSize.Y <= 0.0f)
	{
		return;
	}
	const float Scale = ViewportSize.Y / (2.0f * Arm->TargetArmLength * TanHalfY(Camera->FieldOfView));
	const bool bInteger = FMath::IsNearlyEqual(Scale, FMath::RoundToFloat(Scale), 0.01f);
	UE_LOG(LogTemp, Log, TEXT("[nu.Cam] 뷰포트 %.0fx%.0f --- 도트 1개 = %.3fpx (%s). 이 거리에서 2배=화각 %.2f / 3배=%.2f / 4배=%.2f"),
		ViewportSize.X, ViewportSize.Y, Scale, bInteger ? TEXT("정수") : TEXT("정수 아님"),
		FovForScale(ViewportSize.Y, Arm->TargetArmLength, 2.0f),
		FovForScale(ViewportSize.Y, Arm->TargetArmLength, 3.0f),
		FovForScale(ViewportSize.Y, Arm->TargetArmLength, 4.0f));

	static const auto* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	if (ScreenPercentage != nullptr && !FMath::IsNearlyEqual(ScreenPercentage->GetFloat(), 100.0f, 0.5f))
	{
		UE_LOG(LogTemp, Warning, TEXT("[nu.Cam] 렌더 해상도가 %.0f%%다(업스케일 켜짐) --- 선명도 판단 전에 옵션에서 끌 것"),
			ScreenPercentage->GetFloat());
	}
}

// 거리가 바뀌면 심도 초점도 카메라 거리를 따라간다 (안 그러면 초점이 캐릭터 뒤에 남아 화면 전체가 흐려진다)
static void SyncDofFocal(UCameraComponent* Camera, float Distance)
{
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	if (PP.bOverride_DepthOfFieldFocalDistance && PP.DepthOfFieldFocalDistance > 0.0f)
	{
		PP.DepthOfFieldFocalDistance = Distance;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRigCmd(
	TEXT("nu.Cam.Rig"),
	TEXT("필드 카메라 리그: nu.Cam.Rig [피치] [거리] [화각] [시선높이]  (생략한 값은 그대로, 인자 없이 치면 지금 값과 도트 배율만 찍는다. 시선높이를 올리면 캐릭터가 화면 아래로 내려간다)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		USpringArmComponent* Arm = nullptr;
		UCameraComponent* Camera = nullptr;
		if (!FindRig(World, Arm, Camera))
		{
			return;
		}
		const float Pitch = ArgOrKeep(Args, 0);
		const float Distance = ArgOrKeep(Args, 1);
		const float Fov = ArgOrKeep(Args, 2);
		const float LookHeight = ArgOrKeep(Args, 3);
		if (Pitch != Keep)
		{
			// 좌우 회전(요)은 리그가 쿼터뷰로 돌려 둔 값이라 건드리지 않는다
			FRotator Rotation = Arm->GetRelativeRotation();
			Rotation.Pitch = Pitch;
			Arm->SetRelativeRotation(Rotation);
		}
		if (Distance != Keep)
		{
			Arm->TargetArmLength = Distance;
			SyncDofFocal(Camera, Distance);
		}
		if (Fov != Keep)
		{
			Camera->SetFieldOfView(Fov);
		}
		if (LookHeight != Keep)
		{
			// 카메라가 바라보는 지점을 올린다 --- 캐릭터가 화면 아래쪽에 서고 배경이 더 보인다
			Arm->TargetOffset.Z = LookHeight;
		}
		DumpRig(World, Arm, Camera);
	}));

static FAutoConsoleCommandWithWorldAndArgs GZoomCmd(
	TEXT("nu.Cam.Zoom"),
	TEXT("도트 배율을 정수로 맞춘다: nu.Cam.Zoom <배율> [거리]  (지금 뷰포트 세로와 거리에서 그 배율이 나오는 화각을 역산해 넣는다)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		USpringArmComponent* Arm = nullptr;
		UCameraComponent* Camera = nullptr;
		if (!FindRig(World, Arm, Camera))
		{
			return;
		}
		const float Scale = ArgOrKeep(Args, 0);
		if (Scale == Keep || Scale <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Cam] 배율을 넣어라 --- 예: nu.Cam.Zoom 3"));
			return;
		}
		const float Distance = ArgOrKeep(Args, 1);
		if (Distance != Keep)
		{
			Arm->TargetArmLength = Distance;
			SyncDofFocal(Camera, Distance);
		}
		FVector2D ViewportSize = FVector2D::ZeroVector;
		if (World->GetGameViewport() != nullptr)
		{
			World->GetGameViewport()->GetViewportSize(ViewportSize);
		}
		if (ViewportSize.Y <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Cam] 뷰포트 크기를 못 읽음"));
			return;
		}
		Camera->SetFieldOfView(FovForScale(ViewportSize.Y, Arm->TargetArmLength, Scale));
		DumpRig(World, Arm, Camera);
	}));

// 2026-09-19에 구도를 바꾸기 전 값 --- A/B 비교용. 거리는 그때도 1900이라 안 건드린다
constexpr float BeforePitch = -30.0f;
constexpr float BeforeFov = 38.0f;
constexpr float BeforeLookHeight = 0.0f;
static bool GShowingBefore = false;

static FAutoConsoleCommandWithWorldAndArgs GBeforeCmd(
	TEXT("nu.Cam.Before"),
	TEXT("구도를 바꾸기 전 값(피치 -30 화각 38)과 지금 값을 번갈아 본다. 한 번 더 치면 돌아온다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		USpringArmComponent* Arm = nullptr;
		UCameraComponent* Camera = nullptr;
		if (!FindRig(World, Arm, Camera))
		{
			return;
		}
		GShowingBefore = !GShowingBefore;
		FRotator Rotation = Arm->GetRelativeRotation();
		if (GShowingBefore)
		{
			Rotation.Pitch = BeforePitch;
			Arm->SetRelativeRotation(Rotation);
			Camera->SetFieldOfView(BeforeFov);
			Arm->TargetOffset.Z = BeforeLookHeight;
			UE_LOG(LogTemp, Log, TEXT("[nu.Cam] === 바꾸기 전 값 ==="));
		}
		else
		{
			// 처음 본 값 = 지금 리그(BP)에 들어 있는 값
			Rotation.Pitch = GFirstSeen.Rotation.Pitch;
			Arm->SetRelativeRotation(Rotation);
			Camera->SetFieldOfView(GFirstSeen.Fov);
			Arm->TargetOffset = GFirstSeen.TargetOffset;
			UE_LOG(LogTemp, Log, TEXT("[nu.Cam] === 지금 값 ==="));
		}
		DumpRig(World, Arm, Camera);
	}));

static FAutoConsoleCommandWithWorldAndArgs GResetCmd(
	TEXT("nu.Cam.Reset"),
	TEXT("필드 카메라를 이 판에서 처음 만지기 전 값으로 되돌린다 (심도도 끈다)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		USpringArmComponent* Arm = nullptr;
		UCameraComponent* Camera = nullptr;
		if (!FindRig(World, Arm, Camera))
		{
			return;
		}
		GShowingBefore = false;
		Arm->SetRelativeRotation(GFirstSeen.Rotation);
		Arm->TargetArmLength = GFirstSeen.ArmLength;
		Camera->SetFieldOfView(GFirstSeen.Fov);
		Arm->TargetOffset = GFirstSeen.TargetOffset;
		Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = false;
		Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = false;
		Camera->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = false;
		UE_LOG(LogTemp, Log, TEXT("[nu.Cam] 처음 값으로 복구"));
		DumpRig(World, Arm, Camera);
	}));

static FAutoConsoleCommandWithWorldAndArgs GDofCmd(
	TEXT("nu.Cam.Dof"),
	TEXT("필드 카메라 심도: nu.Cam.Dof [초점거리] [조리개] [센서폭]  (초점거리 0 = 끄기, 조리개가 작고 센서폭이 클수록 많이 흐려진다)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		USpringArmComponent* Arm = nullptr;
		UCameraComponent* Camera = nullptr;
		if (!FindRig(World, Arm, Camera))
		{
			return;
		}
		FPostProcessSettings& PP = Camera->PostProcessSettings;
		const float Focal = ArgOrKeep(Args, 0);
		const float FStop = ArgOrKeep(Args, 1);
		const float SensorWidth = ArgOrKeep(Args, 2);
		if (Focal != Keep)
		{
			PP.bOverride_DepthOfFieldFocalDistance = true;
			PP.DepthOfFieldFocalDistance = Focal; // 0이면 엔진이 심도를 끈다
		}
		if (FStop != Keep)
		{
			PP.bOverride_DepthOfFieldFstop = true;
			PP.DepthOfFieldFstop = FStop;
		}
		if (SensorWidth != Keep)
		{
			PP.bOverride_DepthOfFieldSensorWidth = true;
			PP.DepthOfFieldSensorWidth = SensorWidth;
		}
		Camera->PostProcessBlendWeight = 1.0f;
		UE_LOG(LogTemp, Log, TEXT("[nu.Cam] 초점거리 %.0f 조리개 %.2f 센서폭 %.1f"),
			PP.DepthOfFieldFocalDistance, PP.DepthOfFieldFstop, PP.DepthOfFieldSensorWidth);
	}));

}

#endif
