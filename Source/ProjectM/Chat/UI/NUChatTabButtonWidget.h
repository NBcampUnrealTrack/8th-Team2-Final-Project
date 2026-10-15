// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUChatTabButtonWidget.generated.h"

class UButton;
class UNU_CommonButtonWidget;
class UTextBlock;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FOnNUChatTabButtonClicked, int32);

// 채팅창 상단 탭 하나 
// 
// 위젯 계약(BindWidgetOptional --- 넷 다 없어도 WBP 컴파일은 통과한다. 대신 클릭을 받을 것이
// TabButton/TabCommonButton 중 하나는 있어야 하고, 둘 다 없으면 NativeOnInitialized가 경고를 남긴다):
//   TabButton       (Button)              순정 버튼으로 짤 때
//   TabCommonButton (WBP_CommonButton)    공용 부품을 그대로 쓸 때 --- 지금 모양을 유지하는 가장 짧은 길
//   LabelText       (TextBlock)           탭 이름을 이 위젯이 직접 그릴 때. 없으면 TabCommonButton의 라벨로 넘긴다
//   SelectedFrame   (아무 위젯)           고른 탭에만 보이는 테두리 --- 색·두께는 WBP가 갖고 C++는 켜고 끄기만 한다
UCLASS(Abstract)
class PROJECTM_API UNUChatTabButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 탭 하나로 채움, TabIndex는 클릭을 돌려줄 때 같이 실어 보내는 값
	void SetTab(int32 InTabIndex, const FText& InLabel);

	void SetSelected(bool bInSelected);

	void SetIconTexture(UTexture2D* InTexture);
	void SetIconSize(const FVector2D& InSize);

	int32 GetTabIndex() const;

public:
	FOnNUChatTabButtonClicked OnTabClicked;

protected:
	virtual void NativeOnInitialized() override;

	// 디자이너 프리뷰/기본값 --- SetTab()이 아직 한 번도 호출 안 됐을 때(TabIndex가 기본값 INDEX_NONE)만
	// 이 값을 라벨에 반영한다. 실제 사용처(채팅 탭/전송 채널 버튼 등)는 런타임에 SetTab()을 호출해서
	// 그 값으로 덮어쓰므로, 여기 채워둔 값은 디자이너에서 미리 보거나 직접 타이핑하는 용도로만 쓰인다.
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	FText DefaultLabel;

	// 라벨·테두리는 C++가 끝내므로 WBP는 연출만 얹으면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Chat")
	void OnSelectedChanged(bool bInSelected);

	// 선택 글자색
	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	FLinearColor SelectedLabelColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.f);

	// 비선택 글자색
	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	FLinearColor IdleLabelColor = FLinearColor(0.35f, 0.35f, 0.38f, 1.f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> TabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UNU_CommonButtonWidget> TabCommonButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedFrame;

private:
	UFUNCTION()
	void HandleClicked();

	// 라벨 문구·색을 실제로 그리는 위젯으로 흘려보냄
	void ApplyLabel();

	int32 TabIndex = INDEX_NONE;

	FText Label;

	bool bSelected = false;
};
