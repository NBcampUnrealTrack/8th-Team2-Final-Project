// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseInGameWidget.h"
#include "NU_BaseFieldWidget.generated.h"

/*
필드 / 마을 등 "전투가 아닌" 인게임 상태의 UI 베이스.

UNU_BaseBattleWidget 의 거울상이다. 같은 판정(부모의 IsInBattle)을 쓰지만 반응이 반대다.
  - 전투 위젯 : 전투 중에 펴짐
  - 필드 위젯 : 전투 중에 접힘

전투 진입은 카메라 전환 / 레벨 시퀀스 연출을 타고 오는데, 그 연출 위에 필드 HUD 가
남아 있으면 안 되므로 접기를 베이스가 책임진다. 자식이 잊어버릴 수 있는 종류의 일이다.

Encounter 내부 진행(페이즈 / 턴 / 전환연출)은 구독하지 않는다 —
필드 UI 는 "전투인지 아닌지"까지만 알면 되고, 그 이상은 알 필요가 없다.

상호작용 프롬프트는 여기 두지 않는다.
ANUPlayerController 가 InteractionIndicatorWidget 을 따로 소유하고
UNUInteractorComponent::OnCurrentInteractableChanged 를 직접 받아 화면 좌표까지 갱신하므로,
같은 델리게이트를 여기서 또 구독하면 소유자가 둘이 된다.
*/

class ANUBattleEncounter;

UCLASS(Abstract)
class PROJECTM_API UNU_BaseFieldWidget : public UNU_BaseInGameWidget
{
	GENERATED_BODY()

public:
	/** 필드 UI 가 보여야 하는 상태인지 (= 전투 중이 아님). */
	UFUNCTION(BlueprintPure, Category = "NU|Field|UI")
	bool ShouldShowFieldUI() const;

protected:
	virtual void InitializeWidget() override;

	/**
	 * 전투 입장 / 퇴장.
	 * 기본 구현이 접기·펴기를 수행하므로 오버라이드할 때 Super 를 호출해야 한다.
	 */
	virtual void HandleEncounterChanged(ANUBattleEncounter* NewEncounter) override;

	/**
	 * 전투 여부에 따라 자신을 접고 펴는 기본 동작.
	 * 페이드 인·아웃처럼 즉시 전환이 아닌 연출이 필요하면 오버라이드한다.
	 */
	virtual void ApplyFieldVisibility();

	/** 접기 / 펴기가 적용된 뒤 발화. BP 에서 애니메이션을 얹는 지점. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Field|UI")
	void BP_OnFieldVisibilityChanged(bool bVisible);
};
