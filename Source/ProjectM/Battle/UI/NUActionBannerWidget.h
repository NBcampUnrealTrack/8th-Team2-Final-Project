// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Blueprint/UserWidget.h"
#include "UI/NUScreenMotion.h"
#include "NUActionBannerWidget.generated.h"


// 행동 실행 중 화면 중앙 상단에 뜨는 행동 이름
// 인카운터가 행동마다 멀티캐스트로 알리고 PlayerController가 이 위젯에 넘김
UCLASS(Abstract)
class PROJECTM_API UNUActionBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 행동 설정, 이미 떠 있으면 내용을 갈아치우고 수명을 다시 설정함
	void Setup(const FNUBattleActionBanner& InBanner);

	// WBP가 바인딩할 최종 문구, 표기 규칙을 한곳에 모아둠
	UFUNCTION(BlueprintPure, Category = "NU|UI")
	FText GetBannerText() const;

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 선택: 있으면 C++가 문구를 넣고(BannerText) 띠(BannerRoot)를 위에서 미끄러져 들어오게, 사라질 때 흐려지게 한다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI")
	TObjectPtr<class UTextBlock> BannerText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|UI")
	TObjectPtr<UWidget> BannerRoot;
	
	// Setup 직후 호출, WBP가 등장 애니메이션을 재생함
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnBannerShown();

	UFUNCTION()
	void HideBanner();
	
protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FNUBattleActionBanner Banner;

	// 이 시간이 지나면 스스로 숨김
	// WBP 애니메이션이 없거나 중간에 끊겨도 사라지게 하므로 애니메이션 길이보다 길게 둬야함
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI", meta = (ClampMin = "0.1"))
	float Lifetime = 1.2f;

private:
	FTimerHandle HideTimer;
	FNUScreenMotion BannerMotion;
	bool bLeaving = false;
};
