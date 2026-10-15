// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Battle/Data/NUJobResourceDefinition.h"
#include "NU_CombatantStatusWidget.generated.h"

struct FOnAttributeChangeData;
struct FGameplayAttribute;
class UTextBlock;
class UProgressBar;
class UPanelWidget;
class UNU_StatusIconWidget;
class UNU_JobResourceGaugeWidget;
class UNU_JobResourceIconWidget;
class ANUBattleEncounter;
class UNUAbilitySystemComponent;

/*
전투원 1명의 HP/SP를 표시하는 재사용 부품. BattleHUD 가 AllyCombatantIds/EnemyCombatantIds
목록 크기만큼 동적으로 만들어서 붙인다 (아군/적 모두 이 하나로 커버 — FNUBattleParticipant::ASCOwner 가
플레이어/정령/적 전부 같은 NUVitalAttributeSet 을 쓰기 때문). Mana 는 신 시스템(main)에서
제거되고 SP 로 통합되었다.

UI/Common/NU_PlayerVitalWidget 과 표시 계약(HealthText/SPText 직접 갱신)은 같지만,
로컬 플레이어의 PlayerState/ASC 를 스스로 찾지 않는다 — 어떤 전투원을 보여줄지는
만드는 쪽(BattleHUD)이 SetCombatant() 로 지정해준다. 그래서 UNU_BaseInGameWidget 을
상속하지 않는다 — 로컬 플레이어 전용 문맥이 필요 없다.

HUD 패널(BattleHUD의 Panel_Allies/Panel_Enemies)뿐 아니라, ANUBattleAvatar::VitalBarWidget(전투원
머리 위 월드스페이스 위젯 컴포넌트)의 표시 위젯으로도 그대로 쓴다 — Avatar 는 자기 CombatantId 와
Encounter 를 이미 알고 있으므로 SetCombatant() 를 한 번 호출해주기만 하면 이후는 이 위젯이 스스로
구독해서 갱신한다(과거 이 역할을 하던 Battle/UI/NUCombatantVitalWidget 은 폐기 대상).

데미지/힐 팝업은 이 위젯이 담당하지 않는다 — main 의 UNUVitalAttributeSet::ShowCombatPopup()이
ANUBattleAvatar::MulticastShowCombatPopup() 을 거쳐 UNUCombatPopupLayer 로 직접 처리한다.
*/
UCLASS()
class PROJECTM_API UNU_CombatantStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 전투원을 지정한다. Encounter 가 null 이거나 CombatantId 를 못 찾으면 표시를 비운다. */
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|HUD")
	void SetCombatant(ANUBattleEncounter* Encounter, int32 InCombatantId);

	/**
	 * 아직 서버에 제출하지 않은 예정 SP 소모량. 표시되는 SP 에서 미리 빼서 보여주고(음수 방지),
	 * 실제 서버 값(ActualSP)은 그대로 보존한다 — 스킬 계수를 ←/→ 로 조절할 때 즉각적인 피드백을 주는 용도.
	 * (과거 NUCombatantVitalWidget::SetPendingSpend 와 동일 계약)
	 */
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|HUD")
	void SetPendingSpend(float InPendingSpend);

	// 서버 전투 전용 - ASC 없이 서버 스냅샷 값을 직접 반영함
	// SetCombatant 로 바인딩을 걸은 로컬 과는 별개
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|HUD")
	void SetVitalsDirect(int32 InHealth, int32 InMaxHealth, int32 InSP, int32 InMaxSP);

	// 서버가 보낸 이름/레벨을 표시한다. 적은 이름·레벨·HP 바, 아군은 HP/SP 바만 표시.
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|HUD")
	void SetIdentityDirect(const FText& InName, int32 InLevel, bool bEnemy);

	// 서버 전투 전용 - ASC 대신 서버가 준 상태이상으로 아이콘을 다시 그림, SetVitalsDirect 와 짝
	// 정의 조회와 배지 숫자 결정은 넘어오기 전에 끝나 있음 (NUBattleServer::BuildStatusViews)
	// 그래서 이 위젯은 서버 전투 자료구조를 몰라도 됨
	void SetStatusViewsDirect(const TArray<FNUStatusEffectView>& InViews);

	// 서버 전투 전용 - ASC 대신 서버가 준 직업 자원으로 게이지 · 아이콘 줄을 다시 그림, SetStatusViewsDirect 와 같은 모양
	// 무엇을 띄울지는 넘어오기 전에 NUJobResourceView 규칙으로 정해져 있음 (로컬 ASC 경로와 같은 규칙)
	void SetJobResourceViewsDirect(const TArray<FNUJobResourceView>& InGauges, const TArray<FNUJobResourceView>& InBadges);

	// 대상 고르기에서 지금 골라져 있는 전투원 (전투 명령 판이 켜고 끈다). TargetMark(테두리 등)가 있으면 보이며 숨쉬고 표시가 조금 커진다
	void SetTargeted(bool bInTargeted);

	// 대상 고르기에서 고르지 않은 같은 편 후보 --- 흐리게 해 고른 대상이 도드라지게 한다
	void SetDimmed(bool bInDimmed);

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 대상 표시 (선택) --- 평소 접혀 있고 SetTargeted(true)일 때만 보인다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UWidget> TargetMark;

	/** 수치가 갱신된 뒤 1회 발화. BP 는 추가 표현(ProgressBar 등)이 필요할 때만 여기서 처리한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|HUD")
	void OnVitalsChanged();

private:
	void BindVitalAttributes();
	void UnbindVitalAttributes();
	void HandleVitalAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshFromASC();

	// RefreshFromASC/SetVitalsDirect 공용 - 값을 다 채운 뒤 퍼센트, 텍스트 갱신과 OnVitalsChanged를 담당함
	void ApplyVitalsToDisplay();

	void BindStatusEffects();
	void UnbindStatusEffects();
	void HandleStatusEffectsChanged();

	// Panel_StatusIcons 를 비우고 걸려 있는 상태이상 개수만큼 아이콘을 다시 채운다.
	void RebuildStatusIcons();

	void BindJobResources();
	void UnbindJobResources();
	void HandleJobResourcesChanged();

	// 직업 자원 게이지와 아이콘 줄을 다시 채움, 지난번과 같으면 그대로 둠
	// bForce 면 비교 없이 다시 그림 (전투원을 새로 지정할 때)
	void RebuildJobResources(bool bForce = false);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UTextBlock> CombatantNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UTextBlock> CombatantLevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UPanelWidget> CombatantIdentity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UWidget> SB_SP;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	FLinearColor AllyHealthColor = FLinearColor(0.24f, 0.68f, 0.13f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	FLinearColor EnemyHealthColor = FLinearColor(0.78f, 0.12f, 0.14f, 1.f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UTextBlock> SPText;

	/** HealthText 뒤에 겹쳐서 채워지는 바. WBP 에 같은 이름의 ProgressBar 를 두면 그래프 배선 없이 C++ 이 직접 갱신한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UProgressBar> SPBar;

	
	// 체력바 아래에 상태이상 아이콘을 나열하는 패널. C++ 이 ClearChildren 하므로
	// 이 패널 안에 다른 위젯을 같이 두면 안 된다 --- 첫 갱신에 비워지고 안보인다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UPanelWidget> Panel_StatusIcons;

	// 상한을 넘긴 개수를 "+N" 으로 표시. 위 이유로 Panel_StatusIcons 바깥에 형제로 둬야 한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UTextBlock> Txt_StatusOverflow;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	TSubclassOf<UNU_StatusIconWidget> StatusIconWidgetClass;

	// 한 번에 띄우는 아이콘 개수. 자르는 곳을 C++ 한 곳으로 모아 +n 과 어긋나지 않게 한다. 
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	int32 MaxVisibleStatusIcons = 6;

	// 자기 칸 직업 자원 게이지(영력 · 템포 · 동조율)를 나열하는 패널, 없는 전투원(적 · 나이아)이면 접힘
	// C++ 이 ClearChildren 하므로 이 패널 안에 다른 위젯을 같이 두면 안 된다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UPanelWidget> Panel_JobResources;

	// 대상에게 붙은 직업 자원(마크 · 약점 · 문양)과 직업 상태(번아웃 · 지맥 붕괴) 아이콘 줄, 비면 접힘
	// 상태이상 줄(Panel_StatusIcons)과 따로 둔다 --- 상태이상의 6개 상한과 +n 셈에 섞이지 않게
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|HUD")
	TObjectPtr<UPanelWidget> Panel_JobResourceIcons;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	TSubclassOf<UNU_JobResourceGaugeWidget> JobResourceGaugeWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|HUD")
	TSubclassOf<UNU_JobResourceIconWidget> JobResourceIconWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	int32 CombatantId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	float SP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	float MaxSP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	float SPPercent = 0.f;

	// 예정 소모량. "-15" 같은 문구를 따로 띄우고 싶을 때만 BP 에서 참고하면 된다. SP 는 이미 이 값이 빠진 표시값이다.
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|HUD")
	float PendingSPSpend = 0.f;

private:
	// 서버가 준 실제 SP. 표시값(SP)과 달리 PendingSPSpend 가 빠지지 않은 원본값이다.
	float ActualSP = 0.f;

	TWeakObjectPtr<UNUAbilitySystemComponent> CachedASC;

	// FGameplayAttribute 는 TMap 키로 못 써서(Battle/Avatar/NUBattleAvatar 선례와 동일) TArray<TPair<>> 로 추적한다.
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> VitalDelegateHandles;

	FDelegateHandle StatusEffectsDelegateHandle;

	FDelegateHandle JobResourcesDelegateHandle;

	// 지금 그려 둔 직업 자원, 알림은 안 바뀌어도 오므로 같으면 다시 만들지 않는다
	TArray<FNUJobResourceView> ShownJobResourceGauges;
	TArray<FNUJobResourceView> ShownJobResourceBadges;

	// 넷 전투에서 쓰는 직업 자원 목록, 아바타가 넣어줌
	// ASC 가 없을 때만 RebuildJobResources 가 이 값을 씀 - 로컬 경로와 출처를 섞지 않으려고 DirectStatusViews 처럼 따로 둠
	TArray<FNUJobResourceView> ServerJobResourceGauges;
	TArray<FNUJobResourceView> ServerJobResourceBadges;

	// 넷 전투에서 쓰는 상태이상 목록, 아바타가 넣어줌
	// ASC 가 없을 때만 RebuildStatusIcons 가 이 값을 쓴다 - 로컬 경로와 출처가 섞이면
	// 어느 쪽이 최신인지 알 수 없게 되므로 둘을 합치지 않고 나눠 둔다
	TArray<FNUStatusEffectView> DirectStatusViews;

	bool bTargeted = false;
	float TargetedTime = 0.f;
};
