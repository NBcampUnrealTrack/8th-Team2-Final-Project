#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUBattleEventSubsystem.generated.h"

class APlayerState;

// 적 한 마리 처치됐을 때 전달되는 전투 결과
struct FNUBattleEnemyDefeatedEvent
{
	FGuid EncounterId;
	int32 CombatantId = INDEX_NONE;
	FName EnemyId = NAME_None;

	// 해당 처치로 퀘스트 진행 결과를 받을 수 있는 플레이어
	TArray<TWeakObjectPtr<APlayerState>> EligiblePlayerStates;
};

// 전투 승리 시 한 번 전달되는 결과
struct FNUBattleWonEvent
{
	FGuid EncounterId;
	FName EnemyId = NAME_None;

	// 전투에 참여해 결과를 받을 수 있는 플레이어
	TArray<TWeakObjectPtr<APlayerState>> EligiblePlayerStates;
};

// 교감석 포획이 로스터에 등록됐을 때 전달되는 결과
struct FNUBattleNaiaCapturedEvent
{
	FGuid EncounterId;
	FName NaiaId = NAME_None;

	// 포획에 성공해 퀘스트 진행 결과를 받을 플레이어
	TArray<TWeakObjectPtr<APlayerState>> EligiblePlayerStates;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUBattleEnemyDefeated, const FNUBattleEnemyDefeatedEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUBattleWon, const FNUBattleWonEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUBattleNaiaCaptured, const FNUBattleNaiaCapturedEvent&);

/** 
*	전투 시스템이 확정한 결과를 다르 시스템에 전달하기 위한 이벤트 허브
*	퀘스트와 같이 전투 결과가 필요한 시스템은 이 서브시스템을 구독함
*/
UCLASS()
class PROJECTM_API UNUBattleEventSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 전투 시스템에서 확정된 결과를 구독자들에게 발행
	void PublishEnemyDefeated(const FNUBattleEnemyDefeatedEvent& Event);
	void PublishBattleWon(const FNUBattleWonEvent& Event);
	void PublishNaiaCaptured(const FNUBattleNaiaCapturedEvent& Event);

	// 외부 시스템이 구독하는 전투 결과 이벤트
	FOnNUBattleEnemyDefeated OnEnemyDefeated;
	FOnNUBattleWon OnBattleWon;
	FOnNUBattleNaiaCaptured OnNaiaCaptured;
};
