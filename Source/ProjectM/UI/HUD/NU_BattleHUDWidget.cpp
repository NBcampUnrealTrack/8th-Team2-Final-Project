// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/NU_BattleHUDWidget.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Net/NUBattleNetActionQueueSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/Common/NU_CombatantStatusWidget.h"

void UNU_BattleHUDWidget::InitializeWidget()
{
	// 부모가 ASC / Encounter / 패리 구독을 끝내고 현재 상태를 1회 흘려준다.
	// 그 과정에서 HandleEncounterChanged / HandlePhaseChanged 가 이미 호출되므로
	// 여기서는 표시 상태만 최종 확정한다.
	Super::InitializeWidget();

	// 서버의 전투 종료 통지는 마지막 행동 연출보다 먼저 도착할 수 있다.
	// 재생이 끝났을 때도 갱신해야 결과창 위에 TURN 표시가 남지 않는다.
	if (UWorld* World = GetWorld())
	{
		if (UNUBattleNetActionQueueSubsystem* Queue = World->GetSubsystem<UNUBattleNetActionQueueSubsystem>())
		{
			Queue->OnPlaybackDrained.RemoveAll(this);
			Queue->OnPlaybackDrained.AddWeakLambda(this, [this]()
			{
				if (GetBattleEncounter() == nullptr)
				{
					HandleEncounterChanged(nullptr);
				}
			});
		}
	}

	ApplyBattleVisibility();
}

void UNU_BattleHUDWidget::ReleaseWidget()
{
	if (UWorld* World = GetWorld())
	{
		if (UNUBattleNetActionQueueSubsystem* Queue = World->GetSubsystem<UNUBattleNetActionQueueSubsystem>())
		{
			Queue->OnPlaybackDrained.RemoveAll(this);
		}
	}
	Super::ReleaseWidget();
}

bool UNU_BattleHUDWidget::IsCommandPhase() const
{
	return CachedPhase == ENUBattlePhase::CommandSelection;
}

void UNU_BattleHUDWidget::HandleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
	Super::HandleEncounterChanged(NewEncounter);

	// BaseInGameWidget은 서버 전투/무대 변화도 이 경로로 알리지만,
	// 로컬 Encounter가 없는 서버 전투의 턴/페이즈 캐시는 별도로 채워야 한다.
	if (NewEncounter == nullptr)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UNUBattleNetSubsystem* BattleNet = GameInstance
			? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
		const UWorld* World = GetWorld();
		const UNUBattleNetActionQueueSubsystem* Queue = World
			? World->GetSubsystem<UNUBattleNetActionQueueSubsystem>() : nullptr;
		const bool bPlaybackBusy = Queue && Queue->IsPlaybackBusy();

		if (BattleNet && BattleNet->IsInBattle())
		{
			CachedTurnNumber = BattleNet->GetTurn();
			// TurnBegin은 LastOrder를 비우고, Order는 실행 순서를 채운다.
			// 마지막 명령 제출 직후의 빈 Pending 목록만으로 실행 단계를 판단하지 않는다.
			CachedPhase = bPlaybackBusy || !BattleNet->GetLastOrder().IsEmpty()
				? ENUBattlePhase::ActionExecution : ENUBattlePhase::CommandSelection;
		}
		else if (IsInBattle())
		{
			// 서버 미러가 먼저 비어도 연출 중에는 마지막 턴 번호를 유지한다.
			CachedPhase = bPlaybackBusy ? ENUBattlePhase::ActionExecution : ENUBattlePhase::Ending;
		}
		else
		{
			CachedTurnNumber = 0;
			CachedPhase = ENUBattlePhase::None;
		}
	}

	RefreshCombatants();
	ApplyBattleVisibility();
	RefreshHUDText();
	RefreshTurnOrder();
	BP_OnHUDRefreshed();
}

void UNU_BattleHUDWidget::HandlePhaseChanged(ENUBattlePhase NewPhase, int32 TurnNumber)
{
	Super::HandlePhaseChanged(NewPhase, TurnNumber);

	// 사망으로 목록이 줄어드는 시점이 행동 실행 중이라, 페이즈가 넘어갈 때마다 재계산한다
	RefreshCombatants();

	if (PhasePill != nullptr)
	{
		HudMotion.AddPulse(PhasePill, 0.f, 0.45f, 1.f, 1.06f);
		HudMotion.Start();
	}

	// 표시 조건에 페이즈가 들어가므로 페이즈 통지마다 다시 판단해야 한다.
	// (Super 가 CachedPhase 를 먼저 갱신한 뒤 이 함수를 호출한다)
	ApplyBattleVisibility();

	RefreshHUDText();

	// BuildActionQueue 가 ActionExecution 진입 직전에 ActionQueue 를 다시 정렬해두므로,
	// 이 페이즈로 넘어오는 통지 시점에는 항상 최신 순서를 읽을 수 있다.
	RefreshTurnOrder();

	BP_OnHUDRefreshed();
}

void UNU_BattleHUDWidget::HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result)
{
	Super::HandleEncounterCompleted(Encounter, Result);

	// 접기는 페이즈 통지(Ending/Ended)나 ASC 의 Encounter 해제 중 먼저 오는 쪽이 처리한다.
	// 결과 연출(승리/패배)은 Switcher 계열이 담당하므로 여기서는 목록만 정리한다.
	RefreshCombatants();
	RefreshTurnOrder();
	BP_OnHUDRefreshed();
}

void UNU_BattleHUDWidget::RefreshCombatants()
{
	AllyCombatantIds.Reset();
	EnemyCombatantIds.Reset();

	ANUBattleEncounter* Encounter = GetBattleEncounter();
	if (Encounter != nullptr)
	{
		Encounter->GetLivingCombatantIds(ENUBattleTeam::Ally, AllyCombatantIds);
		Encounter->GetLivingCombatantIds(ENUBattleTeam::Enemy, EnemyCombatantIds);
	}

	RebuildCombatantPanel(Panel_Allies, AllyCombatantIds, Encounter);
	RebuildCombatantPanel(Panel_Enemies, EnemyCombatantIds, Encounter);
}

void UNU_BattleHUDWidget::RebuildCombatantPanel(UPanelWidget* Panel, const TArray<int32>& CombatantIds, ANUBattleEncounter* Encounter)
{
	if (!IsValid(Panel) || CombatantStatusWidgetClass == nullptr)
	{
		return;
	}

	Panel->ClearChildren();

	for (int32 CombatantId : CombatantIds)
	{
		UNU_CombatantStatusWidget* StatusWidget = WidgetTree->ConstructWidget<UNU_CombatantStatusWidget>(CombatantStatusWidgetClass);
		if (IsValid(StatusWidget))
		{
			StatusWidget->SetCombatant(Encounter, CombatantId);
			Panel->AddChild(StatusWidget);
		}
	}
}

void UNU_BattleHUDWidget::RefreshTurnOrder()
{
	if (!IsValid(Panel_TurnOrder))
	{
		return;
	}

	Panel_TurnOrder->ClearChildren();

	// ActionQueue 는 ActionExecution 진입 시점에만 다시 정렬된다. 그 외 페이즈에는 지난 턴 순서가
	// 남아 있을 수 있으므로, 의미 있는 구간에서만 채운다.
	if (CachedPhase != ENUBattlePhase::ActionExecution)
	{
		return;
	}

	ANUBattleEncounter* Encounter = GetBattleEncounter();
	if (Encounter == nullptr)
	{
		return;
	}

	TArray<int32> TurnOrderIds;
	Encounter->GetUpcomingTurnOrder(TurnOrderIds);

	// 칸 클래스가 있으면 이름이 든 칸으로, 차례로 미끄러져 들어온다
	if (TurnOrderEntryClass != nullptr)
	{
		for (int32 CombatantId : TurnOrderIds)
		{
			UNU_CombatantStatusWidget* Entry = WidgetTree->ConstructWidget<UNU_CombatantStatusWidget>(TurnOrderEntryClass);
			if (IsValid(Entry))
			{
				Entry->SetCombatant(Encounter, CombatantId);
				Panel_TurnOrder->AddChild(Entry);
			}
		}
		HudMotion.AddChildren(Panel_TurnOrder, 0.05f, 0.06f, 0.3f, FVector2D(16.f, 0.f));
		HudMotion.Start();
		return;
	}

	// HP/Mana 는 Panel_Allies/Panel_Enemies 에 이미 떠 있다. 여기서 CombatantStatusWidgetClass 를
	// 또 만들면 같은 전투원의 체력/마나가 화면에 두 번 찍혀 중복으로 보이므로, 순서만 보여주는
	// 가벼운 텍스트로 표시한다.
	for (int32 CombatantId : TurnOrderIds)
	{
		UTextBlock* OrderText = WidgetTree->ConstructWidget<UTextBlock>();
		if (IsValid(OrderText))
		{
			OrderText->SetText(FText::AsNumber(CombatantId));
			Panel_TurnOrder->AddChild(OrderText);
		}
	}
}

void UNU_BattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	HudMotion.Advance(InDeltaTime);
}

void UNU_BattleHUDWidget::RefreshHUDText()
{
	if (IsValid(Txt_Turn))
	{
		Txt_Turn->SetText(FText::FromString(FString::Printf(TEXT("%02d"), CachedTurnNumber)));
	}

	if (IsValid(Txt_Phase))
	{
		const FText PhaseName = CachedPhase == ENUBattlePhase::CommandSelection
			? NSLOCTEXT("NUBattle", "PhaseCommand", "행동 선택")
			: (CachedPhase == ENUBattlePhase::ActionExecution
				? NSLOCTEXT("NUBattle", "PhaseAction", "행동")
				: NSLOCTEXT("NUBattle", "PhasePreparing", "전투"));
		Txt_Phase->SetText(PhaseName);
	}
}

void UNU_BattleHUDWidget::ApplyBattleVisibility()
{
	// 상시 HUD 지만 "전투 중 상시" 이므로 필드에서는 접어둔다.
	// Ending/Ended 는 Switcher 가 전체 화면 결과를 덮는 구간이라 함께 접는다 —
	// 결과 화면 배경이 반투명해도 턴/페이즈 표시가 비치지 않게 하고, 그리기 비용도 없앤다.
	const bool bShouldShow = IsInBattle()
		&& CachedPhase != ENUBattlePhase::Ending
		&& CachedPhase != ENUBattlePhase::Ended;

	// SelfHitTestInvisible: 자식 버튼은 살리고 컨테이너 자체는 히트 테스트에서 제외
	SetVisibility(bShouldShow
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}
