// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/Data/NUJobResourceDefinition.h"
#include "NU_JobResourceIconWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/*
대상에게 붙은 직업 자원(마크 · 약점 · 문양)이나 직업 상태(번아웃 · 지맥 붕괴) 아이콘 한 칸.
UNU_CombatantStatusWidget 이 상태이상 아이콘 줄과 따로 둔 줄에 개수만큼 만들어 붙인다.

아이콘 텍스처는 상태이상과 같은 폴더(UNUUISettings::StatusIconFolder)에서 자원표의 ResourceKey 로 찾는다 ---
자원은 T_Resource_{키}, 상태는 T_JobState_{키} (ST_RES_WEAKNESS -> T_Resource_Weakness, ST_RES_TEMPO -> T_JobState_Tempo).
텍스처가 없으면 아이콘을 접고 이름 글자(Txt_Name)를 대신 보여 준다 --- 아트가 오기 전에도 무엇인지 읽히게.

숫자는 자원이면 중첩 수, 상태면 남은 턴이다. 고르는 곳은 이 위젯 한 곳뿐이다.
*/
UCLASS()
class PROJECTM_API UNU_JobResourceIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 만드는 쪽이 한 번 호출함, 이후 갱신은 다시 만드는 방식이라 구독하지 않음
	void SetJobResource(const FNUJobResourceView& InView);

protected:
	// 채운 뒤 1회 발화, BP 는 대기 중인 상태를 흐리게 하는 등 추가 표현만 하면 됨 (View.bStatePending)
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|JobResource")
	void OnJobResourceChanged();

	// 키에서 텍스처를 찾음, 없으면 nullptr 이고 이름당 한 번만 경고를 남김
	static UTexture2D* ResolveIcon(const FNUJobResourceView& InView);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|JobResource")
	TObjectPtr<UImage> Img_Icon;

	// 텍스처가 없을 때 아이콘 대신 보이는 이름, 텍스처가 있으면 접음
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|JobResource")
	TObjectPtr<UTextBlock> Txt_Name;

	// 자원이면 중첩 수, 상태면 남은 턴, 0 이면 접음
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|JobResource")
	TObjectPtr<UTextBlock> Txt_Badge;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|JobResource")
	FNUJobResourceView View;
};
