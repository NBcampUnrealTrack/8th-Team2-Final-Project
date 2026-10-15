// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Naia/Data/NUNaiaTypes.h"
#include "NUNaiaRosterComponent.generated.h"


class ANUPlayerState;
class ANUSpiritState;
class UNUNaiaNetSubsystem;
struct FNUNaiaDefinition;
struct FNUServerNaiaSlot;
enum class ENUNaiaUpdateReason : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUNaiaRosterChanged);

// 플레이어가 보유한 나이아를 관리하는 컴포넌트
// 파티는 액터, 파티0번이 전투에 참가함, 보관함은 데이터
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUNaiaRosterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUNaiaRosterComponent();
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	
	// --- 서버 처리 ---
	
	// 파티나 보관함에 빈 자리가 있는지 확인함
	bool CanAcceptNaia() const;
	
	// 포획 성공 시 호출함, 파티에 빈칸이 있다면 파티, 없다면 보관함으로 이동
	// Level 0 이면 SpawnLevel을 따라감, 스폰된 액터가 필요하면 GetParty로 조회 해서 사용
	bool AcquireNaia(int32 NaiaId, int32 Level, int64* OutInstanceId = nullptr);

	// 최초 직업 선택 후 시작 나이아를 한 번만 확정함
	bool AuthorizeStarterNaiaSelection();
	bool IsStarterNaiaSelectionAuthorized() const { return bStarterNaiaSelectionAuthorized; }
	bool CanSelectStarterNaia(int32 NaiaId, FString& OutError) const;
	bool SelectStarterNaia(int32 NaiaId, FString& OutError);
	
	// 빈칸 = 이동, 빈칸X = 교환, 인벤토리와 유사하게 작동
	// 전투 중에는 전부 거절함, 전투 중 교체는 아래 SwapPartyOrderForBattle 만 통과함
	bool MoveNaia(int32 FromContainer, int32 FromSlot, int32 ToContainer, int32 ToSlot);

	// 전투 중 파티 순서 교체, 인카운터에서만 호출함
	// 아바타 변경, 명령 환불이 필요하므로 별도 처리
	bool SwapPartyOrderForBattle(int32 SlotA, int32 SlotB);

	// 서버에서 목록을 받아와서 반영함, 컨테이너 하나를 통째로 교체함
	// 지금은 디버그 시작에 사용, 이후에 서버 연동시 로그인 복원에 사용함
	bool ApplyServerNaiaList(int32 Container, const TArray<FNUNaiaInstance>& Entries);

	// UNUNaiaNetSubsystem이 들고 있는 현재 서버 칸 목록으로 파티·보관함을 통째로 교체함
	// 부분 갱신(SVR_Naia_Updated)은 서브시스템에서 처리하기 때문에 여기선 항상 통째로 교체하면 됨
	bool ApplyServerNaiaSlots(const TArray<FNUServerNaiaSlot>& Slots);

	// 회복 서비스가 보관함 나이아의 체력도 함께 회복할 때 사용
	bool RestoreStorageHealth();
	bool RecoverStorageHealth(float HealthRatio);

	// --- 조회 ---
	
	ANUSpiritState* GetActiveNaia() const;	// 파티슬롯 0번이 활성화
	const TArray<TObjectPtr<ANUSpiritState>>& GetParty() const;
	const TArray<FNUNaiaInstance>& GetStorage() const;
	
	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	int32 GetPartyCount() const;
	
	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	bool IsPartyFull() const;

	// 파티와 보관함의 빈 자리 합계
	// 포획할 나이아들이 정원을 넘지 않는지 인카운터가 확인할때 사용함
	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	int32 GetFreeSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	int32 GetMaxPartySize() const;

	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	int32 GetMaxStorageSlots() const;

	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	bool HasSelectedStarterNaia() const { return StarterNaiaInstanceId != 0; }

	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	int32 GetSelectedStarterNaiaId() const { return SelectedStarterNaiaId; }

	UFUNCTION(BlueprintPure, Category = "NU|Naia")
	int64 GetStarterNaiaInstanceId() const { return StarterNaiaInstanceId; }

	// 편성창(인벤토리와 유사함) 토글, 로컬 클라에서만 동작함
	UFUNCTION(BlueprintCallable, Category = "NU|Naia")
	void ToggleRosterUI();


	// --- 클라이언트 -> 서버 ---
	
	// 클라이언트에서 위치를 옮긴것을 서버에 반영하라고 알림
	UFUNCTION(Server, Reliable)
	void ServerMoveNaia(int32 FromContainer, int32 FromSlot, int32 ToContainer, int32 ToSlot);

	// 디버그 전용
	UFUNCTION(Server, Reliable, Category = "NU|Naia|Debug")
	void ServerDebugAcquireNaia(int32 NaiaId, int32 Level);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "NU|Naia")
	void ServerSelectStarterNaia(int32 NaiaId);
	
	// FNUNaiaList 변경 시 처리
	void HandleStorageReplicated();
	
public:
	// 로스터 변경 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnNUNaiaRosterChanged OnRosterChanged;
	
protected:
	UFUNCTION()
	void OnRep_PartyNaias();

	UFUNCTION()
	void OnRep_StarterNaiaSelection();

	UFUNCTION(Client, Reliable)
	void ClientStarterNaiaSelectionRejected(const FString& Reason);
	
	// 파티 0번이 바뀌었으면 ActiveSpiritState 처리
	// 필드 아바타 생성, 필드 동기화
	void SyncActiveNaia();
	
	// 파티, 보관함 조작
	bool TakeFromParty(int32 Slot, FNUNaiaInstance& OutEntry);	// 파티에서 제외
	bool TakeFromStorage(int32 Slot, FNUNaiaInstance& OutEntry);	// 보관함에서 제외
	ANUSpiritState* PutToParty(const FNUNaiaInstance& Entry, int32 Slot);	// 파티에 등록
	bool PutToStorage(FNUNaiaInstance Entry, int32 Slot); // 보관함에 등록
	
	// 순서 변경, 액터를 그대로 두고 자리만 맞바꾸기
	bool SwapPartySlots(int32 SlotA, int32 SlotB);
	bool SwapStorageSlots(int32 SlotA, int32 SlotB);
	
	// 액터 -> 데이터, 보관함에 넣을 형태로 변경함
	FNUNaiaInstance MakeSnapshot(const ANUSpiritState* Naia) const;

	// 새로 얻은 나이아의 데이터를 생성함, Level이 0이면 SpawnLevel 다라감
	// 서버 로스터를 아직 못 받은 로컬 생성분이라 InstanceId는 음수로 임시 발급함
	FNUNaiaInstance MakeNewEntry(int32 NaiaId, int32 Level) const;
	TSubclassOf<ANUSpiritState> ResolveNaiaStateClass() const;
	
	int32 FindFreeStorageSlot() const;
	FNUNaiaInstance* FindMutableStorageBySlot(int32 SlotNo);

	ANUPlayerState* GetOwningPlayerState() const;
	const FNUNaiaDefinition* FindDefinition(int32 NaiaId) const;

	// 전투 중에는 스냅샷·스폰이 끼면 안 되므로 편성을 막아야 함
	bool IsInBattle() const;

	// 이 컴포넌트가 로컬 클라 자신의 PlayerState에 붙어 있는지
	// 본인의 로스터 상태만 반영할 수 있도록 확인하는 용도
	bool IsLocalPlayerRoster() const;

	// 서버 나이아 로스터 상태를 들고 있는 서브시스템 (없으면 nullptr)
	UNUNaiaNetSubsystem* GetNaiaNetSubsystem() const;

	// UNUNaiaNetSubsystem::OnRosterNetChanged, 서버 스냅샷을 반영함
	void HandleServerRosterChanged();

	// UNUNaiaNetSubsystem::OnMoveRejected, 이동이 거절되면 UI를 띄움
	void HandleMoveRejected(FText Reason);

	// UNUNaiaNetSubsystem::OnStarterNaiaContractRejected, 계약 실패를 알리고 선택 UI를 다시 활성화함
	void HandleStarterNaiaContractRejected(FText Reason);

private:
	// 파티에 등록된 나이아, 0번 슬롯이 항상 활성화
	UPROPERTY(ReplicatedUsing = OnRep_PartyNaias)
	TArray<TObjectPtr<ANUSpiritState>> PartyNaias;
	
	// 보관함
	UPROPERTY(Replicated)
	FNUNaiaList StorageList;

	// 외부 서버는 양수 영구 ID, 로컬 테스트는 음수 임시 ID를 사용하며 0만 미계약 상태임
	UPROPERTY(ReplicatedUsing = OnRep_StarterNaiaSelection)
	int32 SelectedStarterNaiaId = 0;

	UPROPERTY(Replicated)
	int64 StarterNaiaInstanceId = 0;

	// Q9 진행자가 계약 선택 단계를 열었을 때만 서버에 존재하는 일회성 권한
	bool bStarterNaiaSelectionAuthorized = false;
	
	// 파티 최대 상한
	UPROPERTY(EditDefaultsOnly, meta = (AllowPrivateAccess = "true", ClampMin = 1, ClampMax = 4))
	int32 MaxPartySize = 4;
	
	// 보관함 최대 상한
	UPROPERTY(EditDefaultsOnly, meta = (AllowPrivateAccess = "true", ClampMin = 1))
	int32 MaxStorageSlots = 500;
	
	// PlayerState별 특수 클래스가 필요할 때만 사용하며, 비어 있으면 NU Naia Data 공용 설정을 사용
	UPROPERTY(EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ANUSpiritState> NaiaStateClass;
	
	// 로컬 생성분(디버그 포획 등)에 임시로 발급하는 InstanceId, 매번 1씩 감소함
	// 서버가 발급하는 값은 양수라서 음수로 두면 겹치지 않음, 0은 유효하지 않은 값
	mutable int64 NextLocalInstanceId = -1;

	// 편성창 WBP, PlayerState BP에서 지정
	UPROPERTY(EditAnywhere, Category = "NU|Naia|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UNUNaiaRosterWindowWidget> RosterWindowWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<class UNUSheetWidgetBase> RosterWindowWidget;
};
