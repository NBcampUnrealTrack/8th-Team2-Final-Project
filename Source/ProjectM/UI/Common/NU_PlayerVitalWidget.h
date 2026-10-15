// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseInGameWidget.h"
#include "NU_PlayerVitalWidget.generated.h"

struct FOnAttributeChangeData;
struct FGameplayAttribute;
class UTextBlock;
class UProgressBar;
class UOverlay;

/*
로컬 플레이어 HP/SP 를 표시하는 화면공간 재사용 부품 (Field HUD 용 Common 부품).
Mana 는 신 시스템(main)에서 제거되고 SP 로 통합되었다 — Health/SP 두 자원만 존재한다.

Battle/UI/NUCombatantVitalWidget 과 표시 계약(Health/MaxHealth/.../OnVitalsChanged)은 같지만,
그건 월드스페이스 전용(적 머리 위)이고 Avatar 가 SetVitals() 를 직접 호출하는 푸시 방식이라
상속으로 묶지 않는다. 이 위젯은 화면 HUD 부품이라 값을 대신 밀어넣어 줄 소유자가 없으므로,
Battle/Avatar/NUBattleAvatar 가 쓰는 구독 방식(GetGameplayAttributeValueChangeDelegate +
FDelegateHandle 개별 추적)을 그대로 가져와 스스로 로컬 ASC 를 구독하는 풀 방식으로 만든다.

HealthText/SPText/HealthBar/SPBar 는 전부 BindWidgetOptional 이라 WBP 에 같은 이름의
위젯을 두면 그래프 배선 없이 C++ 이 직접 갱신한다(텍스트는 문자열, 바는 Percent). 그 이상의
표현(색상 전환, 애니메이션 등)이 필요하면 OnVitalsChanged() 에서 Health/MaxHealth/HealthPercent
등을 읽어 확장하면 된다.

데미지/힐 팝업은 이 위젯이 담당하지 않는다 — main 의 UNUVitalAttributeSet::ShowCombatPopup()이
ANUBattleAvatar::MulticastShowCombatPopup() 을 거쳐 UNUCombatPopupLayer 로 직접 처리한다.
*/
UCLASS()
class PROJECTM_API UNU_PlayerVitalWidget : public UNU_BaseInGameWidget
{
	GENERATED_BODY()

protected:
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 수치가 갱신된 뒤 1회 발화. BP 는 ProgressBar 등 추가 표현이 필요할 때만 여기서 처리한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI|Vital")
	void OnVitalsChanged();

private:
	void BindVitalAttributes();
	void UnbindVitalAttributes();
	void HandleVitalAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshFromASC();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UTextBlock> SPText;

	/** HealthText 뒤에 겹쳐서 채워지는 바. WBP 에 같은 이름의 ProgressBar 를 두면 그래프 배선 없이 C++ 이 직접 갱신한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UProgressBar> SPBar;

	// ---- 새 HUD(글래스 엣지) 상태 판용. 모두 선택 ----
	// 숫자만 ("812"). HealthText/SPText는 "HP  812 / 990" 형식 그대로 둔다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UTextBlock> HealthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UTextBlock> SPValueText;

	// HealthBar 뒤에 깔리는 막대. 깎인 만큼 잠깐 남았다가 따라 내려간다 (회복은 바로 따라붙는다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UProgressBar> HealthGhostBar;

	// 고른 캐릭터의 이름·레벨·직업 (레벨·직업은 플레이어 상태의 지금 값)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UTextBlock> PlayerLevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UTextBlock> PlayerJobText;

	// 초상 --- 선택창 줄과 같은 방법으로 몸·머리·얼굴 플립북을 겹친다 (위 정렬, PortraitPixelScale배)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI|Vital")
	TObjectPtr<UOverlay> PortraitBox;

	UPROPERTY(EditAnywhere, Category = "NU|UI|Vital")
	float PortraitPixelScale = 2.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI|Vital")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI|Vital")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI|Vital")
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI|Vital")
	float SP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI|Vital")
	float MaxSP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI|Vital")
	float SPPercent = 0.f;

private:
	void RefreshIdentity();

	float GhostPercent = -1.f;
	float GhostHold = 0.f;
	float IdentityTimer = 0.f;
	int32 PortraitJobId = INDEX_NONE;

	// FGameplayAttribute 는 TMap 키로 못 써서(Battle/Avatar/NUBattleAvatar 선례와 동일) TArray<TPair<>> 로 추적한다.
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> VitalDelegateHandles;
};
