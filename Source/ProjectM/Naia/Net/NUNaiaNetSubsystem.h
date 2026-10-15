// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUNaiaServerTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUNaiaNetSubsystem.generated.h"

struct IConsoleCommand;
class UEmberServerManager;
enum class EEmberConnState : uint8;
struct FGrpcRevGameNaiaList;
struct FGrpcRevGameNaiaStarterSelect;
struct FGrpcRevGameSVR_Naia_Updated;

// 로스터가 서버에서 갱신되면(전체 조회 응답, 이동/포획/전투종료 푸시) 알림
DECLARE_MULTICAST_DELEGATE(FNUOnNaiaRosterNetChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnNaiaRosterUpdated, ENUNaiaUpdateReason);
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnNaiaMoveRejected, FText);
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnStarterNaiaContractRejected, FText);

UCLASS()
class PROJECTM_API UNUNaiaNetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;


	// ── 송신 ──

	// 파티+보관함 전체 조회, 캐릭터 선택 직후 한 번 부르고 이후로는 푸시로 따라감
	void RequestNaiaList();

	// 목적지가 비었으면 이동, 차 있으면 교환
	void RequestNaiaMove(int32 FromContainer, int32 FromSlot, int32 ToContainer, int32 ToSlot);

	// 최종 확정한 스타팅 나이아의 계약 요청. 재전송 시 처리 중인 mutation_id를 재사용함
	bool RequestStarterNaiaContract(int32 NaiaId, FText& OutError);


	// ── 수신, EmberServerManager가 호출 ──

	// NaiaList는 요청 응답 자리로 오지만 response_data가 아니라 타입 있는 본문
	void HandleListResponse(int32 ResultCode, const FGrpcRevGameNaiaList& Message);

	// 부분 갱신 — 파티가 걸리면 파티 4칸이 전부, 보관함은 건드린 칸만 옴
	void HandleUpdated(const FGrpcRevGameSVR_Naia_Updated& Message);

	// NaiaMove의 단순 응답 (result_code만, 실제 결과는 HandleUpdated로 옴)
	void HandleAck(int32 ResultCode, const TCHAR* RequestName);

	// NaiaStarterSelect 응답. 성공 후 최종 개체 데이터는 SVR_Naia_Updated/NaiaList로 확정함
	void HandleStarterSelectResponse(int32 ResultCode, const FString& ResultMessage, const FGrpcRevGameNaiaStarterSelect& Message);


	// ── 조회 ──

	// 서버 로스터를 한 번이라도 받았는지 (편성 이동을 로컬/서버 어느 쪽으로 보낼지 가를 때 사용)
	bool HasReceivedRoster() const;
	bool HasActiveServerSession() const;
	bool HasStarterNaiaContract() const { return StarterNaiaInstanceId > 0; }
	bool IsStarterNaiaContractPending() const { return !PendingStarterNaiaMutationId.IsEmpty(); }
	int32 GetStarterNaiaId() const { return StarterNaiaId; }
	int64 GetStarterNaiaInstanceId() const { return StarterNaiaInstanceId; }

	const TArray<FNUServerNaiaSlot>& GetPartySlots() const;
	const TArray<FNUServerNaiaSlot>& GetStorageSlots() const;
	int32 GetPartySlotCount() const;
	int32 GetStorageSlotCount() const;

	const FNUServerNaiaSlot* FindSlot(int32 Container, int32 SlotNo) const;

public:
	FNUOnNaiaRosterNetChanged OnRosterNetChanged;
	FNUOnNaiaRosterUpdated OnRosterUpdated;
	FNUOnNaiaMoveRejected OnMoveRejected;
	FNUOnStarterNaiaContractRejected OnStarterNaiaContractRejected;

private:
	UFUNCTION()
	void HandleServerStateChanged(EEmberConnState NewState);

	UEmberServerManager* GetServer() const;
	void ResetRosterState();
	bool ResolveStarterNaiaContractFromSlots();
	void ClearPendingStarterNaiaContract();

	// 컨테이너 종류로 나눠 캐시에 반영, bFullSnapshot이면 두 컨테이너 다 통째로 교체
	// 파티가 있으면 파티만 통째로, 보관함은 건드린 칸만 병합 (SVR_Naia_Updated)
	void ApplySlots(const TArray<FNUServerNaiaSlot>& Slots, bool bFullSnapshot);

	UEmberServerManager* GetLoggedInServer(const TCHAR* Operation) const;

	// NU.NaiaNet 콘솔 디버그 명령
#if !UE_BUILD_SHIPPING
	void RegisterDebugCommands();
	void UnregisterDebugCommands();
	void DumpDebugState() const;
#endif

private:
	TArray<FNUServerNaiaSlot> PartySlots;
	TArray<FNUServerNaiaSlot> StorageSlots;

	int32 PartySlotCount = 0;
	int32 StorageSlotCount = 0;

	// NaiaList 응답을 한 번이라도 받았는지, 못 받았으면 로스터가 비어있는지 아직 모르는 상태
	bool bRosterReceived = false;

	// 계약 완료 판단은 영구 개체 ID가 기준이고, 종 ID는 로스터 칸에서 역으로 찾음
	int64 StarterNaiaInstanceId = 0;
	int32 StarterNaiaId = 0;

	// 응답을 못 받아 같은 요청을 재전송할 때 mutation_id를 바꾸면 중복 생성 위험이 있으므로 보존함
	int32 PendingStarterNaiaId = 0;
	FString PendingStarterNaiaMutationId;

#if !UE_BUILD_SHIPPING
	TArray<IConsoleCommand*> DebugCommands;
#endif
};
