// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUMapImageSettings.generated.h"

class UTexture2D;

// 한 맵의 지도 이미지. Scripts/bake_map_image.py 가 구운 텍스처와 그 텍스처가 덮는 월드 범위.
USTRUCT()
struct FNUMapImageEntry
{
	GENERATED_BODY()

	// 이 지도가 속한 맵
	UPROPERTY(Config, EditAnywhere, Category = "Map Image")
	TSoftObjectPtr<UWorld> Map;

	// 구운 톱다운 지도 텍스처. 위쪽 = 월드 +Y = 게임 화면 위 (카메라 yaw 90 과 같은 방향)
	UPROPERTY(Config, EditAnywhere, Category = "Map Image")
	TSoftObjectPtr<UTexture2D> Texture;

	// 텍스처가 덮는 XY 범위 --- 그 레벨을 단독으로 열었을 때의 좌표다. 스트리밍으로 옮겨 붙는 공간(사냥터)은
	// 지도 뷰가 레벨 배치를 되돌려서 이 좌표로 바꾼다. 캡처 직교 폭 기준 (bake 가 남기는 map_bounds.json 의 coverage)
	UPROPERTY(Config, EditAnywhere, Category = "Map Image")
	FVector2D WorldMin = FVector2D::ZeroVector;

	UPROPERTY(Config, EditAnywhere, Category = "Map Image")
	FVector2D WorldMax = FVector2D::ZeroVector;

	bool IsValidEntry() const { return !Texture.IsNull() && WorldMax.X > WorldMin.X && WorldMax.Y > WorldMin.Y; }
};

// 맵별 지도 이미지 표. 미니맵과 전체 지도가 같은 표를 읽는다.
// 지역 표(NUWorldDataSettings)와 별개로 맵 경로를 직접 키로 쓴다 --- 지역 등록 여부와 무관하게 동작해야 해서.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Map Image"))
class PROJECTM_API UNUMapImageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	// 맵 패키지 이름(/Game/.../L_Name)으로 지도 항목 조회. 없으면 nullptr.
	// 월드가 아니라 공간 단위로 찾는다 --- 사냥터는 마을 월드에 스트리밍 레벨로 붙어서 월드 이름은 마을이다
	const FNUMapImageEntry* FindForPackage(const FString& PackageName) const;

	// 레벨 좌표 => 지도 UV. 굽기 방향 규약(위=+Y, 오른쪽=-X)을 아는 곳은 여기 하나여야 한다.
	// 범위 밖이면 0~1 밖의 값이 그대로 나온다 --- 마커를 숨길지는 호출부가 정한다
	static FVector2D WorldToMapUV(const FNUMapImageEntry& Entry, const FVector& LevelLocation);

	UPROPERTY(Config, EditAnywhere, Category = "Map Image", meta = (TitleProperty = "Map"))
	TArray<FNUMapImageEntry> Maps;
};
