#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WorldTransition/Data/NUWorldPortalDefinition.h"
#include "WorldTransition/Data/NUWorldPortalEntryAsset.h"
#include "NUWorldTransitionSettings.generated.h"

class ULevelSequence;
class UDataTable;

// 개발자 사본 맵을 기본 레벨로 열었을 때만 포탈 장소 에셋의 목적 레벨을 사본으로 바꾼다. 팀 맵으로 열면 적용되지 않는다.
USTRUCT()
struct FNUWorldMapVariant
{
	GENERATED_BODY()

	// 이 맵을 기본 레벨로 열었을 때 적용한다. Levels의 사본 맵 안에서 배치를 검증할 때도 같은 변형을 쓴다.
	UPROPERTY(EditAnywhere, Category = "Map Variant")
	TSoftObjectPtr<UWorld> PersistentLevel;

	// 포탈 장소 에셋의 목적 레벨 → 대신 열 레벨
	UPROPERTY(EditAnywhere, Category = "Map Variant")
	TMap<TSoftObjectPtr<UWorld>, TSoftObjectPtr<UWorld>> Levels;
};

// 필드 진입 로딩과 공간 전환의 페이드 시퀀스, 기본 시간을 설정
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU World Transition"))
class PROJECTM_API UNUWorldTransitionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 다음 필드 진입부터 로딩 화면과 해당 입력 차단을 적용한다. 서버 위치 복원은 별도로 유지한다.
	UPROPERTY(Config, EditAnywhere, Category = "Field Loading", meta = (DisplayName = "Enable Field Loading", ToolTip = "Show the field loading screen and block input until the field is ready. Disabling this does not bypass server location restoration. Applies on the next field entry."))
	bool bEnableFieldLoading = true;

	// 정상 완료는 실제 준비 상태로 판단하고, 멈춘 로딩만 이 시간 이후 실패 처리한다.
	UPROPERTY(Config, EditAnywhere, Category = "Field Loading", meta = (ClampMin = "1", UIMin = "1", Units = "s", EditCondition = "bEnableFieldLoading"))
	float FieldLoadingTimeoutSeconds = 15.0f;

	// 확정 응답이 유실되면 세션을 종료하고 재접속 위치 복원으로 동기화한다.
	UPROPERTY(Config, EditAnywhere, Category = "Location Transition", meta = (ClampMin = "1", Units = "s"))
	float LocationMoveTimeoutSeconds = 15.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Portal Data")
	TSoftObjectPtr<UDataTable> PortalDefinitionTable;

	// 장소별 에셋을 등록한다. 목록 순서는 지역 대표 진입점의 우선순위다.
	UPROPERTY(Config, EditAnywhere, Category = "Portal Data")
	TArray<TSoftObjectPtr<UNUWorldPortalEntryAsset>> PortalEntryAssets;

	UPROPERTY(Config, EditAnywhere, Category = "Portal Data")
	TArray<FNUWorldMapVariant> MapVariants;

	UPROPERTY(Config, EditAnywhere, Category = "Fade")
	bool bEnableFade = true;

	// 암전 시퀀스 재생 완료 후 이동
	UPROPERTY(Config, EditAnywhere, Category = "Fade", meta = (EditCondition = "bEnableFade"))
	TSoftObjectPtr<ULevelSequence> FadeOutSequence;

	// 화면 복귀 시퀀스 재생 완료 후 입력을 복구
	UPROPERTY(Config, EditAnywhere, Category = "Fade", meta = (EditCondition = "bEnableFade"))
	TSoftObjectPtr<ULevelSequence> FadeInSequence;

	// 시퀀스 미사용 시 암전 시간
	UPROPERTY(Config, EditAnywhere, Category = "Fallback", meta = (ClampMin = "0", UIMin = "0", Units = "s", EditCondition = "bEnableFade"))
	float FadeOutSeconds = 0.15f;

	// 시퀀스 미사용 또는 암전 중 취소 시 화면 복귀 시간
	UPROPERTY(Config, EditAnywhere, Category = "Fallback", meta = (ClampMin = "0", UIMin = "0", Units = "s", EditCondition = "bEnableFade"))
	float FadeInSeconds = 0.2f;
};
