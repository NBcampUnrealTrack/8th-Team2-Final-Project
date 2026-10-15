// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseUserWidget.h"
#include "NU_BaseInGameWidget.generated.h"

/*
인게임(필드 / 전투) UI 의 공통 부모.

담당 범위
  1) 로컬 플레이어의 게임플레이 컨텍스트 해석 / 캐싱
       - ANUPlayerState            : 인벤토리 / 장비 / 퀘스트 컴포넌트의 소유자
       - UNUAbilitySystemComponent : HP/MP 등 Attribute, 그리고 현재 참여 중인 전투(Encounter)
  2) "지금 전투 중인가" 라는 하나의 사실
       - ASC->OnBattleEncounterChanged 를 구독해 CachedEncounter 를 유지한다.
       - 전투 UI 는 전투 중에 켜지고, 필드 UI 는 전투 중에 꺼지므로 판정은 같고 반응만 반대다.
         그래서 판정을 여기 두고, 반대 방향의 반응은 각 자식 베이스가 정한다.

담당하지 않는 범위
  - Encounter 내부 진행(페이즈 / 전환연출 / 종료)  -> UNU_BaseBattleWidget
  - 패리 판정                                      -> UNU_BaseBattleWidget
  - 인벤토리 / 장비 / 퀘스트 델리게이트            -> UNU_BaseSheetWidget 계열

타이틀 / 로그인 / 서버 선택 / 캐릭터 선택 화면은 PlayerState 가 없거나 의미가 없으므로
이 클래스를 거치지 않고 UNU_BaseUserWidget 을 직접 상속한다.
*/

class ANUPlayerState;
class ANUBattleEncounter;
class UNUAbilitySystemComponent;
class UNUBattleNetSubsystem;
class UNUBattleNetStageSubsystem;

UCLASS(Abstract)
class PROJECTM_API UNU_BaseInGameWidget : public UNU_BaseUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "NU|UI")
	ANUPlayerState* GetNUPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "NU|UI")
	UNUAbilitySystemComponent* GetNUASC() const;

	/** 로컬 플레이어가 현재 참여 중인 전투. 필드에 있으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "NU|UI")
	ANUBattleEncounter* GetBattleEncounter() const;

	UFUNCTION(BlueprintPure, Category = "NU|UI")
	bool IsInBattle() const;

protected:
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;

	/**
	 * PlayerState / ASC 를 해석해 캐싱한다.
	 * 클라이언트에서는 PlayerState 복제가 위젯 생성보다 늦을 수 있어 실패할 수 있다.
	 * 그때는 false 를 돌려주므로, 구독이 필요한 자식은 나중에 다시 호출하면 된다.
	 * (팀 관례: UNUSheetWidgetBase::ResolveComponents 선례와 같은 형태)
	 */
	bool ResolvePlayerContext();

	/**
	 * 전투 입장(NewEncounter != nullptr) / 퇴장(nullptr).
	 * 구독 직후 현재 상태로 1회 호출되므로, 자식은 여기서 초기 표시까지 맞출 수 있다.
	 * 호출 시점에 CachedEncounter 는 이미 갱신되어 있다.
	 */
	virtual void HandleEncounterChanged(ANUBattleEncounter* NewEncounter);

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void BP_OnEncounterChanged(ANUBattleEncounter* NewEncounter);

private:
	void BindLocalASC();
	void UnbindLocalASC();
	void BindNetBattle();
	void UnbindNetBattle();

	// ASC 쪽은 비다이나믹 델리게이트(DECLARE_MULTICAST_DELEGATE_*)라 UFUNCTION 이 필요 없고
	// AddUObject 로 붙인 뒤 RemoveAll(this) 로 해제한다.
	void HandleEncounterChanged_Internal(ANUBattleEncounter* NewEncounter);
	void HandleNetBattleStateChanged();
	void HandleNetStageReadyChanged(bool bReady);

	// IsInBattle()이 false여도 연출을 재생하느라 전투 세트가 아직 안 내려간 동안은
	// 전투 UI를 그대로 유지해야 함 - 두 신호를 여기서 합침
	void RecomputeNetBattleActive();
	UNUBattleNetStageSubsystem* GetNetStageSubsystem() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	TWeakObjectPtr<ANUPlayerState> CachedNUPlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	TWeakObjectPtr<UNUAbilitySystemComponent> CachedNUASC;

	/** 현재 참여 중인 전투. 이 클래스가 유일한 소유자이며 자식은 읽기만 한다. */
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	TWeakObjectPtr<ANUBattleEncounter> CachedEncounter;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	bool bNetBattleActive = false;
};
