#pragma once

#include "CoreMinimal.h"
#include "Recovery/Net/NURecoveryServerTypes.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NURecoveryNetSubsystem.generated.h"

class UEmberServerManager;
struct FGrpcRevGameSVR_Respawn_Result;
enum class EEmberConnState : uint8;

DECLARE_MULTICAST_DELEGATE(FNUOnRecoveryStateChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNUOnRecoveryRequestFailed, int32, const FText&);
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnRecoveryPointRegistered, int32);

// gRPC 상태와 재전송 식별자를 보관한다. HP 보간은 표시용이며 GAS를 변경하지 않는다.
UCLASS()
class PROJECTM_API UNURecoveryNetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 상태 조회
	void BeginCharacterSession(int64 CharacterId);
	bool RequestService(int32 NpcId, bool bRest);
	bool RequestStatus();
	bool RequestRegisterPoint(int32 LocationId);

	// EmberServerManager에서 전달한 응답/부활 푸시 처리
	void HandleResponse(ENURecoveryRequest Request, int32 Code, const FString& Message, const FString& Json);
	void HandleRespawn(const FGrpcRevGameSVR_Respawn_Result& Message);

	// 전투 종료 통보와 실제 거점 이동 완료 구분 및 부활 연출 관리
	void HandleBattleStarted();
	void HandleBattleEnded(int32 Reason);
	void CompleteRespawnPresentation();

	// 현재 선택 캐릭터의 서버 확정 상태와 부활 대기 상태 조회
	bool IsActive() const;
	bool HasState() const { return bHasState; }
	bool IsResting() const { return IsActive() && bHasState && State.RecoveryState == 1; }
	bool IsAwaitingRespawn() const { return bAwaitingRespawn; }
	bool HasPendingRespawn() const { return PendingPresentationDeathId > 0; }
	const FNURecoveryServerState& GetState() const { return State; }
	uint64 GetStateSerial() const { return StateSerial; }

	// 서버 응답 이후의 경과 시간으로 UI 수치만 보간한다. 
	// GAS에는 적용하지 않는다.
	float GetDisplayedPlayerHealth() const;
	float GetDisplayedNaiaRatio(int64 InstanceId, float Fallback) const;

	// 확정 상태 변경 / 표시 갱신 / 요청 실패 구독자에게 전달
	FNUOnRecoveryStateChanged OnStateChanged;
	FNUOnRecoveryStateChanged OnDisplayChanged;
	FNUOnRecoveryRequestFailed OnRequestFailed;
	FNUOnRecoveryPointRegistered OnPointRegistered;

private:
	// 접속 상태와 서버 송신 창구
	UFUNCTION()
	void HandleConnectionChanged(EEmberConnState NewState);
	UEmberServerManager* GetServer() const;

	// 응답 제한 시간 검사와 UI 표시 갱신
	bool Tick(float DeltaSeconds);
	void CaptureNaiaRatios();
	void PublishState();

	// 현재 캐릭터의 마지막 확정 응답
	FNURecoveryServerState State;
	int64 ActiveCharacterId = 0;

	// 사망 ID는 크기 순서가 아닌 동일 여부로 비교하여 중복 이동을 막는다.
	int64 LastPresentedDeathId = 0;
	int64 PendingPresentationDeathId = 0;
	int64 DeathBeforeBattle = 0;
	double RespawnWaitStarted = 0.0;

	// 로컬 반영 횟수 식별자다. 서버 버전 번호가 아니며 HP와 골드의 중복 적용을 막는 데 쓴다.
	uint64 StateSerial = 0;

	// 응답 수신 여부, 캐릭터 선택 여부, 요청 대기 여부는 별도로 관리한다.
	bool bHasState = false;
	bool bCharacterSelected = false;
	bool bAwaitingRespawn = false;
	bool bStatusPending = false;
	bool bCanRetry = false;

	// 결과가 불명확하면 UUID와 NPC를 유지하고, 새 시도에서만 새 UUID를 발급한다.
	ENURecoveryRequest PendingRequest = ENURecoveryRequest::None;
	FString PendingMutationId;
	int32 PendingNpcId = 0;
	int32 PendingLocationId = 0;
	double RequestStarted = 0.0;

	// 조회 응답 제한 시간과 마지막 확정 HP의 표시 기준 시각
	double StatusStarted = 0.0;
	double StateReceived = 0.0;

	// 파티와 보관함의 마지막 서버 HP 비율
	// 1초 Ticker는 표시와 재조회 대기를 갱신한다.
	TMap<int64, float> NaiaRatios;
	FTSTicker::FDelegateHandle TickerHandle;
};
