#include "Shop/UI/NUCraftMaterialWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UNUCraftMaterialWidget::SetMaterial(UTexture2D* Icon, const FText& Name, int32 Have, int32 Need, bool bFirst)
{
	// 아이콘이 없는 아이템은 이름 글자가 아이콘 노릇 (인벤 칸과 같은 규칙)
	IconImage->SetBrushFromTexture(Icon);
	IconImage->SetVisibility(Icon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (LabelText != nullptr)
	{
		LabelText->SetText(Name);
		LabelText->SetVisibility(Icon != nullptr ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (PlusBox != nullptr)
	{
		PlusBox->SetVisibility(bFirst ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetToolTipText(Name);
	SetCounts(Have, Need);
}

void UNUCraftMaterialWidget::SetCounts(int32 Have, int32 Need)
{
	CountText->SetText(FText::Format(NSLOCTEXT("NUCraft", "MaterialCount", "{0}/{1}"), FText::AsNumber(Have), FText::AsNumber(Need)));
	// 모자람은 색이 아니라 위젯으로 (색은 WBP)
	if (ShortBox != nullptr)
	{
		ShortBox->SetVisibility(Have < Need ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
