#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldTransition/Data/NUWorldTransitionTypes.h"
#include "NUWorldStreamingSubsystem.generated.h"

class ULevel;
class ULevelStreaming;

// 로드한 공간의 참조 횟수와 소유 여부를 보관
USTRUCT()
struct FNUWorldStreamingRecord
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<ULevelStreaming> StreamingLevel;

	FNUWorldDestination Destination;

	// 0일 때만 공간 해제 가능
	int32 Users = 0;
	double RequestedAt = 0.0;

	// 직접 로드한 레벨만 해제
	bool bOwned = false;
};

// 목적 레벨의 동적 로딩과 재사용을 담당
// 마지막 사용이 끝나면 직접 로드한 공간을 해제
UCLASS()
class PROJECTM_API UNUWorldStreamingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// 공간 사용 시작
	// 로딩 완료는 GetPreparation으로 확인
	bool AcquireSpace(const FNUWorldDestination& Destination, FNUWorldSpaceHandle& OutHandle, ENUWorldTransitionFailure& OutFailure);

	ENUWorldSpacePreparation GetPreparation(const FNUWorldSpaceHandle& Handle, ENUWorldTransitionFailure& OutFailure) const;
	// 준비가 완료된 레벨만 반환
	ULevel* GetPreparedLevel(const FNUWorldSpaceHandle& Handle) const;
	ULevelStreaming* GetStreamingLevel(const FNUWorldSpaceHandle& Handle) const;

	// 마지막 사용이 끝나면 공간 해제
	void ReleaseSpace(FNUWorldSpaceHandle& Handle);

private:
	void RemoveSpace(const FGuid& Id);

	UPROPERTY(Transient)
	TMap<FGuid, FNUWorldStreamingRecord> Spaces;

	static constexpr double LoadTimeoutSeconds = 15.0;
};
