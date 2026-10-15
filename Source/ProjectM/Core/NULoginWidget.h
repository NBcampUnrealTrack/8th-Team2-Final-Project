// 로그인 화면 위젯 베이스 --- 모양은 WBP, C++는 로그인 게임모드(ANULoginGameMode)와 이어 주기만 한다.
//
// 이름 계약 (전부 선택 --- 없는 조각은 건너뛴다)
//   GoogleLoginButton  (Button)          구글 계정으로 로그인 (브라우저가 열린다)
//   GoogleCancelButton (Button)          브라우저에서 계정을 고르는 동안만 보인다 --- 그만두기
//   GoogleSetupHint    (아무 위젯)       구글 로그인 설정(클라이언트 ID)이 없을 때만 보인다 (개발용 안내)
//   DevLoginBox        (아무 위젯)       개발용 이름 접속 묶음. 출시 빌드·설정 끔이면 접힌다
//   UserIdInput        (EditableTextBox) 이름. Enter 로 접속
//   DevLoginButton     (Button)          이름으로 접속
//   StatusText         (TextBlock)       진행 상황 / 오류
//   IntroTree / IntroTitle / IntroBar / IntroMenu (아무 위젯)   열릴 때 차례로 떠오르는 타이틀 그림·메뉴 상자 (bPlayIntro)
// 진행 중(bBusy)이면 버튼을 잠근다. 상태 문구는 게임모드의 OnLoginStatusChanged 를 그대로 받는다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "UI/NUScreenMotion.h"
#include "NULoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class ANULoginGameMode;

UCLASS(Abstract)
class PROJECTM_API UNULoginWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GoogleLoginButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GoogleCancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GoogleSetupHint;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DevLoginBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> UserIdInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DevLoginButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	// 열릴 때 타이틀 그림(나무 → 로고 → 띠)과 메뉴 상자가 차례로 떠오른다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Login")
	bool bPlayIntro = true;

private:
	ANULoginGameMode* GetLoginGameMode() const;
	void ApplyBusy(bool bInBusy);

	UFUNCTION()
	void HandleStatusChanged(const FText& InStatusText, bool bInBusy);

	UFUNCTION()
	void HandleGoogleClicked();

	UFUNCTION()
	void HandleGoogleCancelClicked();

	UFUNCTION()
	void HandleDevLoginClicked();

	UFUNCTION()
	void HandleUserIdCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	bool bBusy = false;
	bool bLastPending = false;
	FNUScreenMotion IntroMotion;
};
