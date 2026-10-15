// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chat/NUChatTypes.h"
#include "NUChatLineWidget.generated.h"

class UTextBlock;

// 채팅 로그의 한 줄 
//
// 두 가지 중 하나로 짜면 됨:
//   (1) 한 덩어리  LineText   (TextBlock)  "[채널] 이름 : 본문" 이 통째로 들어감
//   (2) 나눠 쓰기  ChannelText / SenderText / BodyText (TextBlock 3개)
//                  채널 꼬리표만 색을 넣거나 이름만 굵게 하는 식으로 따로 꾸밀 수 음
//                  시스템 메세지는 발신자가 없어서 SenderText가 접힘
// 둘 다 배치하면 (1) 사용함
//
// 긴 채팅은 여러 줄로 나오고 짧은 채팅은은 한 줄로 남음
// 접는 것은 bAutoWrapLines 가 켜 주므로 WBP 에서 Auto Wrap Text 를 따로 켜지 않아도 됨
UCLASS(Abstract)
class PROJECTM_API UNUChatLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 한 줄을 채움, (나) 붙이기와 채널 라벨 붙이기는 이 안에서 처리됨
	void SetLine(const FNUChatLine& InLine);

	// 여분으로 접어 두거나 다시 꺼냄, 줄 위젯은 버려지지 않고 재사용됨)
	void SetDisplayed(bool bDisplayed);

protected:
	virtual void NativeOnInitialized() override;

	// 표시는 C++ 이 끝내므로 WBP 는 연출만 얹으면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Chat")
	void OnLineSet();

	
protected:
	// 채널 색을 글자에 입힐지. 끄면 WBP 디자인타임 색이 그대로 남음
	// 본문은 흰색으로 두고 채널 꼬리표만 색을 주고 싶을 때 끄고 OnLineSet에서 직접 칠하면 됨
	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	bool bApplyChannelColor = true;

	// 긴 줄을 다음 줄로 접을지, 켜면 TextBlock 의 Auto Wrap Text 를 코드가 켜 줌
	// 끄더라도 WBP 가 켜 둔 것을 끄지는 않음, 켜는 쪽으로만 작동함
	// 폰트·크기와 달리 이것은 디자인이 아니라 동작이라 기본값을 켠 쪽으로 둠
	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	bool bAutoWrapLines = true;

	
	// --- WBP가 읽을 수 있는 원본 값 ---

	UPROPERTY(BlueprintReadOnly, Category = "NU|Chat")
	ENUChatChannel Channel = ENUChatChannel::System;

	// (나) 까지 붙인 뒤의 이름. 시스템 메세지면 비어 있음
	UPROPERTY(BlueprintReadOnly, Category = "NU|Chat")
	FText SenderDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Chat")
	FText MessageText;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Chat")
	bool bIsMine = false;

	
	// --- 위젯 계약 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChannelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SenderText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BodyText;

private:
	void ApplyChannelColor(const FLinearColor& InColor);

	// 있는 TextBlock 전부에 줄바꿈을 켬. NativeOnInitialized 에서 한 번만 부름
	void ApplyWrapSettings();

private:
	// NativeOnInitialized 에서 한 번 받아 두는 WBP 디자인타임 가시성
	ESlateVisibility DesignVisibility = ESlateVisibility::SelfHitTestInvisible;
};
