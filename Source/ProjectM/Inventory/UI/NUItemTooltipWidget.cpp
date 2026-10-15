#include "Inventory/UI/NUItemTooltipWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Inventory/NUEquipmentTypes.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUItemUI.h"

namespace
{
	// 선택 위젯에 글자를 넣고 보이기/접기 --- WBP가 뺀 줄(nullptr)은 건너뛴다
	void SetLine(UTextBlock* Text, const FText& Value, bool bShow)
	{
		if (Text == nullptr)
		{
			return;
		}
		if (bShow)
		{
			Text->SetText(Value);
		}
		Text->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUItemTooltipStatWidget::SetStat(const FNUTipStat& Stat)
{
	ValueText->SetText(FText::FromString(Stat.Text));
	const FText Delta = FText::FromString(Stat.Delta);
	const bool bHasDelta = !Stat.Delta.IsEmpty();
	SetLine(GainText, Delta, bHasDelta && Stat.bGain);
	SetLine(LossText, Delta, bHasDelta && !Stat.bGain);
}

void UNUItemTooltipWidget::SetItem(const FNUItemDefRow& Def, FName ItemRowName, int32 MyLevel, int32 MyJobId, const TArray<FNUTipStat>& Stats, const FText& WornName)
{
	const bool bEquip = Def.ItemType == ENUItemType::Equipment;
	const bool bConsumable = Def.ItemType == ENUItemType::Consumable;

	NameText->SetText(Def.DisplayName.IsEmpty() ? FText::FromName(ItemRowName) : Def.DisplayName);

	// 분류 (장비면 부위까지). 글자 규칙은 NUItemUI --- 통합 메뉴의 상세 카드와 같은 말을 쓴다
	CategoryText->SetText(NUItemUI::GetCategoryText(Def));

	// 장비 요구 레벨 --- 내 레벨이 못 미치면 Low 줄만 (실제 판정은 서버 1308, 여기는 미리 알려주기)
	const FText LevelLine = NUItemUI::GetRequiredLevelText(Def);
	const bool bShowLevel = !LevelLine.IsEmpty();
	const bool bTooLow = bShowLevel && MyLevel > 0 && MyLevel < Def.RequiredLevel;
	SetLine(RequiredLevelText, LevelLine, bShowLevel && !bTooLow);
	SetLine(RequiredLevelLowText, LevelLine, bTooLow);

	// 직업 전용 무기 --- 설명문이 사라져도 직업 정보는 남아야 한다.
	// 내 직업이 다르면 Low 줄만 (착용도 막힌다). 직업이 없으면(0) 붉게 하지 않는다 --- 아직 못 고른 것이지 틀린 게 아니다
	const FText JobLine = NUItemUI::GetRequiredJobText(Def);
	const bool bShowJob = !JobLine.IsEmpty();
	const bool bWrongJob = bShowJob && Def.RequiredJobId != 0 && MyJobId != 0 && MyJobId != Def.RequiredJobId;
	SetLine(RequiredJobText, JobLine, bShowJob && !bWrongJob);
	SetLine(RequiredJobLowText, JobLine, bWrongJob);

	// 설명 --- 장비는 뺀다: 기획 설명문이 스탯 요약이라 아래 스탯 줄과 같은 말이 두 번 보인다 (8/24)
	SetLine(DescriptionText, Def.Description, !bEquip && !Def.Description.IsEmpty());

	// 장비 스탯 --- 줄마다 StatRowClass 하나. 같은 부위에 낀 것이 있으면 차이가 옆에 붙어 온다
	if (StatsBox != nullptr)
	{
		StatsBox->ClearChildren();
		if (StatRowClass != nullptr)
		{
			for (const FNUTipStat& Stat : Stats)
			{
				UNUItemTooltipStatWidget* Row = CreateWidget<UNUItemTooltipStatWidget>(this, StatRowClass);
				Row->SetStat(Stat);
				StatsBox->AddChild(Row);
			}
		}
		StatsBox->SetVisibility(StatsBox->GetChildrenCount() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetLine(WornText, FText::Format(NSLOCTEXT("NUInventory", "TipWorn", "장착 중  {0}"), WornName), !WornName.IsEmpty());

	// 소모품 사용 정보 --- 효과 수치는 표의 Effect1/2 (IsPercent면 %)
	const FString EffectValues = NUItemUI::GetEffectValuesText(Def);
	SetLine(EffectText, FText::FromString(TEXT("효과 수치  ") + EffectValues), bConsumable && !EffectValues.IsEmpty());
	SetLine(UsageText, NUItemUI::GetUsageText(Def), bConsumable);
}
