#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUCameraShotActor.generated.h"

class APlayerController;
class UCameraComponent;

// 연출 중 잠시 보여줄 카메라 컷. 대사 표의 CameraShotId가 ShotId를 가리키며, 퀘스트는 모른다.
// 기본은 지금 게임 카메라의 각도·거리·화각을 그대로 두고 이 액터 위치를 바라보게 옮긴다.
UCLASS(Blueprintable)
class PROJECTM_API ANUCameraShotActor : public AActor
{
	GENERATED_BODY()

public:
	ANUCameraShotActor();

	FName GetShotId() const;
	float GetBlendInSeconds() const;
	float GetBlendOutSeconds() const;
	float GetHoldSeconds() const;

	// 보여주기 직전에 지금 시점을 기준으로 카메라 위치와 모습을 맞춘다.
	void PrepareShot(const APlayerController& PlayerController);

	// 월드에 배치된 카메라 컷을 ID로 찾는다. 없으면 nullptr.
	static ANUCameraShotActor* FindShot(UWorld* World, FName ShotId);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	// 게임 시점을 쓸 때는 이 위치를 바라본다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	TObjectPtr<USceneComponent> FocusPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	FName ShotId = NAME_None;

	// 켜면 게임 카메라의 각도·거리·화각·후처리를 그대로 쓰고 FocusPoint를 바라본다. 끄면 배치한 Camera 그대로 보여준다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	bool bUseGameplayView = true;

	// 게임 카메라 구도를 유지하면서 전경은 넓게, 인물은 가까이 보여준다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Camera", meta = (EditCondition = "bUseGameplayView", ClampMin = "0.1", UIMin = "0.1"))
	float GameplayDistanceScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Camera", meta = (ClampMin = "0"))
	float BlendInSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Camera", meta = (ClampMin = "0"))
	float BlendOutSeconds = 0.8f;

	// 대사 없이 카메라만 보여줄 때 머무는 시간. 대사가 있으면 대화창이 닫힐 때까지 머문다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Camera", meta = (ClampMin = "0"))
	float HoldSeconds = 2.5f;
};
