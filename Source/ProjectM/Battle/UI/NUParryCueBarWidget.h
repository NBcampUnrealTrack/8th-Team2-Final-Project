// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/UI/NUParryCueWidget.h"
#include "NUParryCueBarWidget.generated.h"

class UCanvasPanel;
class UImage;

// 패링 타이밍 바 클래스
UCLASS()
class PROJECTM_API UNUParryCueBarWidget : public UNUParryCueWidget
{
	GENERATED_BODY()

public:
	virtual void InitializeZones(const FNUParryTimingSpec& InTiming, int32 InSPCost) override;
	virtual void SetProgress(float InProgress) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void BuildWidgetTree();
	UImage* AddBarImage(FName WidgetName, const FLinearColor& Color);
	static void ApplyZoneAnchors(UImage* Zone, float Start, float End);

	FLinearColor GetMarkerColor(bool bInSuccess) const;

protected:
	// ---- 배치 ----

	UPROPERTY(EditAnywhere, Category = "NU|UI|Layout")
	FVector2D BarSize = FVector2D(480.f, 28.f);

	// 화면 하단 중앙 기준 오프셋. 커맨드 UI와 겹치지 않도록 위로 띄운다
	UPROPERTY(EditAnywhere, Category = "NU|UI|Layout")
	FVector2D BarScreenOffset = FVector2D(0.f, -220.f);

	UPROPERTY(EditAnywhere, Category = "NU|UI|Layout", meta = (ClampMin = "1"))
	float MarkerWidth = 4.f;

	// ---- 색 ----

	UPROPERTY(EditAnywhere, Category = "NU|UI|Color")
	FLinearColor MissColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.55f);

	UPROPERTY(EditAnywhere, Category = "NU|UI|Color")
	FLinearColor SuccessColor = FLinearColor(0.71f, 0.65f, 0.42f, 0.95f);

	UPROPERTY(EditAnywhere, Category = "NU|UI|Color")
	FLinearColor MarkerMissColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.f);

	UPROPERTY(EditAnywhere, Category = "NU|UI|Color")
	FLinearColor MarkerSuccessColor = FLinearColor(1.f, 0.85f, 0.35f, 1.f);

private:
	// RebuildWidget에서 구성. 그리는 순서 = 뒤에 있는 것이 위로 온다
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BarCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UImage> MissZone;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SuccessZone;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Marker;
};
