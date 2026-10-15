// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Battle/UI/NUBattleResultTypes.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "NUBattleResultEntryWidget.generated.h"

class UImage;
class UNUFlipbookImage;
class UTextBlock;

// 획득 아이템·획득 나이아 목록의 한 줄, 이름과 개수만 다르고 모양은 같아 한 위젯을 씀
UCLASS(Abstract)
class PROJECTM_API UNUBattleResultEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEntry(const FNUBattleResultEntry& InEntry, bool bInIsNaia);

protected:
	// 표시는 C++ 이 끝내므로 WBP 는 연출만 얹으면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|Result")
	void OnEntrySet();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	FNUBattleResultEntry Entry;

	// 아이콘을 어디서 찾을지 가르는 값, 참이면 나이아 초상화
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	bool bIsNaia = false;

	// --- 필수 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CountText;

	// --- 선택 ---

	// 아이콘 에셋이 없는 아이템이 있어서, 못 찾으면 이미지를 숨기고 이름만 남김
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	// --- 선택 (2026-10 결과창) --- 없으면 예전 줄과 같음

	// 잡은 나이아의 그림 --- 나이아 표의 아이콘 칸이 비어 있어 PreviewFlipbook 을 NaiaPixelScale배로 세운다 (아이콘이 있으면 아이콘)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UNUFlipbookImage> NaiaFigure;

	// 잡은 나이아 칸에만 보이는 "새 동료" 표시
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NewNaiaRoot;

	// 개수 배지 묶음 --- 나이아 칸(한 마리)은 접는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CountRoot;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	float NaiaPixelScale = 1.f;
};
