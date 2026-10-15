// 구글 계정 로그인 (데스크톱 앱 OAuth 2.0: 시스템 브라우저 + 127.0.0.1 콜백 + PKCE).
//
// 1. SignIn: 127.0.0.1 포트를 하나 열고 기본 브라우저로 구글 로그인 페이지를 연다 (게임 안 웹뷰는 구글이 막는다)
// 2. 사용자가 계정을 고르면 브라우저가 http://127.0.0.1:<포트>/?code=..&state=.. 로 돌아온다 --- 여기서 받아 "게임으로 돌아가세요" 페이지를 보여 준다
// 3. code + PKCE verifier 를 구글 토큰 주소에서 ID 토큰으로 바꾼다
// 4. ID 토큰(JWT)의 내용(sub, email, name)을 읽어 둔다. 서명 검증은 서버 몫이다 (클라는 aud·iss·만료·nonce 만 확인)
//
// 서버 연결은 로그인 게임모드가 한다 (UEmberServerManager::Connect). 이 서브시스템은 계정 정보만 들고 있다.
// 콘솔(개발용): nu.Google.SignIn / nu.Google.Cancel / nu.Google.Status
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUGoogleAuthSubsystem.generated.h"

class IHttpRouter;
struct FHttpServerRequest;

// 로그인된 구글 계정. IdToken 은 1시간짜리 --- 재접속 때는 EnsureFreshIdToken 으로 새로 받는다
USTRUCT(BlueprintType)
struct PROJECTM_API FNUGoogleAccount
{
	GENERATED_BODY()

	// 구글 계정 고유 번호 (바뀌지 않는다)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Google")
	FString Sub;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Google")
	FString Email;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Google")
	FString Name;

	FString IdToken;
	FString RefreshToken;     // 메모리에만 둔다 (디스크 저장 안 함)
	FDateTime IdTokenExpiry;

	bool IsValid() const { return !Sub.IsEmpty() && !IdToken.IsEmpty(); }
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FNUGoogleSignInFinished, bool /*bSuccess*/, const FText& /*Message*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FNUGoogleSignInProgress, const FText& /*Message*/);

UCLASS()
class PROJECTM_API UNUGoogleAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// 브라우저로 구글 로그인을 시작한다. 이미 진행 중이면 false
	bool SignIn();

	// 진행 중인 로그인을 그만둔다 (브라우저 창은 사용자가 닫는다)
	void Cancel(const FText& Reason);

	bool IsSigningIn() const { return bSigningIn; }
	bool IsSignedIn() const { return Account.IsValid(); }
	const FNUGoogleAccount& GetAccount() const { return Account; }

	// 설정에 클라이언트 ID가 들어 있는지 (없으면 로그인 화면이 버튼 대신 안내를 띄운다)
	static bool IsConfigured();

	// 게임 user_id. 토큰 검증 서버(bServerVerifiesIdToken): "g_" + 구글 sub 그대로 --- 서버가 토큰의 sub 와 맞춰 보고
	// 다르면 401 AuthKey subject mismatch. 임시 방식: 접두어 + SHA-256(sub) 앞자리 20자
	FString MakeGameUserId() const;

	// ID 토큰이 5분 안에 만료되면 refresh token 으로 새로 받는다. 끝나면 Done(성공 여부)
	void EnsureFreshIdToken(TFunction<void(bool)> Done);

	// 계정 정보를 지운다 (로그아웃)
	void SignOut();

	FNUGoogleSignInFinished OnSignInFinished;
	FNUGoogleSignInProgress OnSignInProgress;

private:
	bool OpenCallbackListener();
	void CloseCallbackListener();
	bool HandleCallback(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	void ExchangeCode(const FString& Code);
	bool ApplyTokenResponse(const FString& Json, FText& OutError);
	void Finish(bool bSuccess, const FText& Message);
	bool HandleTimeout(float DeltaTime);
	static void BringGameWindowToFront();

	FNUGoogleAccount Account;

	bool bSigningIn = false;
	FString CodeVerifier;
	FString State;
	FString Nonce;
	FString RedirectUri;

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RouteHandle;
	FTSTicker::FDelegateHandle TimeoutHandle;
};
