// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Common/NU_JobResourceIconWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/NUUISettings.h"

namespace
{
	// ST_RES_WEAKNESS -> Weakness, 앞의 ST_RES_ 를 떼고 조각마다 첫 글자만 대문자로
	FString NUResourceKeyToAssetPart(FName ResourceKey)
	{
		FString Key = ResourceKey.ToString();
		Key.RemoveFromStart(TEXT("ST_RES_"), ESearchCase::IgnoreCase);

		TArray<FString> Parts;
		Key.ParseIntoArray(Parts, TEXT("_"));

		FString Result;
		for (const FString& Part : Parts)
		{
			if (Part.IsEmpty())
			{
				continue;
			}

			Result += Part.Left(1).ToUpper() + Part.Mid(1).ToLower();
		}

		return Result;
	}
}

UTexture2D* UNU_JobResourceIconWidget::ResolveIcon(const FNUJobResourceView& InView)
{
	if (InView.ResourceKey.IsNone())
	{
		return nullptr;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (!Settings || Settings->StatusIconFolder.IsEmpty())
	{
		return nullptr;
	}

	const FString AssetName = FString::Printf(TEXT("%s_%s"),
		InView.bIsState ? TEXT("T_JobState") : TEXT("T_Resource"), *NUResourceKeyToAssetPart(InView.ResourceKey));

	const FSoftObjectPath IconPath(
		FString::Printf(TEXT("%s/%s.%s"), *Settings->StatusIconFolder, *AssetName, *AssetName));

	UTexture2D* Icon = Cast<UTexture2D>(IconPath.TryLoad());

	// 아트가 오기 전에는 없는 게 정상이라 이름 글자로 대신함, 칸이 자주 다시 만들어지므로 이름당 한 번만 남김
	if (!Icon)
	{
		static TSet<FName> LoggedMisses;

		const FName MissKey(*AssetName);
		if (!LoggedMisses.Contains(MissKey))
		{
			LoggedMisses.Add(MissKey);
			UE_LOG(LogTemp, Log, TEXT("직업 자원 아이콘이 없어 이름으로 대신합니다 : %s"), *IconPath.ToString());
		}
	}

	return Icon;
}

void UNU_JobResourceIconWidget::SetJobResource(const FNUJobResourceView& InView)
{
	View = InView;

	UTexture2D* Icon = ResolveIcon(View);

	if (IsValid(Img_Icon))
	{
		Img_Icon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (Icon)
		{
			Img_Icon->SetBrushFromTexture(Icon);
		}
	}

	if (IsValid(Txt_Name))
	{
		Txt_Name->SetVisibility(Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Txt_Name->SetText(View.DisplayName);
	}

	if (IsValid(Txt_Badge))
	{
		// 자원은 중첩 수, 상태는 남은 턴
		const int32 BadgeNumber = View.bIsState ? View.RemainingTurns : View.Amount;
		const bool bShowBadge = BadgeNumber > 0;

		Txt_Badge->SetVisibility(bShowBadge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (bShowBadge)
		{
			Txt_Badge->SetText(FText::AsNumber(BadgeNumber));
		}
	}

	OnJobResourceChanged();
}
