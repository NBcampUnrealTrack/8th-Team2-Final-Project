// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chat/NUChatTypes.h"
#include "NUChatChannelButtonWidget.generated.h"

class UButton;
class UNU_CommonButtonWidget;
class USizeBox;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnNUChatChannelButtonClicked, int32 /*ChannelValue*/);

// 전송 채널 목록(ChatSendChannelMenu)의 항목 하
// 위젯 계약
//   ChannelButton       (Button)             순정 버튼으로 짤 때
//   ChannelCommonButton (WBP_CommonButton)   공용 부품을 그대로 쓸 때
//   LabelText           (TextBlock)          채널 이름을 이 위젯이 직접 그릴 때.
//                                            없으면 ChannelCommonButton의 라벨로 넘긴다
//   RootSizeBox         (SizeBox)            두면 ChatSendChannelButton과 같은 크기를 받아 간다.
//                                            빼면 이 WBP가 정한 크기가 그대로 쓰인다
UCLASS(Abstract)
class PROJECTM_API UNUChatChannelButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 채널 하나로 채움
	void SetChannel(ENUChatChannel InChannel, const FText& InLabel, const FLinearColor& InColor);

	// 위에 있는 전송 채널 버튼과 폭을 맞추기 위한 크기, RootSizeBox 가 없으면 아무 일도 하지 않음
	void SetPreferredSize(const FVector2D& InSize);

	ENUChatChannel GetChannel() const;

public:
	FOnNUChatChannelButtonClicked OnChannelClicked;

protected:
	virtual void NativeOnInitialized() override;

	// 표시는 C++ 이 끝내므로 WBP 는 연출만 얹으면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Chat")
	void OnChannelSet();

protected:
	// 채널 색을 글자에 입힐지. 끄면 WBP 디자인타임 색이 그대로 남음
	// 채널 구분을 글자색이 아니라 아이콘이나 배경으로 하고 싶을 때 쓴다
	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	bool bApplyChannelColor = true;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Chat")
	ENUChatChannel Channel = ENUChatChannel::Local;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ChannelButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UNU_CommonButtonWidget> ChannelCommonButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSizeBox;

private:
	UFUNCTION()
	void HandleClicked();
};
