// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Common/NU_StatusIconWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/NUUISettings.h"

namespace
{
	// NU.State.Ailment.Burn 에서 마지막 Burn 만 떼어낸다. 텍스처 이름이 T_Status_Burn 형태라 이를 맞추기 위함
	FString NUStatusTagLeafName(const FGameplayTag& Tag)
	{
		const FString Full = Tag.GetTagName().ToString();

		FString Leaf;
		if (Full.Split(TEXT("."), nullptr, &Leaf, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			return Leaf;
		}

		return Full;
	}
}

UTexture2D* UNU_StatusIconWidget::ResolveStatusIcon(const FGameplayTag& StatusEffectTag)
{
	if (!StatusEffectTag.IsValid())
	{
		return nullptr;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (!Settings || Settings->StatusIconFolder.IsEmpty())
	{
		return nullptr;
	}

	const FString AssetName = FString::Printf(TEXT("T_Status_%s"), *NUStatusTagLeafName(StatusEffectTag));
	const FSoftObjectPath IconPath(
		FString::Printf(TEXT("%s/%s.%s"), *Settings->StatusIconFolder, *AssetName, *AssetName));

	UTexture2D* Icon = Cast<UTexture2D>(IconPath.TryLoad());

	// 이름 규칙이라 텍스처가 없으면 조용히 빈 칸이 된다.
	// 아이콘 칸은 상태이상이 바뀔 때마다 다시 만들어지므로 이름당 한 번만 남긴다
	if (!Icon)
	{
		static TSet<FName> LoggedMisses;

		const FName MissKey(*AssetName);
		if (!LoggedMisses.Contains(MissKey))
		{
			LoggedMisses.Add(MissKey);
			UE_LOG(LogTemp, Warning, TEXT("상태이상 아이콘을 찾을 수 없습니다 : %s"), *IconPath.ToString());
		}
	}

	return Icon;
}

void UNU_StatusIconWidget::SetStatusEffect(const FNUStatusEffectView& InView)
{
	View = InView;

	if (IsValid(Img_Icon))
	{
		if (UTexture2D* Icon = ResolveStatusIcon(View.StatusEffectTag))
		{
			Img_Icon->SetBrushFromTexture(Icon);
		}
	}

	if (IsValid(Txt_Badge))
	{
		// 스택 수든 남은 턴이든 0이면 띄울 것이 없다.
		// 3/5 처럼 상한을 같이 보여주고 싶으면 BP 가 View.MaxStack 을 읽으면 된다
		const bool bShowBadge = View.BadgeNumber > 0;

		Txt_Badge->SetVisibility(
			bShowBadge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (bShowBadge)
		{
			Txt_Badge->SetText(FText::AsNumber(View.BadgeNumber));
		}
	}

	OnStatusEffectChanged();
}
