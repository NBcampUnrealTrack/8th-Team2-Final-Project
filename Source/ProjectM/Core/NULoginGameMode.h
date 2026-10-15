#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Net/EmberServerManager.h"
#include "NULoginGameMode.generated.h"

class UUserWidget;

// 위젯이 표시할 상태, bBusy가 true면 확인 버튼을 잠그면 됨
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNULoginStatusChanged, const FText&, StatusText, bool, bBusy);

// 임시 로그인 레벨 전용 게임모드
UCLASS()
class PROJECTM_API ANULoginGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANULoginGameMode();

	// 위젯의 확인 버튼이 호출, 접속 성공 시 자동으로 필드 레벨로 이동
	UFUNCTION(BlueprintCallable, Category = "NU|Login")
	void RequestLogin(const FString& InUserId);

	// 구글 계정으로 로그인 (브라우저가 열리고, 계정을 고르면 서버에 접속해 필드로 이동)
	UFUNCTION(BlueprintCallable, Category = "NU|Login")
	void RequestGoogleLogin();

	// 브라우저에서 계정을 고르는 중에 그만둔다
	UFUNCTION(BlueprintCallable, Category = "NU|Login")
	void CancelGoogleLogin();

	// 구글 로그인 설정(클라이언트 ID)이 들어 있는지 --- 없으면 위젯이 버튼을 막고 안내를 띄운다
	UFUNCTION(BlueprintPure, Category = "NU|Login")
	bool IsGoogleLoginAvailable() const;

	// 이름 입력 접속(개발용)을 보일지. 출시 빌드는 늘 false
	UFUNCTION(BlueprintPure, Category = "NU|Login")
	bool IsDevNameLoginVisible() const;

	// 구글 계정을 고르는 중인지 (위젯이 "취소" 버튼을 보일 때)
	UFUNCTION(BlueprintPure, Category = "NU|Login")
	bool IsGoogleSignInPending() const;

public:
	// 로그인 상태 변경 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "NU|Login")
	FOnNULoginStatusChanged OnLoginStatusChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleConnStateChanged(EEmberConnState NewState);

	void SetStatus(const FText& StatusText, bool bBusy);

	void HandleGoogleProgress(const FText& Message);
	void HandleGoogleFinished(bool bSuccess, const FText& Message);
	class UNUGoogleAuthSubsystem* GetGoogleAuth() const;

	UEmberServerManager* GetServerManager() const;

protected:
	// 로그인 위젯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Login")
	TSubclassOf<UUserWidget> LoginWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Login")
	TObjectPtr<UUserWidget> LoginWidget;
	
	// 접속 성공 후 이동할 필드 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Login")
	TSoftObjectPtr<UWorld> FieldLevel;

	// 접속 진행 중 여부, 중복 접속 방지
	bool bLoginInProgress = false;
};
