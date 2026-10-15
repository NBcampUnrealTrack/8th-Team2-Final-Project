#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NUScreenMotion.h"
#include "UI/HUD/NU_HudBattleFold.h"
#include "NUWorldLocationWidget.generated.h"

class UTextBlock;

// 지역/장소 이름만 표시한다. 판정과 위젯 수명은 UI 컨트롤러가 담당한다.
// 진입 배너(선택): WBP에 BannerRoot가 있으면 장소가 바뀔 때 화면 가운데 위에 큰 이름이 잠깐 떴다 사라진다.
UCLASS()
class PROJECTM_API UNUWorldLocationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLocationNames(const FText& InAreaName, const FText& InLocationName);

	// 진입 배너가 떠 있는지 --- 퀘스트 띠(FNUQuestFx)가 같은 자리에 겹치지 않게 끝날 때까지 기다린다
	bool IsBannerShowing() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|WorldLocation")
	TObjectPtr<UTextBlock> AreaText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|WorldLocation")
	TObjectPtr<UTextBlock> LocationText;

	// 진입 배너 (모두 선택). BannerRoot를 나타냈다 숨기고, 가는 금색 줄 두 개는 가운데에서 펴진다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|WorldLocation")
	TObjectPtr<UWidget> BannerRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|WorldLocation")
	TObjectPtr<UTextBlock> BannerAreaText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|WorldLocation")
	TObjectPtr<UTextBlock> BannerLocationText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|WorldLocation")
	TObjectPtr<UWidget> BannerRuleLeft;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|WorldLocation")
	TObjectPtr<UWidget> BannerRuleRight;

	// 배너가 다 나타난 뒤 머무는 시간(초)
	UPROPERTY(EditAnywhere, Category = "NU|WorldLocation", meta = (ClampMin = "0.5"))
	float BannerHoldSeconds = 2.4f;

private:
	void RefreshText();
	void ShowBanner();
	void HideBannerNow();

	FText AreaName;
	FText LocationName;

	FNUScreenMotion BannerMotion;
	FNUHudBattleFold BattleFold;     // 전투 동안 지역 이름 줄·배너를 접는다
	float BannerHoldLeft = 0.f;
	bool bBannerLeaving = false;
	FText LastBannerKey;
};
