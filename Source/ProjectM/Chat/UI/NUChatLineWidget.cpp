// Fill out your copyright notice in the Description page of Project Settings.

#include "Chat/UI/NUChatLineWidget.h"

#include "Components/TextBlock.h"
#include "Framework/Text/TextLayout.h"

void UNUChatLineWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 접었다 되돌릴 때 쓸 기존 가시성을 받아둠
	// 디자인타임부터 접혀 있으면 되돌릴 값이 없으므로 기본값으로 되돌림
	DesignVisibility = GetVisibility();
	if (DesignVisibility == ESlateVisibility::Collapsed || DesignVisibility == ESlateVisibility::Hidden)
	{
		DesignVisibility = ESlateVisibility::SelfHitTestInvisible;
	}

	// LineText, ChannelText/SenderText/BodyText 가 없다면 빈 줄만 쌓이므로 로그 띄우기
	if (!IsValid(LineText) && !IsValid(ChannelText) && !IsValid(SenderText) && !IsValid(BodyText))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] %s 에 글자를 넣을 곳이 없음 --- LineText(TextBlock) 하나를 두거나 ChannelText/SenderText/BodyText 로 나눠 두세요"),
			*GetClass()->GetName());
	}

	ApplyWrapSettings();
}

void UNUChatLineWidget::ApplyWrapSettings()
{
	if (!bAutoWrapLines)
	{
		return;
	}

	for (UTextBlock* Text : { LineText.Get(), ChannelText.Get(), SenderText.Get(), BodyText.Get() })
	{
		if (!IsValid(Text))
		{
			continue;
		}

		// 스크롤박스가 자식에게 자기 폭을 그대로 주므로(ScrollBoxSlot 기본이 HAlign_Fill)
		// 이것만 켜면 폭을 넘는 줄이 다음 줄로 접히고 짧은 줄은 한 줄로 남음
		Text->SetAutoWrapText(true);

		// 띄어쓰기 없는 긴 덩어리(ㅋㅋㅋ 도배·URL)는 낱말 단위로는 끊을 데가 없어
		// 한 줄을 통째로 넘겨 버림. 그럴 때만 글자 단위로 끊게 함
		Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
	}
}

void UNUChatLineWidget::SetDisplayed(bool bDisplayed)
{
	SetVisibility(bDisplayed ? DesignVisibility : ESlateVisibility::Collapsed);
}

void UNUChatLineWidget::SetLine(const FNUChatLine& InLine)
{
	Channel = InLine.Channel;
	MessageText = InLine.Text;
	bIsMine = InLine.bIsMine;

	// 내 말은 이름 뒤에 (나)를 붙여 구분함
	// 캐릭터 미선택이면 이름이 비어 오므로 (나)만 남게됨
	SenderDisplay = InLine.SenderName;
	if (bIsMine)
	{
		SenderDisplay = SenderDisplay.IsEmpty()
			? NSLOCTEXT("NUChat", "SenderMineOnly", "나")
			: FText::Format(NSLOCTEXT("NUChat", "SenderMine", "{0}(나)"), SenderDisplay);
	}

	const FText ChannelLabel = NUChat::GetChannelLabel(Channel);

	if (IsValid(LineText))
	{
		// 한 덩어리로 짠 WBP, 시스템 메세지는 발신자가 없음
		LineText->SetText(SenderDisplay.IsEmpty()
			? FText::Format(NSLOCTEXT("NUChat", "LineNoSender", "[{0}] {1}"), ChannelLabel, MessageText)
			: FText::Format(NSLOCTEXT("NUChat", "Line", "[{0}] {1} : {2}"), ChannelLabel, SenderDisplay, MessageText));
	}
	else
	{
		// 채널, 몸통, 발신자를 나눈 WBP, 없는 칸은 그냥 건너뜀
		if (IsValid(ChannelText))
		{
			ChannelText->SetText(ChannelLabel);
		}
		if (IsValid(BodyText))
		{
			BodyText->SetText(MessageText);
		}
		if (IsValid(SenderText))
		{
			SenderText->SetText(SenderDisplay);
			SenderText->SetVisibility(SenderDisplay.IsEmpty()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
		}
	}

	if (bApplyChannelColor)
	{
		ApplyChannelColor(NUChat::GetChannelColor(Channel));
	}

	OnLineSet();
}

void UNUChatLineWidget::ApplyChannelColor(const FLinearColor& InColor)
{
	// 있는 칸에만 칠함, 한 덩어리든 나눠 쓰든 같은 코드로 처리됨
	const FSlateColor SlateColor(InColor);
	for (UTextBlock* Text : { LineText.Get(), ChannelText.Get(), SenderText.Get(), BodyText.Get() })
	{
		if (IsValid(Text))
		{
			Text->SetColorAndOpacity(SlateColor);
		}
	}
}
