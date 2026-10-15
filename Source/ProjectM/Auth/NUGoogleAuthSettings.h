// 구글 로그인 설정 (프로젝트 세팅 > Game > NU Google Login, DefaultGame.ini [/Script/ProjectM.NUGoogleAuthSettings]).
//
// 구글 클라우드 콘솔에서 "데스크톱 앱" 종류의 OAuth 클라이언트를 만들어 ClientId·ClientSecret을 넣는다.
// 데스크톱 앱의 클라이언트 보안 비밀은 게임 파일에 들어가므로 비밀로 취급되지 않는다(구글 문서 기준) ---
// 대신 PKCE와 127.0.0.1 콜백이 가로채기를 막는다. 리디렉션 URI 는 따로 등록하지 않아도 된다 (루프백은 포트 자유).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUGoogleAuthSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Google Login"))
class PROJECTM_API UNUGoogleAuthSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 구글 클라우드 콘솔 > API 및 서비스 > 사용자 인증 정보 > OAuth 클라이언트 ID (애플리케이션 유형: 데스크톱 앱)
	UPROPERTY(Config, EditAnywhere, Category = "Google")
	FString ClientId;

	// 같은 클라이언트의 보안 비밀. 데스크톱 앱은 토큰 교환 때 구글이 함께 요구한다
	UPROPERTY(Config, EditAnywhere, Category = "Google")
	FString ClientSecret;

	UPROPERTY(Config, EditAnywhere, Category = "Google")
	FString Scopes = TEXT("openid email profile");

	// 브라우저가 돌아올 127.0.0.1 포트 후보. 앞에서부터 비어 있는 것을 쓴다
	UPROPERTY(Config, EditAnywhere, Category = "Google")
	TArray<int32> CallbackPorts = {47110, 47111, 47112, 47113, 47114};

	// 브라우저에서 계정을 고르지 않고 이 시간이 지나면 취소한다 (초)
	UPROPERTY(Config, EditAnywhere, Category = "Google", meta = (ClampMin = "30"))
	float TimeoutSeconds = 180.f;

	// 서버가 구글 ID 토큰을 검증하는지 (Docs/SERVER_REQUEST_GOOGLE_LOGIN.md, 서버 10/14 반영·배포).
	// true  (지금): AuthUser platform_type 1, auth_key = ID 토큰, user_id = "g_" + sub. 서버가 서명·aud·iss·exp 와 sub 일치를 본다
	// false (옛 임시 방식): sub 해시로 만든 id 를 임시 인증 키로 접속 --- 서버가 토큰을 안 보므로 이름 로그인과 보안이 같다
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	bool bServerVerifiesIdToken = true;

	// 임시 방식의 user_id 앞머리. 서버 규약(2~20자, 영숫자와 _)에 맞춰 뒤에 계정 번호 해시 18자를 붙인다
	UPROPERTY(Config, EditAnywhere, Category = "Server", meta = (EditCondition = "!bServerVerifiesIdToken"))
	FString UserIdPrefix = TEXT("g_");

	// 이름 입력 접속(개발용)을 로그인 화면에 보일지. 출시 빌드에서는 이 값과 상관없이 숨긴다
	UPROPERTY(Config, EditAnywhere, Category = "Login")
	bool bShowDevNameLogin = true;

	// 로그인 게임모드 BP 의 LoginWidgetClass 대신 쓸 위젯 (구글 버튼이 있는 화면). 비우면 BP 값 그대로
	UPROPERTY(Config, EditAnywhere, Category = "Login", meta = (MetaClass = "/Script/UMG.UserWidget"))
	FSoftClassPath LoginWidgetOverride;
};
