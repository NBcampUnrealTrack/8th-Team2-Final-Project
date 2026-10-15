#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NULocationRestoreSubsystem.generated.h"

class AFieldPlayerCharacter;
class UNUWalkableInteriorComponent;
struct FGrpcRevGameLocationRestore;
struct FGrpcRevGamePortalArrive;
struct FNUWorldDestination;

// 캐릭터 선택과 필드 진입 사이에서 서버의 마지막 확정 공간을 보관하고 적용한다.
UCLASS()
class PROJECTM_API UNULocationRestoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 캐릭터가 선택될 때 이전 복원 상태를 비우고 서버의 마지막 확정 위치를 요청한다.
	void BeginCharacterRestore(int64 CharacterIdx);
	// 서버 응답을 검증하고 미완료 전환이 있으면 취소 확정까지 이어서 처리한다.
	void HandleLocationRestoreResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationRestore* Response);
	// 복원 과정에서 보낸 전환 취소 응답만 소비하고, 일반 포탈 응답은 기존 시스템에 넘긴다.
	bool HandlePortalArriveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGamePortalArrive* Response);

	// true면 즉시 FieldEnter를 보내도 된다. false면 복원 준비 완료 후 RetryDelegate를 호출한다.
	bool PrepareFieldEntry(AFieldPlayerCharacter* Character, const FSimpleDelegate& RetryDelegate);
	// 복원 직후 FieldEnter가 서버 확정 location_id를 사용하도록 값을 제공한다.
	bool GetFieldEnterLocationId(int32& OutLocationId) const;
	// 부활 뒤에는 로그인 시 복원한 장소가 새 FieldEnter를 덮지 않게 한다.
	void ForgetAppliedLocationAfterRespawn();
	bool IsRecoveryConfirmationValid(int32 AreaId, int32 LocationId, bool bIndoor) const;
	bool IsFieldEntryReady() const { return State == ERestoreState::None || State == ERestoreState::Applied; }
	bool HasFieldEntryFailed() const { return State == ERestoreState::Failed; }
	void CancelFieldEntry();
	void ResetFieldEntry();

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FNURestoreRetryLifetimeTest;
	friend class FNUVerifyInteriorRestoreCommand;
#endif
	enum class ERestoreState : uint8
	{
		None,
		WaitingRestore,
		WaitingTransitionCancel,
		Ready,
		Applying,
		Applied,
		Failed
	};

	struct FRestoreSnapshot
	{
		int64 CharacterIdx = 0;
		int32 AreaId = 0;
		int32 LocationId = 0;
		FVector Location = FVector::ZeroVector;
		int32 Facing = 0;
		bool bIndoor = false;
		bool bHasSavedLocation = false;
		FString TransitionId;
		int32 TransitionState = 0;
	};

	// proto 응답을 필드 진입 이후에도 유지할 수 있는 클라이언트 스냅샷으로 변환한다.
	bool ParseRestoreSnapshot(const FGrpcRevGameLocationRestore& Response, FRestoreSnapshot& OutSnapshot) const;
	// 좌표, 방향, 실내외 상태를 현재 로컬 캐릭터에 반영한다.
	bool ApplySnapshot(AFieldPlayerCharacter* Character);
	// 지역과 장소에 대응하는 레벨 및 기본 진입점 설정을 찾는다.
	bool ResolveRestoreDestination(FNUWorldDestination& OutDestination, int32* OutEntryLocationId = nullptr) const;
	void ReconcileRespawnLocation();
	UNUWalkableInteriorComponent* FindInterior(int32 LocationId) const;
	
	// 서버 단계 완료와 월드 적용 완료를 분리해 캐릭터 선택 화면과 필드 진입 순서를 보장한다.
	void CompleteServerRestore();
	void FailServerRestore(int32 ResultCode, const FString& ResultMessage);
	void HandleWorldRestoreSucceeded();
	void HandleWorldRestoreFailed();
	void QueueFieldEntryRetry();
	void ExecuteFieldEntryRetry();

	ERestoreState State = ERestoreState::None;
	FRestoreSnapshot Snapshot;
	bool bRecoveredLocation = false;
	int64 ActiveCharacterIdx = 0;
	// 필드 진입 직후 Level Instance가 늦게 등록되는 동안 실내 복원을 재시도할 제한 시간이다.
	double InteriorReadyDeadline = 0.0;
	TWeakObjectPtr<AFieldPlayerCharacter> ApplyingCharacter;
	FSimpleDelegate FieldEntryRetryDelegate;
};
