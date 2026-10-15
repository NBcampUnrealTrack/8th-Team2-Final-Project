// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "UI/Controller/NUUIController.h"
#include "NUCharacterProfileUIController.generated.h"

class UNUCharacterManager;
class UNUCharacterProfileWidget;

// 남의 캐릭터 정보 UI 컨트롤러 --- 조회(142)를 보내고 응답이 오면 창을 연다.
// 요청·파싱은 UNUCharacterManager(GameInstance 수명)가 하고, 여기는 결과를 구독해 창만 여닫는다
UCLASS()
class PROJECTM_API UNUCharacterProfileUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	// 컨텍스트 메뉴의 "캐릭터 정보"가 부른다. 0 = 캐릭터 미선택 대상 --- 안내만 띄운다
	void ShowProfile(int64 CharacterIdx);

	// 창 X버튼·ESC 닫기 요청이 부른다
	void CloseProfileWindow();

	virtual void OnServerSessionEnded() override;

protected:
	virtual void OnInitialize() override;
	virtual void OnDeinitialize() override;

private:
	void HandleProfileResponse(int32 ResultCode, const FNUCharacterProfile& Profile);

	UNUCharacterManager* GetCharacterManager() const;

	// 정보 창 --- 프로젝트 세팅(NU UI)의 CharacterProfileWindowClass로 만든다
	UPROPERTY(Transient)
	TObjectPtr<UNUCharacterProfileWidget> ProfileWindow;

	FDelegateHandle ProfileHandle;

	// 응답을 기다리는 대상 (0 = 없음) --- 다른 곳이 낸 조회의 응답에 창을 열지 않기 위한 구분
	int64 PendingIdx = 0;
};
