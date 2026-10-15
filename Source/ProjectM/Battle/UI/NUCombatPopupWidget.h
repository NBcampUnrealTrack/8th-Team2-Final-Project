// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Blueprint/UserWidget.h"
#include "NUCombatPopupWidget.generated.h"


// 전투원 머리 위에 떠오르는 글자 하나
// 위치 갱신과 제거는 UNUCombatPopupLayer 가 담당함, 이 위젯은 표시할 값만 들고 있음
UCLASS(Abstract)
class PROJECTM_API UNUCombatPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 레이어가 생성 직후 1회 호출함, 초기화
	void Setup(AActor* InAnchorActor, const FNUCombatPopupData& InPopupData, int32 StackIndex);

	// 레이어가 매 프레임 위치를 잡을 때 사용함
	AActor* GetAnchorActor() const;
	const FVector2D& GetStackOffset() const;

	// 수명이 다했는지, 레이어가 이걸 보고 걷어냄
	bool IsExpired(float WorldTime) const;

protected:
	// WBP가 문구·색을 정하고 떠오르며 사라지는 UMG 애니메이션을 재생함
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnPopupSetup();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FNUCombatPopupData PopupData;

	// 이 시간이 지나면 레이어가 제거함
	// WBP 애니메이션이 없거나 중간에 끊겨도 반드시 사라지게 하는 안전장치라 애니메이션보다 길게 둬야 함
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI", meta = (ClampMin = "0.1"))
	float Lifetime = 1.0f;

	// 같은 아바타에 여러 개가 뜰 때 서로 겹치지 않게 밀어내는 간격
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	FVector2D StackStep = FVector2D(0.f, -22.f);

private:
	// 앵커가 먼저 파괴될 수 있으므로 WeakPtr로 앵커 관찰
	TWeakObjectPtr<AActor> AnchorActor;

	// StackIndex로 미리 계산해둔 화면 오프셋
	FVector2D StackOffset = FVector2D::ZeroVector;

	// 만료 판정 기준, 생성 시점의 월드 시간
	float SpawnWorldTime = 0.f;
};
