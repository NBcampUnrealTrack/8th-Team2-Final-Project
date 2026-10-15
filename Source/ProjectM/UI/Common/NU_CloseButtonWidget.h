// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NU_CloseButtonWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUCloseButtonClicked);

/*
공용 닫기 버튼 부품 (WBP_CloseButton). 루트가 Btn_Close(Button) 하나뿐이라 다른 위젯의
트리에 자식으로 얹으면 UButton 이 아니라 이 UUserWidget 래퍼로 취급된다. 그래서 상위 위젯은
CloseButton 을 UButton* 대신 이 클래스로 BindWidget 하고, OnCloseClicked 를 구독한다.
*/
UCLASS()
class PROJECTM_API UNU_CloseButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "NU|UI")
	FOnNUCloseButtonClicked OnCloseClicked;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Close;

private:
	UFUNCTION()
	void HandleClicked();
};
