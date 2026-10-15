// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/NU_BattleSwitcherWidget.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"

void UNU_BattleSwitcherWidget::InitializeWidget()
{
	// 부모가 구독을 끝내고 현재 상태를 1회 흘려주므로 HandleEncounterChanged 가 이미 호출된다.
	Super::InitializeWidget();

	// 초기 표시 확정. 전투 밖이면 None -> Collapsed.
	ApplyScreen(ResolveScreenForPhase(CachedPhase));
}

ENUBattleScreen UNU_BattleSwitcherWidget::GetCurrentScreen() const
{
	return CurrentScreen;
}

ENUBattleResult UNU_BattleSwitcherWidget::GetBattleResult() const
{
	return CachedResult;
}

void UNU_BattleSwitcherWidget::HandleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
	// Super 가 Encounter 델리게이트를 갈아끼우고 현재 페이즈를 흘려주므로,
	// 그 과정에서 HandlePhaseChanged 가 먼저 호출된다.
	Super::HandleEncounterChanged(NewEncounter);

	// 전투를 떠났으면 결과를 비우고 접는다
	if (NewEncounter == nullptr)
	{
		CachedResult = ENUBattleResult::None;
		RefreshResultText();
		ApplyScreen(ENUBattleScreen::None);
	}
}

void UNU_BattleSwitcherWidget::HandlePhaseChanged(ENUBattlePhase NewPhase, int32 TurnNumber)
{
	Super::HandlePhaseChanged(NewPhase, TurnNumber);

	// 결과는 델리게이트로 오지 않는다.
	// ANUBattleEncounter::NotifyEncounterCompleted 가 HasAuthority 로 막혀 있어서
	// OnEncounterCompleted 는 서버에서만 발화한다. 클라이언트는 복제된 BattleResult 를 직접 읽어야 한다.
	// BattleResult 와 Phase 는 같은 갱신에 실려 오지만 OnRep 순서는 보장되지 않으므로,
	// 페이즈 통지마다 다시 읽어 늦게 도착한 결과를 따라잡는다.
	if (const ANUBattleEncounter* Encounter = GetBattleEncounter())
	{
		const ENUBattleResult Result = Encounter->GetBattleResult();
		if (Result != ENUBattleResult::None)
		{
			CachedResult = Result;
			RefreshResultText();
		}
	}

	ApplyScreen(ResolveScreenForPhase(NewPhase));
}

void UNU_BattleSwitcherWidget::HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result)
{
	Super::HandleEncounterCompleted(Encounter, Result);

	// 리슨 서버(호스트)에서는 이 델리게이트가 로컬로도 오므로 그때는 여기서 바로 받는다.
	// 순수 클라이언트에서는 호출되지 않으므로 HandlePhaseChanged 쪽 경로가 본선이다.
	if (Result != ENUBattleResult::None)
	{
		CachedResult = Result;
		RefreshResultText();
	}

	ApplyScreen(ENUBattleScreen::Result);
}

ENUBattleScreen UNU_BattleSwitcherWidget::ResolveScreenForPhase(ENUBattlePhase Phase) const
{
	switch (Phase)
	{
	case ENUBattlePhase::Preparing:
		return ENUBattleScreen::Intro;

	case ENUBattlePhase::Ending:
	case ENUBattlePhase::Ended:
		return ENUBattleScreen::Result;

	// 커맨드 선택창은 PC 의 BattleCommandWidget, 패리 바는 PC 의 ParryCueWidget 이 담당한다.
	// 이 구간에 라우터가 끼어들면 그 위젯들을 덮어버리므로 접힌 상태를 유지한다.
	case ENUBattlePhase::CommandSelection:
	case ENUBattlePhase::ActionExecution:
	case ENUBattlePhase::None:
	default:
		return ENUBattleScreen::None;
	}
}

void UNU_BattleSwitcherWidget::ApplyScreen(ENUBattleScreen NewScreen)
{
	if (CurrentScreen == NewScreen)
	{
		return;
	}

	const ENUBattleScreen OldScreen = CurrentScreen;
	CurrentScreen = NewScreen;

	// 띄울 화면이 없으면 접어서 아래 HUD 의 입력을 막지 않는다
	SetVisibility(CurrentScreen == ENUBattleScreen::None
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);

	// 스위처는 선택 사항이다. 슬롯이 지정된 화면만 전환한다.
	if (IsValid(ScreenSwitcher))
	{
		if (const int32* SlotIndex = ScreenSlotIndices.Find(CurrentScreen))
		{
			ScreenSwitcher->SetActiveWidgetIndex(*SlotIndex);
		}
	}

	BP_OnScreenChanged(CurrentScreen, OldScreen);
}

void UNU_BattleSwitcherWidget::RefreshResultText()
{
	if (!IsValid(Txt_Result))
	{
		return;
	}

	FText ResultText;
	switch (CachedResult)
	{
	case ENUBattleResult::Victory:
		ResultText = FText::FromString(TEXT("승리"));
		break;

	case ENUBattleResult::Defeat:
		ResultText = FText::FromString(TEXT("패배"));
		break;

	case ENUBattleResult::Fled:
		ResultText = FText::FromString(TEXT("도주"));
		break;

	case ENUBattleResult::None:
	default:
		ResultText = FText::GetEmpty();
		break;
	}

	Txt_Result->SetText(ResultText);
}
