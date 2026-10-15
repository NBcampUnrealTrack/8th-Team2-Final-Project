// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NUBattleNetStageSubsystem.generated.h"

class ANUBattleAvatar;
class APawn;
class ULevelSequence;
class UNUBattleNetSubsystem;
struct FNUServerCombatant;
struct FNUServerBattleStatus;
struct FNUServerJobResource;
struct FNUServerJobState;

DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnBattleNetStageReadyChanged, bool);

// 스폰한 아바타 하나와, 그 아바타가 서 있는 자리의 신원
// 나이아 교체는 들어온 전투원에게 새 번호를 발급하므로 번호만으로는 같은 자리인지 알 수 없음
// 종류와 소유 캐릭터가 같으면 같은 자리로 보고 아바타를 재사용함 
USTRUCT()
struct FNUNetStageAvatar
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ANUBattleAvatar> Avatar = nullptr;

	// 0 캐릭터 · 1 나이아 · 2 적
	int32 Kind = 0;

	// 조종하는 캐릭터, 적이면 0. 파티 전투가 붙으면 나이아가 여럿이라 종류만으로는 안 갈림
	int64 OwnerCharacterIdx = 0;
};

// 넷 전투를 화면에 띄움 - 페이드 전환, 세트 로딩, 전투원 아바타 스폰, HP/SP 반영
// 로컬 전투 ANUBattleEncounter는 그대로 두고 완전히 별개로 동작함
UCLASS()
class PROJECTM_API UNUBattleNetStageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 필드 트리거가 서버 전투를 요청하기 직전에 호출함
	// 전투 배경 서브레벨은 서버가 모르는 클라 전용 정보라 여기서 기억해두고 있어야 함
	void SetPendingStageLevel(const TSoftObjectPtr<UWorld>& StageLevel);

	ANUBattleAvatar* FindAvatar(int32 CombatantId) const;
	bool IsNetStageReady() const;

	// 서버 스냅샷의 HP/SP/생사/상태이상을 아바타에 반영함
	void RefreshVitals();
	void SetVitalsAutoRefreshSuppressed(bool bSuppressed);

	// 행동 큐가 마지막 연출까지 다 재생한 뒤 호출함
	// 전투 종료를 받았을 때 아직 연출이 남아 종료를 미뤄뒀을 때 연출이 끝나면 사용
	void NotifyActionPlaybackDrained();
	bool IsTransitionActive() const;

	// 재생 중인 포획 흔들림 중 가장 오래 남은 시간, 재생 큐가 행동을 미룰 때 물어봄
	float GetCaptureShakeRemainingSeconds() const;

public:
	FNUOnBattleNetStageReadyChanged OnNetStageReadyChanged;
	
private:
	UNUBattleNetSubsystem* GetBattleNet() const;

	// 행동 큐가 아직 재생할 것을 들고 있는지, 없으면(큐 자체가 없으면) false
	bool IsActionPlaybackBusy() const;

	void HandleBattleStateChanged();

	// 전투원 명단이 통째로 갈렸을 때(나이아 교체·재동기화) 아바타를 명단에 맞춰 다시 맞춤
	// 자리가 같으면 아바타를 재사용하고 그림·체력만 갈아끼우며, 짝이 없을 때만 스폰·파괴함
	void ReconcileAvatars();

	// 진형에서 이 전투원이 설 슬롯. 아군은 행이 갈려 한 명씩이고, 적만 번호 순으로 늘어섬
	void ResolveFormationSlot(int32 CombatantId, int32& OutSlotIndex, int32& OutSlotCount) const;

	// 스폰한 아바타를 자리 신원과 함께 등록함
	void RegisterAvatar(ANUBattleAvatar* Avatar, const FNUServerCombatant& Combatant);

	// 서버 스냅샷의 상태이상을 아이콘 뷰로 바꿔 아바타에 반영함, SetVitals 를 부르는 자리마다 같이 부름
	// 재생 중에는 행동 큐가 자기 사본으로 다시 덮으므로 여기 값은 전투 시작·교체 시점의 기준선임
	void PushStatusViews(ANUBattleAvatar* Avatar, const TArray<FNUServerBattleStatus>& Statuses) const;

	// 서버 미러의 직업 자원 · 상태를 체력바에 그림, 상태의 켜짐 · 남은 턴은 서버 턴으로 셈
	void PushJobResourceViews(ANUBattleAvatar* Avatar, const TArray<FNUServerJobResource>& Resources,
		const TArray<FNUServerJobState>& JobStates) const;

	// 진입 순서: 페이드아웃 재생 -> 배경 로딩 요청 -> 페이드아웃 재생 시간만큼 대기
	void BeginSpawningStage();
	void HandleFadeOutFinished();

	// EnsureStageLoaded는 스트리밍을 비동기로 요청만 하고 바로 리턴함
	// 레벨 스트리밍의 OnLevelShown을 기다리고 완료되면 보여줌 
	// 에셋 경로가 잘못되면 OnLevelShown 이벤트가 영영 안 올 수 있어 최대 대기 타이머를 안전망으로 같이 둠
	void HandleStageShown();
	void HandleStageWaitTimeout();
	void FinishWaitingForStage();
	void SetNetStageReady(bool bReady);

	// 아바타 스폰 + 카메라 전환 + 페이드인, 페이드아웃과 배경 스트리밍이 끝난 뒤에만 불림
	void SpawnStage();

	// 종료 순서: 페이드아웃 재생 -> 재생 시간만큼 대기 -> 아바타 정리·카메라 복구 -> 즉시 화면 정상화
	void BeginTeardownStage();
	void TeardownStage();

	ANUBattleAvatar* SpawnAvatarFor(const FNUServerCombatant& Combatant, int32 SlotIndex, int32 SlotCount);
	FTransform GetSpawnTransform(const FNUServerCombatant& Combatant, int32 SlotIndex, int32 SlotCount) const;

	// 넷 전투 중엔 필드 폰과 전투 아바타가 동시에 보이면 안 되므로 필드 폰을 숨김/복구함
	void SetLocalFieldPawnHidden(bool bHidden);

	// 배경 세트가 들고 있는 카메라로 시점을 옮김, 세트에 카메라가 없으면 내 캐릭터 아바타를 비춤
	void EnterBattleCamera();

	// 전투 시작 전 시점으로 되돌림
	void RestoreFieldCamera();

	// NUBattleDataSettings::NetFadeOutSequence/NetFadeInSequence를 로컬 플레이어 컨트롤러에서 재생함
	void PlayFadeOut();
	void PlayFadeIn();
	void StopFadeTransition();

	// 페이드아웃 시퀀스 길이, 그동안 화면이 완전히 검게 유지되므로 그 사이에 스폰 준비를 끝내야 함
	float GetFadeOutSeconds() const;

	class ANUPlayerController* GetLocalPlayerController() const;

private:
	// 이번 전투에 쓸 배경, SetPendingStageLevel로 미리 받아둠
	TSoftObjectPtr<UWorld> PendingStageLevel;

	// 전투원 Id -> 스폰한 아바타, 로컬의 FNUBattleParticipant::AvatarActor에 해당
	// 교체로 번호가 바뀌면 키만 갈아끼우고 아바타는 그대로 둠 (ReconcileAvatars)
	UPROPERTY()
	TMap<int32, FNUNetStageAvatar> Avatars;

	// 전투 시작 시 숨긴 필드 폰, 종료되면 되돌림
	TWeakObjectPtr<APawn> HiddenFieldPawn;

	// 전투 시작 전 시점, 종료되면 되돌림
	TWeakObjectPtr<AActor> PreviousViewTarget;

	FTimerHandle StageWaitTimerHandle;

	// UNUBattleNetSubsystem::IsInBattle() 전이를 감지하기 위함 (false -> true 가 시작, 그 반대가 종료)
	bool bWasInBattle = false;

	bool bVitalsAutoRefreshSuppressed = false;

	// 전투 종료 시점에 행동 큐가 재생 중이어서 정리를 미뤄둔 상태
	bool bTeardownDeferred = false;
	bool bNetStageReady = false;
};
