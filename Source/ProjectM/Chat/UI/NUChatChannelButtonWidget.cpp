// Fill out your copyright notice in the Description page of Project Settings.

#include "Chat/UI/NUChatChannelButtonWidget.h"

#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/Common/NU_CommonButtonWidget.h"

void UNUChatChannelButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 순정 버튼과 공용 부품 둘 다 인자 없는 델리게이트라 핸들러 하나로 받음
	if (IsValid(ChannelButton))
	{
		ChannelButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}

	if (IsValid(ChannelCommonButton))
	{
		ChannelCommonButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}

	// 클릭을 받을 곳과 글자를 넣을 곳을 따로 확인함
	if (!IsValid(ChannelButton) && !IsValid(ChannelCommonButton))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] %s 에 클릭을 받을 위젯이 없음 --- ChannelButton(Button) 이나 ChannelCommonButton(WBP_CommonButton) 중 하나로 이름을 맞추세요"),
			*GetClass()->GetName());
	}

	if (!IsValid(LabelText) && !IsValid(ChannelCommonButton))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] %s 에 채널 이름을 넣을 곳이 없음 --- LabelText(TextBlock) 이나 ChannelCommonButton(WBP_CommonButton) 중 하나로 이름을 맞추세요"),
			*GetClass()->GetName());
	}
}

void UNUChatChannelButtonWidget::SetChannel(ENUChatChannel InChannel, const FText& InLabel, const FLinearColor& InColor)
{
	Channel = InChannel;

	// LabelText가 있으면 이 위젯이 직접 그리고, 없으면 공용 부품의 라벨로 넘김
	if (IsValid(LabelText))
	{
		LabelText->SetText(InLabel);
		if (bApplyChannelColor)
		{
			LabelText->SetColorAndOpacity(FSlateColor(InColor));
		}
	}
	else if (IsValid(ChannelCommonButton))
	{
		ChannelCommonButton->SetLabelText(InLabel);
		if (bApplyChannelColor)
		{
			ChannelCommonButton->SetLabelColor(InColor);
		}
		ChannelCommonButton->SetIconSize(FVector2D(24.f, 24.f));
		ChannelCommonButton->SetIconTexture(NUChat::GetChannelIcon(Channel));
	}

	OnChannelSet();
}

void UNUChatChannelButtonWidget::SetPreferredSize(const FVector2D& InSize)
{
	// RootSizeBox를 안 둔 WBP는 자기가 정한 크기를 그대로 씀
	if (!IsValid(RootSizeBox))
	{
		return;
	}

	// 축을 따로 봄, 폭만 맞추고 높이는 WBP 가 정하는 식도 되어야 하고,
	// 맞출 기준이 없어 0 이 온 축을 0 으로 덮어쓰면 칸이 찌그러짐
	if (InSize.X > 0.f)
	{
		RootSizeBox->SetWidthOverride(InSize.X);
	}
	if (InSize.Y > 0.f)
	{
		RootSizeBox->SetHeightOverride(InSize.Y);
	}
}

ENUChatChannel UNUChatChannelButtonWidget::GetChannel() const
{
	return Channel;
}

void UNUChatChannelButtonWidget::HandleClicked()
{
	OnChannelClicked.ExecuteIfBound(static_cast<int32>(Channel));
}
