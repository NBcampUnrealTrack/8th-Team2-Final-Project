// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Base/NU_BaseFieldWidget.h"

void UNU_BaseFieldWidget::InitializeWidget()
{
	// Super 가 ASC 를 구독하고 현재 전투 상태를 1회 통지하므로,
	// 그 통지가 HandleEncounterChanged -> ApplyFieldVisibility 로 흘러 초기 표시가 맞춰진다.
	Super::InitializeWidget();
}

bool UNU_BaseFieldWidget::ShouldShowFieldUI() const
{
	return !IsInBattle();
}

void UNU_BaseFieldWidget::HandleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
	Super::HandleEncounterChanged(NewEncounter);

	ApplyFieldVisibility();
}

void UNU_BaseFieldWidget::ApplyFieldVisibility()
{
	const bool bVisible = ShouldShowFieldUI();

	// SelfHitTestInvisible: 자식 버튼은 클릭 가능하게 두고 컨테이너 자체만 히트 테스트에서 제외
	SetVisibility(bVisible
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	BP_OnFieldVisibilityChanged(bVisible);
}
