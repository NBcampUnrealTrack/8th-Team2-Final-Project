// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseBattleWidget.h"
#include "NU_BattleSwitcherWidget.generated.h"

/*
전투 중 "상황에 따라 갈아끼우는" 전체 화면 연출의 라우터.
UNU_BattleHUDWidget(상시 표시 컴포저)의 짝이며, 그보다 위 레이어에 올라간다.

담당 범위
  - 현재 페이즈를 보고 어떤 화면을 띄울지 결정한다. 그게 전부다.
  - 보여줄 화면이 없으면 자기 자신을 Collapsed 로 접어 아래 HUD 를 가리지 않는다.

담당하지 않는 범위 — 로직을 여기 두지 않는다
  - 각 화면의 내용            -> WidgetSwitcher 슬롯에 놓인 자식 위젯
  - 커맨드 선택창             -> ANUPlayerController 의 BattleCommandWidget
  - 패리 타이밍 바            -> ANUPlayerController 의 ParryCueWidget
                                 (UNUParryComponent 가 프레임 단위로 직접 갱신하므로 소유자를 옮기지 않는다)

화면 전환은 WBP 의 UWidgetSwitcher 를 직접 몰거나(ScreenSwitcher + ScreenSlotIndices),
BP_OnScreenChanged 만 받아 BP 에서 처리해도 된다. 둘 중 하나만 쓰면 된다.
*/

class UWidgetSwitcher;
class UTextBlock;

/** 라우터가 띄울 수 있는 화면. 페이즈보다 개수가 적다 — 여러 페이즈가 한 화면에 모인다. */
UENUM(BlueprintType)
enum class ENUBattleScreen : uint8
{
	// 띄울 전용 화면이 없는 상태.
	// 전투 밖, 그리고 커맨드 선택 / 행동 실행처럼 다른 소유자가 화면을 담당하는 구간.
	None,

	// 전투 개시 (ENUBattlePhase::Preparing)
	Intro,

	// 승리 / 패배 / 도주 (ENUBattlePhase::Ending, Ended)
	Result
};

UCLASS()
class PROJECTM_API UNU_BattleSwitcherWidget : public UNU_BaseBattleWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "NU|Battle|Switcher")
	ENUBattleScreen GetCurrentScreen() const;

	/** 결과 화면에 쓸 승패. 결과 화면이 아니면 None. */
	UFUNCTION(BlueprintPure, Category = "NU|Battle|Switcher")
	ENUBattleResult GetBattleResult() const;

protected:
	virtual void InitializeWidget() override;

	virtual void HandleEncounterChanged(ANUBattleEncounter* NewEncounter) override;
	virtual void HandlePhaseChanged(ENUBattlePhase NewPhase, int32 TurnNumber) override;
	virtual void HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result) override;

	/** 페이즈 -> 화면 매핑 규칙. 규칙만 바꾸고 싶으면 여기만 오버라이드한다. */
	virtual ENUBattleScreen ResolveScreenForPhase(ENUBattlePhase Phase) const;

	/** 결정된 화면을 실제 표시에 반영한다. 화면이 바뀌지 않았으면 아무것도 하지 않는다. */
	void ApplyScreen(ENUBattleScreen NewScreen);

	/** Txt_Result 를 CachedResult 기준으로 다시 그린다. BattleResult 의 OnRep 순서가 Phase 보다 늦게 도착할 수 있어, 화면 전환과 별개로 CachedResult 가 바뀔 때마다 호출한다. */
	void RefreshResultText();

	/** 화면이 실제로 바뀐 뒤 1회 발화. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|Switcher")
	void BP_OnScreenChanged(ENUBattleScreen NewScreen, ENUBattleScreen OldScreen);

protected:
	/**
	 * 화면을 담은 스위처. 없어도 동작한다(BindWidgetOptional) —
	 * 그 경우 BP_OnScreenChanged 만 발화하므로 BP 에서 직접 전환하면 된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|Switcher")
	TObjectPtr<UWidgetSwitcher> ScreenSwitcher;

	/** 결과 화면의 승패 텍스트. WBP 에 같은 이름의 TextBlock 을 두면 그래프 배선 없이 C++ 이 직접 갱신한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|Switcher")
	TObjectPtr<UTextBlock> Txt_Result;

	/** 화면 -> ScreenSwitcher 슬롯 인덱스. 지정되지 않은 화면은 스위처를 건드리지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|Switcher")
	TMap<ENUBattleScreen, int32> ScreenSlotIndices;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Switcher")
	ENUBattleScreen CurrentScreen = ENUBattleScreen::None;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Switcher")
	ENUBattleResult CachedResult = ENUBattleResult::None;
};
