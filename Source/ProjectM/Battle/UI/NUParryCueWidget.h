// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Blueprint/UserWidget.h"
#include "NUParryCueWidget.generated.h"


UCLASS(Abstract)
class PROJECTM_API UNUParryCueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 창이 열릴 때 1회 호출, 창 위치와 소모 SP를 확정함
	virtual void InitializeZones(const FNUParryTimingSpec& InTiming, int32 InSPCost);

	// 매 프레임. 도전 구간 내 0~1
	virtual void SetProgress(float InProgress);

	// 패링 불가 공격이 올 때, 타이밍 판정이 없어 소모 SP만 알려주면 됨
	void SetDodgePrompt(bool bInOpen, int32 InSPCost);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnZonesInitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnProgressChanged();

	// 회피 안내를 켜고 끔, 타이밍 바 대신 "SP N 소모" 정도만 보여주면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnDodgePromptChanged();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	bool bDodgePromptOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	int32 DodgeSPCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	int32 ParrySPCost = 0;

protected:
	// 0~1 정규화. SuccessZone 시작
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI") 
	float SuccessStart = 0.f;
	
	// 0~1 정규화. SuccessZone 끝
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	float SuccessEnd = 0.f;
	
	// 이동하는 마커 위치
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI") 
	float Progress = 0.f;
	
	// 지금 누르면 나올 결과
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI") 
	bool bSuccess = false;
	
	// ---- 선택: 있으면 C++가 직접 그린다 (WBP 그래프 없이) ----
	// 타이밍 막대: CueBarCanvas(가로 CueBarWidth) 안의 CueZone(성공 구간)과 CueNeedle(지금 위치)을 옮긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UCanvasPanel> CueBarCanvas;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CueZone;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CueNeedle;

	// 바늘이 성공 구간 안에 있을 때만 보이는 빛
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CueSuccessGlow;

	// 패링 줄 / 회피 줄 (회피 안내가 열려 있으면 회피 줄만)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ParryRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DodgeRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> ParryCostText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> DodgeCostText;

	UPROPERTY(EditAnywhere, Category = "NU|UI")
	float CueBarWidth = 300.f;

private:
	void ApplyNativeCue();
	FNUParryTimingSpec CachedTiming;
};
