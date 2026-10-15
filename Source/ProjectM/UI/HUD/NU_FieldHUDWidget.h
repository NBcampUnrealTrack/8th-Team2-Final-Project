// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseFieldWidget.h"
#include "UI/NUScreenMotion.h"
#include "NU_FieldHUDWidget.generated.h"

/*
마을 / 필드를 돌아다니는 동안 "상시" 표시되는 HUD 컨테이너.
UNU_BattleHUDWidget 의 필드 쪽 짝이다.

담당 범위
  - 필드 UI 조각들을 담는 자리(컴포저). 무엇을 보여주고 숨길지만 정한다.
  - 전투 진입 시 접기 / 퇴장 시 펴기 -> 베이스(UNU_BaseFieldWidget)가 처리한다.
  - 우상단 바로가기 아이콘 6개 --- 클릭되면 ESC 전체 메뉴와 같은 등록 목록(UNUUIManager의
    게임 메뉴 항목)에서 EntryId로 찾아 그대로 실행한다. 창을 여는 진짜 로직은 각 기능(캐릭터/
    인벤토리 등)이 갖고 있고, 여기는 "그 델리게이트를 대신 눌러주는" 얇은 배선만 한다.

담당하지 않는 범위
  - 상호작용 프롬프트        -> ANUPlayerController 의 InteractionIndicatorWidget
  - 인벤토리 / 캐릭터 정보창 자체의 내용 -> UNU_BaseSheetWidget 계열 (입력으로 열고 닫는 별도 창)
  - 플레이어 컨텍스트 메뉴   -> UNUPlayerContextMenu

로직을 여기 두지 않는다. HP 바 같은 조각은 각자 자기 데이터 소유자를 구독하는
자식 위젯으로 만들어 WBP 에 배치하고, 이 클래스는 그 배치를 담는 역할만 한다.
*/

UCLASS()
class PROJECTM_API UNU_FieldHUDWidget : public UNU_BaseFieldWidget
{
	GENERATED_BODY()

protected:
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;

	virtual void ApplyFieldVisibility() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 접기 / 펴기가 확정된 뒤 1회 발화. WBP 에서 조각들을 갱신하는 지점. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Field|HUD")
	void BP_OnFieldHUDRefreshed(bool bVisible);

private:
	void RefreshHUDPresentation();
	UFUNCTION() void HandleHUDVisibilityClicked();

	// The toggle is a sibling of this content, so hiding the HUD leaves a way back.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> FieldHUDContent;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> HUDVisibilityButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> HUDVisibilityLabel;

	// The icon describes the user's current HUD state: 0 = open eye, 1 = closed eye.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidgetSwitcher> HUDVisibilityIcons;

	// HUD 숨김 버튼 뒤의 판 (선택). HUD가 보일 땐 버튼이 메뉴 독 안에 들어가 있는 것처럼 판 없이,
	// 숨겼을 땐 독이 사라지므로 이 판이 나타나 버튼만 남는다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EyeBackdrop;

	// UNUUIManager의 게임 메뉴 항목 목록에서 같은 EntryId를 찾아 그대로 실행한다 (ESC 메뉴 버튼과 동일 델리게이트).
	// 없으면(기능 없음/등록 해제) 조용히 무시한다
	void ActivateEntry(FName EntryId);

	UFUNCTION() void HandleQuestJournalClicked();
	UFUNCTION() void HandleCharacterInfoClicked();
	UFUNCTION() void HandleStatsClicked();
	UFUNCTION() void HandleGrowthTreeClicked();
	UFUNCTION() void HandleInventoryClicked();
	UFUNCTION() void HandleNaiaRosterClicked();

	// 우상단 바로가기 아이콘 6개 (WBP_HUDIconButton). 이름 그대로 UNUUIManager의 EntryId와 짝짓는다.
	// 다른 필드 HUD 변형에는 없을 수 있어 Optional --- 없으면 그냥 조용히 건너뛴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UNU_CommonButtonWidget> QuestJournalButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UNU_CommonButtonWidget> CharacterInfoButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UNU_CommonButtonWidget> StatsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UNU_CommonButtonWidget> GrowthTreeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UNU_CommonButtonWidget> InventoryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UNU_CommonButtonWidget> NaiaRosterButton;

	// 전체 메뉴(ESC 메뉴) 버튼. 선택
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UNU_CommonButtonWidget> GameMenuButton;
	UFUNCTION() void HandleGameMenuClicked();

	// 네 모서리 묶음 (선택). HUD가 나타날 때(필드 진입, 전투에서 돌아옴, 숨김 해제) 바깥에서 제자리로 미끄러져 들어온다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HudTopLeft;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HudTopRight;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HudBottomLeft;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HudBottomRight;

	void PlayIntro();
	FNUScreenMotion IntroMotion;
	bool bWasShown = false;
	bool bLastUserVisible = true;
};
