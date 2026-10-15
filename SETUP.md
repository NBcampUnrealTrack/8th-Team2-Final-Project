# ProjectM 세팅 가이드

## 처음 받았다면

```
Scripts\Setup.bat   ← 더블클릭
```

이 하나로 끝납니다. (서브모듈 받기 → TurboLink SDK 설치 → proto 코드 생성)

> **약 6GB를 내려받으므로 처음 한 번은 오래 걸립니다.** 그다음부터는 안 받습니다.

끝나면 `ProjectM.uproject` 를 열면 됩니다.

### 클론부터 하는 경우

```bash
git clone --recurse-submodules https://github.com/RevStudio/ProjectM.git
```

`--recurse-submodules` 를 빠뜨렸어도 `Setup.bat` 이 알아서 받아옵니다.

---

## ⚠️ 서버에서 proto 가 바뀌었다면

`Protos\game.proto` 가 갱신되면 **반드시** 아래를 실행하세요.

```
Scripts\GenerateProto.bat   ← 더블클릭
```

**이걸 빠뜨리면** proto 와 실제 통신 코드가 어긋난 채 빌드되어,
패킷이 안 가거나 엉뚱한 값이 들어가는 형태로 조용히 깨집니다.

> 서버 빌드가 `game.proto` 를 클라로 자동 복사하므로,
> `git pull` 후 `Protos/game.proto` 가 바뀌었으면 위를 돌리는 습관을 들이세요.

---

## 자주 겪는 문제

| 증상 | 원인 / 해결 |
|---|---|
| `protoc.exe is missing` | SDK 미설치 → `Scripts\Setup.bat` |
| `Plugins/TurboLink` 폴더가 비어 있음 | 서브모듈 미초기화 → `Scripts\Setup.bat` |
| 서브모듈 접근 거부 | `RevStudio/TurboLink` 저장소 권한 필요 (관리자에게 요청) |
| 빌드 중 DLL 을 못 쓴다는 오류 | **언리얼 에디터를 닫고** 다시 빌드 |
| proto 를 바꿨는데 반영이 안 됨 | `Scripts\GenerateProto.bat` 실행 후 재빌드 |

---

## 구조 (알아두면 좋은 것)

```
Plugins/
  TurboLink/   ← 서브모듈 (RevStudio/TurboLink)  gRPC 플러그인
  Revvy/       ← 서브모듈 (RevStudio/Revvy)
Protos/
  game.proto   ← 서버가 빌드할 때 자동으로 복사해 넣는다 (여기서 직접 고치지 말 것)
Scripts/
  Setup.bat            최초 세팅
  GenerateProto.bat    proto 바뀌면 실행
```

**TurboLink 의 proto 생성 코드와 6GB SDK 는 git 에 올리지 않습니다.**
각자 스크립트로 만들고 받습니다 — 그래서 위 두 bat 이 필요합니다.

---

## 위치 동기화 (필드에서 다른 플레이어 보이기)

게임 코드에서 쓰는 API 는 이게 전부입니다.

```cpp
UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();

Sync->EnterField();                          // 로그인 후 1회 — 이때부터 남들이 보인다
Sync->SetActivity(EFieldActivity::Battle);   // 전투 진입 (남들 눈엔 "전투중" 상태로 멈춰 보임)
Sync->SetActivity(EFieldActivity::Idle);     // 전투 종료
Sync->LeaveField();                          // 선택 (접속이 끊기면 서버가 알아서 정리)
```

내 위치 전송과 남들 캐릭터 생성/이동/삭제는 **전부 자동**입니다.
블루프린트에서도 같은 이름으로 호출할 수 있고,
`OnRemotePlayerSpawned` / `OnRemotePlayerDespawned` 이벤트로 이름표 같은 걸 붙일 수 있습니다.
