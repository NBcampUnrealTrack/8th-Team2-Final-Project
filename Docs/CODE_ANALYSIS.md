# ProjectM — "Ember Chronicle" 코드 분석

> 분석일: 2026-07-22 · 대상 커밋: `565f533` (Initial commit) · UE 5.8 · C++ 클라이언트 프로토타입
> 소스 ~5,600줄 (에셋 포함 초기 커밋 48,113줄) · 싱글플레이(오프라인), 서버 권위 대비 설계

---

## 1. 한 줄 요약

**HD-2D 턴제 JRPG** — 옥토패스 트래블러 / 브레이블리 디폴트 스타일에 **타이밍 액션(패링/스트라이크)** + **포켓몬식 몬스터 포획**을 얹은 프로토타입. 결정론적 전투 수학을 액터/애니메이션과 분리해 **미래 서버 권위(RevMMOServer 등)로의 이전을 처음부터 상정**하고 있다.

## 2. 게임 정체성 (차용 원작)

| 요소 | 차용 원작 |
|------|-----------|
| 심볼 인카운터 + 선제/기습(백스탭) 판정 | 크로노 트리거 / 옥토패스 |
| 약점·실드·BREAK 시스템 | 옥토패스 |
| 부스트 포인트(BP, 0~3) | 브레이블리 디폴트 |
| 타이밍 패링/스트라이크 | 슈퍼 마리오 RPG / 세아 오브 스타즈 |
| 몬스터 포획·수집 | 포켓몬 |
| HD-2D 비주얼 | 2D 스프라이트(Paper2D/PaperZD) 빌보딩 + 3D 월드 |

문서·주석이 외부 "design doc v0.1"의 § 번호를 계속 참조 → 상세 기획서가 별도 존재.

## 3. 프로젝트 구조 (기능별 폴더)

```
Source/ProjectM/
  Field/    ← 오버월드: FieldPlayerCharacter / FieldEnemy / FieldCompanion
  Battle/   ← 전투: BattleManager(1,334줄) / BattleUnit / ParryComponent
  Core/     ← EmberGameInstance / EmberBattleStatics / EmberProtoGameMode
              EmberPlayerController / EmberHUD / EmberTypes.h
  Animation/← EmberPaperZDAnimInstance
Source/ProjectMEditor/  ← PaperZD 마이그레이션 에디터 유틸
Content/    ← Art / Maps(L_Proto) / Cinematics
Scripts/    ← 파이썬 에셋 생성, PowerShell 검증 하네스
Plugins/Revvy ← 자체 MCP 서버 연동 플러그인 (서브모듈, 클론 시 비어있음)
```

| 클래스 | 위치 | 역할 |
|---|---|---|
| `UEmberGameInstance` | Core | 필드↔전투 상태 이관 (파티 HP/SP/BP, 인카운터, 복귀 위치, 처치/포획 목록) |
| `UEmberBattleStatics` | Core | **순수 함수** 데미지/패링/포획 판정 (서버 재검증 대비 분리) |
| `AEmberProtoGameMode` | Core | 씬 전체(지형·조명·포스트·아레나·슬라임) 런타임 자동 생성 — 맵 비어도 플레이 |
| `AEmberPlayerController` | Core | 전투 커맨드 메뉴 상태머신 |
| `AEmberHUD` | Core | 캔버스 드로잉 UI (UMG 미사용, 델리게이트 구독) |
| `AFieldPlayerCharacter` | Field | 스프라이트 캐릭터, 스프링암 원근 카메라, 빌보딩, Enhanced Input |
| `AFieldEnemy` | Field | 심볼 인카운터 (감지 스피어, 추적, 선제/기습 판정) |
| `AFieldCompanion` | Field | 요정 팔로우 (Interp + Sine Bobbing), 장식용 |
| `ABattleManager` | Battle | 전투 상태머신 (턴오더/브레이크/부스트/패링), 카메라 전환, ~13 델리게이트 |
| `ABattleUnit` | Battle | 전투 유닛 (HP/SP/BP/실드/브레이크 런타임 + 절차적 플립북 애니메이션) |
| `UParryComponent` | Battle | 패링/스트라이크 타이밍 윈도우 (하나만 동시 오픈) |

## 4. 전투 모델 — 하이브리드 (턴제 코어 + 실시간 타이밍)

상태머신 (`EBattleState`):
```
Inactive → Init → TurnOrderCalc → SelectAction / EnemyThink
        → ResolveAction → CheckState → NextTurn → BattleEnd
```

**플로우**: `StartBattle()` → 인카운터 전환(페이드/카메라 컷) → `SpawnParty()` → `StepTurnOrderCalc()`(Speed 정렬 + 선제 어드밴티지) → `StepNextTurn()`. 플레이어 턴은 `OnActionRequired` 브로드캐스트 후 `SubmitPlayerAction()` 대기, 적 턴은 `StepEnemyThink()`(프로토 AI = 생존 파티원 랜덤 평타). `StartResolve()` → 텔레그래프/예비동작 → `ExecuteImpact()`(데미지) → `StepCheckState()`(승패) → 다음 턴. `FinishBattle()`가 파티 HP/SP/BP를 GameInstance에 기록.

**타이밍 액션** (하나의 `UParryComponent`가 `bOffenseWindow` 플래그로 공/방 겸용):
- **패링(방어)**: PERFECT ≤ 60ms → 데미지 0 + 반격(`ApplyPerfectCounter`) + 슬로모 / GOOD ≤ 150ms → 50% 경감
- **스트라이크(공격)**: PERFECT → ×1.5 크리 + 추가타(`PerfectStrikeExtraHits`) + 슬로모 / GOOD → ×1.2
- 세심한 처리: 계획 임팩트 vs 실제 임팩트 중 **유리한 쪽 채택**(프레임 히칭 방어), 입력 버퍼(최근접 press만 유지), 접근성 윈도우 스케일 + `bAutoGuard`(방어만 GOOD 보장, 공격 제외)

## 5. 데미지 공식 (`UEmberBattleStatics::CalculateDamage`, 결정론)

```
if Parry == Perfect: return 0
base = AttackerPower * SkillPower − DefenderDefense      (min 1)
if BoostSpent > 0:   base *= 1 + 0.5*BoostSpent           (BP 0..3)
if TargetBroken:     base *= 1.5
if TargetDefending:  base *= 0.5
if Parry == Good:    base *= 0.5
if AttackTiming == Perfect: base *= 1.5   elif Good: base *= 1.2
base *= FRandomStream(RandomSeed).FRandRange(0.95, 1.05)
return max(1, round(base))
```
- `AttackerPower` = 원소 스킬이면 `Magic`, 아니면 `Attack`
- RNG = **시드 주입 `FRandomStream`**(`BattleSeed + ActionCounter`) → 롤 재현 가능 (서버 검증 대비)
- 약점: `HitsWeakness()`(무기 또는 원소 ∈ 대상 약점) → `ReduceShield()` → 실드 0이면 `bBroken`(1.5× + 회복 턴 스킵)
- 힐 = `Magic * Skill.Power * (1 + 0.5*BoostSpent)`
- 포획 확률 = `0.35 + (1−HP%)*0.5 + (broken ? 0.25 : 0)`, clamp 0.05~0.95

## 6. 필드 모드

- **플레이어**(`AFieldPlayerCharacter`): 카메라 상대 이동(MaxWalkSpeed 350), 스프링암 1400 / Pitch −40° / FOV 35(HD-2D 쿼터뷰), 매 Tick 스프라이트를 카메라로 빌보딩, 4방향 facing → PaperZD
- **적**(`AFieldEnemy`): **랜덤 인카운터 아님, 가시 심볼**. 감지 스피어(반경 500)로 추적(ChaseSpeed 250), 접촉 오버랩 → 전투. `EnemyPartyIds`로 적 파티 지정
- **전환**: **맵/게임모드 전환 없음** — 같은 레벨 far away(`(20000,0,0)`)에 아레나를 런타임 생성("option B"), HUD 인카운터 와이프 커튼이 화면을 덮은 사이 카메라 컷. 필드 액터는 숨김 후 복원(승리 시 심볼 파괴, `DefeatedFieldEnemies` 기록)
- **선제 판정**(`JudgeAdvantage`, dot product): 미감지 상태로 적 등 접촉 → Player(백스탭), 추적 중 플레이어 등 타격 → Enemy, 그 외 Neutral

## 7. 입력 / 애니메이션

- **Enhanced Input** + 런타임 폴백(`EnsureRuntimeInput`): BP에 IMC 에셋 미지정 시 C++에서 IMC/IA를 생성 → 에셋 없이도 동작
  - Move(Axis2D, WASD+게임패드), Interact(E, 미구현 BP 훅), Parry(Space → 전투 중 `NotifyParryPressed`)
  - **주의**: 전투 메뉴 네비게이션은 EnhancedInput이 아니라 `PlayerTick`에서 raw 키 폴링 — 두 입력계가 `CanAcceptFieldInput()/CanAcceptBattleInput()`로 게이팅되어 공존
- **PaperZD**(2D 스프라이트 in 3D): `UEmberPaperZDAnimInstance`(Abstract)가 `bIsMoving`/`DirectionInput` 두 필드만 노출, 애님 그래프가 소비. `ABattleUnit`은 스켈레탈 리그 없이 플립북 트랜스폼/색을 직접 조작(idle bob, lunge, hit-shake, cast-pop, death sink)

## 8. 네트워킹 — 현재 없음, 서버 대비 설계됨

**현재 순수 싱글플레이/로컬.** replication UPROPERTY·RPC·`GetLifetimeReplicatedProps`·NetMulticast 전무. `GameModeBase`(비네트워크), 상태는 클라 `UGameInstance`. `GetPlayerController(this, 0)` 로컬 1인 전제. gRPC/proto 연동 코드 없음.

**그러나 서버 권위로의 이전을 명시적으로 설계**:
- `EmberBattleStatics` 헤더 주석: "전체 데미지 파이프라인이 나중에 서버 권위로 unchanged 이동 가능(§9)"
- `ParryComponent`: "미래 서버가 보고된 press 타임스탬프로 `JudgeParry` 재실행 가능"
- RNG 시드 재현성 + 순수 함수 분리 = **서버 검증 drop-in 준비 상태**

→ 다음 단계는 이 결정론적 전투 결과를 서버(RevMMOServer)로 태워 권위화하는 작업이 될 것으로 보임.

## 9. 코드 품질

**강점**
- 결정론적 수학(순수 statics)과 상태 액터의 깨끗한 분리 → 테스트/권위화 용이
- 델리게이트 서피스로 UI 완전 디커플 (매니저에 UI 로직 없음)
- 프레젠테이션 단계(`EBattlePresentationPhase`)를 턴 상태와 분리 → 카메라/전환이 전투 FSM 밖. 필드 상태 스냅샷/복원 견고
- 엣지 처리: 프레임히칭 관용 패링, 입력 버퍼, 슬로모 타이머 보정, 타겟 사망 시 재지정, deferred spawn/init

**우려 / 레드플래그**
- **죽은 분기(off-by-one)**: `StepNextTurn`의 첫 턴 Defend가 `RoundIndex == 0` 조건인데, `StepTurnOrderCalc`가 그 전에 `++RoundIndex` → 첫 턴엔 이미 1 → **절대 발동 안 됨**. AutoVerify의 "적 선공 → 패링 경로" 검증이 실제로는 안 돎 (스모크 테스트 한정 영향)
- **전역 시간 팽창**(슬로모, `SetGlobalTimeDilation`)이 배틀 스텝 타이머까지 영향 — 보정은 했으나 실제 netcode 붙으면 취약
- FSM 재귀: `StepNextTurn`(사망 유닛) / `StepTurnOrderCalc↔StepNextTurn`(빈 라운드) — 전멸 라운드 엣지에서 `StepCheckState` 전에 재귀 가능
- 튜닝 상수(윈도우/배율/블렌드/포획) 코드·주석에 산재 → 데이터로 뺄 것
- 검증 훅(CVar `ember.AutoVerify`/`ember.ForceCapture`, 자동 스크린샷)이 프로덕션 함수에 섞임
- 사소: `bAutoPlayerAction` 리셋 안 됨, Fay 컴패니언이 슬라임 애님 재사용(인정된 스탠드인), 파티원 실드 0 하드코딩(브레이크 불가)

## 10. 부수 관찰

- **`.mcp.json` + `Plugins/Revvy`**: 자체 MCP 서버(`http://127.0.0.1:8089/mcp/safe`)와 언리얼 플러그인 연동 — AI 도구로 에디터 조작하는 자체 툴로 추정. 서브모듈은 클론 시 비어있음(`github.com/RevStudio/Revvy`)
- `Scripts/generate_proto_assets.py` 경로에 `Ndus_072` 사용자명 → 다른 팀원이 작성
- 검증 하네스: `ember.AutoVerify 1` CVar 셀프플레이 + `Scripts/run_autoverify.ps1`(실행+캡처+로그추출), `verify_strike_timing.ps1`(Space 스팸으로 PERFECT 유도). 2026-07-06 기록상 PERFECT strike 크리(43 vs 평타 11)·PERFECT parry(dmg 0)·BREAK·승리 검증됨
- 기본 맵 `L_Proto`, GameMode `EmberProtoGameMode`, GameInstance `EmberGameInstance` (Config에 지정)

## 11. 결론

잘 짜인 턴제 JRPG 클라이언트 프로토타입 — 동작하는 수직 슬라이스(필드→조우→전투→복귀 전체 루프 + 포획/파티/인벤 + 캔버스 전투 UI + 헤드리스 셀프플레이 검증). **RevMMOServer와의 결합을 상정한 구조**(결정론적 전투 수학 분리)이나, 아직 네트워킹 코드는 없는 "서버 붙일 준비만 된 오프라인 클라" 상태다.
