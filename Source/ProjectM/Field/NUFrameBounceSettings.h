// 몸통 프레임별 세로 흔들림 표 (프로젝트 세팅 > 게임 > NU Frame Bounce).
//
// 장비 파츠는 부위당 그림이 한 장뿐이라, 몸이 프레임마다 몇 픽셀 오르내리는 것을 혼자 따라가지 못한다.
// 그 이동량을 여기 적어두면 UNUSpriteVisualComponent가 겹침 레이어를 같이 올렸다 내린다.
//
// 표는 손으로 채우지 않는다 --- 에디터에서 그 애니메이션이 한 번 재생되면 픽셀을 재서 여기에 적고
// DefaultGame.ini에 저장한다. 패키징 빌드는 소스 텍스처가 없어 픽셀을 못 읽으므로 이 표가 유일한 출처다.
// 그래서 표는 반드시 커밋되어야 한다 --- 비어 있으면 패키징된 게임에서 장비가 몸을 따라가지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUFrameBounceSettings.generated.h"

class UPaperFlipbook;

// 플립북 하나의 프레임별 세로 이동량
USTRUCT()
struct FNUFrameBounceEntry
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "NU|Bounce")
	TSoftObjectPtr<UPaperFlipbook> Flipbook;

	// 키프레임 순서대로, 기준 프레임이 0이고 나머지는 그 차이 (+가 위, 언리얼 단위)
	UPROPERTY(Config, EditAnywhere, Category = "NU|Bounce")
	TArray<float> Offsets;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Frame Bounce"))
class PROJECTM_API UNUFrameBounceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	// 측정 결과 보관함. 없는 플립북은 흔들림 0으로 본다
	UPROPERTY(Config, EditAnywhere, Category = "NU|Bounce")
	TArray<FNUFrameBounceEntry> Tracks;

	// 표에서 플립북을 찾는다. 없으면 nullptr
	const FNUFrameBounceEntry* FindTrack(const UPaperFlipbook* Flipbook) const;
};
