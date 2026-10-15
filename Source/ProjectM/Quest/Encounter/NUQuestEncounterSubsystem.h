#pragma once

#include "CoreMinimal.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "Battle/NUBattleTypes.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUQuestEncounterSubsystem.generated.h"

class AActor;
class ANUBattleEncounter;
class ANUPlayerState;
class UNUQuestBattleDefinition;
class UNUQuestPracticeGuideWidget;
class UWorld;
enum class ENUQuestBattleKind : uint8;
struct FGrpcRevGameSVR_Battle_Started;
struct FGrpcRevGameSVR_Battle_End;

// 서버가 확정한 퀘스트 전투 문맥과 연습 진행. 동요 규칙은 이 계약에 포함되지 않는다.
struct FNUQuestBattleServerState
{
	int32 QuestId = 0;
	int32 ObjectiveId = 0;
	int32 BattleMode = 0;
	int32 PracticeCount = 0;
	int32 PracticeRequired = 0;
	bool bQuestSuccess = false;
};

// 서버가 승인한 진정 도전. 시작 승인(105) 응답을 기다리는 동안부터 로컬 진정 전투가 끝나 결과(106)를 보낼 때까지 유지한다.
struct FNUQuestCalmAttempt
{
	TWeakObjectPtr<ANUPlayerState> Player;
	FNUBattleEncounterRequest Placement;
	int32 QuestId = 0;
	int32 ObjectiveId = 0;
	FString AttemptId;
	TWeakObjectPtr<ANUBattleEncounter> Encounter;
	double RequestedAt = 0.0;

	bool IsPending() const { return QuestId > 0; }
};

// 퀘스트 특수 전투의 시작 요청과 서버 진행 상태를 소유한다.
UCLASS()
class PROJECTM_API UNUQuestEncounterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 연습 횟수는 표시용이며 퀘스트 완료 판정과 분리한다.
	FSimpleMulticastDelegate OnPracticeStateChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	bool TryStartQuestEncounter(ANUPlayerState* Player, const FNUBattleEncounterRequest& Placement, bool bServerSession);
	void HandleStarted(const FGrpcRevGameSVR_Battle_Started& Message);
	void HandlePracticeCount(int64 InBattleId, int32 Count);
	void HandleEnd(const FGrpcRevGameSVR_Battle_End& Message);

	// 이 전투 정의로 서버가 승인한 진정 도전이 열려 있는지. 온라인에서 로컬 진정 전투를 여는 조건이다.
	bool IsCalmAttemptApproved(const UNUQuestBattleDefinition* Definition) const;
	void HandleCalmStartResponse(int32 ResultCode, const FString& AttemptId);
	void HandleCalmResultResponse(int32 ResultCode);

	const FNUQuestBattleServerState& GetQuestBattleState() const
	{
		return QuestBattleState;
	}

	const FNUQuestBattleServerState& GetLastQuestBattleResult() const
	{
		return LastQuestBattleResult;
	}

	// 오프라인 연습 전투(로컬 규칙)의 횟수. 온라인은 서버 상태(QuestBattleState)를 쓴다.
	// 둘 중 진행 중인 쪽을 전투 튜토리얼 안내(UNUQuestPracticeGuideWidget)가 보여 준다.
	void SetLocalPracticeState(ENUQuestBattleKind Kind, int32 Count, int32 Required);
	void ClearLocalPracticeState();

private:
	void HandleBattleStateChanged();
	// 연습 상태가 바뀔 때마다 안내를 띄우거나 갱신하거나 내린다.
	void RefreshPracticeGuide();
	UNUQuestPracticeGuideWidget* EnsurePracticeGuide();

	// 진정 도전: 승인 요청 -> 로컬 진정 전투 -> 결과 보고
	bool RequestClientCalm(ANUPlayerState* Player, const FNUBattleEncounterRequest& Placement, int32 QuestId, int32 ObjectiveId);
	ANUBattleEncounter* StartLocalQuestEncounter(ANUPlayerState* Player, const FNUBattleEncounterRequest& Placement, const UNUQuestBattleDefinition* Battle) const;
	void HandleCalmEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result);

	UFUNCTION()
	void HandleCalmEncounterEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	void ReportCalmResult(bool bSuccess);

	int64 BattleId = 0;
	FNUQuestBattleServerState QuestBattleState;
	FNUQuestBattleServerState LastQuestBattleResult;
	FNUQuestCalmAttempt CalmAttempt;

	ENUQuestBattleKind LocalPracticeKind{};
	int32 LocalPracticeCount = 0;
	int32 LocalPracticeRequired = 0;
	TWeakObjectPtr<UNUQuestPracticeGuideWidget> PracticeGuide;
};
