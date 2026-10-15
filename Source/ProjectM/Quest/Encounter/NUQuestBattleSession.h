#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Battle/NUBattleTypes.h"
#include "Engine/EngineTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Quest/Encounter/NUQuestTutorialBattleRules.h"
#include "NUQuestBattleSession.generated.h"

class ANUBattleEncounter;
class ANUQuestActionActor;
class UNUAbilitySystemComponent;
class UNUQuestBattleDefinition;
class UNUQuestComponent;
class UUserWidget;
class UWidgetComponent;
struct FNUBattleActionContext;
struct FNUBattleCommandRequest;

// 연습·진정·대련 전투 하나 동안만 사는 퀘스트 전투 상태. 전투 연결부가 인카운터마다 하나 만들고, 전투가 끝나면 버린다.
// 판정 규칙(FNUQuestTutorialBattleRules)·임시 어빌리티·동요/안개/돌 표시를 소유하고, 퀘스트 진행은 QuestComponent의 공개 API로만 올린다.
UCLASS(Transient)
class PROJECTM_API UNUQuestBattleSession : public UObject
{
	GENERATED_BODY()

public:
	// 미완료 목표와 로컬 백엔드를 확인한 뒤 인카운터에 연결한다. 실패하면 아무것도 바꾸지 않는다.
	bool Initialize(ANUBattleEncounter* InEncounter, UNUQuestBattleDefinition* InDefinition, UNUQuestComponent* InQuestComponent);

	// 등록한 델리게이트·임시 어빌리티·표시를 회수한다. 여러 번 불러도 된다.
	void Shutdown();

	bool IsRunning() const;
	ANUBattleEncounter* GetEncounter() const { return Encounter.Get(); }
	UNUQuestBattleDefinition* GetDefinition() const { return Definition; }
	UNUQuestComponent* GetQuestComponent() const { return QuestComponent.Get(); }

	// 현재 전투 상태 조회
	bool IsCalming() const;
	bool IsEnemyBurrowed() const;
	bool ShouldEnemyBurrow() const;
	bool HasSucceeded() const;

	// 전투 연결부가 턴과 행동 규칙을 전달한다.
	void BeginTurn(int32 TurnNumber);
	bool IsAbilityAllowed(FGameplayTag AbilityId) const;
	void AdjustAction(const FNUBattleCommandRequest& Command, FNUBattleActionContext& Context) const;
	float AdjustSpeed(bool bEnemy, float Speed) const;

	// 실제 피해와 명중·방어 반응으로 성공 조건을 갱신한다.
	bool InterceptDamage(UNUAbilitySystemComponent* Target, float& Damage);
	void NotifyHit(UNUAbilitySystemComponent* Actor, FGameplayTag AbilityId);
	void NotifyReaction(UNUAbilitySystemComponent* Actor, bool bParry);

	// GAS 실행 후에만 효과를 적용한다. 위젯은 조회와 기존 명령 제출만 담당한다.
	void ApplyCalm();
	void ApplyBurrow();

	// 땅 진정 전투의 매몰 턴에만 돌 상호작용을 허용한다.
	bool CanMoveStone() const;
	bool MoveStone();

private:
	void HandlePhaseChanged(ENUBattlePhase Phase, int32 TurnNumber);
	void HandleCompleted(ANUBattleEncounter* InEncounter, ENUBattleResult Result);

	UFUNCTION()
	void HandleEncounterEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	UNUAbilitySystemComponent* GetPlayerASC() const;
	void GrantAbilities();
	void ReleaseAbilities();
	void PublishResult();

	// HP와 독립된 동요 위젯을 이벤트가 발생할 때만 갱신한다.
	void RefreshPresentation();
	void ClearPresentation();
	// 연습 횟수를 전투 튜토리얼 안내(UNUQuestEncounterSubsystem)에 넘기거나 비운다.
	void PublishPracticeState(bool bClear) const;

	// 달래기·돌 옮기기의 결과 큐, 나이아가라는 블루프린트 큐(GCN_Quest*)가 띄운다.
	void PlayResultCue(AActor* Target, FGameplayTag CueTag, const FVector& Location) const;

	TWeakObjectPtr<ANUBattleEncounter> Encounter;
	TWeakObjectPtr<UNUQuestComponent> QuestComponent;

	UPROPERTY()
	TObjectPtr<UNUQuestBattleDefinition> Definition;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> AgitationWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> Mist;

	TWeakObjectPtr<ANUQuestActionActor> WorldObject;

	// 영구 퀘스트 기록과 분리된 임시 데이터. 서버 동기화·저장 대상이 아니다.
	TWeakObjectPtr<UNUAbilitySystemComponent> GrantedASC;
	TArray<FGameplayAbilitySpecHandle> GrantedHandles;
	FNUQuestTutorialBattleRules Rules;
	bool bPublished = false;
};
