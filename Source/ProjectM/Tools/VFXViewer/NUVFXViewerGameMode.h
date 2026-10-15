// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NUVFXViewerGameMode.generated.h"

class ANUVFXViewerDirector;

// 전투 VFX 뷰어 맵 전용, 로그인 · 서버 · 필드 캐릭터 없이 전투 무대와 전투원만 세움
// 게임 흐름(ANUGameMode · ANUPlayerController)을 타지 않으므로 서버 상태를 바꾸지 않음
// 실행: Scripts/run_vfx_viewer.ps1 (에디터 없이 -game) 또는 에디터에서 뷰어 맵을 Play
UCLASS()
class PROJECTM_API ANUVFXViewerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANUVFXViewerGameMode();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ANUVFXViewerDirector> Director;
};
