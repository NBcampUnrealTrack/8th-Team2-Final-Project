// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCombatantVitalWidget.generated.h"


// 전투원 체력/SP 프로그레스 바 위젯
UCLASS(Abstract)
class PROJECTM_API UNUCombatantVitalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// C++에서만 호출, 비율 계산해서 BP에는 결과만 넘김
	void SetVitals(float InHealth, float InMaxHealth, float InSP, float InMaxSP, bool bInShowSP);

	// 아직 제출하지 않은 예정 소모량, 표시값에서 바로 빼므로 WBP는 평소대로 그리면 됨
	void SetPendingSpend(float InPendingSpend);

private:
	// SP/SPPercent는 예정 소모를 뺀 표시값이라 갱신 경로를 한 곳으로 모음
	void RefreshDisplay();

protected:
	// BP가 구현할 이벤트, 아래 값들을 읽어 ProgressBar / Text만 갱신하면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnVitalsChanged();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float SP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float MaxSP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float SPPercent = 0.f;

	// 예정 소모량, 위 SP에서 이미 빠져 있고 "-15" 같은 문구를 띄우고 싶을 때만 쓰면 됨
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float PendingSpend = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	bool bShowSP = true;

private:
	// 서버가 준 실제 값, 표시값과 달리 예정 소모가 빠지지 않음
	float ActualSP = 0.f;
};
