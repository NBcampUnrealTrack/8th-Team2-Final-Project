#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NUCameraOcclusionFadeComponent.generated.h"

class UMaterialParameterCollection;

// 카메라와 캐릭터 사이를 가리는 물체를 디더링으로 흐리게 만듦
//
// MPC 가 들고 있어야 하는 값, 이름이 어긋나면 BeginPlay 에서 경고를 남김
//   Vector  CharacterOffset    xyz = 캐릭터 가슴 높이 - 카메라 위치
//                              w   = 캡슐 바닥 높이 + FloorMargin, 월드 높이 그대로임 (카메라를 빼지 않음)
//                                    이보다 낮은 픽셀(딛고 선 바닥)은 걷어내지 않음
//   Scalar  FadeRadius         캐릭터 쪽에서의 원뿔 반경, 카메라 쪽으로 갈수록 비례해 좁아짐
//   Scalar  FadeStrength       0 = 페이드 없음, 1 = 완전히 사라짐 (1 - MinFade 가 들어감)
//
// AFieldPlayerCharacter 전용 --- Camera 컴포넌트를 직접 참조함
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUCameraOcclusionFadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUCameraOcclusionFadeComponent();

	// 필드 카메라가 아닌 동안 꺼 둠
	// 넷 전투는 필드 폰이 숨겨지는 것으로 자동 판별하므로 따로 부를 필요 없음
	UFUNCTION(BlueprintCallable, Category = "NU|Camera Fade")
	void SetFadeEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 이번 프레임 값을 MPC 에 적음
	void PushFadeParameters(const FVector& CharacterOffset, float FloorZ, float Strength);

	// 시작할 때 유효한지 한 번 확인함
	void ValidateCollection() const;

private:
	// 페이드 값을 적어 둘 MPC, MF_CameraOcclusionFade 가 읽는 것과 같은 것이어야 함
	UPROPERTY(EditDefaultsOnly, Category = "NU|Camera Fade")
	TSoftObjectPtr<UMaterialParameterCollection> FadeCollection;

	// 캐릭터 쪽에서의 원뿔 반경, 화면에서 캐릭터 주변이 얼마나 넓게 뚫릴지를 정함
	UPROPERTY(EditAnywhere, Category = "NU|Camera Fade", meta = (ClampMin = "0.0"))
	float FadeRadius = 165.f;

	// 가장 흐려졌을 때 남는 불투명도
	UPROPERTY(EditAnywhere, Category = "NU|Camera Fade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinFade = 0.f;

	// 캐릭터 발 위치 기준, 원뿔의 끝으로 삼을 높이
	UPROPERTY(EditAnywhere, Category = "NU|Camera Fade", meta = (ClampMin = "0.0"))
	float ChestHeightOffset = 0.f;

	// 캡슐 바닥에서 이만큼 위까지는 어떤 면이든 걷어내지 않음 (실제 바닥은 캡슐 바닥보다 2쯤 아래에 있음)
	// 원뿔이 발 앞의 바닥까지 덮어서, 다리 상판처럼 페이드를 쓰는 바닥을 딛고 서면 발 앞이 뚫려 보였음
	// 위를 향한 면(바닥)을 그 위로 얼마나 더 남길지는 MF_CameraOcclusionFade 의 FloorMask 가 정함
	UPROPERTY(EditAnywhere, Category = "NU|Camera Fade", meta = (ClampMin = "0.0"))
	float FloorMargin = 2.f;

	// 켜고 끌 때의 보간 속도, 전투에 들어가고 나올 때 페이드가 툭 끊기지 않게 함
	UPROPERTY(EditAnywhere, Category = "NU|Camera Fade", meta = (ClampMin = "0.0"))
	float EnableInterpSpeed = 8.f;

	// 켜면 마스크가 실제로 덮는 원뿔을 그려 줌, FadeRadius 를 맞출 때 사용
	UPROPERTY(EditAnywhere, Category = "NU|Camera Fade")
	bool bDebugDrawCone = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> LoadedCollection;

	bool bFadeEnabled = true;

	// 실제로 MPC 에 나가는 세기, 목표값으로 보간됨
	float CurrentStrength = 0.f;
};
