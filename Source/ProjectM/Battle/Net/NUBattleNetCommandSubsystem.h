// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleServerTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUBattleNetCommandSubsystem.generated.h"

struct IConsoleCommand;
class UNUBattleNetSubsystem;

// 서버 전투에서 내가 지금 뭘 하고 있는지, 로컬 UNUCommandSelectionPhase를 축소한 버전
UENUM(BlueprintType)
enum class ENUBattleNetCommandPhase : uint8
{
	Inactive,			// 서버 전투가 없음
	Selecting,			// 현재 전투원의 명령을 기다리는 중
	Submitting,			// 제출하고 응답 대기
	WaitingForOthers	// 내 전투원은 다 냈고 다른 플레이어 대기
};

// 명령을 받을 전투원이 바뀜 (턴 시작, 다음 전투원, 전부 제출해 비워짐) 없으면 INDEX_NONE
DECLARE_MULTICAST_DELEGATE_OneParam(FNUOnBattleNetCommandTurnChanged, int32);

// 제출이 거절됨, 사유를 UI로 보여줄 자리
DECLARE_MULTICAST_DELEGATE_TwoParams(FNUOnBattleNetCommandRejected, int32, ENUBattleCommandSubmitResult);

// 서버 전투에서 내 전투원 중 누가, 언제 명령해야 하는가만 담당함
// 서버 상태 미러(UNUBattleNetSubsystem)는 그대로 두고, 그 위에 순서 계층 하나만 얹음
// 커서를 직접 들지 않음, 내 전투원 목록과 서버 PendingCombatantIds의 교집합 맨 앞이 곧 "지금 순서"
UCLASS()
class PROJECTM_API UNUBattleNetCommandSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;


	// ── 조회 ──

	// 지금 명령을 받아야 하는 내 전투원, 응답 대기중이거나 없으면 INDEX_NONE
	int32 GetCurrentCombatantId() const;
	const TArray<int32>& GetControllableCombatantIds() const;
	ENUBattleNetCommandPhase GetPhase() const;


	// ── 송신 ──

	// 현재 전투원의 명령을 제출함, CombatantId와 BattleId는 여기서 채움
	// 지금 서버에 보낼 수 없는 행동(NaiaSwap, 교감석 아닌 아이템)이면 보내지 않고 false
	bool SubmitCurrentCommand(ENUBattleRootAction RootAction, int32 SkillId,
		int32 TargetId, int32 InvestSP, int32 ItemId, bool bCommuneItem);

public:
	FNUOnBattleNetCommandTurnChanged OnCommandTurnChanged;
	FNUOnBattleNetCommandRejected OnCommandRejected;

private:
	UNUBattleNetSubsystem* GetBattleNet() const;

	void HandleBattleStateChanged();
	void HandleCommandAckReceived(ENUBattleCommandSubmitResult Result, const FNUServerCommandAck& Ack);

	// 서버 상태가 바뀔 때마다 내 전투원 목록을 다시 만듦, 제출 대기 중이면 목록만 갱신하고 페이즈는 유지함
	void RebuildControllableList();

	void ResetSelection();

	// 같은 전투원으로 중복 알림을 쏘지 않을 때만 델리게이트를 쏨
	void NotifyCurrentCombatantIfChanged();

	int64 GetMyCharacterIdx() const;

	// Side 아군 + 내 캐릭터 소유 + 살아있음
	bool IsMyCombatant(const FNUServerCombatant& Combatant) const;

	// NU.BattleNet 콘솔 디버그 명령 (UNUBattleNetSubsystem과 접두사를 나눠 사용함)
#if !UE_BUILD_SHIPPING
	void RegisterDebugCommands();
	void UnregisterDebugCommands();
	void DumpMyState() const;
#endif

private:
	// 이번 턴에 내가 명령을 낼 전투원, 캐릭터가 앞, 서버 pending과 교차한 파생 상태
	TArray<int32> ControllableCombatantIds;

	// 응답을 기다리는 전투원, 거절 응답엔 전투원 번호가 없어서 여기서 기억해야 함
	int32 PendingSubmissionCombatantId = INDEX_NONE;

	// 목록을 만든 턴, 늦게 온 응답과 턴 경계를 가릴 때 사용
	int32 SelectionTurn = INDEX_NONE;

	// 마지막으로 알린 전투원, 같은 전투원에 중복 알림을 막음 (SVR_Battle_Submit이 자주 옴)
	int32 LastNotifiedCombatantId = INDEX_NONE;

	ENUBattleNetCommandPhase Phase = ENUBattleNetCommandPhase::Inactive;

	// 서버가 아군 전투원의 OwnerCharacterIdx를 안 채운 경고를 전투당 한 번만 남김
	// IsMyCombatant가 const라 mutable로 둠
	mutable bool bWarnedMissingOwner = false;

	// 서버 버그(OwnerCharacterIdx 미기재) 우회용 디버그 스위치, 기본은 꺼짐
	// 켜면 Side==0(아군)이기만 하면 내 것으로 간주함 - 다른 플레이어가 있으면 잘못될 수 있어 기본값이 아님
#if !UE_BUILD_SHIPPING
	bool bAssumeOwnerless = false;

	TArray<IConsoleCommand*> DebugCommands;
#endif
};
