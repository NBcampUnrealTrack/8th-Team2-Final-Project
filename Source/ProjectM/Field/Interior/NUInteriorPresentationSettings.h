#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "NUInteriorPresentationSettings.generated.h"

// 건물 레벨 인스턴스 내부가 아닌 필드의 퍼시스턴트 레벨에 하나 배치한다.
// 플레이 중에는 현재 화면에 보이는 실내가 이 값을 읽어 주변 연출에 반영한다.
UCLASS(Blueprintable, HideCategories = (Actor, Input, Replication, Rendering, Collision, HLOD, Physics, WorldPartition, DataLayers))
class PROJECTM_API ANUInteriorPresentationSettings : public AInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "nu|settins", meta = (DisplayName = "주변 밝기", ClampMin = "0.0", ClampMax = "1.0"))
	float ExteriorBrightness = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "nu|settins", meta = (DisplayName = "주변 표시 거리", ClampMin = "0.0", UIMax = "500.0"))
	float ExteriorVisibleDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "nu|settins", meta = (DisplayName = "경계 부드러움", ClampMin = "0.0", ClampMax = "1.0"))
	float ExteriorFadeSoftness = 1.0f;
};
