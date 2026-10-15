#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Input/Reply.h"
#include "NUFieldLoadingSubsystem.generated.h"

class ANUPlayerController;
class AWorldSettings;
class SWidget;
class SButton;
class UGameViewportClient;

// Level Instance의 로드, 표시 대기 목록 및 실패 정보를 모은다.
struct FNUFieldLevelReadiness
{
	int32 InstanceCount = 0;
	TArray<FString> PendingInstances;
	FString FailedInstance;

	bool IsReady() const { return PendingInstances.IsEmpty() && FailedInstance.IsEmpty(); }
};

// 필드 맵 전환 동안 로딩 화면과 입력 차단을 유지하고 Level Instance 표시 준비를 확인한다.
UCLASS()
class PROJECTM_API UNUFieldLoadingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 맵을 열기 전에 호출하고, 새 컨트롤러가 준비되면 입력 차단을 이어받는다.
	void BeginLoading(const FString& FieldMapName);
	void HandleFieldControllerReady(ANUPlayerController* Controller);

	// 대기 중인 검사 결과는 로딩 화면과 위치 복원이 공유한다.
	FNUFieldLevelReadiness InspectLevelInstances(UWorld* World) const;

	// 필드 로딩 화면(실패 화면 포함)이 떠 있는 동안 true. 진입 연출(프롤로그)은 이게 걷힌 뒤에 시작한다.
	bool IsLoadingField() const { return bLoading || bFailed; }
	// 입장 실패 화면이 떠 있는 동안 true
	bool HasFailedField() const { return bFailed; }

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	bool TickLoading(float DeltaSeconds);
	// 레벨 표시, 위치 복원, 도착 지점 바닥 충돌, 서버 입장 확정을 확인한다.
	void CheckFieldReady();

	void ShowLoadingScreen();

	void FinishLoading();
	// 실패 시 필드 입력 차단은 유지하고 초기화면 복귀 UI를 표시한다.
	void FailLoading(const FString& Reason);
	void UpdateLoadingStage(const FString& Stage);

	FReply ReturnToLogin();
	// 이 로딩 화면이 요청한 입력·커서 제어만 해제한다.
	void ReleaseController();
	// 입력이 차단된 초기 로딩 동안 엔진의 추가 비동기 로딩 예산을 사용한다.
	void PrioritizeLevelLoading(UWorld* World);
	void RestoreLevelLoadingPriority();

	FString TargetMapName;
	TWeakObjectPtr<UWorld> FieldWorld;
	TWeakObjectPtr<AWorldSettings> PrioritizedWorldSettings;
	bool bPreviousHighPriorityLoading = false;

	mutable TWeakObjectPtr<UWorld> InspectedWorld;
	mutable FNUFieldLevelReadiness PendingLevelReadiness;
	mutable double NextLevelInspectionAt = 0.0;
	static constexpr float LevelInspectionInterval = 0.05f;

	TWeakObjectPtr<ANUPlayerController> BlockedController;

	TWeakObjectPtr<UGameViewportClient> LoadingViewport;
	TSharedPtr<SWidget> LoadingWidget;
	TSharedPtr<SButton> ReturnButton;

	// 월드 교체, 일시정지 중에도 실제 경과 시간으로 제한 시간을 검사한다.
	FTSTicker::FDelegateHandle ReadyCheckTicker;
	double LoadingStartedAt = 0.0;
	float LoadingTimeoutSeconds = 15.0f;

	FString LoadingStage;
	bool bFailed = false;
	bool bLoading = false;
	bool bOverlayAdded = false;
};
