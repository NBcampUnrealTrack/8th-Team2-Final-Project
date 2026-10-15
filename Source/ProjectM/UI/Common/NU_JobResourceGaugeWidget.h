// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/Data/NUJobResourceDefinition.h"
#include "NU_JobResourceGaugeWidget.generated.h"

class UProgressBar;
class UTextBlock;

/*
직업 자원 게이지 한 줄 (영력 3 / 20). UNU_CombatantStatusWidget 이 자기 칸 자원 개수만큼 만들어 붙인다.
무엇을 띄울지와 수치는 UNUAbilitySystemComponent::GetJobResourceViews 가 정해서 넘기고 이 위젯은 그리기만 한다.
갱신은 이 위젯을 다시 만드는 방식이라 구독하지 않는다 (상태이상 아이콘 UNU_StatusIconWidget 과 같은 방식).
*/
UCLASS()
class PROJECTM_API UNU_JobResourceGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 만드는 쪽이 한 번 호출함
	void SetJobResource(const FNUJobResourceView& InView);

protected:
	// 이름 · 수치 · 바를 채운 뒤 1회 발화, BP 는 자원마다 색을 바꾸는 등 추가 표현만 하면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|JobResource")
	void OnJobResourceChanged();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|JobResource")
	TObjectPtr<UTextBlock> Txt_Name;

	// "3 / 20", 상한이 없는 자원이면 "3"
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|JobResource")
	TObjectPtr<UTextBlock> Txt_Value;

	// 상한이 없으면 비움
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|JobResource")
	TObjectPtr<UProgressBar> Bar_Amount;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|JobResource")
	FNUJobResourceView View;
};
