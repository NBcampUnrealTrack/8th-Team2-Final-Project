// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseSheetWidget.h"
#include "NU_WorldMapWindowWidget.generated.h"

/*
전체 지도 창. 내용은 WBP 에 배치한 지도 뷰 조각(WBP_MapImageView)이 다 하고,
이 클래스는 옵션창과 같은 Show 로 여는 길만 제공한다.

지도가 없는 맵에서는 조각이 스스로 접힌다. 열기 전에 막고 싶으면 조각의
HasMapImage 를 보면 되지만, 지도 없는 맵이 많은 개발 단계라 일단 열리게 둔다.
*/
UCLASS()
class PROJECTM_API UNU_WorldMapWindowWidget : public UNU_BaseSheetWidget
{
	GENERATED_BODY()

public:
	UNU_WorldMapWindowWidget(const FObjectInitializer& ObjectInitializer);

	// 전체 메뉴의 [지도] 가 부른다. 클래스는 프로젝트 세팅(NU UI)의 WorldMapWindowClass
	static UNU_WorldMapWindowWidget* Show(APlayerController* OwningPlayer);

	// 창 시스템 등록 해제(딤·입력 차단 해제)까지 포함한 닫기.
	// ESC(OnCloseRequested)·M 토글·전투 진입이 같은 길을 쓴다 --- CloseSheet 만 부르면 딤이 남는다
	void Close();

protected:
	// 전투에 들어가면 Close 로 닫는다. 베이스의 자동 닫기(CloseSheet)는 창 시스템 등록을 안 풀어서 끈다
	virtual void HandleEncounterChanged(ANUBattleEncounter* NewEncounter) override;
};
