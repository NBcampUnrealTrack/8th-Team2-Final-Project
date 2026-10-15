// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NUScreenMotion.h"
#include "NUPartyPromptWidget.generated.h"

// 파티 초대·신청을 받았을 때 뜨는 수락/거절 팝업
// 여는건 PC가, 닫는건 파티서브시스템이 처리
UCLASS(Abstract)
class PROJECTM_API UNUPartyPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// PlayerController가 호출함. bIsInvite가 false면 가입 신청
	void Open(bool bIsInvite, const FString& FromName, int32 ExpireSec);

	void Close();

	bool IsOpen() const;

	// WBP의 수락 버튼이 호출함
	UFUNCTION(BlueprintCallable, Category = "NU|Party")
	void Accept();

	// WBP의 거절 버튼이 호출함
	UFUNCTION(BlueprintCallable, Category = "NU|Party")
	void Decline();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP가 문구를 넣고 남은 시간 애니메이션을 돌림
	// ExpireSec은 표시용, 실제 만료 판정은 서브시스템 타이머가 함
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Party")
	void OnPromptOpened(const FText& Message, int32 ExpireSec);

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Party")
	void OnPromptClosed();

protected:
	// 텍스트 바인딩으로 쓰고 싶을 때
	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	FText PromptMessage;

	// 버튼 라벨을 초대/신청에 따라 바꾸고 싶을 때
	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	bool bIsInvitePrompt = false;

	// 선택: 있으면 C++가 문구·버튼·남은 시간 막대를 직접 다루고, PromptRoot를 살짝 튀어나오게 띄운다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> PromptMessageLine;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> PromptAccept;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> PromptDecline;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UProgressBar> PromptTimeBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> PromptRoot;

private:
	class UNUPartySubsystem* GetPartySubsystem() const;

	FNUScreenMotion PromptMotion;
	float ExpireTotal = 0.f;
	float ExpireLeft = 0.f;
};
