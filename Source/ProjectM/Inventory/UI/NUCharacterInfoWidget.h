// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUCharacterInfoWidget.generated.h"

// 캐릭터 정보창 (P키) — 미리보기 + 신상(이름·레벨·직업) + 스탯.
// 장비 슬롯은 8/25 회의 결정으로 장비창(UNUEquipmentWidget)이 가져갔다 --- 인벤과 함께 뜬다.
//
// 주의: 스탯은 UNUCharacterStatsWidget(CharacterProgression)이 서버 스냅샷까지 반영한
// 완전판을 이미 갖고 있다. 여기 값은 GAS에서 직접 읽는 축소판이라 겹친다.
// 어느 쪽을 남길지 정해지기 전까지 늘리지 않는다
UCLASS(Abstract)
class PROJECTM_API UNUCharacterInfoWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override; // 명명 계약 "StatsDetailButton" 바인딩
	virtual void NativeDestruct() override;  // 여기서 연 스탯 창을 같이 닫는다
	virtual void RefreshData() override;
	virtual void CloseSelf() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Sheet")
	void OnStatsChanged();

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	FText JobName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float SP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float MaxSP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float AttackPower = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float MagicAttackPower = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float Defense = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float CriticalRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	float CriticalDamage = 0.f;

private:
	// 순정 Button(레거시) / UNU_CommonButtonWidget(WBP_CommonButton) 둘 다 지원한다 --- UWidget으로
	// 받아 캐스팅한다. 클릭 바인딩은 NativeConstruct가 실제 타입을 캐스팅해 처리한다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> StatsDetailButton;

	// 창의 보이는 몸체 --- 스탯 창을 옆에 붙일 때 기준 자리로 쓴다 (타입은 WBP 자유)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> WindowBG;

	// 이름·레벨·직업을 읽어 명명 계약 텍스트에 넣는다 (NameText / LevelText / JobText).
	// 직업은 JobId만으로는 표기가 안 나와 성장 데이터 표를 거친다
	void PopulateProfile();

	// WBP의 "StatsListBox"에 스탯 행(이름 왼쪽·수치 오른쪽)을 C++로 생성 — 그래프 배선 불필요
	void PopulateStatsList();

	// [스탯 자세히] --- 진영 님 스탯 창(서버 스냅샷 포함 완전판)을 토글한다.
	// 여기 요약과 저 창의 관계가 곧 "요약은 정보창, 상세는 스탯 창" 분담이다
	UFUNCTION()
	void HandleStatsDetailClicked();

	// 스탯 창을 이 창 오른쪽에 붙인다. 갓 열린 위젯은 배치 전이라 지오메트리가 잡힐 때까지
	// 몇 틱 재시도한다 (한 틱 뒤에도 안 잡히는 경우가 실제로 있었다)
	void PositionStatsWindow(int32 AttemptsLeft);

	// [스탯 자세히]로 열었는지 --- 이 창이 닫힐 때 그때 연 스탯 창만 같이 닫는다.
	// 진영 님 단축키로 따로 연 스탯 창은 남의 것이니 건드리지 않는다
	bool bStatsOpenedHere = false;
};
