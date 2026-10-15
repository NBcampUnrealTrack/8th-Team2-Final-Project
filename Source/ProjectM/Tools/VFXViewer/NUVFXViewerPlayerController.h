// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NUVFXViewerPlayerController.generated.h"

class ANUVFXViewerDirector;

// 전투 VFX 뷰어의 입력, 마우스로 패널을 쓰고 단축키로 재생을 조작함
// R 처음부터 · Space 재생/일시정지 · ← → 한 프레임 (Shift 0.1초) · Home 처음으로 · F5 새로고침 · Tab 패널 · 1~4 속도
UCLASS()
class PROJECTM_API ANUVFXViewerPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANUVFXViewerPlayerController();

	void SetDirector(ANUVFXViewerDirector* InDirector);

	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

protected:
	virtual void BeginPlay() override;

private:
	// 검색 칸에 글자를 치는 중이면 단축키로 받지 않음
	static bool IsTypingText();

	UPROPERTY(Transient)
	TObjectPtr<ANUVFXViewerDirector> Director;
};
