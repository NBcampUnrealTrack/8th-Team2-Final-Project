// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/NUNameplateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UNUNameplateWidget::SetName(const FText& InName)
{
	PendingName = InName;
	if (NameTextBlock != nullptr)
	{
		NameTextBlock->SetText(PendingName);
	}
}

TSharedRef<SWidget> UNUNameplateWidget::RebuildWidget()
{
	// WBP로 만든 위젯이면 이미 트리가 있다 --- 그때는 손대지 않는다
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
		Text->SetJustification(ETextJustify::Center);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.98f)));
		// 밝은 바닥 위에서도 읽히게 그림자를 깐다 (배경판 없이 글자만 띄우는 방식)
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		WidgetTree->RootWidget = Text;
	}
	return Super::RebuildWidget();
}

void UNUNameplateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 이름 계약 --- 코드 폴백도 같은 이름이라 여기서 잡힌다
	NameTextBlock = Cast<UTextBlock>(GetWidgetFromName(TEXT("NameText")));
	if (NameTextBlock != nullptr && !PendingName.IsEmpty())
	{
		NameTextBlock->SetText(PendingName);
	}
}
