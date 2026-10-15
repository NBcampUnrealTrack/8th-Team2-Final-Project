# 머리 색 — 서버 요청서

작성 2026-10-14 · 대상: 서버 담당 · 관련 클라 코드는 아직 커밋 전(로컬 작업본)

## 한눈에

캐릭터 생성 화면에 머리 색 고르기(11색)를 넣었습니다. 서버에는 머리 색을 담을 칸이 없어서, 지금은 클라가 **`hair_id` 한 숫자에 머리 모양과 머리 색을 같이 실어** 보냅니다. 서버가 `hair_id`를 저장하고 그대로 돌려주기만 하는 동안에는 이대로 동작합니다.

부탁드리는 것은 두 가지입니다.

1. **지금**: `hair_id`를 지금처럼 검증 없이 저장하고 그대로 돌려주는 동작을 유지해 주세요. 머리 표와 대조하는 검사를 넣을 계획이 있으면 2번을 먼저 해야 합니다.
2. **정식**: 머리 색 칸 `hair_color`를 따로 만들어 주세요. 생기면 클라는 같이 싣는 방식을 걷어냅니다.

## 지금 클라가 보내는 값

```
hair_id = 머리 모양 번호 + 머리 색 번호 × 1000
```

- 머리 모양 번호는 머리 파츠 표(`DT_HairPart`)의 `PartId`입니다. 지금은 1과 2뿐이고, 1000보다 작게 유지합니다.
- 머리 색 번호는 0~10입니다. **0은 "그림 원래 색"** 이라, 머리 색 기능 전에 만든 캐릭터(`hair_id` = 1 또는 2)는 그대로 읽힙니다.
- 머리가 몸 그림에 들어 있는 몸(지금은 여성)은 머리 색을 고를 수 없어 `hair_id`가 0으로 갑니다.

| 보내는 `hair_id` | 뜻 |
|---|---|
| 1 | 머리 1번, 원래 색 |
| 2 | 머리 2번, 원래 색 |
| 3001 | 머리 1번, 3번 색(적색) |
| 7002 | 머리 2번, 7번 색(청색) |
| 10002 | 머리 2번, 10번 색(보라) — 지금 나올 수 있는 가장 큰 값 |

이 값이 지나는 곳은 네 군데입니다(`Protos/game.proto`).

| 어디 | 칸 | 방향 |
|---|---|---|
| `CharacterCreate` | `hair_id = 5` | 클라 → 서버 |
| 캐릭터 정보 JSON (`CharacterList`의 `Characters[]`, `CharacterCreate` 응답) | `"HairId"` | 서버 → 클라 |
| `SVR_Field_Appeared` | `hair_id = 13` | 서버 → 클라 (필드에서 남의 캐릭터) |
| `BattleCombatant` | `hair_id = 22` | 서버 → 클라 (전투) |

### 머리 색 번호표

번호가 곧 저장되는 값이라 **순서를 바꾸거나 중간에 끼워 넣지 않고 뒤에만 보탭니다.** 색 값 자체는 클라 설정(프로젝트 세팅 > NU Appearance Data > Hair Color)에 있고 서버는 번호만 알면 됩니다.

| 번호 | 색 | sRGB |
|---|---|---|
| 0 | 원래 색 (물들이지 않음) | — |
| 1 | 흑색 | `2B2A33` |
| 2 | 갈색 | `6B4A32` |
| 3 | 적색 | `B4412F` |
| 4 | 금색 | `E0B86A` |
| 5 | 은색 | `C9CDD6` |
| 6 | 백색 | `F2EEE6` |
| 7 | 청색 | `3F6FB5` |
| 8 | 녹색 | `4E8F5A` |
| 9 | 분홍 | `E58AA8` |
| 10 | 보라 | `7A56B0` |

## 요청 1 — 지금 유지해 주실 것

- `hair_id`는 `int32` 그대로 저장하고, 위 세 군데 응답에 **보낸 값 그대로** 돌려주세요. 지금 동작과 같습니다.
- `hair_id`가 머리 표에 있는 번호인지, 일정 범위 안인지 **검사하지 말아 주세요.** 검사가 들어가면 머리 색을 고른 캐릭터의 생성이 거절되거나 색이 사라집니다.
- 이미 그런 검사가 있거나 넣을 예정이면 알려 주세요. 그 경우 요청 2를 먼저 반영해야 합니다.

## 요청 2 — `hair_color` 칸 추가

### proto 변경안

번호는 각 메시지에서 지금 비어 있는 다음 번호입니다. 다른 번호가 편하면 바꾸셔도 됩니다.

```proto
message CharacterCreate {
  ...
  int32  hair_id    = 5;  // IN  머리 모양 (서버는 저장·에코만)
  int32  face_id    = 6;  // IN  외모
  int32  hair_color = 7;  // IN  머리 색 번호. 0 = 원래 색 (서버는 저장·에코만)
}

message SVR_Field_Appeared {
  ...
  int32  hair_id    = 13; // OUT
  ...
  int32  hair_color = 19; // OUT 머리 색 번호. CharacterList 의 값 그대로
}

message BattleCombatant {
  ...
  int32  hair_id    = 22; // OUT
  ...
  int32  hair_color = 28; // OUT 캐릭터(kind 0) 머리 색 번호. 아니면 0
}
```

캐릭터 정보 JSON에는 `"HairColor"` 키를 추가해 주세요(`"HairId"` 옆, PascalCase 규약대로). `CharacterList`의 `Characters[]`와 `CharacterCreate` 응답 둘 다입니다.

### 저장

- 캐릭터 테이블에 정수 칸 하나, 기본값 0.
- `hair_id`와 마찬가지로 저장하고 그대로 돌려주면 됩니다. 서버가 색을 해석할 일은 없습니다.

### 검증 (선택)

검증을 넣으신다면 이 정도를 권합니다.

- `hair_color`는 0 이상, 색 개수 상한 이하. 상한은 지금 10이고 색이 늘면 커지므로, 코드에 박기보다 공용 상수 시트에 두는 편이 낫습니다.
- 범위를 벗어난 값을 거절할지 0으로 바꿔 저장할지는 서버 관례를 따르겠습니다. 클라는 모르는 번호가 오면 원래 색으로 그립니다.

### 기존 데이터 옮기기

머리 색을 고른 채 만든 캐릭터는 `hair_id`가 1000 이상입니다. 칸을 만든 뒤 한 번 옮겨 주세요.

```
hair_id >= 1000 인 캐릭터:
  hair_color = hair_id / 1000   (정수 나눗셈)
  hair_id    = hair_id % 1000
```

### 반영 순서

서버와 클라가 같은 날 바뀌지 않아도 깨지지 않게 하려면 이 순서가 안전합니다.

1. **서버**: `hair_color` 칸 추가, 저장·에코. 기존 데이터는 아직 그대로.
2. **클라**: 보낼 때 `hair_id`에는 모양만, `hair_color`에 색. 읽을 때는 `hair_color`를 쓰되 `hair_id`가 1000 이상이면 옛 방식으로도 풉니다.
3. **서버**: 기존 데이터 옮기기.
4. **클라**: 옛 방식 읽기를 지웁니다.

1번이 배포되면 알려 주세요. 2번은 클라에서 하루 안에 됩니다.

## 이번 요청에 없는 것

- **생성 뒤 머리 색·모양 바꾸기**(미용실 같은 기능). 패킷과 비용 규칙이 필요한데 기획이 아직 없습니다.
- **피부색**(`skin_id`). 클라에 자리만 있고 서버로 보내지 않습니다.

## 클라에서 바뀔 곳 (참고)

서버 칸이 생기면 고칠 곳입니다. 지금은 전부 `hair_id` 하나로 동작합니다.

- 싣고 푸는 함수: `NUAppearance::PackHairId` / `SplitHairId` (`Source/ProjectM/CharacterSelect/NUCharacterCreateTypes.h`)
- 보내기: `UEmberServerManager::SendCharacterCreate` (`Source/ProjectM/Net/EmberServerManager.cpp`)
- 받기: 캐릭터 정보 JSON 파싱(`NUCharacterManager.cpp`), `SVR_Field_Appeared`(`EmberServerManager.cpp`), `BattleCombatant`(`Battle/Net/NUBattleServerTypes.cpp`)
- proto를 고친 뒤에는 `Scripts/GenerateProto.bat`을 돌려야 합니다.

## 확인 순서

서버 반영 뒤 같이 확인할 것입니다.

1. 머리 2번 + 청색으로 캐릭터 생성 → 응답의 `HairId` 2, `HairColor` 7.
2. 다시 접속해 `CharacterList` → 같은 값.
3. 다른 계정으로 같은 필드에 들어가 `SVR_Field_Appeared` → `hair_id` 2, `hair_color` 7, 화면에서 파란 머리.
4. 전투 진입 → `BattleCombatant`에 같은 값.
5. 머리 색 기능 전에 만든 캐릭터 → `hair_color` 0, 원래 색.

## 서버 쪽 답이 필요한 것

- `hair_id`에 지금 검증이 있나요, 넣을 계획이 있나요?
- `hair_color` 범위 검증을 서버에서 할까요, 저장·에코만 할까요?
- 범위를 벗어난 값은 거절(코드 무엇)인가요, 0으로 저장인가요?
- 캐릭터 정보를 내려주는 곳이 위 네 군데 말고 더 있나요(예: 다른 사람 정보 보기 `CharacterProfile`)? 있으면 거기에도 `HairColor`가 필요합니다.
