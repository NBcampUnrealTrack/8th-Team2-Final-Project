// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Base/NU_BaseBattleWidget.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "PlayerController/NUPlayerController.h"
#include "PlayerController/Component/NUParryComponent.h"

void UNU_BaseBattleWidget::InitializeWidget()
{
	// 패리는 Encounter 와 무관하게 PC 컴포넌트에 붙으므로 먼저 구독한다.
	// 그 뒤 Super 가 ASC 를 구독하고 현재 Encounter 를 1회 통지하는데,
	// 그 통지가 아래 HandleEncounterChanged 오버라이드로 들어와 Encounter 델리게이트를 건다.
	BindParryComponent();

	Super::InitializeWidget();
}

void UNU_BaseBattleWidget::ReleaseWidget()
{
	UnbindEncounterDelegates();
	UnbindParryComponent();

	// Super 가 ASC 구독을 해제한다
	Super::ReleaseWidget();
}

UNUParryComponent* UNU_BaseBattleWidget::GetParryComponent() const
{
	return CachedParryComponent.Get();
}

//-----------------------구독-------------------------

void UNU_BaseBattleWidget::BindEncounterDelegates(ANUBattleEncounter* Encounter)
{
	if (BoundEncounter.Get() == Encounter)
	{
		return;
	}

	UnbindEncounterDelegates();

	if (Encounter == nullptr)
	{
		return;
	}

	BoundEncounter = Encounter;

	Encounter->OnBattlePhaseChanged.AddUObject(this, &UNU_BaseBattleWidget::HandlePhaseChanged_Internal);
	Encounter->OnTransitionCue.AddUObject(this, &UNU_BaseBattleWidget::HandleTransitionCue_Internal);
	Encounter->OnEncounterCompleted.AddUObject(this, &UNU_BaseBattleWidget::HandleEncounterCompleted_Internal);
}

void UNU_BaseBattleWidget::UnbindEncounterDelegates()
{
	if (ANUBattleEncounter* Encounter = BoundEncounter.Get())
	{
		Encounter->OnBattlePhaseChanged.RemoveAll(this);
		Encounter->OnTransitionCue.RemoveAll(this);
		Encounter->OnEncounterCompleted.RemoveAll(this);
	}

	BoundEncounter.Reset();
	CachedPhase = ENUBattlePhase::None;
	CachedTurnNumber = 0;
}

void UNU_BaseBattleWidget::BindParryComponent()
{
	if (CachedParryComponent.IsValid())
	{
		return;
	}

	const ANUPlayerController* PC = Cast<ANUPlayerController>(GetNUPlayerController());
	UNUParryComponent* Parry = PC ? PC->GetParryComponent() : nullptr;
	if (Parry == nullptr)
	{
		return;
	}

	CachedParryComponent = Parry;

	Parry->OnParryJudged.RemoveDynamic(this, &UNU_BaseBattleWidget::HandleParryJudged_Internal);
	Parry->OnParryJudged.AddDynamic(this, &UNU_BaseBattleWidget::HandleParryJudged_Internal);
}

void UNU_BaseBattleWidget::UnbindParryComponent()
{
	if (CachedParryComponent.IsValid())
	{
		CachedParryComponent->OnParryJudged.RemoveDynamic(this, &UNU_BaseBattleWidget::HandleParryJudged_Internal);
	}

	CachedParryComponent.Reset();
}

//---------------------------델리게이트 진입점-------------------------

void UNU_BaseBattleWidget::HandlePhaseChanged_Internal(ENUBattlePhase NewPhase, int32 TurnNumber)
{
	CachedPhase = NewPhase;
	CachedTurnNumber = TurnNumber;

	HandlePhaseChanged(NewPhase, TurnNumber);
	BP_OnPhaseChanged(NewPhase, TurnNumber);
}

void UNU_BaseBattleWidget::HandleTransitionCue_Internal(ENUTransitionCue Cue)
{
	HandleTransitionCue(Cue);
	BP_OnTransitionCue(Cue);
}

void UNU_BaseBattleWidget::HandleEncounterCompleted_Internal(ANUBattleEncounter* Encounter, ENUBattleResult Result)
{
	HandleEncounterCompleted(Encounter, Result);
	BP_OnEncounterCompleted(Result);
}

void UNU_BaseBattleWidget::HandleParryJudged_Internal(bool bSuccess)
{
	HandleParryJudged(bSuccess);
	BP_OnParryJudged(bSuccess);
}

//-------------------------C++ 확장 함수---------------------------

void UNU_BaseBattleWidget::HandleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
	Super::HandleEncounterChanged(NewEncounter);

	BindEncounterDelegates(NewEncounter);

	// 상태 스냅샷만 1회 재생한다.
	// 구독 시점이 페이즈 통지보다 늦을 수 있으므로 현재 페이즈는 직접 흘려주고,
	// 전환 연출 / 패리 판정 같은 순간 이벤트는 재생하지 않는다.
	if (NewEncounter)
	{
		HandlePhaseChanged_Internal(NewEncounter->GetPhase(), NewEncounter->GetTurnNumber());
	}
}

void UNU_BaseBattleWidget::HandlePhaseChanged(ENUBattlePhase NewPhase, int32 TurnNumber)
{
}

void UNU_BaseBattleWidget::HandleTransitionCue(ENUTransitionCue Cue)
{
}

void UNU_BaseBattleWidget::HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result)
{
}

void UNU_BaseBattleWidget::HandleParryJudged(bool bSuccess)
{
}
