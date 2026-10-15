# 서버 요청: 구글 로그인 (ID 토큰 검증)

작성 2026-10-14 · 클라이언트 쪽 구현은 끝났고(`Source/ProjectM/Auth/`), 서버가 아래를 반영하면 설정 한 줄로 켭니다.

## 서버 회신 (2026-10-14, 반영·배포 완료) → 클라 반영

서버 main `cf961af` 배포, 클라이언트 ID 등록 완료 (상세: reverb "구글 로그인 연동" 페이지, gRPC 연동 가이드 §4.2).
아래 요청 2(서버가 user_id 를 정함)는 채택되지 않고 **클라가 `"g_" + sub` 를 보내는 방식**으로 정해졌습니다.

| | 서버 확정 | 클라 반영 |
|---|---|---|
| 패킷 | 기존 AuthUser(11) 그대로, proto 변경 없음 | 그대로 |
| `platform_type` | 1 | 1 (`PLATFORM_GOOGLE`) |
| `auth_key` | 구글 ID 토큰(JWT) 그대로 | `bServerVerifiesIdToken=True` (DefaultGame.ini) |
| `user_id` | `"g_" + sub` (예 `g_110123456789012345678`, 23자). 다르면 401 `AuthKey subject mismatch` | `UNUGoogleAuthSubsystem::MakeGameUserId` |
| user_id 길이 | 상한 64자 | 로그인 화면 검사 20 → 64자 |
| 검증 | 구글 공개키 서명(RS256만), iss, aud = 우리 클라이언트 ID, exp(시간 오차 0), sub | 재접속 때 만료 5분 전이면 토큰 새로 받음 |
| 받지 않는 것 | access/refresh 토큰, 이메일, nonce | nonce 는 클라에서만 확인 |
| 고정 키 | 출시 전까지 개발용으로 켜 둠 (출시 설정에서 끔). 구글로 한 번 인증한 계정은 고정 키로 못 엶 | 개발용 이름 접속은 그대로 |
| 실패 (전부 401) | Invalid AuthKey / AuthKey subject mismatch / Unsupported platform / Temp AuthKey disabled / Temp AuthKey not allowed for Google account / UserId length is invalid | 로그인 화면 상태 줄에 접속 실패 |

서버: `43.202.139.187:7000` (테스트·운영 같은 서버), 지금은 평문 gRPC(TLS 는 별도 일정). 계정 DB 에는 `g_`+sub·플랫폼·가입/접속 시각만,
이메일·이름·사진·토큰은 저장하지 않음. 탈퇴·삭제 절차와 연령 제한은 기획 미정.

아래는 처음 보낸 요청 원문입니다.

## 지금 상태

| | 지금 | 요청 후 |
|---|---|---|
| 클라 로그인 화면 | 구글 계정으로 로그인 (브라우저) + 개발용 이름 접속 | 같음 |
| `GameMessage.user_id` | 클라가 정함: 구글 계정 번호(sub)의 해시 `g_` + 18자 (서버 규약 20자) | 클라는 아무 값이나(힌트) 보냄, **서버가 정함** |
| `AuthUser.platform_type` | 1 (Google) | 1 (Google) |
| `AuthUser.auth_key` | `AUTH_KEY_TEMP` (임시 고정 키) | **구글 ID 토큰 (JWT)** |
| 보안 | 지금 이름 로그인과 같음 (누구나 남의 user_id 로 접속 가능) | 구글이 보증한 계정만 접속 |

지금도 같은 구글 계정이면 늘 같은 게임 계정으로 들어갑니다(서버 변경 없이 동작). 다만 서버가 토큰을 확인하지 않으므로,
user_id 문자열을 아는 사람은 그 계정으로 접속할 수 있습니다 --- 출시 전에 아래 반영이 필요합니다.

## 요청 1. AuthUser 에서 구글 ID 토큰 검증

`platform_type = 1` 이고 `auth_key` 가 `AUTH_KEY_TEMP` 가 아니면 구글 ID 토큰으로 보고 검증합니다.

1. JWT 서명 검증: 구글 공개키 `https://www.googleapis.com/oauth2/v3/certs` (헤더 `kid` 로 키 선택, RS256). 키는 캐시 (응답의 Cache-Control 만큼)
   - C# 은 `Google.Apis.Auth` 패키지의 `GoogleJsonWebSignature.ValidateAsync(idToken, new ValidationSettings { Audience = new[] { "<클라이언트 ID>" } })` 한 줄로 아래 2~4까지 됩니다
2. `aud` == 우리 OAuth 클라이언트 ID (데스크톱 앱 클라이언트, 클라와 같은 값 --- 프로젝트 세팅 `NU Google Login > Client Id`)
3. `iss` == `https://accounts.google.com` 또는 `accounts.google.com`
4. `exp` 가 지나지 않음 (시계 오차 몇 분 허용)
5. 계정 키 = `sub` (구글 계정 고유 번호, 바뀌지 않음). `email` 은 표시·연락용으로만 (바뀔 수 있음)

실패하면 지금처럼 인증 실패 ResultCode 로 거절해 주세요 (클라는 "접속 실패"를 띄우고 다시 로그인하게 합니다).

## 요청 2. 서버가 user_id 를 정해서 돌려주기

지금은 클라가 보낸 `user_id` 로 세션 토큰 재료를 만들고 이후 모든 요청을 그 id 로 받습니다.
구글 로그인에서는 서버가 `sub` 로 계정을 찾거나 만들고, **그 계정의 user_id 를 AuthUser 응답의 `GameMessage.user_id` 에 실어** 주세요.

- 클라는 응답 user_id 가 보낸 것과 다르면 그 값으로 바꾼 뒤 세션 토큰을 만듭니다(`FEmberBearerToken::GenerateToken(PassKey, UserId, ...)`) ---
  서버도 같은 user_id 로 토큰을 검증해 주세요
- user_id 형식은 지금 규약(2~20자, 영숫자와 `_`)을 그대로 쓰면 클라 변경이 없습니다. 기존 임시 방식 id(`g_` + sha256("google:" + sub) 앞 18자)를
  그대로 써 주시면 지금 만들어진 계정이 그대로 이어집니다 (권장)
- 이어지는 `LoginUser` → 계정 없음(1101) → `CreateUser` 흐름은 지금과 같습니다

## 요청 3. 재접속

ID 토큰은 1시간짜리입니다. 클라는 재접속 때 refresh token 으로 새 ID 토큰을 받아 다시 `AuthUser` 를 보냅니다. 서버 쪽 추가 작업은 없습니다.

## 반영 순서

1. 서버: 요청 1·2 반영 (기존 `AUTH_KEY_TEMP` 경로는 개발용으로 남겨 두면 이름 접속·지금 클라가 계속 동작)
2. 클라: `Config/DefaultGame.ini` `[/Script/ProjectM.NUGoogleAuthSettings]` 의 `bServerVerifiesIdToken=True` 한 줄
3. 출시 빌드: 서버에서 `AUTH_KEY_TEMP` 를 막기 (클라 출시 빌드는 개발용 이름 접속을 이미 숨깁니다)

## 확인 방법

- 클라 개발 콘솔: `nu.Google.SignIn` (구글 로그인만), `nu.Google.Status` (이메일·user_id·토큰 만료 시각 로그)
- 서버 로그에 `sub`·`email`·결정한 user_id 를 남겨 주시면 맞춰 보기 쉽습니다

## 질문

1. 계정 키를 `sub` 그대로 저장할지, 지금 클라 방식의 해시 id 를 쓸지 (요청 2 권장안)
2. 구글 외 로그인(애플 등)을 같은 `platform_type` 체계로 넣을 계획이 있는지 --- 있으면 클라도 같은 구조로 붙입니다
3. 같은 구글 계정이 이미 접속 중일 때 새 접속을 받으면 이전 세션을 끊을지 (지금 Kick 메시지 사용 여부)
