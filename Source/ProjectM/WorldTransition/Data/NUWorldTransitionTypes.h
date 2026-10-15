#pragma once

#include "CoreMinimal.h"
#include "NUWorldTransitionTypes.generated.h"

class UWorld;

// 출입구의 이동 요청 방식
UENUM(BlueprintType)
enum class ENUWorldPortalActivation : uint8
{
	Interaction,
	Overlap,
	// CSV의 Interact 표기를 지원하며 기존 에셋 값은 유지한다.
	Interact
};

// 테이블의 어느 끝점에 배치된 포탈인지 지정한다.
UENUM(BlueprintType)
enum class ENUWorldPortalSide : uint8
{
	Source,
	Destination
};

// 페이드 종료까지 포함한 전환 단계
UENUM(BlueprintType)
enum class ENUWorldTransitionState : uint8
{
	Idle,
	PreparingDestination,
	Moving,
	FadingOut,
	FadingIn,
	AwaitingServerApproval,
	AwaitingServerConfirmation
};

// 마지막 전환의 실패 원인
UENUM(BlueprintType)
enum class ENUWorldTransitionFailure : uint8
{
	None,
	InvalidRequest,			// 캐릭터 또는 목적지 설정 오류
	TransitionInProgress,	// 다른 이동 진행 중
	InputBlocked,			// 다른 기능에서 입력 차단 중
	BattleActive,			// 전투 진행 중
	UnsupportedDestination UMETA(Hidden),	// 기존 에셋의 열거형 값 보존용
	LoadRequestRejected,	// 레벨 로딩 요청 실패
	LoadTimeout,			// 로딩 시간 초과
	EntryPointMissing,		// 도착 지점 누락 또는 ID 중복
	TeleportFailed,			// 순간이동 실패
	UnexpectedUnload,		// 목적 레벨이 예기치 않게 해제됨
	Cancelled,				// 전환 취소
	UnsafeEntryPoint,		// 도착 위치의 충돌 또는 바닥 검사 실패
	PortalDataInvalid,
	PortalRequirementsNotMet,
	ServerContractUnavailable,
	ServerRejected,
	ServerResponseInvalid
};

// 목적 레벨과 도착 지점, 배치 정보를 전달하는 요청 데이터
USTRUCT(BlueprintType)
struct PROJECTM_API FNUWorldDestination
{
	GENERATED_BODY()

	// 미지정 시 월드의 기본 레벨(Persistent Level) 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	TSoftObjectPtr<UWorld> Level;

	// 목적 레벨의 EntryPointId
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	FName EntryPointId = NAME_None;

	// 같은 레벨도 배치가 다르면 별도 공간으로 구분한다. 기본 레벨에는 미적용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	FTransform LevelTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	bool bUseFade = true;

	// 테이블 또는 서버 결과에서 해석한 지역. 0이면 기존 맵 매핑을 사용한다.
	int32 ResolvedAreaId = 0;

	bool IsConfigured() const { return !EntryPointId.IsNone(); }
	bool IsPersistentLevelDestination() const { return Level.IsNull(); }
};

// 사용 중인 공간을 식별하는 핸들로, 빈 ID는 월드의 기본 레벨
struct PROJECTM_API FNUWorldSpaceHandle
{
	FGuid Id;

	bool IsPersistent() const { return !Id.IsValid(); }
};

// 목적지 로딩 결과
enum class ENUWorldSpacePreparation : uint8
{
	Pending,
	Ready,
	Failed
};
