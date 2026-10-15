// NPC 스프라이트 크기·밝기 맞춤 (프로젝트 세팅 > 게임 > NU NPC Visual).
// NPC 그림은 플레이어와 같은 도트 크기(1텍셀 = 1.04cm)로 그려졌지만 덩치·밝기가 제각각이다 --- 그림을 다시 그리지 않고
// NPC 마다 크기 배율과 밝기(감마·게인)를 여기서 맞춘다. 값은 Tools/HUDRenewal/apply_npc_visual.py 가 그림을 재서 넣는다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUNPCVisualSettings.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct FNUNPCVisualTuning
{
	GENERATED_BODY()

	// 스프라이트 크기 배율 (1 = 그림 그대로). 드워프·아이처럼 키가 작아야 하는 NPC 를 줄인다
	UPROPERTY(EditAnywhere, Category = "NPC Visual", meta = (ClampMin = "0.3", ClampMax = "2"))
	float Scale = 1.f;

	// 밝기 감마 (1 = 그대로, 작을수록 어두운 부분이 밝아짐) --- 캐릭터 머티리얼 ToneGamma
	UPROPERTY(EditAnywhere, Category = "NPC Visual", meta = (ClampMin = "0.3", ClampMax = "2"))
	float ToneGamma = 1.f;

	// 밝기 배율 (1 = 그대로) --- 캐릭터 머티리얼 ToneGain
	UPROPERTY(EditAnywhere, Category = "NPC Visual", meta = (ClampMin = "0.3", ClampMax = "2"))
	float ToneGain = 1.f;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU NPC Visual"))
class PROJECTM_API UNUNPCVisualSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 이 머티리얼(캐릭터 스프라이트)을 쓰는 NPC 만 밝기·도트 보정을 받는다. 옛 조명 머티리얼 NPC 는 그대로 둔다
	UPROPERTY(Config, EditAnywhere, Category = "NPC Visual")
	TSoftObjectPtr<UMaterialInterface> CharacterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/M_NUCharacterSprite.M_NUCharacterSprite")));

	// 멀리 선 NPC 는 도트 한 칸이 1~2 화면 픽셀에 걸려 칸이 들쭉날쭉해진다(원근). 1 이면 칸 가장자리를 화면 픽셀 크기로 섞어
	// 칸 폭을 고르게 유지한다(정수 배율에서는 원래 도트 그대로). 캐릭터 머티리얼 NUA_PixelAA. 음수면 머티리얼 값 그대로
	UPROPERTY(Config, EditAnywhere, Category = "NPC Visual", meta = (ClampMax = "1"))
	float PixelAA = 1.f;

	// 표에 없는 NPC 의 값
	UPROPERTY(Config, EditAnywhere, Category = "NPC Visual")
	FNUNPCVisualTuning Default;

	// NPC 별 값. 키는 NpcId (0 인 옛 BP 는 클래스 이름의 숫자, 예 BP_NPC_4017 → 4017)
	UPROPERTY(Config, EditAnywhere, Category = "NPC Visual")
	TMap<int32, FNUNPCVisualTuning> PerNpc;
};
