// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "UI/Base/NU_BaseInGameWidget.h"
#include "NU_BaseBattleWidget.generated.h"

/*
인게임 전투를 위한 베이스 위젯

전투 진행은 ANUBattleEncounter 가 담당하고, 로컬 플레이어의 ASC 가
현재 참여 중인 Encounter 를 들고 있다(UNUAbilitySystemComponent::GetBattleEncounter).
구독 수명은 2단인데, 그중 1단은 부모가 이미 처리한다.

  1) ASC->OnBattleEncounterChanged   : 전투 입장/퇴장  -> UNU_BaseInGameWidget 이 담당
  2) Encounter 델리게이트            : 페이즈/전환연출/종료 -> 이 클래스가 담당
                                       (Encounter 가 바뀔 때마다 재구독)

즉 이 클래스는 부모의 HandleEncounterChanged 통지를 받아 Encounter 델리게이트를 갈아끼우는
역할만 추가한다. "전투 중인가" 판정 자체는 부모의 IsInBattle() / GetBattleEncounter() 를 쓴다.

패리는 Encounter 가 아니라 로컬 PlayerController 의 UNUParryComponent 에서 판정되므로
그쪽 델리게이트를 따로 구독한다.

각 이벤트는 Handle*_Internal(구독 진입점) -> Handle*(C++ 확장) + BP_On*(BP 확장) 순으로 흐른다.
*/

class ANUBattleEncounter;
class UNUParryComponent;

UCLASS(Abstract)
class PROJECTM_API UNU_BaseBattleWidget : public UNU_BaseInGameWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "NU|Battle")
	UNUParryComponent* GetParryComponent() const;

protected:
	// 부모(UNU_BaseUserWidget)가 NativeConstruct / NativeDestruct 에서 호출한다
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;

	// C++ 확장 포인트 (기본은 빈 함수)

	/**
	 * 전투 입장(NewEncounter != nullptr) / 퇴장(nullptr).
	 * 이 클래스의 구현이 Encounter 델리게이트 재구독과 페이즈 스냅샷 재생을 담당하므로,
	 * 오버라이드할 때 Super 호출을 빼먹으면 페이즈 통지가 끊긴다.
	 */
	virtual void HandleEncounterChanged(ANUBattleEncounter* NewEncounter) override;

	virtual void HandlePhaseChanged(ENUBattlePhase NewPhase, int32 TurnNumber);
	virtual void HandleTransitionCue(ENUTransitionCue Cue);
	virtual void HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result);
	virtual void HandleParryJudged(bool bSuccess);

	// BP 확장 포인트
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle") void BP_OnPhaseChanged(ENUBattlePhase NewPhase, int32 TurnNumber);
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle") void BP_OnTransitionCue(ENUTransitionCue Cue);
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle") void BP_OnEncounterCompleted(ENUBattleResult Result);
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle") void BP_OnParryJudged(bool bSuccess);

private:
	// 구독 / 해제
	void BindEncounterDelegates(ANUBattleEncounter* Encounter);
	void UnbindEncounterDelegates();
	void BindParryComponent();
	void UnbindParryComponent();

	// 델리게이트 진입점.
	// Encounter 쪽은 비다이나믹 델리게이트(DECLARE_MULTICAST_DELEGATE_*)라 UFUNCTION 이 필요 없고
	// AddUObject 로 붙인 뒤 RemoveAll(this) 로 해제한다.
	void HandlePhaseChanged_Internal(ENUBattlePhase NewPhase, int32 TurnNumber);
	void HandleTransitionCue_Internal(ENUTransitionCue Cue);
	void HandleEncounterCompleted_Internal(ANUBattleEncounter* Encounter, ENUBattleResult Result);

	// 패리만 다이나믹 델리게이트(FOnNUParryJudged)라 UFUNCTION() 필수
	UFUNCTION()
	void HandleParryJudged_Internal(bool bSuccess);

private:
	/**
	 * 현재 델리게이트를 걸어둔 Encounter.
	 * 부모의 CachedEncounter 는 "어느 전투에 참여 중인가"(상태)이고
	 * 이쪽은 "어느 Encounter 에 구독을 걸어뒀는가"(장부)라서 목적이 다르다.
	 * 둘을 하나로 합치면 재구독 여부를 판단할 근거가 사라진다.
	 */
	TWeakObjectPtr<ANUBattleEncounter> BoundEncounter;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle")
	TWeakObjectPtr<UNUParryComponent> CachedParryComponent;

	/** 마지막으로 통지받은 페이즈. Encounter 가 없으면 None. */
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle")
	ENUBattlePhase CachedPhase = ENUBattlePhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle")
	int32 CachedTurnNumber = 0;
};
