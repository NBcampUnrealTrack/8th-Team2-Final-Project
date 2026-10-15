// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseFieldWidget.h"
#include "NU_MinimapWidget.generated.h"

class UNU_MapImageViewWidget;

/*
필드 미니맵 틀. 모양(원형 마스크·테두리 링)은 WBP_Minimap 이 갖고, 이 클래스는 "언제 보이는가"만 정한다.

전투 중이거나 지금 공간에 지도가 없으면 틀째 접는다 --- 9/9 UI 회의 결정(미니맵이 없으면 자리만 두고 숨김).
안쪽 지도 조각이 공간 이동으로 지도가 생기고 사라지는 것을 알려 준다.

이름 계약
  MapView   지도 조각(WBP_MapImageView) 인스턴스
*/
UCLASS()
class PROJECTM_API UNU_MinimapWidget : public UNU_BaseFieldWidget
{
	GENERATED_BODY()

protected:
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;
	virtual void ApplyFieldVisibility() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_MapImageViewWidget> MapView;

	FDelegateHandle MapChangedHandle;
};
