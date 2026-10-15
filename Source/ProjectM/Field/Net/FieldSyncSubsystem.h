// 필드 위치 동기화 서비스.
//
// 역할 분담:
//   UEmberServerManager  — 연결/인증/패킷 송수신 (전송 계층)
//   UFieldSyncSubsystem  — 필드 기능 (내 위치 보고 + 남들 캐릭터 관리)  ← 이 파일
//
// ── 게임 코드 사용법 (이게 전부다) ──────────────────────────────────────────
//   UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();
//   Sync->EnterField();                          // 로그인 후 1회
//   Sync->SetActivity(EFieldActivity::Battle);   // 전투 진입 (남들 눈엔 "전투중")
//   Sync->SetActivity(EFieldActivity::Idle);     // 전투 종료
//
// 내 위치 전송과 남들 캐릭터 생성/이동/삭제는 전부 자동이다.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Field/FieldPlayerCharacter.h"      // EFieldFacing
#include "Field/Net/FieldRemoteCharacter.h"  // EFieldActivity
#include "FieldSyncSubsystem.generated.h"

class UEmberServerManager;
class UNUWorldLocationComponent;
struct FGrpcRevGameLocationMove;

/** 이동 보고 계기 (서버 FieldMoveReason과 값이 1:1). 서버에선 통계용으로만 쓴다. */
UENUM(BlueprintType)
enum class EFieldMoveReason : uint8
{
	Start = 0,
	FacingChange = 1,
	Stop = 2,
	Heartbeat = 3,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemotePlayerSpawned, AFieldRemoteCharacter*, RemotePlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemotePlayerDespawned, int64, PlayerId);

UCLASS()
class PROJECTM_API UFieldSyncSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── 게임 코드용 ──
	UFUNCTION(BlueprintCallable, Category = "Ember|Field")
	void EnterField();
	// 부활 후 현재 위치의 장소로 서버 가시성을 다시 확정한다.
	bool EnterFieldAtLocation(int32 LocationId);

	UFUNCTION(BlueprintCallable, Category = "Ember|Field")
	void LeaveField();

	// 전환 소유자별로 위치 송신을 중단한다. 마지막 차단 해제 시 기준을 초기화한다.
	void SuspendPositionReporting(const UObject* Requester);
	void ResumePositionReporting(const UObject* Requester, bool bReportCurrentPosition = true);
	bool IsPositionReportingSuspended() const;

	/** 전투 진입/이탈. 값이 바뀔 때만 서버로 나간다. */
	UFUNCTION(BlueprintCallable, Category = "Ember|Field")
	void SetActivity(EFieldActivity NewActivity);

	// 정령 생성, 전환 시 전달
	UFUNCTION(BlueprintCallable, Category = "Ember|Field")
	void SetSpirit(int32 NewSpiritId);

	/**
	 * 내 정령 id를 직접 다시 읽어서 반영한다. 정령이 바뀐 쪽에서 효출
	 *
	 * id를 넘겨받지 않는 이유: PlayerState는 전원분이 모든 클라에 복제되므로
	 * 값을 밀어넣는 방식이면 남의 PlayerState 변경이 내 정령 id를 덮어쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ember|Field")
	void RefreshMySpirit();

	UFUNCTION(BlueprintPure, Category = "Ember|Field")
	bool IsInField() const { return bInField; }
	// IsInField는 요청 중에도 true이므로 로딩 완료는 서버 응답 상태로 판단한다.
	bool HasConfirmedFieldEntry() const { return bInField && bFieldEntryConfirmed; }
	bool HasFieldEntryFailed() const { return bFieldEntryFailed; }

	// 서버에 보고하고 있는 내 활동 상태. 화면 상태와 출처가 같아 어긋나지 않는다
	UFUNCTION(BlueprintPure, Category = "Ember|Field")
	EFieldActivity GetMyActivity() const;

	UFUNCTION(BlueprintPure, Category = "Ember|Field")
	AFieldRemoteCharacter* FindRemotePlayer(int64 PlayerId) const;

	// 캐릭터 idx로 원격 캐릭터를 찾음, 시야 밖이면 nullptr
	UFUNCTION(BlueprintPure, Category = "Ember|Field")
	AFieldRemoteCharacter* FindRemoteByCharacterIdx(int64 CharacterIdx) const;

	UPROPERTY(BlueprintAssignable, Category = "Ember|Field")
	FOnRemotePlayerSpawned OnRemotePlayerSpawned;

	UPROPERTY(BlueprintAssignable, Category = "Ember|Field")
	FOnRemotePlayerDespawned OnRemotePlayerDespawned;

	// ── 서버 푸시 수신 (UEmberServerManager가 호출) ──
	void OnAppeared(int64 PlayerId, const FNUFieldAppearance& Data);
	void OnDisappeared(int64 PlayerId);
	void OnMoved(int64 PlayerId, const FVector& Location, EFieldFacing Facing);
	void OnActivityChanged(int64 PlayerId, EFieldActivity Activity);
	void OnSpiritChanged(int64 PlayerId, int32 SpiritId);
	void HandleEnterResponse(int32 ResultCode, const FString& ResultMessage, int32 AreaId, int32 LocationId, bool bIndoor);
	// 서버가 마지막으로 확정한 공간이다. 문 출입의 from_location_id는 이 값을 사용한다.
	bool GetConfirmedSpace(int32& OutAreaId, int32& OutLocationId, bool& bOutIndoor) const;
	void ApplyConfirmedSpace(int32 AreaId, int32 LocationId, bool bIndoor);
	// 야외 구역 이동 요청의 응답이면 소비하고, 문 출입 응답이면 false를 반환한다.
	bool HandleLocationMoveResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response);
	// 2402 응답의 서버 현재 장소를 확정 상태에 반영한다.
	bool ReconcileLocationMoveFailure(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameLocationMove* Response);
	bool IsLocationMoveReady(int32 LocalLocationId) const;
	void EvaluateOutdoorLocation();

	// ── 서브시스템 ──
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool EnterFieldInternal(int32 LocationIdOverride);
	/** 내 위치를 보낼 이유가 있으면 보낸다 (시작 / 방향전환 / 정지 / 이동 중 안전망). */
	void TickReportMyPosition(float DeltaSeconds);

	/** 활성 정령의 Naia Data Index. 없으면 0 */
	int32 GetMySpiritId() const;

	AFieldPlayerCharacter* GetMyCharacter() const;
	UEmberServerManager* GetServer() const;
	void BindWorldLocation();
	void UnbindWorldLocation();
	void TrySendOutdoorLocationMove();
	void ResetOutdoorLocationMove();
	int32 ResolveServerLocationId(const FString& ResultMessage, const FGrpcRevGameLocationMove* Response) const;

	UFUNCTION()
	void HandleLocalLocationChanged();

	UPROPERTY()
	TMap<int64, TObjectPtr<AFieldRemoteCharacter>> Remotes;

	bool bInField = false;
	bool bFieldEntryConfirmed = false;
	bool bFieldEntryFailed = false;
	bool bWasMoving = false;
	FVector SentLocation = FVector::ZeroVector;	// z값 차이가 발생해서 z 차이또한 보정해줘야 함
	EFieldFacing SentFacing = EFieldFacing::Down;
	EFieldActivity MyActivity = EFieldActivity::Idle;
	float TimeSinceSend = 0.0f;
	int32 MySpiritId = -1;
	int32 ConfirmedAreaId = 0;
	int32 ConfirmedLocationId = 0;
	bool bConfirmedIndoor = false;
	TWeakObjectPtr<UNUWorldLocationComponent> BoundWorldLocation;
	int32 DesiredOutdoorLocationId = 0;
	int32 PendingOutdoorFromLocationId = 0;
	int32 PendingOutdoorToLocationId = 0;
	int32 PendingOutdoorAreaId = 0;
	FString PendingOutdoorMutationId;
	FVector PendingOutdoorLocation = FVector::ZeroVector;
	int32 PendingOutdoorFacing = 0;
	bool bOutdoorLocationMovePending = false;
	bool bOutdoorMismatchRetried = false;
	TSet<TWeakObjectPtr<const UObject>> PositionReportBlockers;
};
