// 아이템 툴팁 --- 모양은 WBP_ItemTooltip이 갖고, C++는 글자와 어느 줄을 보일지만 넣는다.
// 인벤 칸·장비창·상점 줄·거래 창·남의 정보창이 UNUSheetWidgetBase::BuildItemTooltip으로 같이 쓴다.
// 클래스는 프로젝트 세팅(NU UI)의 ItemTooltipClass.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   NameText              (TextBlock)        이름
//   CategoryText          (TextBlock)        "장비 · 무기" / "소모품" ...
//   RequiredLevelText     (TextBlock, 선택)  "필요 레벨 N" --- 내 레벨이 충분할 때
//   RequiredLevelLowText  (TextBlock, 선택)  같은 글자, 내 레벨이 못 미칠 때만 --- 색은 WBP (둘 중 하나만 보인다)
//   RequiredJobText       (TextBlock, 선택)  "{직업} 전용" --- 내 직업이 맞거나 아직 직업이 없을 때
//   RequiredJobLowText    (TextBlock, 선택)  같은 글자, 내 직업이 다를 때만 --- 색은 WBP (둘 중 하나만 보인다)
//   DescriptionText       (TextBlock, 선택)  설명 --- 장비는 스탯 줄과 겹쳐 뺀다
//   StatsBox              (Panel, 선택)      장비 스탯 줄이 StatRowClass(WBP_ItemTooltipStat)로 채워진다
//   WornText              (TextBlock, 선택)  "장착 중  {이름}" --- 같은 부위에 낀 것이 있을 때
//   EffectText            (TextBlock, 선택)  소모품 "효과 수치  N"
//   UsageText             (TextBlock, 선택)  소모품 사용 가능 범위
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUItemTooltipWidget.generated.h"

class UPanelWidget;
class UTextBlock;
struct FNUItemDefRow;

// 장비 스탯 한 줄 --- 이 장비의 값과, 같은 부위에 낀 것과의 차이 (UNUSheetWidgetBase가 채운다)
struct FNUTipStat
{
	FString Text;   // "공격력  +11.2%"
	FString Delta;  // "(+7.0%)" --- 비교 대상이 없으면 빈 문자열
	bool bGain = true;
};

// 스탯 한 줄 (WBP_ItemTooltipStat). 차이가 오르면 GainText, 내리면 LossText만 보인다 --- 색은 WBP
UCLASS(Abstract)
class PROJECTM_API UNUItemTooltipStatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStat(const FNUTipStat& Stat);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GainText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LossText;
};

UCLASS(Abstract)
class PROJECTM_API UNUItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// MyLevel 0 = 아직 모름 (필요 레벨을 붉게 하지 않는다). WornName이 비어 있지 않으면 "장착 중" 줄이 보인다
	void SetItem(const FNUItemDefRow& Def, FName ItemRowName, int32 MyLevel, int32 MyJobId, const TArray<FNUTipStat>& Stats, const FText& WornName);

protected:
	// 스탯 줄 WBP (UNUItemTooltipStatWidget 파생). WBP_ItemTooltip 기본값에서 지정한다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Tooltip")
	TSubclassOf<UNUItemTooltipStatWidget> StatRowClass;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CategoryText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RequiredLevelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RequiredLevelLowText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RequiredJobText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RequiredJobLowText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StatsBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WornText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EffectText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UsageText;
};
