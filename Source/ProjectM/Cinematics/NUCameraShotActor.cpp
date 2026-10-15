#include "Cinematics/NUCameraShotActor.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ANUCameraShotActor::ANUCameraShotActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	FocusPoint = CreateDefaultSubobject<USceneComponent>(TEXT("FocusPoint"));
	SetRootComponent(FocusPoint);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(FocusPoint);
}

FName ANUCameraShotActor::GetShotId() const
{
	return ShotId;
}

float ANUCameraShotActor::GetBlendInSeconds() const
{
	return BlendInSeconds;
}

float ANUCameraShotActor::GetBlendOutSeconds() const
{
	return BlendOutSeconds;
}

float ANUCameraShotActor::GetHoldSeconds() const
{
	return HoldSeconds;
}

void ANUCameraShotActor::PrepareShot(const APlayerController& PlayerController)
{
	if (!bUseGameplayView)
	{
		return;
	}

	const APlayerCameraManager* CameraManager = PlayerController.PlayerCameraManager;
	const AActor* ViewTarget = PlayerController.GetPawn() != nullptr ? PlayerController.GetPawn() : PlayerController.GetViewTarget();
	if (CameraManager == nullptr || ViewTarget == nullptr)
	{
		return;
	}

	// 게임 카메라가 캐릭터를 보던 거리와 각도를 그대로 옮겨, 이 위치를 같은 구도로 보여준다.
	const UCameraComponent* ViewCamera = ViewTarget->FindComponentByClass<UCameraComponent>();
	const FRotator ViewRotation = ViewCamera != nullptr ? ViewCamera->GetComponentRotation() : CameraManager->GetCameraRotation();
	const float DistanceScale = FMath::Max(GameplayDistanceScale, 0.1f);
	const FVector ViewLocation = ViewCamera != nullptr ? ViewCamera->GetComponentLocation() : CameraManager->GetCameraLocation();
	const float ViewDistance = FVector::Dist(ViewLocation, ViewTarget->GetActorLocation()) * DistanceScale;
	Camera->SetWorldLocationAndRotation(FocusPoint->GetComponentLocation() - ViewRotation.Vector() * ViewDistance, ViewRotation);

	// 화각과 후처리가 다르면 컷이 바뀌는 순간 화면 느낌이 튀므로 게임 카메라 값을 따른다.
	if (ViewCamera != nullptr)
	{
		Camera->SetFieldOfView(ViewCamera->FieldOfView);
		Camera->SetProjectionMode(ViewCamera->ProjectionMode);
		Camera->SetOrthoWidth(ViewCamera->OrthoWidth * DistanceScale);
		Camera->SetConstraintAspectRatio(ViewCamera->bConstrainAspectRatio);
		Camera->SetAspectRatio(ViewCamera->AspectRatio);
		Camera->PostProcessSettings = ViewCamera->PostProcessSettings;
		Camera->PostProcessBlendWeight = ViewCamera->PostProcessBlendWeight;
	}

	// 레벨의 피사계 심도는 게임 카메라 거리에 초점이 고정돼 있어, 거리 배율을 바꾼 컷은 비추는 자리에 초점을 다시 맞춘다.
	Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	Camera->PostProcessSettings.DepthOfFieldFocalDistance = ViewDistance;
}

ANUCameraShotActor* ANUCameraShotActor::FindShot(UWorld* World, FName ShotId)
{
	if (World == nullptr || ShotId.IsNone())
	{
		return nullptr;
	}

	for (TActorIterator<ANUCameraShotActor> It(World); It; ++It)
	{
		if (It->ShotId == ShotId)
		{
			return *It;
		}
	}
	return nullptr;
}

#if WITH_EDITOR
EDataValidationResult ANUCameraShotActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (ShotId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("ShotId가 비어 있습니다.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
