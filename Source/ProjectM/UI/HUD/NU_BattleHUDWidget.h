// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseBattleWidget.h"
#include "UI/NUScreenMotion.h"
#include "NU_BattleHUDWidget.generated.h"

/*
전투 중 "상시" 표시되는 HUD 컨테이너.

담당 범위
  - 턴 번호 / 현재 페이즈 표시
  - 생존 중인 아군·적군 전투원 ID 목록 유지, Panel_Allies/Panel_Enemies 에
    CombatantStatusWidgetClass 인스턴스를 목록 크기만큼 채워 넣는다
  - 로컬/서버 전투 밖에서는 자기 자신을 Collapsed 로 접는다

담당하지 않는 범위
  - 패리 화면 / 교감 등 상황에 따라 갈아끼우는 연출  -> Switcher 계열 위젯이 담당
  - 각 전투원 위젯 내부의 HP/Mana 구독/표시           -> UNU_CombatantStatusWidget (ASC Attribute 구독)

전투원 목록은 페이즈 전환 시점에만 재계산한다. 매 프레임 폴링이 아니라
ANUBattleEncounter 의 페이즈 통지와 서버 전투 상태/재생 완료 통지에서 갱신한다.

Txt_Turn/Txt_Phase 는 BindWidgetOptional 이라 WBP 에 같은 이름의 TextBlock 을 두면
그래프 배선 없이 C++ 이 직접 갱신한다 (UI/Common/NU_PlayerVitalWidget 과 같은 패턴).
*/

class UTextBlock;
class UPanelWidget;
class UNU_CombatantStatusWidget;

UCLASS()
class PROJECTM_API UNU_BattleHUDWidget : public UNU_BaseBattleWidget
{
	GENERATED_BODY()

public:
	/** 현재 CommandSelection 페이즈인지 — 커맨드 안내 표시 조건. */
	UFUNCTION(BlueprintPure, Category = "NU|Battle|HUD")
	bool IsCommandPhase() const;

protected:
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;

	virtual void HandleEncounterChanged(ANUBattleEncounter* NewEncounter) override;
	virtual void HandlePhaseChanged(ENUBattlePhase NewPhase, int32 TurnNumber) override;
	virtual void HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 생존 전투원 ID 목록을 Encounter 에서 다시 읽고, 패널의 전투원 위젯을 새로 채운다. */
	void RefreshCombatants();

	/** Panel 을 비우고 CombatantIds 개수만큼 CombatantStatusWidgetClass 를 만들어 채운다. */
	void RebuildCombatantPanel(UPanelWidget* Panel, const TArray<int32>& CombatantIds, ANUBattleEncounter* Encounter);

	/**
	 * 실행 대기 중인 행동 순서를 Encounter 에서 다시 읽고 Panel_TurnOrder 를 채운다.
	 * ActionExecution 페이즈에서만 의미 있는 데이터라 그 외 페이즈에는 비운다 (지난 턴 순서가 남아 보이지 않게).
	 */
	void RefreshTurnOrder();

	/** Encounter 유무에 따라 HUD 전체를 켜고 끈다. */
	void ApplyBattleVisibility();

	/** TurnText/PhaseText 를 CachedTurnNumber/CachedPhase 기준으로 다시 그린다. */
	void RefreshHUDText();

	/** 목록/페이즈가 갱신된 뒤 1회 발화 — WBP 에서 바 위젯을 재구성한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|HUD")
	void BP_OnHUDRefreshed();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	TArray<int32> AllyCombatantIds;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	TArray<int32> EnemyCombatantIds;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UTextBlock> Txt_Turn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UTextBlock> Txt_Phase;

	/** 아군/적군 전투원 상태 위젯을 만들 때 쓸 클래스. */
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	TSubclassOf<UNU_CombatantStatusWidget> CombatantStatusWidgetClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UPanelWidget> Panel_Allies;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UPanelWidget> Panel_Enemies;

	/**
	 * 행동 순서 칸 클래스(선택). 지정하면 순서 패널에 숫자 대신 이 위젯을 전투원마다 하나씩 놓는다 (SetCombatant로 이름 등이 채워진다 ---
	 * 체력 막대가 없는 가벼운 칸을 쓰면 머리 위 상태 표시와 겹치지 않는다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	TSubclassOf<UNU_CombatantStatusWidget> TurnOrderEntryClass;

	/** 턴·페이즈 표시 묶음(선택). 페이즈가 바뀔 때 한 번 반짝인다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UWidget> PhasePill;

	FNUScreenMotion HudMotion;

	/** 행동 순서대로 전투원 상태 위젯을 나열하는 패널. 없어도 동작한다(BindWidgetOptional). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UPanelWidget> Panel_TurnOrder;
};
