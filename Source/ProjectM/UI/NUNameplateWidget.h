// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUNameplateWidget.generated.h"

class UTextBlock;

// 캐릭터 머리 위 이름표 --- 필드 로컬·원격 캐릭터의 이름표 칸(UNUNameplateComponent)에 들어간다.
// 모양은 이 클래스를 부모로 한 WBP(명명 계약 "NameText")가 갖고, 프로젝트 세팅 NU UI > NameplateClass로 지정한다.
// 세팅이 비면 WBP 없이 그리는 코드 폴백 (나누기 창 선례).
// 전투 아바타에 이름 표기가 필요해지면 이 위젯을 그대로 붙이면 된다
UCLASS()
class PROJECTM_API UNUNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetName(const FText& InName);

protected:
	// WBP가 없을 때 쓸 최소 UI(그림자 있는 가운데 정렬 텍스트)를 코드로 만든다
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameTextBlock;

	// SetName이 위젯 생성(Construct)보다 먼저 올 수 있어 보관해 둔다
	FText PendingName;
};
