# 엠버 크로니클 프로토타입 — 에디터 셋업 가이드

> **⚡ 2026-07-06 업데이트: 즉시 플레이 가능한 프로토 모드 추가.**
> 아래 수동 셋업 없이도 바로 플레이됩니다: 에디터를 열면 `L_Proto` 맵이 뜨고 (Config에 기본 지정),
> Play 누르면 `EmberProtoGameMode`가 씬 전체(지형·조명·포스트프로세스·아레나·슬라임)를 런타임 생성.
> 스프라이트/플립북은 `Content/Art/Sprites/`에 자동 생성돼 있음 (CC0: sodri 4방향 캐릭터, rvros 슬라임).
> 입력·DataTable은 에셋 미지정 시 C++ 런타임 폴백 사용. HUD/전투 UI는 `AEmberHUD` 캔버스 드로잉.
> **조작**: WASD 이동 · 슬라임 접촉 = 전투 · 메뉴 W/S(↑↓) 이동, Enter 확인, Q 뒤로, ←→ 부스트 · **Space = 패링**.
> 아래 원본 가이드는 에셋을 직접 만들어 교체할 때(BP 서브클래스, DataTable 에셋, UMG) 참고용으로 유지.

C++ 뼈대는 빌드 완료 상태입니다 (`ProjectM` 게임 타겟 + `ProjectMEditor` 타겟 모두 성공).
이 문서는 에디터에서 해야 할 나머지 작업을 순서대로 안내합니다. 순서대로 따라가면 M1→M5까지 검증할 수 있습니다.

## 구현된 C++ 클래스 요약

| 클래스 | 위치 | 역할 |
|---|---|---|
| `UEmberGameInstance` | Core | 필드↔전투 상태 이관 (파티 HP/SP/BP, 인카운터, 복귀 위치, 처치 목록) |
| `UEmberBattleStatics` | Core | 순수 함수 데미지/패링 판정 (§9 결정론 분리) |
| `EmberTypes.h` | Core | 모든 enum + `FUnitStatsRow`/`FSkillRow` DataTable 구조체 |
| `AFieldPlayerCharacter` | Field | 스프라이트 캐릭터, 스프링암 원근 카메라, 빌보딩, Enhanced Input |
| `AFieldCompanion` | Field | 페이 팔로우 (Interp + Sine Bobbing) |
| `AFieldEnemy` | Field | 심볼 인카운터 (감지 스피어, 추적, 선제/기습 판정) |
| `ABattleManager` | Battle | 전투 상태머신 (턴오더/브레이크/부스트/패링 통합), 카메라 전환 |
| `ABattleUnit` | Battle | 전투 유닛 (HP/SP/BP/실드/브레이크 런타임 상태) |
| `UParryComponent` | Battle | 패링 윈도우 (PERFECT 60ms / GOOD 150ms, 접근성 스케일) |

---

## 1. GameInstance 지정

**Edit → Project Settings → Maps & Modes → Game Instance Class** = `EmberGameInstance`
(또는 이를 상속한 BP를 만들어 `PartyUnitIds` 편집. 기본값은 `Eliot` 1인.)

## 2. Enhanced Input 에셋

`Content/Blueprints/Core/Input/` 에 생성:

| 에셋 | 타입 | 설정 |
|---|---|---|
| `IA_Move` | Input Action | Value Type = **Axis2D (Vector2D)** |
| `IA_Interact` | Input Action | Digital (bool) |
| `IA_Parry` | Input Action | Digital (bool) |
| `IMC_Field` | Input Mapping Context | 아래 매핑 |

`IMC_Field` 매핑:
- `IA_Move` ← W (기본), S (Negate Y... 아님 → **Swizzle 없이**: W=+Y이 되도록 W에 `Swizzle Input Axis Values(YXZ)`, S에 동일 Swizzle+`Negate`, D=+X 그대로, A=`Negate`) — 표준 탑다운 패턴
- `IA_Move` ← Gamepad Left Thumbstick 2D-Axis (모디파이어 불필요)
- `IA_Interact` ← E / Gamepad Face Button Bottom
- `IA_Parry` ← Space / Gamepad Face Button Left

> C++에서 X=우측, Y=전방(카메라 기준)으로 해석합니다.

## 3. 스프라이트 임포트 (픽셀 선명도 — §1.5)

1. PNG 임포트 → 텍스처 열기 → **Filter = Nearest**, **Mip Gen Settings = NoMipmaps**, Compression = **UserInterface2D 또는 VectorDisplacementmap(무압축)**
   - 일괄 처리: 텍스처 다중 선택 → 우클릭 → Sprite Actions → Apply Paper2D Texture Settings (필터/압축 자동 적용)
2. 텍스처 우클릭 → Sprite Actions → **Create Sprite** (시트면 Extract Sprites)
3. 스프라이트 **Pivot = Bottom Center** (발밑 기준 — 지형 파묻힘 방지)
4. 스프라이트 다중 선택 → 우클릭 → **Create Flipbook** → 방향별 Idle/Walk 플립북 생성

## 4. BP_FieldPlayer

`Content/Blueprints/Field/BP_FieldPlayer` — 부모 = `FieldPlayerCharacter`

Class Defaults에서 할당:
- **Ember|Input**: `DefaultMappingContext = IMC_Field`, `MoveAction/InteractAction/ParryAction` 각각 지정
- **Ember|Animation**: `IdleFlipbooks` / `WalkFlipbooks` 맵에 Down/Up/Left/Right 4방향 플립북 등록
- SpringArm/카메라 기본값은 C++에 설정돼 있음 (FOV 35, Pitch -40, Lag on) — BP에서 미세조정 가능

GameMode: `BP_FieldGameMode` 생성 → Default Pawn = `BP_FieldPlayer` → World Settings 또는 Project Settings에 지정.

## 5. BP_Companion / BP_FieldEnemy

- `BP_Companion` (부모 `FieldCompanion`): Sprite에 페이 플립북. 레벨에 배치만 하면 자동으로 Player Pawn 0 팔로우.
- `BP_FieldEnemy` (부모 `FieldEnemy`): Sprite에 몬스터 플립북, `EnemyPartyIds`에 DT_Units 행 이름 (기본 `Slime`). 레벨에 배치.
  - **주의**: 액터 인스턴스 이름이 처치 기록 키(FieldEnemyGuid)로 쓰이므로 배치 후 이름 변경 금지.

## 6. DataTable

`Content/Data/`:
1. 우클릭 → Miscellaneous → **Data Table** → Row Structure = `UnitStatsRow` → `DT_Units`
   - 행 `Eliot`: HP 120, Attack 14, Speed 12, WeaponType Sword, Skills [`PowerStrike`]
   - 행 `Slime`: HP 60, Attack 8, Speed 6, Weaknesses [Sword], ShieldPoints 2, BattleFlipbook 지정
2. Row Structure = `SkillRow` → `DT_Skills`
   - 행 `PowerStrike`: Power 1.6, WeaponType Sword, SPCost 4
   - 행 `Fireball`: Power 1.4, Element Fire, SPCost 5, HitCount 1

## 7. 전투 아레나 (§5 옵션 B — 같은 레벨)

필드 레벨 구석 (플레이 영역 밖, 예: 지하 -2000):
1. 바닥 메시 + 배경 장식 배치
2. **CameraActor** 배치 — 사이드뷰 각도 (아군 우측, 적 좌측 보이게)
3. 빈 액터(TargetPoint) 6개: 우측에 `PlayerSpawn_0~2`, 좌측에 `EnemySpawn_0~2`
4. `BP_BattleUnit` (부모 `BattleUnit`) 생성
5. `BP_BattleManager` (부모 `BattleManager`) 레벨에 배치, Details에서:
   - `UnitStatsTable = DT_Units`, `SkillTable = DT_Skills`, `BattleUnitClass = BP_BattleUnit`
   - `BattleCameraActor` = 위 CameraActor, 스폰포인트 배열 연결
   - **M3 빠른 검증**: `bAutoPlayerAction = true` → 커맨드 UI 없이 전투 루프 자동 진행

`BP_FieldEnemy`가 접촉 시 자동으로 레벨의 BattleManager를 찾아 `StartBattle` 호출합니다.

## 8. 포스트프로세스 (HD-2D 룩 — §1.4)

PostProcessVolume 배치 → **Infinite Extent (Unbound)** 체크:
- **Depth of Field**: Cinematic, Focal Distance = 캐릭터까지 거리(스프링암 길이 근사), F-stop 낮게 (1.0~2.0) → 틸트시프트 감
- **Bloom**: Intensity 1.0~1.5
- **Color Grading**: Saturation 1.15, Contrast 1.05, 약한 Vignette 0.3

## 9. UI (UMG — 이벤트 바인딩 지점)

전투 UI 위젯에서 레벨의 `BattleManager`를 얻어 (Get Actor Of Class) 아래 델리게이트에 바인딩:

| 델리게이트 | 용도 |
|---|---|
| `OnBattleStarted` / `OnBattleEnded(bVictory)` | 전투 UI 표시/숨김, 필드 HUD 복원 |
| `OnTurnOrderUpdated(TurnOrder)` + `GetTurnOrderPreview()` | 상단 턴오더 바 (§6.3) |
| `OnActionRequired(Unit)` | 커맨드 메뉴 열기 → 선택 후 `SubmitPlayerAction(FBattleAction)` |
| `OnTelegraph(Attacker, Target, TimeToImpact)` | 패링 텔레그래프 연출 (§7.3 — 반짝임/예비동작) |
| `OnUnitDamaged(Unit, Damage, bWeaknessHit, Parry)` | 데미지 숫자, WEAK 표기, 패링 결과 연출 |
| `OnUnitBroken(Unit)` | BREAK 연출 |
| `Parry(ParryComponent).OnParryJudged(Result)` | PERFECT/GOOD/MISS 텍스트 |

커맨드 제출 예 (공격):
```
FBattleAction: Type=Attack, Instigator=GetActiveUnit(), Target=적 유닛, BoostSpent=0~3
```

패링 입력은 이미 C++에서 처리됨 — `AFieldPlayerCharacter`의 `IA_Parry`가 전투 중 자동으로 `BattleManager.NotifyParryPressed()` 호출.

## 10. 마일스톤 체크리스트

- [ ] **M1**: PIE → WASD 이동, 카메라 팔로우, 페이 팔로우, DoF/픽셀 선명
- [ ] **M2**: 필드 몬스터 접촉 → 아레나 카메라 전환 → 전투 종료 후 복귀 + 몬스터 제거
- [ ] **M3**: `bAutoPlayerAction=true`로 턴 루프 확인 → 커맨드 UI 붙이면 false로
- [ ] **M4**: Sword로 Slime 타격 → 실드 감소 → BREAK → 부스트 3 폭딜
- [ ] **M5**: 적 공격 텔레그래프(1.2초) → Space 타이밍 → PERFECT 슬로모 + 무피해 + 반격

## 튜닝 포인트 (Details에서 바로 조정 가능)

- 패링: `ParryComponent.PerfectWindow/GoodWindow/AccessibilityWindowScale/bAutoGuard` (§7.4)
- 전투 페이싱: `BattleManager.TelegraphLeadTime/PostActionDelay/PerfectParrySlomo`
- 인카운터: `FieldEnemy.DetectRadius/ChaseSpeed/BackstabDot`

## 11. 공격 타이밍 STRIKE (기획서 외 추가 — 2026-07-06)

아군 공격에도 패링과 대칭인 타이밍 판정. 임팩트 직전 **Space**:
- **PERFECT**: 크리티컬 배율 + 추가타 / **GOOD**: 데미지 보너스 / **MISS**: 기본 데미지
- 구현: `ParryComponent` 윈도우 로직 재사용 (공격 방향만 반전). 판정 → `EmberBattleStatics` 순수 함수 데미지식에 반영 (§9 서버 대비 분리 유지).
- HUD: 아군 공격 시 STRIKE 수축 바 표시, PERFECT 시 CRITICAL 팝업.
- 로그 태그: `[Ember] StrikeTiming` / `Impact: ... strike=1(GOOD)|3(PERFECT)`

## 12. 조작법 (프로토 기본)

| 키 | 동작 |
|---|---|
| WASD | 필드 이동 |
| W/S + Enter | 커맨드 메뉴 이동·확정 (Q 뒤로) |
| ← → | 부스트 BP 조절 (0~3) |
| Space | 패링(적 공격 임팩트) / 스트라이크(아군 공격 임팩트) |

## 13. 자동 검증 (AutoVerify)

- 콘솔 CVar `ember.AutoVerify 1` (또는 `-ExecCmds="ember.AutoVerify 1"` 실행 인자) → 폰이 자동으로 이동→조우→커맨드→타이밍 입력→승리까지 셀프플레이.
- `Scripts/run_autoverify.ps1` — 게임 실행 + 구간 캡처 + `[Ember]` 로그 추출 일괄.
- 검증 기록 (2026-07-06): AutoVerify와 사용자 PIE 실플레이 모두 PERFECT strike 크리(43 vs 평타 11), PERFECT parry(dmg 0), BREAK, 승리 확인.
