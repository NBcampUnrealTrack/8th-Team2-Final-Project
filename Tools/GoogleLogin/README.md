# 구글 로그인

이름만 넣으면 접속되던 로그인 화면을 구글 계정 로그인으로 바꿨습니다. 개발용 이름 접속은 개발 빌드에만 남깁니다.

## 흐름

1. 로그인 화면의 **Google 계정으로 로그인** → 기본 브라우저에 구글 계정 선택 화면이 열립니다
   (게임 안 웹뷰는 구글이 막기 때문에 시스템 브라우저를 씁니다 --- 데스크톱 앱 OAuth 표준 방식)
2. 계정을 고르면 브라우저가 `http://127.0.0.1:47110` (후보 47110~47114) 으로 돌아오고, 게임이 받아서 "이 창을 닫고 게임으로 돌아가세요"를 보여 준 뒤 게임 창을 앞으로 가져옵니다
3. 게임이 받은 코드를 구글 토큰 주소에서 ID 토큰으로 바꿉니다 (PKCE + state + nonce)
4. 서버 접속: ID 토큰을 `AuthUser.auth_key` 로, user_id 는 `g_` + 구글 계정 번호(sub) --- 서버가 서명·aud·iss·만료와 sub 일치를 확인
   (서버 10/14 반영, `bServerVerifiesIdToken=True`). 끄면 옛 임시 방식(sub 해시 20자 + 고정 키)

## 처음 한 번: 구글 클라우드 설정 (팀에서 한 명)

1. https://console.cloud.google.com → 프로젝트 만들기(또는 기존 것)
2. **API 및 서비스 > OAuth 동의 화면**: 앱 이름(Nia Unbound), 지원 이메일, 범위 `openid` `email` `profile`.
   게시 전(테스트 중)에는 **테스트 사용자**에 팀원 구글 계정을 넣어야 로그인할 수 있습니다
3. **사용자 인증 정보 > 사용자 인증 정보 만들기 > OAuth 클라이언트 ID > 애플리케이션 유형: 데스크톱 앱**
4. 나온 클라이언트 ID·보안 비밀을 `Config/DefaultGame.ini` 에:
   ```ini
   [/Script/ProjectM.NUGoogleAuthSettings]
   ClientId=1234567890-xxxx.apps.googleusercontent.com
   ClientSecret=GOCSPX-xxxx
   ```
   (또는 에디터 프로젝트 세팅 > Game > NU Google Login). 데스크톱 앱의 보안 비밀은 게임 파일에 들어가므로 구글도 비밀로 보지 않습니다 ---
   가로채기는 PKCE와 127.0.0.1 콜백이 막습니다. 리디렉션 URI 는 따로 등록하지 않아도 됩니다

ClientId 가 비어 있으면 구글 버튼이 잠기고, 개발 빌드에서는 "구글 로그인 설정 전" 안내와 이름 접속이 보입니다.

## 코드·애셋

| | |
|---|---|
| `Source/ProjectM/Auth/NUGoogleAuthSubsystem` | 브라우저 열기, 127.0.0.1 콜백(엔진 HTTPServer, localhost 전용), 토큰 교환, ID 토큰 읽기, refresh |
| `Source/ProjectM/Auth/NUGoogleAuthSettings` | ClientId·ClientSecret·포트·`bServerVerifiesIdToken`·`LoginWidgetOverride`·`bShowDevNameLogin` |
| `Source/ProjectM/Core/NULoginGameMode` | `RequestGoogleLogin` / `CancelGoogleLogin`, 성공하면 `UEmberServerManager::ConnectWithGoogle` |
| `Source/ProjectM/Core/NULoginWidget` | 로그인 화면 C++ 베이스 (버튼·상태·취소·개발용 묶음·등장 연출) |
| `Source/ProjectM/Net/EmberServerManager` | 인증 키 교체(`AuthKey`), 서버가 준 user_id 채택, 재접속 때 ID 토큰 갱신 |
| `/Game/Art/UI/Login/WBP_Login_Google` | 새 로그인 화면 (기존 타이틀 그림·자리 그대로 + 메뉴 상자 안에 구글 버튼). `apply_login.py` 가 매번 새로 만듦 |

기존 `Developers/dongh/UI/Login/WBP_Login` 은 건드리지 않았습니다. 게임모드 BP 도 그대로이고, `DefaultGame.ini` 의
`LoginWidgetOverride` 한 줄로 새 화면을 씁니다.

## 실행

```
Tools\GoogleLogin\run.ps1 -Steps login,apply    # 위젯 만들기 + ini 적용
Tools\GoogleLogin\run.ps1 -Steps revert         # ini 에서 덮어쓰기 줄 제거 → 옛 로그인 화면
```
개발 콘솔: `nu.Google.SignIn`(구글 로그인만), `nu.Google.Cancel`, `nu.Google.Status`(이메일·user_id·토큰 만료).

## 남은 것

- ~~구글 클라우드 OAuth 클라이언트 만들기~~ --- 10/14 데스크톱 클라이언트(프로젝트 `adroit-dock-511606-i5`) 값을 `DefaultGame.ini` 에 넣음.
  구글 토큰 주소에 가짜 코드를 보내 ID·비밀번호가 맞는 것까지 확인(`invalid_grant`). 계정을 골라 끝까지 로그인해 보는 것은 아직
  (앱이 "테스트 중"이면 그 계정이 테스트 사용자에 있어야 함)
- ~~서버의 ID 토큰 검증~~ --- 10/14 서버 반영·배포, 클라도 켬 (`Docs/SERVER_REQUEST_GOOGLE_LOGIN.md` 맨 위)
- 출시 전: 서버에서 임시 키(`AUTH_KEY_TEMP`) 막기
