# 구글 로그인 켜는 법 (OAuth 클라이언트 만들기)

게임의 구글 로그인 코드는 들어가 있습니다. 실제로 로그인되려면 **구글 클라우드에 우리 게임을 등록하고 받은 ID 두 개**를
설정에 넣어야 합니다. 구글 계정이 있는 사람이 한 번만 하면 되고, 10분쯤 걸립니다. 비용은 없습니다.

> 화면 이름은 2025년 이후 구글 클라우드 콘솔 기준입니다. 예전 화면이면 "OAuth 동의 화면" 메뉴가 같은 역할입니다.

## 1. 프로젝트 만들기

1. https://console.cloud.google.com 에 회사(또는 팀) 구글 계정으로 접속
2. 위쪽 프로젝트 고르는 칸 → **새 프로젝트** → 이름 `NiaUnbound` → 만들기
3. 위쪽 칸에서 방금 만든 프로젝트가 골라져 있는지 확인

## 2. 로그인 화면에 보일 앱 정보 (Google Auth Platform)

1. 왼쪽 메뉴(또는 검색창에 "Google Auth Platform") → **Google Auth Platform** → **시작하기**
2. **앱 정보**
   - 앱 이름: `Nia Unbound` (구글 계정 고르는 화면에 이 이름이 보입니다)
   - 사용자 지원 이메일: 팀 메일
3. **대상(Audience)**: **외부** 선택 (회사 Google Workspace 사람만 쓸 거면 내부)
4. **연락처 정보**: 팀 메일 → 동의 → **만들기**

### 테스트 사용자 넣기 (중요)

앱이 "테스트 중" 상태인 동안에는 **등록한 계정만** 로그인할 수 있습니다.

1. Google Auth Platform → **대상(Audience)** → **테스트 사용자** → **사용자 추가**
2. 로그인해 볼 팀원들의 구글 이메일을 넣고 저장 (최대 100명)

테스트 중 상태에서는 7일마다 다시 로그인해야 할 수 있습니다(구글이 refresh token 을 7일 뒤 만료). 출시할 때 **앱 게시**를 누르면 풀립니다.
`openid` `email` `profile` 만 쓰므로 게시할 때 구글 심사는 따로 필요 없습니다.

## 3. 클라이언트 ID 만들기 (게임에 넣을 값)

1. Google Auth Platform → **클라이언트** → **클라이언트 만들기**
2. **애플리케이션 유형: 데스크톱 앱** (← 꼭 이것. "웹 애플리케이션"으로 만들면 게임 로그인이 안 됩니다)
3. 이름: `NiaUnbound PC` → 만들기
4. 뜨는 창의 두 값을 복사합니다
   - **클라이언트 ID**: `1234567890-abcdefg.apps.googleusercontent.com` 모양
   - **클라이언트 보안 비밀번호**: `GOCSPX-...` 모양 (창을 닫으면 다시 볼 수 없을 수 있으니 바로 복사하거나 JSON 다운로드)

리디렉션 URI 는 따로 넣지 않습니다. 데스크톱 앱은 `http://127.0.0.1:<포트>` 가 자동으로 허용됩니다.

## 4. 게임에 넣기

둘 중 하나:

- **에디터**: 편집 > 프로젝트 세팅 > Game > **NU Google Login** → Client Id, Client Secret 에 붙여넣기
- **파일**: `Config/DefaultGame.ini` 의 이 칸 (이미 만들어져 있음)
  ```ini
  [/Script/ProjectM.NUGoogleAuthSettings]
  LoginWidgetOverride=/Game/Art/UI/Login/WBP_Login_Google.WBP_Login_Google_C
  ClientId=여기에 클라이언트 ID
  ClientSecret=여기에 클라이언트 보안 비밀번호
  bServerVerifiesIdToken=True
  ```

> 데스크톱 앱의 보안 비밀번호는 게임 파일에 함께 들어가므로 구글도 비밀로 취급하지 않습니다(구글 문서 기준).
> 깃에 올려도 됩니다. 대신 **웹 애플리케이션** 클라이언트의 비밀번호는 절대 넣지 마세요.

## 5. 확인

1. 로그인 레벨(`/Game/Maps/L_LoginLevel`)을 플레이 → 메뉴 상자의 **Google 계정으로 로그인**
2. 브라우저가 열리면 테스트 사용자로 등록한 계정을 고름 → "이 창을 닫고 게임으로 돌아가세요" 페이지 → 게임 창이 앞으로 옴
3. 게임 상태 줄에 "○○@gmail.com 계정으로 로그인했습니다" → 서버 접속 → 캐릭터 선택 화면
4. 콘솔(`~`)에서 `nu.Google.Status` → 이메일, 게임 user_id(`g_`로 시작), 토큰 만료 시각이 로그에 찍힘

로그인 없이 구글 단계만 보고 싶으면 아무 레벨에서 콘솔 `nu.Google.SignIn`.

## 안 될 때

| 증상 | 원인 |
|---|---|
| 버튼이 회색이고 "구글 로그인 설정 전" 안내 | ClientId 가 비어 있음 |
| 브라우저에 "액세스 차단됨: 앱이 테스트 중" | 그 계정을 테스트 사용자에 안 넣음 (2번) |
| 브라우저에 `redirect_uri_mismatch` | 클라이언트 유형이 데스크톱 앱이 아님 (3번에서 다시 만들기) |
| "로그인 응답을 받을 포트를 열지 못했습니다" | 47110~47114 포트를 다른 프로그램이 모두 씀 (설정의 Callback Ports 에서 바꿀 수 있음) |
| "구글 계정 확인에 실패했습니다" | 보안 비밀번호가 틀림 / 다른 클라이언트의 값 (Output Log 의 LogNUGoogle 에 구글 응답이 찍힘) |
| 구글 로그인은 됐는데 "접속 실패" | 게임 서버 문제 (구글 단계와 별개) |
| 구글 로그인은 됐는데 서버가 401 (Output Log `LogEmberNet`) | `Invalid AuthKey`: 토큰 만료·다른 클라이언트 ID·서버 공개키 문제(사유는 서버 로그) / `AuthKey subject mismatch`: user_id 가 `g_`+sub 가 아님(옛 빌드) / `Temp AuthKey not allowed for Google account`: 구글로 만든 계정을 이름 접속으로 열려 함 |

## 서버 쪽

서버가 구글 ID 토큰을 검증합니다 (2026-10-14 반영·배포, `Docs/SERVER_REQUEST_GOOGLE_LOGIN.md` 맨 위 회신).
`bServerVerifiesIdToken=True` 로 켜 두었고, 게임 계정은 구글 계정마다 `g_` + 구글 계정 번호(sub)입니다.
같은 구글 계정 = 늘 같은 게임 계정, 이메일은 서버가 받지도 저장하지도 않습니다.
개발용 이름 접속(고정 키)은 출시 전까지 서버에서 켜 둡니다 --- 출시 설정에서 끕니다.
