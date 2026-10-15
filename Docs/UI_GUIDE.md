# ProjectM UI 가이드

갱신: 2026-10-01. UI 아키텍처, 디자인 원칙, 구현 상태와 작업별 검증 기록을 통합한 문서다. 이후 UI 변경은 날짜별 문서를 새로 만들기보다 해당 절의 현재 설정과 변경 이력을 갱신한다.

현재 구현 설명은 각 절에 표시한 날짜의 작업 기록과 에셋 참조 조사에 근거한다. 전투 UI의 2026-09-10 기록과 2026-10-01 시안 적용은 별도로 표시했다. **2026-09-21 문서 통합 자체는 기능 재검증을 포함하지 않으며, 이후 검증 범위는 6절에 별도로 기록한다.** `Saved/`의 캡처·로그·백업은 로컬 자료이며 다른 팀원의 체크아웃에는 없을 수 있다.

## 1. 화면과 담당 코드

| 영역 | 주요 에셋 / 코드 | 현재 역할 |
|---|---|---|
| 필드 HUD | `/Game/Developers/MinkiCho/UI/WBP_FieldHUD` | 상단 팔각형 메뉴 6개, 네이비·금색 프레임과 아이보리 아이콘 |
| 채팅 | `/Game/Developers/dongh/UI/Chat/WBP_Chat` | 연결된 탭·대화·입력 영역, 미세한 반투명 그라데이션 |
| 퀘스트 저널 | `/Game/Developers/zxcv1/Quest/UI/WBP_QuestJournal` | 전체 화면 블러, 네이비 목록, 크림 상세, 양쪽 카테고리, On/Off 연출 |
| 퀘스트 목록 / HUD | 같은 Quest/UI 폴더의 `WBP_QuestJournalEntry`, `WBP_QuestWidget`, `WBP_QuestTrackerEntry` | 목록 행, 추적기, 의뢰 제안과 수락·거절 영역 |
| 지역 표시 | `/Game/Developers/zxcv1/WorldLocation/UI/WBP_WorldLocation` | 미니맵 아래 가로 명패, 실제 지역·장소 이름 |
| 전체 메뉴 | `/Game/Developers/dbals/UI/WBP_GameMenu` | 캐릭터 프로필, 지도 포함 메뉴 카드 8개, 하단 시스템 동작 3개 |
| 메뉴 진입점 | [NUUIManager.cpp](../Source/ProjectM/UI/NUUIManager.cpp), [NU_FieldHUDWidget.cpp](../Source/ProjectM/UI/HUD/NU_FieldHUDWidget.cpp) | 공통 메뉴 등록 목록과 각 화면 진입·닫기 처리 |
| 퀘스트 연출 | [NUQuestJournalMotion.cpp](../Source/ProjectM/Quest/UI/NUQuestJournalMotion.cpp), [NUQuestJournalWidget.h](../Source/ProjectM/Quest/UI/NUQuestJournalWidget.h), [NUQuestJournalWidget.cpp](../Source/ProjectM/Quest/UI/NUQuestJournalWidget.cpp) | 열기·닫기 수명주기와 전환 중 입력 관리 |
| 메뉴 프로필 | [NUGameMenuWidget.h](../Source/ProjectM/UI/NUGameMenuWidget.h), [NUGameMenuWidget.cpp](../Source/ProjectM/UI/NUGameMenuWidget.cpp), [NUGameMenuProfile.cpp](../Source/ProjectM/UI/NUGameMenuProfile.cpp) | 실제 캐릭터 정보 갱신, 초상화 풀 대여·반환 |

## 2. 아키텍처와 데이터 흐름

### 2.1 기본 구조

전투 UI는 서버 권위의 `ANUBattleEncounter` + GAS를 기준으로 설계했다. 구 `ABattleManager` / `ABattleUnit` / Canvas 기반 `ANUHUD` 프로토타입과 데이터 소스·델리게이트를 혼용하지 않는다.

```text
UNU_BaseUserWidget                 PlayerController / Pawn 컨텍스트
├─ UNU_BaseInGameWidget            PlayerState / ASC, 전투 상태 판정
│  ├─ UNU_BaseFieldWidget          전투 중 접히는 필드 UI
│  │  └─ UNU_FieldHUDWidget
│  ├─ UNU_BaseBattleWidget         Encounter / 전환 / 종료 / 패리 구독
│  │  ├─ UNU_BattleHUDWidget
│  │  └─ UNU_BattleSwitcherWidget
│  └─ UNU_BaseSheetWidget          게임 중 여닫는 창의 기반
└─ UNU_BasePopupWidget             PlayerState가 없어도 가능한 팝업 기반
```

이 계층은 공통 HUD 계열의 설계다. 모든 메뉴가 이 계층을 상속한다는 뜻은 아니다. 현재 `UNUQuestJournalWidget`은 `UUserWidget`을 직접 상속하며 자체 열기·닫기 API와 `UNUUIManager`를 사용한다.

- `IsInBattle()`은 공통 부모인 `UNU_BaseInGameWidget`에 두고 Field/Battle 자식은 결과에 반응한다.
- 기본 데이터 전달은 `Gameplay System → Delegate → Feature Widget → 필요한 하위 Widget`이다.
- `UNU_PlayerVitalWidget`은 로컬 ASC를 구독한다. `UNU_CombatantStatusWidget`은 생성자가 `SetCombatant(Encounter, CombatantId)`로 지정한 전투원의 ASC를 구독한다.
- `PlayerController → CreateWidget → AddToViewport → ZOrder` 방식과 UIManager의 기존 창 관리 흐름을 유지한다.
- 필드 메뉴 버튼은 `GetGameMenuEntries()`의 등록 델리게이트를 호출한다. 동일한 창 열기 로직을 HUD와 ESC 메뉴에 각각 구현하지 않는다.
- ViewModel은 같은 데이터를 여러 화면이 소비하면서 변환·구독 중복이 커질 때 도입을 검토한다. `CommonActivatableWidgetStack`도 현재 구현으로 간주하지 않으며 입력·레이어 관리의 실제 필요가 생길 때 검토한다.

### 2.2 전투 UI의 기존 구현 기록

아래는 **2026-09-10 당시 확인한 내용**이다. 최신 퀘스트·채팅 작업에서 전투 기능을 재검증하지 않았으므로 후속 작업 전 현재 코드와 에셋을 다시 확인한다.

| 구성 | 당시 구현 / 동작 |
|---|---|
| Field/Battle HUD | 로컬 HP/MP, 턴·페이즈 텍스트, 아군·적군 상태 항목 PIE 확인 |
| 턴 순서 | `GetUpcomingTurnOrder()`가 기존 ActionQueue 순서를 사용. `ActionExecution`에서 전투원 번호를 표시하고 다른 페이즈에서는 비움 |
| 전투 명령 | `UNUBattleCommandComponent`의 조회·입력·델리게이트를 `WBP_BattleCommand`가 사용 |
| 데미지 표시 | ASC의 `OnDamageApplied` → `Txt_DamagePopup`에 `-42`, `+30`, `-42!`; `BP_OnDamagePopup`으로 확장 가능. 사라지는 연출은 당시 미구현 |
| 크리티컬 전달 | `NUDamageExecution`에서 `LastHitCritical`을 `IncomingDamage`보다 먼저 출력하여 PostGameplayEffectExecute 처리 순서를 보장 |
| 결과 표시 | `Txt_Result` / `RefreshResultText()`로 승리·패배·도주 표시. Phase와 BattleResult 복제 도착 순서 차이 및 로컬 종료 경로를 모두 처리 |
| 전투 전환 | PlayerController의 `HandleEncounterTransition()`이 `OnTransitionCue`를 받아 FadeOut/FadeIn LevelSequence를 재생. 별도 UMG 전환 오버레이는 당시 불필요 |
| SP | SP/MaxSP 어트리뷰트, 초기화용 SetByCaller 태그와 `SPText` 배선 완료. 실제 초기화 GE 변경은 보류되어 PIE에서 0/0 확인 |

### 2.3 전투 시안 적용 (2026-10-01)

`/Game/Developers/MinkiCho/인게임_전투_UI_시안`을 기준으로 금색 나침반 턴 표시, 좌하단 명령 목록과 선택 프레임, 중앙 하단 패리 바를 적용했다. 화면 중단 왼쪽 지역 표시 위젯은 수정하지 않았다.

| 에셋 | 현재 역할 |
|---|---|
| `MinkiCho/UI/WBP_BattleHUD` | 좌상단 금색 별 문양 옆 TURN + 두 자리 턴 번호 한 줄, 구분선 아래 COMMAND·ACTION. 얇은 화면 장식 유지, 상시 키 가이드는 제거 |
| `dongh/UI/WBP_BattleCommand` | 루트 메뉴는 명령 중인 전투원 옆을 따라가는 반투명 문양 패널, 대상 선택·결정·취소 패널은 TargetSelection 상태에서만 우하단 표시. Root/Target/Item/Skill/Naia/Submitting 상태 순서와 기존 입력·실패 알림 유지 |
| `MinkiCho/UI/WBP_BattleCommandButton` | 전투 전용 버튼. 기존 공통 버튼의 네이티브 부모와 라벨·선택 색 API를 사용하며 공용 버튼 에셋에는 영향 없음 |
| `MinkiCho/UI/WBP_TeamCombatantStatus`, `WBP_EnemyCombatantStatus` | HP/SP 숫자 위젯 제거. 아군은 초록 HP·파랑 SP 바, 적은 이름 → 작은 레벨 → 붉은 HP 바. 상태 효과·직업 자원·SP 투입 미리보기 경로 유지 |
| `dongh/UI/WBP_ParryCue` | 패리 비용을 주황색 포인트 아래 중앙으로 배치. 실제 `ParrySPCost`를 사용하며 타이밍·진행·회피 이벤트는 유지 |
| `dongh/BattleAvatar/BP_TestIndicator` | 기존 선택 대상 머리 앵커에 붙는 금색 마름모. Construction Script에서 전용 머티리얼·0.3배 크기·Z 36을 지정 |
| `dongh/BattleAvatar/BP_TestSpiritAvatar`, `BP_TestEnemyAvatar`, `BP_TestPlayerAvatar` | Construction Script에서 상태 위젯의 원하는 크기에 맞춰 그리도록 설정 |

아트는 `MinkiCho/Art/Battle/Concept20261001`에 모았다. 적 이름·레벨은 `UNU_CombatantStatusWidget::SetIdentityDirect()`로 전달한다. 로컬 전투는 전투 참가자와 Naia 정의/레벨에서, 넷 전투는 `NUBattleNetStageSubsystem`의 실제 combatant Name/Level에서 갱신한다. 넷 전투가 아군용 상태 위젯 클래스를 적에게 재사용해도 적은 SP 바를 숨기고 적 정보와 붉은 HP 바를 표시한다.

기본 입력은 `IMC_BattleCommand.default_key_mappings`와 `NUBattleCommandComponentBase::NavigateSelection()`을 기준으로 한다. **W/S 또는 ↑/↓는 메뉴·대상 선택, A/D 또는 ←/→는 대상 선택 중 SP 투입량 조절, Z는 확인, X는 취소, Space는 패리**다. 대상 선택 패널은 W/S 대상 변경·Z 결정·X 취소를 키캡 형태로 표시한다. 입력 매핑과 SP 조절 동작은 유지하며 동적 키 재설정/게임패드 아이콘 전환 기능은 추가하지 않았다.

2026-10-01 사용자 피드백 교정: `Bd_Background`에 시안을 참조해 생성한 `T_BattleCommandPanel` RGBA 텍스처를 적용했다(브러시 알파 0.55, 좌측 나침반 문양·오른쪽 페이드). 원본과 built-in image_gen 프롬프트는 `Tools/UIBattle/SourceArt/20261001/`에 있다. 전체 화면 WidgetSwitcher의 직접 자식을 상태별 Canvas로 바꾸고 RefreshUI의 SetActiveWidget 참조 8개를 해당 Canvas로 연결했다. 전역 GuideSurface/GuideCanvas 및 중복 좌하단 CompassGhost, 기존 Text_Verify는 제거했다. **이 교정은 사용자 요청대로 이미지 생성·WBP 수정·저장과 컴파일만 수행했으며 PIE 테스트는 사용자가 진행한다. 6절의 기존 전투 PIE 결과와 캡처는 교정 이전 배치 기록이다.** 교정 전 백업과 일회성 스크립트는 `Saved/BattleUICorrection_20261001/`에 있다.

2026-10-01 패널 전용 최신 시안: `/Game/Developers/MinkiCho/인게임_전투_UI_패널부분`의 중앙 행동 메뉴를 기준으로 `Bd_Background` 브러시와 전투 전용 `WBP_BattleCommandButton.Btn_Common`의 배경 스타일만 교체했다. 얇은 금색 테두리·상하 나침반 장식·오른쪽 워터마크·반투명 남색 바탕, 닫힌 모서리형 행과 선택/호버 금색 강조를 사용한다. 에셋은 `MinkiCho/Art/Battle/PanelReference20261001/`에 있으며 `MI_BattleCommandPanel_Reference`의 `PanelTexture`와 `FillOpacity`로 그림과 농도를 바꾼다. `FrameOrigin/FrameSize`는 460×460 디자인 좌표 기준이라 화면 DPI와 별개로 내용 주변에 배경을 맞춘다. 기존 SB_Command 추적·화면 경계 제한, 위젯 계층·위치·패딩·문구·폰트·아이콘·상태 전환·서버 연결은 수정하지 않았다. 변경 전후 그래프와 레이아웃 일치, WBP 두 개 컴파일 0/0 및 머티리얼 컴파일을 확인했다. 오프스크린 프리뷰에서 패널 렌더링을 확인했으나 중첩 네이티브 버튼의 초기화/배치를 재현하지 못하므로 해당 캡처는 플레이 화면 검증 근거가 아니다. PIE는 실행하지 않았다. 원본/생성 프롬프트는 `Tools/UIBattle/SourceArt/PanelReference20261001/`, 백업/비교 기록은 `Saved/BattlePanelReference_20261001/`에 있다.

2026-10-01 로그인 경로 적용 누락 교정: 에디터 재시작 후 조사에서 앞선 패널 수정의 WBP 저장이 누락된 것을 확인했다. 직접 위젯 속성을 수정한 뒤 컴파일해도 패키지가 dirty가 되지 않아 기본 `save_asset()`가 저장을 건너뛰었다. 두 WBP에 새 브러시를 다시 적용하고 `modify()` 및 `save_asset(path, False)`로 저장했으며, 디스크의 uasset에 새 참조가 들어간 것을 확인했다. 이후 직접 위젯을 수정할 때는 컴파일 성공만으로 저장 완료를 판단하지 않는다.

같은 로그인 경로의 TURN 누락은 `BP_MainGamemode`와 필드 GameMode들이 사용하는 `dongh/PlayerController/BP_NUPlayerController.BattleHUDWidgetClass`가 비어 있었기 때문이다. 이 값을 `MinkiCho/UI/WBP_BattleHUD`로 연결했다. `UNU_BattleHUDWidget`도 기존 서버 상태 통지에서 턴 번호와 COMMAND/ACTION 단계를 읽고, 마지막 행동 재생이 끝나면 결과 화면 위의 TURN 표시를 접도록 보완했다. 로컬 Encounter 경로와 패널 배치·입력은 유지한다. 실제 로그인·서버 전투 테스트는 사용자가 진행하며, 백업과 빌드 기록은 `Saved/BattleLoginFix_20261001/`에 있다.

위 교정은 `ProjectMEditor Win64 Development` 기본 DLL 빌드에 성공했고, 에디터를 완전히 재시작해 새 패널/버튼 브러시와 공용 컨트롤러의 HUD 연결이 유지되는 것을 확인했다(`Saved/BattleLoginFix_20261001/verification.json`). `L_LoginLevel`과 기존 에셋 탭 11개를 복원했으며 PIE·실제 서버 전투는 실행하지 않았다.

2026-10-01 같은 경로의 시안 재교체: 최신 이미지(1746×901)는 사각 외곽 패널 없이 왼쪽 나침반 문양과 원형 아이콘이 붙은 개별 행동 메뉴를 사용한다. `WBP_BattleCommand.Bd_Background`는 새 `T_BattleCommandCompass_Revision2`만 0.42 투명도로 그리며, 행은 완만한 곡선 배치와 270×56/260×56 크기, 10 간격으로 변경했다. 전투원 이름·행동 선택 제목 위젯은 바인딩을 유지하고 접었다. 전투 전용 버튼은 원형 배지와 모서리가 꺾인 남색 면, 선택/호버 시 금색 외곽·오른쪽 마름모 꼬리를 표시한다. 기존 460×460 추적 영역과 위치 계산·화면 제한, 그래프·위젯 계층, 대상/아이템/스킬 등 다른 상태, 체력/턴 HUD는 유지했다. WBP 두 개 0오류·0경고, 머티리얼 컴파일·강제 저장·디스크 변화 확인, 키보드 선택과 호버 디자인 프리뷰의 픽셀 일치 확인. 정적 디자인 미리보기이며 PIE/서버 전투는 미실행; 임시 검토 WBP는 삭제했다. 원본·프롬프트·HLSL은 `Tools/UIBattle/SourceArt/PanelRevision20261001/`, 백업·미리보기·검증 기록은 `Saved/BattlePanelRevision_20261001/`에 있다.

2026-10-01 좌상단 턴 표시도 같은 최신 시안에 맞췄다. `WBP_BattleHUD`의 기존 턴 요소 6개만 수정해 금색 별 문양, 한 줄 `TURN` + 두 자리 번호, 얇은 금색 구분선·끝 마름모와 그 아래 상태 문구로 배치했다. 글꼴은 기존 `MaruBuri-SemiBold_Font`를 사용한다. 원본·built-in image_gen 프롬프트는 `Tools/UIBattle/SourceArt/TurnPanel20261001/`, 텍스처는 `MinkiCho/Art/Battle/TurnPanel20261001/T_BattleTurnCrest`다. 다른 HUD 요소·그래프·계층·유효 GUID·네이티브 바인딩은 유지했다. 컴파일 0/0, 강제 저장 후 별도 명령줄 프로세스에서 전체 13개 위젯 속성 및 GUID 일치를 확인했다. 정적 디자인 미리보기·백업은 `Saved/BattleTurnPanel_20261001/`; 임시 검토 WBP는 삭제했으며 PIE/서버 전투는 실행하지 않았다.

2026-10-01 행동 메뉴 글자 정렬 교정: 전투 전용 `WBP_BattleCommandButton.Txt_Label`을 가로 Fill + 가운데 정렬, 세로 가운데 정렬로 바꾸고 패딩을 좌 68·우 42·상하 0으로 맞췄다. 아이콘과 오른쪽 장식을 제외한 글자 영역이 기준이다. 글꼴 크기는 25→22로 줄였으며 WBP 기본값과 `WBP_BattleCommand` 안의 공격·스킬·아이템·교체·도주 인스턴스에도 22를 반영했다. 그래프·계층은 동일하고 두 WBP 컴파일 0/0, 디자이너 화면 확인 및 강제 저장 후 별도 프로세스 재읽기 검증 완료. 실제 전투는 미실행. 백업·캡처·검증은 `Saved/BattleCommandText_20261001/`.

같은 날 추가 교정: 위 68/42 패딩은 중앙 정렬해도 글자 중심을 버튼 중심보다 오른쪽으로 13만큼 이동시켰다. 최종 패딩은 **좌 36·우 42·상하 0**이다. 높이 56에서 셰이더 몸통 중심은 `폭/2 - 2.8`, 글자 중심은 `폭/2 - 3`으로 맞췄다. 글꼴 22 및 나머지 속성은 동일하며 변경은 좌측 패딩 한 필드뿐이다. 컴파일 0/0, 디자이너 전후 캡처와 별도 프로세스 디스크 재읽기 확인. 기록: `Saved/BattleCommandVisualCenter_20261001/`.

2026-10-01 행동 버튼 선택 연출: `WBP_BattleCommandButton.SelectionPulse`의 UMG RenderTransform 트랙으로 0.20초 동안 크기 1 → 0.94 → 1.02 → 1, X 이동 0 → 2 → 8 → 8을 재생한다(키 시간 0/0.04/0.12/0.20초). `WBP_BattleCommand.RefreshUI`의 기존 선택 판정에서 다섯 버튼의 `SetCommandSelected`를 호출하고, 버튼의 OnMouseEnter/Leave도 같은 상태 갱신 함수를 사용한다. 선택/호버 상태가 바뀔 때만 재생하며 해제 시 애니메이션 중단 후 원래 Transform으로 복귀한다. 아이콘은 버튼 내부 `ActionIcon`으로 옮기고 인스턴스별 `ActionIconBrush`에 원래 머티리얼을 보존해 배경·글자와 함께 움직인다. 바깥 UserWidget의 히트 영역은 고정하고 내부 Btn_Common만 움직인다. `ButtonContent`는 Fill/패딩 0, 기존 라벨 글꼴·패딩 값은 건드리지 않았다. 제거한 외부 아이콘 5개의 GUID만 정리했다. 두 WBP 컴파일 0/0, PIE에서 선택·호버 이벤트/해제/재선택/반복 갱신/비활성 및 5개 아이콘 배치 확인, 별도 프로세스에서 전체 위젯 스냅샷·트랙·GUID 재읽기 일치. 실제 서버 전투와 물리 키/마우스 입력 종단 테스트는 미실행. 증거·백업: `Saved/BattleCommandSelectionAnim_20261001/`.

2026-10-01 문양 비율·선택 강조 후속 수정: 문양 텍스처는 947×1661인데 이전 FrameSize 148×344가 가로를 압축했다. `MI_BattleCommandPanel_Reference`의 FrameSize를 `(196.127634,344)`, FrameOrigin을 `(-24.063817,56)`으로 바꿔 원본 비율과 기존 중심 `(74,228)`·높이·투명도를 유지한다. `SelectionPulse`의 최신 키는 크기 **1 → 0.94 → 1.07 → 1.05**, X **0 → 3 → 12 → 12**이며 0.20초 후 선택 버튼이 105%·오른쪽 12 상태로 유지된다. 기존 선택 해제 복귀·반복 갱신 방지 로직은 유지했다. 이번 수정은 문양 MI의 두 벡터와 전투 버튼 WBP 애니메이션 키만 변경; 위젯 속성·그래프·라벨·아이콘 및 부모 WBP는 동일하다. PIE에서 다섯 버튼 순차 선택/이전 버튼 복귀와 반복 선택값 입력을 검증하고 화면에서 문양 비율을 확인했다. 별도 프로세스 저장 재읽기 및 근거: `Saved/BattleCommandEmphasis_20261001/`. 실제 서버 전투/물리 입력 종단 테스트는 미실행.

## 3. 공통 디자인 기준

### 3.1 기준의 우선순위

ProjectM은 **HD-2D 픽셀 월드 위에 고해상도 현대 판타지 UI**를 사용한다. UI까지 픽셀화하거나 네온·SF 효과로 통일하지 않는다. 가독성, 화면 간 통일성, 정보 계층, 형태·비율, 여백을 먼저 정하고 장식을 더한다.

사용자가 지정한 최신 시안과 승인된 결과를 화면별 직접 기준으로 삼는다. 문서의 색상 예시는 고정 팔레트가 아니다. 시안 분석 시 색만 추출하지 말고 다음을 먼저 기록한다.

1. 패널 실루엣과 화면에서 차지하는 면적.
2. 외곽선의 위치·색·두께·연속성, 모서리 모양.
3. 내부 면의 불투명도와 실제 배경 블러의 위치.
4. 문양 밀도, 글자·아이콘 대비와 정보 계층.
5. 선택·호버 표시와 실제 조작 영역.

퀘스트 시안3의 선명한 금색 경계를 안개처럼 번지는 패널로 해석했던 오류를 반복하지 않는다. 기존 구성을 유지한다는 요청은 기존 면 처리까지 유지하라는 뜻이 아니다. 기능 검증과 사용자의 시각적 승인은 별개다.

### 3.2 색과 표면

| 요소 | 기준 |
|---|---|
| Navy | 검게 뭉치지 않고 푸른 색감이 읽히는 기본·탐색 면. `#101A2B`, `#17243A`, `#1D304A`는 참고 예시이며 필요하면 더 밝게 조정 |
| Ivory / Cream | 따뜻한 글자·아이콘·얇은 선. 참고색 `#EEE7D3`, `#F3EBD8` |
| Muted Gold | 선택선, 작은 다이아몬드, 제목·주요 행동 표식 등 제한된 강조. 밝은 노랑·금속 광택·금색 그라데이션은 억제 |
| Muted Blue | 패널보다 약간 밝은 버튼, 절제된 Hover/Selected 변화. 청록 네온과 강한 발광은 피함 |
| 입력 영역 | 패널보다 한 단계 깊은 색으로 구분하되 입력 글자·힌트·포커스 대비 유지 |
| 퀘스트 알림 | 주황 원과 흰 느낌표를 허용하는 기능적 예외. 주황을 일반 UI 전체로 확장하지 않음 |

기존 가이드 v4의 일반 대형 메뉴 방향은 **Mist Blue / Pale Blue-Gray 주 표면 + Deep/Mid Navy 보조 카드**다. 캐릭터·장비·스킬·성장·나이아 등 향후 통합 메뉴에 적용할 설계 방향이며 모두 구현됐다는 뜻은 아니다. 캐릭터 화면의 참고 면적은 Mist/Pale Blue 40–50%, Navy 25–35%, 월드·빈 공간 10–20%, Ivory 5–10%로, 고정 수치가 아니다.

퀘스트는 밝은 크림 기록면을 사용하는 별도 재질 콘셉트를 허용한다. 2026-09-21의 퀘스트 시안3과 인게임 메뉴 허브는 사용자 지정 시안에 따라 네이비·아이보리 분할과 선명한 금색 선을 적용했다. 과거의 일괄적인 ‘큰 Ivory 면 금지’나 ‘모든 큰 면은 Navy’ 문구로 이 결과를 되돌리지 않는다. 퀘스트 종이 표현을 다른 모든 메뉴로 확대하지도 않는다.

최종 농도는 PNG 색·알파, Brush Tint, Material 값과 레이어 중첩의 합성 결과로 판단한다. 글자까지 흐려지는 전체 RenderOpacity 조정보다 배경·테두리·버튼의 전용 값을 조절한다. 밝고 어두운 게임 배경에서 함께 확인한다.

### 3.3 형태, 구성과 장식

Floating Card는 동일한 사각형의 반복이 아니라 중요도에 따른 크기·위치·깊이의 관계다. 기준이 되는 큰 정보 영역을 두고 보조 카드, 살짝 겹치는 헤더, 경계에 걸친 작은 탭·행동, 충분한 빈 공간으로 2–3단계의 얕은 깊이를 만든다. 기본 정렬과 읽는 순서는 유지한다.

| UI 수준 | 적용 강도 |
|---|---|
| HUD: 채팅·추적·체력·미니맵 | 작은 면적, 강한 정렬, 최소한의 겹침과 장식 |
| Utility: 확인창·상점·파티·NPC 대화 | 기능별 카드 분리, 약한 위치 차이와 문양 |
| Major Menu: 캐릭터·장비·스킬·성장 등 | 화면별 고유 구성, 크기 차이·겹침·독립 헤더·초상화 돌출 허용 |

공통 형태 문법은 작은 Cut Corner, 얇은 이중선, 다이아몬드, 짧은 선·호·코너 표식, Floating Tab과 Tag다. 모든 패널에 같은 프레임이나 모든 모서리에 금 장식을 반복하지 않는다. 큰 카드는 차분하게, 작은 기능 표식은 조금 더 또렷하게 만든다.

Naia는 잎·씨앗·눈물·감은 눈·가지·흐르는 곡선, Arcadia는 다이아몬드·기하학적 선·분절 프레임·대칭 표식을 공통 모티브로 사용한다. 큰 메뉴는 고유 실루엣, 제한된 금색, 구분선, 레이어 관계 등 의미 있는 스타일 요소를 최소 3종 정도 드러내되 장식 개수 자체를 목표로 삼지 않는다.

| 화면 | 기능에서 가져올 구조 |
|---|---|
| Quest | Journal의 색인·분류·기록·추적 |
| Character | 초상화와 모험가 프로필, 독립 능력치·장비 카드 |
| Equipment | 캐릭터/아이템 미리보기, 주변 슬롯, 별도 비교 정보 |
| Skill | 마법식의 노드·연결 관계 |
| Growth | 가지의 분기와 성장 방향 |
| Naia Formation | 초상화·슬롯 관계, 선택 대상의 크기·위치·금색 강조 |
| Inventory | 수납의 분류·그리드와 아이템 정보 |
| Map / Party | 지도 구조 / 모험가 명부 |
| NPC Dialogue | 본문과 선택지를 독립된 영역으로 구성 |
| Character Creation | 캐릭터 미리보기가 중심, 외형 아이콘·입력·목록을 분리 |

실제 책·가방·나무·거대한 마법진을 화면 전체 프레임으로 그리기보다 정보 구조를 차용한다. 반대로 개성을 모두 없애 일반 앱의 둥근 사각형과 글자만 남기지 않는다. 얕은 종이 두께·겹침·미세 섬유는 퀘스트에 제한적으로 허용하며 거친 갈색 양피지, 강한 비네트·베벨·글로·노이즈는 피한다.

### 3.4 에셋 분리와 표시 품질

- 텍스트, 데이터, 아이콘, 실제 버튼, 동적 채움과 선택 표시를 가능한 한 독립 위젯·브러시로 구성한다. 배경에는 정적인 프레임·면·고유 장식만 포함한다.
- 상태별 atlas를 사용하는 현재 구현은 UV 영역을 명확히 구분하고 실제 위젯 상태로 전환한다. 한 장의 완성 화면에 조작 요소를 고정해 넣지 않는다.
- 테두리는 직선·모서리에서 끊기지 않아야 한다. 내부 그라데이션·그림자가 선을 덮지 않도록 원본 확대와 실제 표시 크기 모두에서 확인한다.
- 아이콘은 작은 크기에서도 읽히는 고해상도 평면 실루엣으로 통일한다. 세트 안의 선 굵기·내부 구멍·여백과 실제 불투명 형태의 크기를 맞춘다.
- Normal/Hover/Selected는 색·명도·선으로 구분하고 상태 때문에 형태나 배치가 크게 흔들리지 않게 한다.
- 제목 → 분류 → 주요 내용 → 보조 정보 → 행동의 타이포 계층을 유지한다. 대표 화면을 실제 크기로 조립·비교한 뒤 세트를 확장한다.

### 3.5 팝업 버튼 호버

팝업 버튼은 마우스를 올리면 0.12초 동안 원래 크기의 97%로 줄고, 벗어나면 같은 시간으로 복원한다. SmoothStep 보간으로 전환하며 도중 방향을 바꿔도 현재 크기에서 이어진다. 슬롯 크기는 유지하고 버튼과 내부 글자·아이콘의 RenderScale을 함께 조절한다.

`UNUUIManager::OpenWindow`의 Popup~Modal 레이어 창에 자동 적용한다. 별도 창이 아닌 퀘스트 `OfferPanel`과 상점 `PurchaseDialogPanel`도 `SetPopupButtonHover`로 연결한다. 중첩된 공용 버튼 WBP 안의 실제 UButton까지 찾으며, 숨김·비활성화·닫기·컨트롤러 교체 시 원래 배율을 복원한다. 버튼 가장자리에서 축소 때문에 호버가 반복되는 현상은 원래 영역 안에서 유지하도록 처리한다.

배율과 시간은 `UI/Common/NUPopupButtonHover.h`의 `HoverScale`·`Duration`에서 조정한다. 별도 삽입 팝업을 추가하면 표시 수명주기에 `SetPopupButtonHover` 등록·해제를 연결한다. 회귀 테스트: `ProjectM.UI.PopupButtons.HoverLifecycle`.

## 4. 현재 화면별 구현

### 4.1 필드 HUD와 지역·퀘스트 추적기

2026-09-29 `/Game/Developers/MinkiCho/HUD_시안`을 기준으로 필드 HUD를 네이비·금색 테마로 변경했다. 상단 6개 메뉴 버튼은 82×82, 버튼 사이 간격은 8이며 `IconsV2` 아이콘과 `MI_HUD_MenuOctagon`을 사용한다. 아이콘별 기존 표시 크기는 유지한다.

| 기능 | 아이콘 | UMG 크기 |
|---|---|---|
| 퀘스트 | `T_FHUD_V2_Quest` | 56×56 |
| 캐릭터 | `T_FHUD_V2_Character` | 48×48 |
| 성장 | `T_FHUD_V2_Growth` | 50×50 |
| 능력치 | `T_FHUD_V2_Stats` | 54×54 |
| 인벤토리 | `T_FHUD_V2_Inventory` | 55×55 |
| 나이아 | `T_FHUD_V2_Naia` | 48×48 |

팔각형 바탕의 `HoverAmount`는 기존 공용 버튼 이벤트가 0/1로 갱신한다. 툴팁과 메뉴 등록 델리게이트를 유지하며 아이콘·추가 장식은 HitTestInvisible이다. 우측 마지막 버튼 아래에는 작은 금색 펜던트 장식을 둔다.

오른쪽 아래에는 눈 아이콘 토글(82×82, 우측·하단 여백 24)을 둔다. 2026-09-30 `HUD_시안_+_ui_숨김_아이콘_추가`의 팔각형 버튼을 적용했으며 **뜬 눈 = HUD 표시 중, 감은 눈 = 사용자 설정으로 HUD 숨김 중**이다. 버튼 하나가 클릭마다 전환되고 툴팁은 다음 동작인 `HUD 숨기기` / `HUD 표시`를 안내한다. `HUDVisibilityIcons` WidgetSwitcher를 네이티브 표시 이벤트에서 갱신하므로 다른 기능에서 사용자 표시 설정을 바꾸어도 아이콘이 맞춰진다. 눈 아이콘은 `Art/FieldHUD/VisibilityIcon20260930`의 UI 머티리얼이며 상단 메뉴와 동일한 팔각형 바탕 및 별도 hover/pressed MI를 사용한다. 시안의 두 상태 설명 패널은 만들지 않는다.

눈 아이콘 검증: 전체 ProjectMEditor 빌드 성공, WBP 오류/경고 0 및 EventGraph 3노드 유지. 1920×1032/1280×720 PIE에서 실제 마우스 클릭으로 숨김·복귀, 아이콘 0/1·툴팁 동기화와 버튼 잔존을 확인했다. 빠른 반복 토글, 외부 표시 API 변경, 임시 숨김 요청 중 클릭 차단과 해제 후 수동 숨김 보존도 통과했다. 백업·빌드·런타임 QA·캡처는 `Saved/HUDVisibilityIcon_20260930/`에 있다.

`FieldHUDContent` 아래의 미니맵·상단 메뉴·장식과 별도 위젯인 지역명·퀘스트 추적기·채팅을 함께 접는다. 토글은 콘텐츠와 형제여서 수동 숨김 중에도 누를 수 있고, 전투 중에는 기존 Field HUD의 전투 숨김 규칙을 따른다. `UNUUIManager::SetFieldHUDVisible`이 로컬 플레이어 수명의 사용자 설정을 관리하며 각 표시 소유자가 이벤트를 구독한다. 데이터 갱신과 맵 이동 뒤에도 수동 숨김을 유지하고, 추적기의 개별 접힘·닫힘 상태는 보존한다. 퀘스트 제안과 별도로 연 메뉴는 숨기지 않으며, 다른 기능의 임시 숨김 요청이 없을 때 채팅 입력을 시작하면 HUD를 다시 표시한다.

현재 배경은 `/Game/Developers/MinkiCho/Art/FieldHUD/Concept20260929`의 `M_HUDCutFrame`과 전용 MI를 사용한다. `GetUserInterfaceUV`의 Normalized UV / Pixel Size로 절단 모서리와 선 두께를 계산하며, 네이비 면의 `FillOpacity`와 금색 선의 `FrameOpacity`를 분리한다. 본문 좌우 농도 차이는 6%이고 외곽선은 흐려지지 않는다. 이전 `GradientPanels` 설정은 작업 이력이며 현재 추적기·지역 패널 브러시는 새 MI로 교체했다.

미니맵은 (24,24), 240×240이며 이중 금색 링·N 배지·나침반·해 문양을 사용한다. 해 문양은 정적인 장식이다. 지도 데이터와 마커, 지도 없는 공간에서 접히는 동작은 기존 `MapView`를 따른다. 지역 표시는 (24,276), 310×44의 가로 명패이며 `AreaText`·`LocationText` 네이티브 바인딩을 유지한다. 추적기는 (24,332), 폭 360으로 배치해 미니맵과 겹치지 않는다. 추적 항목의 내부 카드 배경을 제거하고 목표 앞에 금색 마름모를 추가했다.

추적기는 접기·펼치기·닫기·다시 열기와 긴 한글 목록 스크롤을 유지한다. 의뢰 제안은 실제 데이터, 종이 본문과 스크롤 밖의 수락·거절 동작을 유지한다. 2026-09-17의 JournalV4 기록은 저널 메인 화면의 현재 디자인은 아니지만 관련 텍스처와 추적기·의뢰 구성의 이력이다.

HUD 토글 연출은 `UI/HUD/NUFieldHUDMotion`에서 공통으로 처리한다. 숨길 때 가까운 좌우 가장자리 방향으로 32 Slate 단위 이동하면서 0.24초 동안 페이드 아웃하고, 표시할 때 같은 곡선을 역으로 재생한다. 화면에 배치된 위젯 중심으로 방향을 선택하며 아직 레이아웃이 없는 경우 슬롯 정렬/앵커를 사용한다. 전체 화면 루트는 좌측 기본 방향을 사용한다. 연출 중 다시 누르면 현재 진행도에서 반전하고, 종료 시에만 Collapsed로 접는다. 숨김 시작부터 자식 입력을 차단하며 표시 시 입력을 복원한다.

`UNUUIManager`의 Slate PreTick은 연출 중에만 등록한다. `FieldHUDContent`는 표시와 입력만 제어하고 직계 자식이 각각 이동하므로 우측 버튼 줄과 좌측 미니맵이 반대 방향으로 움직이며 이중 페이드도 생기지 않는다. 미니맵의 자체 가시성, 추적기의 접힘/닫힘, 채팅 포커스에 따른 바깥 위젯 불투명도는 해당 기능이 계속 소유한다. 새로 만들어진 HUD도 현재 숨김 상태를 적용하며, 맵 이동 경계에서는 잔여 연출을 완료한다. 전투 진입/이탈의 기존 Field HUD 가시성은 즉시 적용한다. 관련 자동화 테스트는 `ProjectM.UI.FieldHUD.Motion`이다.

다른 기능의 임시 HUD 숨김은 `FNUFieldHUDVisibility`의 요청 목록으로 합산한다. 최종 표시 조건은 **사용자 표시 설정 ON + 활성 숨김 요청 0개**다. `SetFieldHUDVisible`/`IsFieldHUDUserVisible`은 사용자 설정, `IsFieldHUDVisible`은 임시 요청까지 반영한 최종 목표 상태다. 메뉴·대화·컷신은 사용자 설정을 바꾸는 대신 다음 API를 사용한다(C++/Blueprint `NU|UI|HUD`).

```cpp
// 시작/종료 쌍: 같은 객체+사유로 반복 요청해도 중복 누적되지 않는다.
UI->RequestFieldHUDHidden(TEXT("Cinematic"), true, this);
UI->RequestFieldHUDHidden(TEXT("Cinematic"), false, this);
// 소유 기능을 정리할 때 해당 객체의 요청을 모두 해제한다.
UI->ReleaseFieldHUDHideRequests(this);
```

`Owner`와 `Reason`은 모두 필수이며 다른 객체의 같은 사유는 별도 요청이다. `bAnimate=false`는 현재 진행 중인 연출까지 최종 상태로 즉시 맞춘다. 기능이 끝나면 반드시 해제하고, 파괴된 요청 소유자와 관리 창을 직접 닫아 남은 요청은 요청이 존재할 때만 동작하는 0.25초 점검에서 정리한다. PlayerController 교체·서버 세션 종료 때 임시 요청은 초기화하고 사용자 설정은 유지한다. 임시 요청 중에는 수동 토글을 비활성화하고 채팅의 BeginFocus가 HUD를 강제로 표시하지 않도록 한다.

공통 창은 `FNUUIWindowParams::bHideFieldHUD = true` 한 줄로 열기/닫기/정리 시 요청을 자동 관리한다. 기본값은 false이며 기존 메뉴에 자동 숨김을 일괄 적용하지 않았다. 새로운 HUD 위젯은 `PresentFieldHUDWidget` 또는 `PresentFieldHUDGroup`으로 현재의 자체 가시성을 전달하고, 자체 상태 변경 때 다시 호출한다. 해제 시 `ForgetFieldHUDWidget`은 등록된 자식까지 복원·정리한다. 이는 숨김 요청의 소유권과 별개이며, HUD 위젯을 등록했다고 임시 요청이 생기지는 않는다.

요청 중첩·중복·소유자 정리·수동 숨김 보존·모션 반전 테스트는 `ProjectM.UI.FieldHUD.Requests`에 추가했다. **2026-09-29 현재 헤더 반영과 소스 공백 검사를 확인했으며, 전체 빌드 및 신규 테스트 실행은 실행 중인 Live Coding 때문에 아직 완료하지 못했다.** 검증 기록: `Saved/HUDMotion_20260929/verification.md`.

### 4.2 채팅: 연결된 프레임과 미세 그라데이션

2026-09-29 시안 적용 후 채팅은 600×258, 화면 좌측·하단에서 24씩 띄운다. `MI_HUD_ChatPanel` 하나로 네이비 면과 금색 외곽선을 그리고 기존 `CJ_OuterFrame`은 런타임·디자이너에서 접는다. 헤더 높이 40과 4개 탭, 입력 채널 버튼은 유지하며 Enter 안내는 68×40이다. 메시지 BlurStrength 2.5 / BlurRadius 9와 스크롤 분리를 유지한다.

선택 탭은 해당 폭 안의 밝은 블루 면·양옆 경계·금색 하단선·다이아몬드로 표시한다. 채널 아이콘과 글자색, 탭 필터·페이지 이동·전송 동작은 기존 네이티브 로직을 따른다. Enter는 기존 전송 단축키 안내다.

**현재 패널 값: 2026-09-29.** 채팅 FillOpacity 0.89 / FrameOpacity 0.96, 추적기 0.77 / 0.78, 지역 명패 0.91 / 0.90, 메뉴 버튼 0.96 / 0.96이다. 입력칸은 `MI_HUD_Input`, Enter 안내는 `MI_HUD_Enter`를 사용한다. 탭·채널 버튼의 기존 선택 상태와 농도는 유지한다.

아래 표는 **2026-09-21의 이전 패널 조정 이력**이다. 본문·외곽 프레임·입력·Enter의 현재 설정은 위의 새 MI를 따른다.

| 대상 | 최종 값 | 설정 위치 |
|---|---|---|
| 본문 / 외곽 프레임 | **0.442 → 0.390** | `MI_HUDFade_Chat`, `MI_HUDFade_ChatFrame` / `OpacityEnds` |
| 하단 보조 면 | **0.130 → 0.117** | `MI_HUDFade_ChatFooter` / `OpacityEnds` |
| 내부 구분선 | **0.364 → 0.325** | `MI_HUDFade_ChatDivider` / `OpacityEnds` |
| 탭·채널 버튼 배경 | **원본 알파 × 0.312** | `MI_ChatRoot_Control` / `BackgroundOpacity` |
| 이전·다음·추가 Hover / Press | 0.20 / 0.30 | WBP의 조작부 스타일 |
| 입력 포커스 배경 / 테두리 | 0.28 / 0.32 | WBP 입력 스타일 |
| Enter 배경 / 테두리 | 0.22 / 0.26 | WBP 안내 표시 |
| 펼친 채널 메뉴 받침 / 테두리 | 0.86 / 0.38 | 겹친 글자 위 메뉴의 가독성 유지 |

당시 30% 증가는 채팅 전용 MI 5개에 적용했다. 현재 시안 적용은 해당 원본 MI를 수정하지 않고 패널 브러시를 교체했다. 위 수치는 레이어 설정값이며 합성 후 화면 알파와 동일하지 않다.

채팅 전용 에셋은 `/Game/Developers/MinkiCho/Art/FieldHUD/Chat_New` 아래에 있다.

- `Textures/T_ChatJoined_Frame`, `T_ChatJoined_TabIdle`, `T_ChatJoined_TabSelected`: 연결 프레임과 탭 상태.
- `Textures/T_ChatRoot_General`, `Local`, `World`, `Party`, `Whisper`, `System`: 채널 아이콘 6종.
- `Widgets/WBP_ChatRootTab`, `WBP_ChatRootChannel`: 채팅 전용 버튼 위젯.
- `Materials/M_ChatTranslucentControl` / `MI_ChatRoot_Control`: 공용 마스터를 복제한 전용 배경 알파 제어. 공유 버튼 마스터는 유지한다.

`ChatTabBackground`의 불투명한 중복 면은 제거했고 글자·아이콘·전체 위젯의 불투명도는 1을 유지한다. `ApplyChatRootIcon`은 알려진 기본 아이콘만 대응시키며 알 수 없는 아이콘·커스텀 탭은 원본을 유지한다. 최초 네이티브 초기화 이후 한 번, `OnSelectedChanged`·`OnChannelSet`·`OnSendChannelChanged`에서 갱신하며 매 프레임 아이콘 교체를 추가하지 않는다. `CustomTabModal` 하위 29개 위젯의 디자인과 동작은 보존 대상이다.

### 4.3 퀘스트 저널: 선명한 패널과 동기화된 카테고리

현재 기준은 사용자가 다시 올린 `/Game/Developers/MinkiCho/퀘스트_시안`이다(2026-09-27, Revision 2). 1760×990 디자인을 중앙 ScaleBox로 유지하고, 네이비 목록 패널 `(80,30,490,920)`이 아이보리 패널 `(558,96,1160,840)` 위로 돌출된다. 네이비 면은 독립된 금색 이중 프레임이며, 왼쪽에는 돌출된 5개 아이콘 레일과 하단 책·나침반·천구 고리 문양을 배치했다. 이전 용·모험가 문양은 새 시안에 맞춰 교체했다. 흐림은 뒤 월드에만 적용한다.

`QM_IndexMist`는 `/Game/Developers/MinkiCho/Art/Quest/Reference20260927/T_QuestJournal_Navy`를 사용한다. 실제 알파가 있는 모서리·금색 프레임·하단 문양만 포함하며 텍스트나 버튼을 굽지 않았다. 원본 PNG와 생성 프롬프트는 `ArtSource/UI/QuestJournal_20260927/`에 있다. `QM_ReadingMist`는 네이비/골드 외곽 Border로, 기존 FrameV3 Reading 텍스처는 자식 `QR_ReadingPaper`로 유지한다. 레일의 `QL_RailDivider`도 이중 테두리 Border이며 기존 모션 이름을 보존한다.

제목·섹션·탭과 목록 제목에는 MaruBuri-SemiBold를 사용한다. 목록 행은 높이 104, 제목 18, 지역명/상태 12이며 다이아몬드 느낌표와 `T_QuestPolish_Control` 프레임, 지역 핀을 사용한다. 상세 시작점은 `(616,226)`이고 제목 34, 본문 17이다. 제목 앞 64×64 다이아몬드, 분류 아래 지역 행을 추가했다. 의뢰 내용·목표·보상에는 속이 빈 네 갈래 별, 가는 가로 구분선과 짧은 세로선을 두었다. 이전 연회색 띠는 제거했다. 긴 내용은 스크롤하고 340×78 추적 버튼은 하단에 고정한다. 스크롤바는 가는 네이비/금색으로 맞췄다.

목표와 보상은 같은 Quest/UI 폴더의 `WBP_QuestJournalObjectiveRow`와 `WBP_QuestJournalRewardTile`로 디자인한다. 저널의 접힌 `QR_Templates`에 각각 템플릿을 참조하며, 네이티브 표시는 `QR_ObjectiveRows`/`QR_RewardRows`에 인스턴스를 채운다. 목표는 읽기 전용 체크박스·설명·우측 수량으로 표시하고 선행 목표 조건, 선택 목표, 완료 여부를 따른다. 보상은 72×72 종류별 아이콘과 이름·수량을 가로 WrapBox에 배치한다. 경험치·골드·아이템 등 실제 보상 개수를 표시하며 시안의 예시 수치는 저장하지 않는다. 이름 없는 아이템은 기존 데이터 한계에 맞춰 ‘아이템’으로 표시한다. 현재 위젯 수는 저널 265, 목록 항목 18, 목표 행 6, 보상 타일 15개다. 기존 저널/항목 EventGraph의 노드·연결 87/106 및 8/6을 보존했다.

`NUQuestJournalPresentation.h`는 퀘스트의 LocationId를 월드 위치 정의의 표시명으로 변환한다. 지역이 없으면 상세 지역 행/목록 핀을 숨기고 목록에는 기존 상태를 표시한다. 템플릿이 없는 이전 WBP는 기존 다중 행 표시를 사용하며, 잠긴 퀘스트의 수락 조건과 완료 보고 안내도 유지한다. 이 변경은 표시용 데이터 연결이며 퀘스트 수락·진행·보상 지급 규칙은 변경하지 않았다.

네이비 맨 왼쪽 5개 버튼과 크림 상단 5개 버튼은 **동일한 카테고리 필터**다.

| 카테고리 | 왼쪽 아이콘 |
|---|---|
| 전체 | 책 |
| 메인 | 느낌표 |
| 서브 | 방패 |
| 일일 | 나침반 |
| 튜토리얼 | 깃펜 |

`OnInitialized`에서 실제 `OnClicked` 델리게이트를 한 번 연결하고 `SelectRail…Category`가 네이티브 `SetCategoryFilter`를 호출한다. `BP_OnJournalFilterChanged`의 동일한 조건으로 양쪽 선택 표시, 목록과 상세를 함께 갱신한다. 빈 분류에서는 상세가 접히고 추적 버튼이 비활성화된다. 선택 탭은 네이비 면·밝은 글자·금색 프레임, 좌측 선택은 같은 프레임과 밝은 아이콘으로 표시한다. 상단 탭 사이에는 짧은 금색 구분선을 두었고, 닫기 버튼은 패널 우상단의 이중 테두리 다이아몬드다. 기존 그래프 연결은 유지하고 선택 색상 핀 값만 바꿨다.

### 4.4 퀘스트 On/Off 연출

| 단계 | 시간 | 동작 |
|---|---|---|
| 열기 배경 | 0–0.12초 | 기존 BlurStrength 4.5까지 빠르게 상승, 네이비 Dim 27% |
| 열기 패널 | 0.04–0.25초 | 좌우 반대 방향 24단위 오프셋에서 Fade + Slide |
| 금색 경계 | 0.08–0.28초 | 중앙 선을 위에서 아래로 표시 |
| 정보 | 0.36초까지 | 제목·탐색·분류·본문·행동을 소그룹으로 Fade, Y +6 → 0 |
| 나침반 | **총 0.38초** | -6° → +1.5° → 0°, 작은 따뜻한 밝기 변화 |
| 닫기 정보 | 0–0.08초 | 빠른 Fade Out |
| 닫기 패널 | 0.16초까지 | 바깥으로 15단위 이동하며 Fade |
| 닫기 배경 | **총 0.20초** | Blur/Dim 제거 후 게임 입력 반환 |

이동 거리는 DPI 영향을 받는 UMG 디자인 단위다. 월드 자체를 Scale하지 않으며 닫을 때 나침반과 금색 선을 역재생하지 않는다. 종료 중 Q를 누르면 현재 표시 상태에서 다시 열리고, 세션 종료 등 강제 닫기는 즉시 처리한다. Revision 2의 제목 배지와 지역 행도 제목과 같은 0.21–0.33초 정보 연출에 포함한다.

실제 구현은 `UUserWidget`의 기존 `OpenJournal` / `CloseJournal` 흐름에 연결된 네이티브 코드다. `CommonActivatableWidget`의 OnActivated/OnDeactivated나 이름만 제안됐던 `Anim_Open_*` UMG 애니메이션을 만든 구현이 아니다. 배경·패널·정보·닫기 역할을 코드에서 분리했다.

플랫폼 시간 기반의 일시적인 weak ticker로 일시정지 상태에서도 동작하며 대기 중 Tick은 없다. 숨김 시 기본 Transform/Color를 복구한다. `QMotion_GoldDivider`는 폭 1.5의 금색 브러시 선이며 `QMotion_InputShield`가 전환 중 포인터 입력을 막는다. 열기 패널이 자리 잡으면 shield는 비활성화된다. PlayerController의 Q/ESC, 닫기 버튼, 강제 정리까지 같은 수명주기로 연결한다. 2026-09-27 추가 장식은 기존 모션 대상의 자식으로 넣었다. 목표/보상은 기존 텍스트 바인딩을 유지한 자동 높이 그룹이며 그룹과 텍스트의 Fade가 함께 적용된다. 네이티브 모션 소스는 수정하지 않았다.

퀘스트 선택 Cross Fade, 카테고리 선택선 이동과 Journey Marker 순차 점등은 이 On/Off 구현에 포함하지 않았다.

### 4.5 인게임 메뉴 허브

기준 시안은 `/Game/Developers/MinkiCho/Art/인게임_메뉴_허브`다. 네이비 프로필·나침반 배너, 얇은 금색 이중선, 밝은 아이보리 메뉴 면, 정사각 카드와 금색 Hover를 구성했다. 불투명 패널 뒤의 월드만 흐린다. ScaleBox는 1672×941 디자인 비율을 유지한다.

- 메인 진입점 8개는 실제 `MainMenuTileView`에 **한 행 5개, 현재 5+3행**으로 배치한다. Options / Character Select / Quit 3개는 하단에 유지한다. 목록은 등록된 비시스템 항목만 순서대로 채우므로 빠진 항목의 빈칸이 남지 않는다.
- 지도(`WorldMap`)를 포함한 총 11개 Entry ID, 실제 단축키 라벨과 메뉴를 닫은 뒤 목적지를 여는 순서를 유지한다. 기존 메인 버튼 7개의 고정 BindWidget을 TileView 하나로 교체했고, 하단 3개 및 닫기 바인딩은 유지한다. 클릭 시 현재 레지스트리를 다시 확인한다.
- main의 `WorldMap` 메뉴 등록과 `ToggleWorldMap` 입력·지도 창 로직을 유지한다. 고정 `WorldMapButton` 대신 등록된 항목을 TileView로 표시하고 같은 델리게이트로 연다. 별도의 지도 열기 로직을 복제하지 않는다.
- 지도 아이콘은 기존 `T_QuestV4_Compass`를 재사용한다. 패드로 메뉴를 열 때는 타일 생성 이후 main의 공통 포커스 처리로 첫 메뉴를 선택하며, 기존 창 탐색과 B 닫기 처리를 유지한다. TileView의 내부 행 선택이 방향키를 소비하지 않도록 `NativeConstruct`에서 `SetIsGamepadScrollingEnabled(false)`를 호출한다. UE 5.8은 해당 저장 속성을 Slate에 자동 동기화하지 않아 런타임 setter도 필요하다.
- `/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuTileEntry`는 `UNUGameMenuTileEntry`와 `IUserObjectListEntry`를 통해 라벨·단축키·아이콘 및 스탯 장식을 갱신한다. `UNUGameMenuTileItem`은 표시 데이터와 메뉴에 대한 약한 참조를 보관한다. 아이콘 매핑은 메인 WBP의 `MenuEntryIcons`, 스타일은 전용 Entry WBP에서 편집한다. 공용 버튼은 수정하지 않았다. 메인 WBP는 현재 43개 위젯이며 EventGraph는 비어 있다.
- TileView는 디자인 좌표 `(492, 271)`, 크기 `1040×416`, Entry `208×208`, 간격 `8×8`, CenterAligned, Selection None이다. 간격을 포함한 Entry 크기이므로 실제 버튼 영역은 `200×200`이고 5열을 유지한다. 마지막 3개도 행 중앙에 배치한다. 기존 ScaleBox가 메뉴 전체를 화면에 맞게 축소·확대한다.
- 프로필은 선택 캐릭터 이름·ID, 레벨·경험치, 현재 지역, 직업·전직, 소지 골드를 읽어 **열 때 갱신**한다. 없는 값은 `—`, 선택된 서버 캐릭터가 없는 로컬 PIE 이름은 ‘모험가’다.
- 기존 캐릭터 미리보기 풀에서 장비가 반영된 초상화 액터를 빌리고 닫을 때 반환한다. `SetBrushFromMaterial` 뒤 `Brush.DrawAs = Image`를 유지해야 초상화가 표시된다.
- 텍스트·초상화·경험치 채움·버튼은 실제 위젯이다. `T_GameMenuHub_Backplate`는 정적 장식, `T_GameMenuHub_Cards`는 상태별 UV atlas다.
- 닫기는 `UNUUIManager::CloseGameMenu`를 통해 커서·입력 소유권을 복구한다. 인벤토리는 기존의 인벤토리/장비 두 창이 함께 열리므로 두 번의 ESC로 닫히는 기존 동작을 유지한다.

2026-10-02 ESC 닫기 영역 스타일 정리: `WBP_GameMenu` 우상단 `HubCloseButton`과 기존 자식 6개의 시각 속성만 변경했다. 280×54 모서리 절삭 네이비 버튼에 얇은 금색 이중 테두리, 52×30 외곽선 ESC 키캡, MaruBuri SemiBold 18의 ‘메뉴 닫기’, 작은 마름모와 금색 ×를 사용한다. 호버는 테두리·면을 밝게, 누름은 면을 짙게 하고 콘텐츠를 1px 아래로 표시한다. 전용 MI 4개는 `MinkiCho/Art/GameMenu/CloseControl20261002/`이며 공용 `M_HUDCutFrame`은 수정하지 않았다. 위젯 43개의 계층·GUID·EventGraph와 네이티브 닫기 연결은 유지했다. 디자이너 화면 검수, 컴파일 오류·경고 0, 강제 저장 후 별도 명령줄 프로세스에서 전체 위젯 속성·GUID·그래프·MI 값과 상태별 브러시 일치를 확인했다. PIE/실제 키·마우스 입력은 재시험하지 않았다. 백업·캡처·검증 기록: `Saved/GameMenuEscPolish_20261002/`.

2026-10-02 키캡 통일(위 ESC 키캡 디자인을 대체): 사용자의 전투 UI 기준에 따라 모든 독립 키 표시를 `dongh/UI/WBP_BattleCommand.TargetKeycap_Confirm` / `TargetKey_Confirm` 스타일로 맞췄다. **아이보리 사각 면 `(0.72,0.69,0.56,0.96)`, 짙은 글자 `(0.022,0.065,0.07,1)`, Roboto Bold, 그림자·외곽선 없음**을 공통 기준으로 사용한다. 기본 글자 17, 좁은 메뉴/설정 행은 12이며 키 이름 길이에 맞춰 폭을 사용한다. ESC는 52×29이며 바깥 닫기 버튼의 금색 테두리·호버·누름 스타일은 유지한다. 새 키 안내도 이 기준을 사용하며 네이비 외곽선 키캡을 별도로 만들지 않는다.

적용 WBP 9개: `WBP_GameMenu`의 ESC, `WBP_Chat`의 Enter, `WBP_DialogueWidget`의 F, `GameMenuHub/WBP_GameMenuTileEntry·Card·Stats·System` 및 `WBP_CommonButton`의 단축키, `WBP_KeyBindingRow`의 키보드/패드 표시. 단축키·설정 행에는 기존 TextBlock을 감싸는 Border만 추가하고 `Visibility`를 해당 TextBlock에 바인딩하여 빈 키는 키캡도 숨긴다. 기존 BindWidget 이름·유효 GUID·EventGraph를 보존했고, 설정 행의 입력 대기 안내가 키캡 위에 그려지는 순서를 유지했다. 전투 키캡 4개는 기준 원본으로 그대로 유지한다. 본문형 조작 설명은 변경하지 않았다.

9개 WBP 컴파일 0/0, 강제 저장 후 별도 명령줄 프로세스에서 전체 위젯·GUID·EventGraph·바인딩·전투 기준 브러시/글자색 일치를 확인했다. 로컬 PIE에서 메뉴 타일/공용 버튼의 `SetShortcutText`를 통해 키 표시→빈 키→재표시와 설정 행의 직접 가시성 변경을 확인했다. 물리 입력·서버 로그인은 시험하지 않았다. 백업·전후 데이터·화면·검증 기록은 `Saved/UnifiedKeycaps_20261002/`. 이전 `MI_GameMenuClose_Keycap`은 더 이상 ESC에 사용하지 않는다.

2026-10-02 닫기 영역 최종 변경: 사용자 요청으로 `WBP_GameMenu` 우상단은 **배경 없이 ×만** 표시한다. `HubCloseButton`의 네이티브 닫기 연결과 우측 위치를 유지하며, 54×54 클릭 영역 안에 Roboto 28의 ×를 중앙 배치했다. 기본 금색, 호버 시 밝은 아이보리, 누름 시 짙은 금색을 사용한다. ESC 키캡·‘메뉴 닫기’ 문구·마름모와 불필요한 컨테이너 6개를 삭제해 43→37개 위젯으로 정리하고, 해당 GUID만 제거했다. 유효 GUID·기존 EventGraph 및 나머지 위젯 속성은 유지한다. 다른 화면의 통일된 키캡 디자인은 그대로다. 컴파일 0/0·디자이너 확인·강제 저장 후 별도 프로세스에서 37개 위젯/GUID 및 그래프 일치를 확인했고 ensure가 발생하지 않았다. PIE/입력 재시험은 생략. 기록: `Saved/GameMenuCloseX_20261002/`.

2026-10-02 메뉴 허브 시안 부분 적용: 사용자 지정 범위는 **왼쪽 네이비 패널의 금빛 문양, 주요 메뉴의 영역 테두리 제거, 하단 시스템 버튼 아이콘화**다. `/Game/Developers/MinkiCho/Art/메뉴_허브_시안`의 전체 메뉴 수나 배치를 복제하지 않는다. 기존 5열 TileView·레지스트리·프로필 데이터·우상단 ×를 유지한다. `T_GameMenuHub_GoldPattern`은 성 실루엣을 금빛 천문 문양으로 교체하며, 원본 배경은 보존한다.

`WBP_GameMenuTileEntry`의 네 상태 브러시는 NoDrawType이다. 기존 아이콘·이름 아래에 단축키 키캡을 배치하고, 아이콘과 스탯 막대는 버튼 foreground를 따라 남색/호버 금색으로 표시한다. 하단 `WBP_GameMenuSystem`은 56×56 아이콘과 72×56 클릭 영역을 사용한다. 설정은 톱니, 캐릭터 변경은 **사람 기호 우측 아래 순환 화살표**, 종료는 문/바깥 화살표다. 기존 라벨·단축키 바인딩은 보존하되 하단에서 렌더 알파 0으로 감추고, 실제 등록 라벨을 툴팁으로 제공한다. 공용 버튼 원본은 변경하지 않았다. 메뉴의 다음 틱 등록에서 생성된 TileView 항목에도 기존 팝업 축소 모션을 연결한다.

관련 WBP 3개 컴파일 오류·경고 0, C++ Live Coding 성공. PIE에서 8개 주요 메뉴·프로필·아이콘 시각 확인, 버튼 델리게이트로 설정창 열기·캐릭터 변경/종료 확인창 열기와 취소·메뉴 재열기를 확인했다. 1920×1032 창 캡처이며 작은 실제 뷰포트와 물리 마우스 클릭은 미검증이다. `ArtSource/UI/GameMenuHub/`에 내장 image_gen 원본·프롬프트, `Tools/UIMenuHubReference/apply.py`에 적용 절차, `Saved/UIMenuHubReference/`에 백업·스냅샷·검증 기록을 보관한다.

2026-10-02 메뉴 버튼 정렬 보정: 메인 각 행과 하단 시스템 버튼의 중심을 아이보리 패널 기준 **X1012**로 통일했다. 하단 72×56 버튼은 Y714, 중심 간격120이며 구분선은 두 버튼 중심 사이·세로 중앙에 배치한다. 하단 가로선은 `(464,692,1096,1)`이고 버튼 아래에는 배경 안쪽 테두리까지 약22의 여백을 둔다. 사용자 후속 요청으로 하단 버튼과 세로 구분선을10 내려 위아래 여백의 균형을 맞췄다. 최종 여백의 컴파일·PIE 배치·화면 확인 기록은 `Saved/UIMenuHubFooterPadding/`에 있다. 변경은 메인 WBP의 기존 위젯 7개 배치/목록 속성에 한정하며 37개 위젯·EventGraph를 유지한다. 컴파일 오류·경고0, PIE에서 5+3 중앙 배치·여백·설정 버튼 델리게이트·닫기/재열기를 확인했다. 적용 수치는 `Tools/UIMenuHubReference/apply.py`의 `apply_layout()`에 모았으며, 최초 정렬의 백업·비교·실행 화면은 `Saved/UIMenuHubAlignment/`에 있다. 물리 마우스와 추가 화면비는 이번 배치 검증에 포함하지 않았다.

2026-10-02 메뉴 이름 호버 색상: `WBP_GameMenuTileEntry.Txt_Label`은 아이콘과 같은 `UseColor_Foreground`를 사용하여 버튼의 Normal/Hovered/Pressed/Disabled 색상을 따른다. 공통 버튼이 `NativePreConstruct`에서 `LabelColor`를 고정 색상으로 적용하므로, `UNUGameMenuTileEntry::NativePreConstruct`가 부모 호출 뒤 전경색 상속을 복원한다. 상태별 색상은 전용 WBP의 `Btn_Common.WidgetStyle`에서 한 번만 지정한다. 적용 스크립트는 같은 설정을 보존하며, 작업 기록은 `Saved/UIMenuHubLabelHover/`에 있다.

호버 색상 검증: WBP 컴파일0/0 및 C++ Live Coding 빌드·패치 성공. PIE의 8개 메뉴 이름이 전경색을 상속하는지 확인하고, 실제 커서 이동으로 스킬 버튼의 `IsHovered` 전환·아이콘/이름의 금색 표시·이탈 시 남색 복귀를 확인했다. Live Coding 콘솔의 `.voltbl` 진단은 있었으나 패치 완료 및 실행 중 적용을 확인했다.

### 4.6 캐릭터 통합 정보창 베이스

**2026-10-02 미사용 위젯 정리:** 같은 폴더의 8개 WBP를 조사하고, 이전 시안의 숨겨진 아치·받침대·중복 제목·구분 장식과 빈 `IdentityMetaRow` 가지 등 **195개**를 삭제했다. `Collapsed`로만 숨겼던 옛 장식을 실제로 제거하여 디자이너의 중복 표시도 없앴다. 아래 과거 위젯 개수보다 이 정리 이후 수치를 우선한다.

- 삭제 수: 정보 33개, 장비 41개, 인벤토리 페이지 2개, 스탯 69개, 나이아 50개. 최종 개수는 정보 **151**, 장비 **267**, 인벤토리 페이지 **131**, 스탯 **298**, 나이아 **481**이다. 베이스 **259**, 인벤토리 셀 **22**, 특성 **1,160**은 파일 변경 없이 유지했다.
- `CharacterPortrait`/`CharacterReflection`, 장비·인벤토리 아이콘/등급/착용/선택 표시, 상세 능력치 구분선, 나이아 초상화·체력·등급 자리, 특성 트리와 `Align_` 정렬 컨테이너는 필요한 표시/데이터/정렬 요소이므로 보존했다. `WorldBlur`/`WorldTint`와 `OpenFadeOverlay`도 OpenFade/CloseFade의 대상이므로 유지한다.
- 삭제 대상은 C++ 참조·EventGraph·바인딩·애니메이션을 확인했다. 삭제 위젯과 자식을 WidgetTree 밖으로 옮긴 뒤 해당 GUID만 제거하고 컴파일·강제 저장했다. 남은 위젯의 속성·슬롯·형제 순서·유효 GUID·그래프·바인딩은 유지했다. 별도 프로세스의 저장 파일 재로드로 전체 8개 스냅샷 일치와 삭제된 UObject/GUID 부재, ensure 미발생을 확인했다.
- 변경한 5개 페이지의 로컬 PIE 전후 화면은 전체 픽셀이 동일하다. 백업, 정확한 삭제 명단, 검증 결과와 전후 화면은 `Saved/CharacterMenuUnusedCleanup_20261002/`에 보관한다. 서버 데이터/실제 입력 전체 흐름은 이번 정리에서 재검증하지 않았다.

**2026-10-02 텍스트 정렬 보정:** `/Game/Developers/MinkiCho/UI/CharacterMenu`의 8개 WBP에 적용했다. 이 단락의 정렬·여백 값은 아래의 과거 시안 수치보다 우선한다.

- 본문 `PageContentPadding`은 **좌24 / 상16 / 우24 / 하16**이다. 기존 좌20 / 상12 / 우52 / 하40의 비대칭 여백을 줄였다. 1200×600 페이지와 ScaleToFit은 유지한다.
- `Align_<원래 텍스트 이름>` SizeBox 229개가 기존 Canvas 영역을 이어받고 텍스트를 세로 중앙에 배치한다. 버튼·리본 제목·수치 등 한 줄 중앙 정렬은 **SizeBoxSlot HAlign_Center / VAlign_Center + TextBlock Justification=Left**로 구현한다. UE 5.8 `SlateTextLayout.cpp`는 한 줄 Center justification에 NoOverflow를 사용하므로, 텍스트 자체의 Center를 켜면 긴 이름이 양쪽에서 잘리고 말줄임이 사라진다. 컨테이너의 중앙 정렬로 짧은 글자의 중앙 배치와 긴 글자의 Ellipsis를 함께 유지한다. 설명 문단과 목록 항목명은 기존 읽기 방향을 유지한다.
- 왼쪽 메뉴 라벨 영역은 `(68,0,168,76)`이다. 스탯 이름·약어 영역의 겹침을 없애고 수치를 동일한 열 중앙에 놓았다. 장비 요약은 4개의 같은 폭 열로 정렬하고 장비·인벤토리 구분선은 해당 능력치 행 사이로 이동했다. 나이아 이름·레벨·분류는 Y268~324의 이름 영역 안에 배치한다.
- 원래 TextBlock 이름·유효 GUID·바인딩·EventGraph·OpenFade/CloseFade 및 버튼 동작은 유지한다. 위젯 삭제는 없으며 정렬 컨테이너 삽입 시 기존 형제의 표시 순서를 보존한다. 글자 위치/영역은 `Align_...`의 Canvas 슬롯, 글꼴·문구는 원래 TextBlock에서 조정한다.
- 전체 8개 컴파일 오류·경고0, 원래 위젯/GUID/그래프/바인딩 보존 및 별도 UnrealEditor-Cmd의 저장 파일 전체 스냅샷 일치를 확인했다. 전후 화면·스크립트·원본 백업·검증 결과는 `Saved/CharacterMenuAlignment_20261002/`에 있다. 6개 페이지는 로컬 PIE 미리보기로 확인하며, 임시 긴 텍스트·작은 할당 영역 검사는 에셋 기본값에 저장하지 않는다.

`/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterMenuBase`는 `UNUCharacterMenuBase`를 부모로 하는 공통 셸이며 기존 `UNUUIManager`의 창·포커스·입력 관리에 연결한다. **현재 기준은 2026-09-28 다시 업로드된 `ProjectM_Character_Info_Concept` 1690×931 이미지**다. 이전 745×356 시안과 문양 보강안보다 이 절의 수치가 우선한다.

- 전체 화면 `RootCanvas → ShellCanvas`의 Anchor 0~1 / Offset 0과 가장자리 금색 이중선은 유지한다. 새 시안에 맞춰 네이비 레일은 252, 상단 나침반 블록은 `252×184`로 정리했다. 기존 ‘캐릭터 정보 / CHARACTER’ 제목과 이전 곡선 코너 장식은 Collapsed다. 큰 나침반 뒤에는 옅은 천체도, 화면 네 귀퉁이에는 편집 가능한 금색 기하학 선을 둔다.
- 프로필 헤더는 X252 / 높이172, 직업 로고는 헤더 기준 `(34,30,116,116)`, 이름은 `(192,42,520,54)`, 직업·레벨 행은 `(194,102,560,34)`다. 사용자가 이미 삭제한 상단 EXP 위젯 세 개는 다시 만들지 않았다. 경험치는 캐릭터 정보 페이지에만 표시한다. 우상단 닫기는 44×44이며 기존 닫기 경로를 유지한다.
- 좌측 순서는 캐릭터 정보 / 장비 / 인벤토리 / 스탯 / 특성 / 나이아다. 버튼은 X12 / Y188부터 88 간격 / 256×76이며 선택 배너의 끝이 레일보다16 돌출된다. 아이콘32, 라벨 시작X72다. 기존 선택 배너·버튼·전환 바인딩과 `Nav…Foreground` 그룹을 유지한다.
- 본문은 `MajorMenuColumn → PageContentPadding → PageScaleFit → PageDesignSize(1200×600) → NamedSlot_PageContent`다. 영역은 X252 / Y172부터이며 실제 Padding은 **L20 / T12 / R52 / B40**다. ScaleToFit으로 페이지 비율을 유지하고 사용자가 설정한 Fill 정렬을 보존한다. 셸 배경과 테두리는 뷰포트 끝을 채운다.
- 새 `BackdropLayers → BackdropCover → BackdropAtmosphere`는 아이보리 영역을 ScaleToFill로 덮는 정적 배경이다. 따뜻한 아이보리·청회색 질감과 금색 궤도가 들어 있으며 이미지의 원형 비율을 유지한 채 가장자리만 잘라 채운다. `AmbientLeavesFar`의 잎5개와 `AmbientLeavesNear`의 잎/가지3개는 **총8개의 독립 Image**다. 가까운 잎도 페이지 내용보다 아래에 놓여 글자와 클릭을 가리지 않는다.
- 새 텍스처는 `/Game/Developers/MinkiCho/Art/CharacterMenu/Reference20260928/T_CharacterMenu_Atmosphere`, `T_CharacterMenu_Traveler`, `T_CharacterMenu_Leaves`다. built-in image_gen으로 배경·인물·잎사귀를 분리 제작했고 원본 PNG와 프롬프트는 `ArtSource/UI/CharacterReferencePolish_20260928/manifest.json`에 있다. UI 압축·Clamp·NoMipmaps로 저장했다. 기존 `Ornaments/T_CharacterMenu_Ornaments` atlas는 나침반 주변과 바닥 궤도에 재사용한다.
- **현재 애니메이션은 없다.** 추후 전체 On/Off는 `ShellCanvas.RenderOpacity`, 배경 미세 이동은 `BackdropAtmosphere`, 잎 움직임은 `AmbientLeavesFar / AmbientLeavesNear` 또는 개별 `Leaf…`, 정보 페이지 실루엣·바닥은 `PortraitAtmosphere / PortraitFloorLayer`를 대상으로 삼을 수 있다. 각 장식은 HitTestInvisible이며 모든 요소가 셸 아래에 있어 전체 Fade에 함께 포함된다.
- `PageClasses`의 여섯 클래스와 NamedSlot 캐시/전환을 보존했다. `Show`로 열고 닫기 버튼·ESC·패드 B로 기존 입력 반환 경로를 사용한다. 이름·직업·레벨은 기존 프로필 구독으로 갱신하고 직업 로고는 실제 JobKey에 맞춘다. 현재 베이스는 **269개 위젯**이며 EventGraph와 C++는 변경하지 않았다.
- 이번 범위는 Base와 Info 디자인이다. 다음 순서는 나머지 통합 메뉴 패널 디자인, 그다음 연출이다. 장비9칸·인벤토리30칸·특성 내부 노드를 포함한 다른5페이지 파일은 이번에 수정하지 않았다. 이전 보강안과 백업은 `Saved/UICharacterOrnaments_20260928/`, 현재 검증·백업·최종 화면은 `Saved/UICharacterReferencePolish_20260928/`에 있다.
- 시안 디자인 검증(반사 추가 이전): Base269 / Info167 컴파일 오류·경고0, EventGraph 동일. 실제6탭 델리게이트·NamedSlot·단일 선택·인벤토리 인스턴스/스크롤200·라이브 초상화·닫기 입력 반환·콘솔 재열기를 확인했다. 표시 텍스트22개가 들어가며 긴 이름/지역 말줄임·19자리ID·큰 골드·일곱 자리EXP도 통과했다. 넓은/높은 임시 할당 영역 검사는 종료했고 최종 새 PIE는 전체 화면이다. 증거 `qa.json`, `text_qa.json`, `long_text_qa.json`, `editor_audit.json`, `final_audit.json`, `final_character00000.png`. 다른736개 보호 파일과 사용자 Git 스테이징은 유지했다. 이후 실루엣·거울 바닥 보완은4.7절에 기록한다.

직업 로고는 `ProfileJobEmblemSwitcher` 안의 ScaleBox / UVRegion으로 원래 비율을 유지한다.

| 직업 상태 | JobKey / 로고 |
|---|---|
| 미전직 `JobId=0` | Novice — 열린 책·길잡이 별, 별도 제작 |
| 나이아 나이트 `1001` | NaiaKnight — 수호의 별·검 장식 |
| 트래커 `1002` | Tracker — 숲·나침반·가지 |
| 엘리멘탈리스트 `1003` | Elementalist — 마법진·원소 불꽃 |
| 갬블러 `1004` | Gambler — 카드·별·문양 |
| 드루이드 `1005` | Druid — 나무·초승달 |

`NUCharacterMenuBase::RefreshProfile`이 기존 `OnJobChanged` 구독을 통해 실제 JobId의 정의를 찾고 `JobKey`에 맞는 `JobEmblem_<JobKey>` 자식을 선택한다. 전직명은 기존대로 표시하되 로고는 기본 직업을 따른다. 직업 정보가 없거나 알 수 없는 JobKey는 0번 Novice로 복귀한다. WBP의 하드 참조가 텍스처를 포함하며 네이티브에 텍스처 경로를 하드코딩하지 않는다. 직업 정의 CSV/테이블이나 서버 직업 선택 규칙은 바꾸지 않았다.

직업 시안 5종의 하단 로고를 참고해 내장 `image_gen`으로 영문 타이틀 없는 투명 PNG 6종을 제작했다. 원본/프롬프트는 `ArtSource/UI/JobEmblems_20260927/`, 런타임 에셋은 `/Game/Developers/MinkiCho/Art/CharacterMenu/JobEmblems/T_JobEmblem_*`이다. UI 압축·sRGB·Clamp·NoMipmaps·최대 512로 저장하고, 헤더에서는 네이비 Tint로 표시한다. 시안 원본은 런타임 배경으로 사용하지 않는다.

현재 `PageClasses`에는 Character / Equipment / Inventory / Stats / Traits / Naia의 여섯 페이지를 연결했다. Designer의 NamedSlot 자체는 비워 두며, 실행 시 선택 섹션에 맞춰 페이지를 생성·캐시·삽입한다. 기존 허브 메뉴의 진입점과 P키의 기존 캐릭터 창은 아직 이 셸로 교체하지 않았다. 각 페이지의 입력·데이터 연결을 확인한 뒤 기존 진입점을 전환한다.

#### Major Menu 공통 간격 규격 — 2026-09-28 최신 시안

셸의 현재 수치는 아래와 같다. 나머지5페이지의 내부 구성은 후속 디자인 단계에서 조정하며 현재1200×600 계약을 유지한다.

| 항목 | 기준값 / 적용 방식 |
|---|---|
| 셸 크기 / 페이지 본문 | 뷰포트 전체 / 비율 유지하는 **1200×600** 디자인 영역 |
| 프로필 헤더 | X252 / 높이 **172**, 이름 박스54 / 보조 정보34 |
| 본문 Padding | **L20 / T12 / R52 / B40** |
| 내비게이션 | 레일252, 버튼256×76, 행 간격12, 아이콘32 / 라벨과의 간격14 |
| 정보 페이지 표 | `(628,12,552,472)`, 제목띠216×45, 행 간격50 |
| 정보 표 텍스트 | 라벨·값15, 제목19, 경험치13; 행 텍스트 높이32 |
| 다른 페이지 내부 기본값 | 주요 Gap24, 밀집 목록 Padding16, 제목띠·행동 버튼 높이40 |

수치는 DPI 적용 전 UMG 단위다. W×H 할당 영역에서 페이지 배율은 `min((W−324)/1200, (H−224)/600)`이다. 원형 문양이 있는 공통 배경만 ScaleToFill을 사용하고 실제 UI·캐릭터는 가로세로 같은 배율을 사용한다. 화면 비율이 달라도 배경·외곽 프레임은 전체 영역을 채운다. 긴 이름·지역 등 가변 길이 문자열에는 기존 말줄임과 클리핑을 유지한다.

### 4.7 캐릭터 정보 페이지

`/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterInfoPage`는 `UNUCharacterInfoPage`를 부모로 하는 독립된 콘텐츠 WBP다. 베이스의 배경·헤더·좌측 메뉴를 복제하지 않으며, 뷰포트에 직접 추가하지 않는다. 베이스의 `PageClasses.Character`가 이 클래스를 참조하여 `NamedSlot_PageContent`에 넣는다.

- 최신1690×931 시안에 맞춰 디자인 크기 **1200×600**, 편집 가능한 위젯 **169개**로 정리했다. 오른쪽 `InfoContent=(628,12,552,472)`의 아이보리 표와 금색 외곽선·기하학 코너, 네이비 제목띠를 사용한다. 아래에는 바닥·잎 문양을 보여 주는 여백을 남긴다. 공통 배경·헤더·내비게이션은 베이스가 제공한다.
- 이름 / 클래스 / 레벨 / 경험치 / 현재 지역 / 캐릭터 ID / 소지 골드의 일곱 행을50 간격으로 배치한다. 라벨 X78, 값 X242 / 너비278이며 값은 왼쪽 정렬한다. 경험치만 숫자를 막대 위에 오른쪽 정렬하고 막대 폭278을 사용한다. 일곱 자리 EXP,19자리 ID,큰 골드 값과 긴 이름·지역 말줄임을 검수했다. 기존 네이티브 위젯 이름을 유지한다.
- 라이브 `CharacterPortrait`는 `(168,0,384,512)`, RenderScale1.3 / Translation(0,-36)이며 원래3:4 캡처 비율과 장비 반영을 유지한다. 반사 공간을 확보하도록 이전보다24 위로 옮겼다. 뒤의 `PortraitAtmosphere → CharacterIllustration`는 별도 청회색 장식 이미지 `T_CharacterMenu_Traveler`를 `(0,-16,624,468)` / Opacity0.86 / Tint(.72,.82,.94)로 배치해 배경과 구별되도록 한다. 장식 인물은 실제 외형과 독립적이다.
- `PortraitFloorLayer`에 `MirrorFloorSurface=(-50,389,820,208)`와 `CharacterReflection=(110.4,472.2,499.2,128)`을 추가했다. 이전의 얇은 나침반·천체도 바닥 이미지는 Collapsed다. 새 바닥은 청회색 거울 표면, 선명한 금색 동심원·눈금·방사선·얕은 광택이며 픽셀 이미지 대신 UI 머티리얼로 그린다. 패널 아래까지 원호가 이어지도록 바닥 그룹의 사각 클리핑을 사용하지 않는다. 표 안의 옅은 `InfoWatermarkCompass`와 독립 변환 대상인 잎사귀·실루엣은 유지한다. 별도의 등장·퇴장·잎사귀 움직임은 아직 추가하지 않았다.
- `/Game/Developers/MinkiCho/Art/CharacterMenu/Mirror/M_CharacterFloorReflection`은 원본 캐릭터와 **같은** `PreviewTexture` 렌더 타깃을 사용한다. 추가 카메라·프레임 복사·UMG Tick 없이 외형·장비·대기 애니메이션 프레임을 공유하며, 반사 UV만 세로로 뒤집고 아래로 갈수록 투명해진다. 기존 LDR 캡처의 역알파를 `1−A`로 처리한다. `FootUV=450/512`, `SourceSpan=.55`, `ReflectionOpacity=.56`, `FadePower=1.45`이며 현재 발 위치는 `(256−36)+(450−256)×1.3=472.2`다. 초상화 배율·세로 위치나 캡처 구도를 바꾸면 반사 슬롯/FootUV도 함께 맞춘다. `M_CharacterMirrorFloor`는 바닥 표면용 별도 머티리얼이다.
- 2026-09-28 거울 보완 검증: WBP169개 컴파일 오류·경고0/EventGraph 동일, 두 머티리얼 셰이더 오류0. PIE에서 동일 렌더 타깃·추가 캡처 없음·장비/직업/인벤토리 갱신·다섯 다른 페이지로 전환·캐시 재진입·닫기·화면 밖 Refresh·콘솔 재열기의10개 검사를 통과했다. 정식 `ProjectMEditor Win64 Development` 빌드 성공 후 별도 `-game` Standalone 프로세스에서도 반사를 캡처했다. 자료는 `Saved/UICharacterMirror_20260928/`의 `qa.json`, `final_audit.json`, `fullbuild.log`, `final_mirror00000.png`, `standalone00000.png`에 있다. 원본 캡처가 없는 Designer에서는 반사를 숨기며, 베이스의 빈 NamedSlot은 런타임에 이 정보 페이지로 채워진다. 새 래스터 이미지 없이 UI 머티리얼2개를 추가했으며 기존 베이스/다른5페이지/시안/다른739개 보호 파일과 Git 인덱스는 보존했다.
- 이름·캐릭터 ID는 선택 캐릭터, 직업·전직·레벨·경험치는 PlayerState의 기존 컴포넌트, 소지 골드는 인벤토리에서 읽는다. **현재 지역**은 WorldLocation 및 기존 월드 데이터 표에서 읽는다. 시안의 소속 지역과 다른 의미이므로 라벨도 현재 지역으로 명시했다. 값이 없는 ID·지역은 `—`로 표시한다. 긴 텍스트 검수는 실행 중 위젯에만 임시 적용하고 실제 데이터로 복원했다.
- `CharacterPortrait`/`PortraitFallback` 필수 바인딩과 이름·직업·지역·골드·ID·레벨·경험치 위젯의 기존 이름을 사용한다. `NUCharacterInfoPage.cpp`의 기존 RefreshPortrait/ReleasePortrait에 선택적 `CharacterReflection` 이미지 연결만 추가했다. 반사 이미지가 없는 장비 페이지도 같은 부모를 그대로 사용할 수 있다. 슬롯을 떠나거나 창을 닫으면 반사를 숨기고 동적 머티리얼의 타깃 참조를 제거하며, 재열 때 다시 연결한다. WBP EventGraph와 실제 데이터 갱신 경로는 동일하다. 긴 이름·직업은 말줄임하고 직업과 레벨은 서로 독립된 행을 사용한다.
- 가입일·사용자 편집 한마디·칭호와 편집 기능은 현재 데이터/API가 없어 추가하지 않았다. 상세 스탯은 추후 별도 스탯 섹션에 연결한다. 비활성 가짜 탭이나 저장되지 않는 편집 버튼은 만들지 않았다.
- `NativeConstruct`에서 캐릭터 목록·성장·직업·장비·인벤토리·위치 변경 알림을 구독하고 `RefreshPage`로 갱신한다. 반복 Tick이나 서버 조회 요청을 추가하지 않는다. `NativeDestruct`에서 구독을 해제한다.
- 미리보기는 기존 `UNUCharacterPreviewSubsystem`에서 액터를 빌려 현재 장비·외형을 반영한다. `M_NUCharacterPreview`와 동적 머티리얼로 캡처의 알파를 처리하며 `Brush.DrawAs = Image`를 유지한다. 슬롯에서 페이지가 빠지거나 창이 닫히면 액터를 반환하고 캡처를 중지한다. 재방문 시 페이지 인스턴스는 캐시에서 재사용하고 미리보기는 풀에서 다시 빌린다.
- 소스는 `UI/CharacterMenu/NUCharacterInfoPage.h/.cpp`다. 베이스 소스에는 공통 헤더의 데이터 변경 구독·해제만 추가했고, 기존 캐릭터 정보·다른 사용자 프로필·장비·스탯 창 및 공용 미리보기 렌더러는 수정하지 않았다.

### 4.8 장비·인벤토리 분리 — 디자인 단계

2026-09-28 `/Game/Developers/MinkiCho/장비창과_인벤토리창_분리_시안`에 맞춰 기존 혼합 장비 페이지를 두 콘텐츠 WBP로 분리했다. 같은 날 후속 요청으로 **좌측 인벤토리 탭과 NamedSlot 전환 연결도 완료했다.** 두 페이지 모두 베이스의 NamedSlot에 들어가는 `1200×600` 콘텐츠이며 아이템 데이터·행동 바인딩은 후속 작업이다.

| 역할 | WBP | 구성 |
|---|---|---|
| 장비 | `/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterEquipmentPage` | 착용 9칸, 캐릭터, 종합 능력치, 선택 장비 설명과 해제 버튼 외형 |
| 인벤토리 | `/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterInventoryPage` | 분류·종류 필터·정렬·보기 버튼, 5열 스크롤 목록, 상세·장착·분해·잠금 버튼 외형 |

- 장비 페이지에서 목록과 분류/필터/정렬 위젯을 실제 제거했다. 왼쪽 장비 영역은 `744×600`, 오른쪽 상세는 `(776,8,424,584)`이며 간격은 32다. 머리·상의·장갑·신발·하의 5칸과 무기·귀걸이·반지 2칸을 양쪽에 배치하여 **게임 규칙의 9개 착용 슬롯**을 유지했다. 각 슬롯은 `88×88`이다. 하단 종합 능력치는 가로 4열이며 `Speed`는 속도로 표기한다.
- 장비의 `NamedSlot_CharacterPreview` 안에는 `CharacterPortrait`와 `PortraitFallback`을 둔다. 부모를 기존 `UNUCharacterInfoPage`로 바꿔 **이미 구현된 실제 외형 렌더링·풀 반환·장비 변경 갱신**만 재사용한다. 새로운 C++나 서버 호출은 없다. 2026-09-28 사용자 요청으로 프로토타입 아이콘을 비워 착용 9칸 모두 빈 슬롯으로 표시한다. 선택 장비와 종합 능력치 수치는 `—`로 비웠으며 실제 데이터 연결은 후속 작업이다. 추후 장비 전용 네이티브 부모를 도입할 때 이 미리보기 수명주기를 보존해야 한다.
- 네이비 슬롯과 금색 모서리, 이중 아치·옅은 나침반, 아이보리 상세 패널을 유지한다. 빈 슬롯은 부위 이름과 작은 마름모로 표시하고 샘플 무기의 선택 강조는 제거했다. 신규 이미지·머티리얼은 추가하지 않았다.
- 새 인벤토리는 기존 WBP를 출발점으로 별도 에셋을 만든 뒤 착용 영역을 제거했고, 일반 `UserWidget`을 부모로 유지한다. `InventoryScroll` 안의 `InventoryGrid`는 **UniformGridPanel 5열×6행, 총 30칸**이며 셀은 `132×132`, 사방 패딩 6, 피치 144다. 기본 용량을 시안의 100으로 바꾸지 않았다. 실제 TileView나 동적 목록은 아니며 현재는 **30칸 모두 빈 슬롯, 수량 `0 / 30`**으로 표시한다. 기존 20개 샘플의 이미지 참조·레벨·착용 표시·등급 강조를 비웠다.
- 위쪽은 장비/소모품/재료/기타 분류와 전체/무기/방어구/장신구 필터다. 초기 선택은 장비/전체이며 내용은 비어 있다. 오른쪽 상세 영역은 `(776,112,424,488)`이고 목록은 `(0,112,752,488)`이다. 목록만 스크롤되며 상세와 관리 버튼은 고정된다.
- 양쪽 상세는 `선택한 장비 없음` 또는 `선택한 아이템 없음`과 `—` 수치를 표시한다. 샘플 설명·레벨·등급·아이콘을 비우고, 선택 대상이 없으므로 장착/해제·분해·잠금 버튼을 비활성화했다. 필터·정렬·보기 변경·아이템 선택과 실제 데이터·거래·장비 변경 동작은 후속 연결 대상이다. 현재 장비 277개, 인벤토리 494개 위젯이다. 이미지 위젯 이름은 향후 바인딩을 위해 유지하며 원본 텍스처 에셋은 삭제하지 않았다.
- 장비는 `nu.CharacterMenu.Preview Equipment`, 인벤토리는 `nu.CharacterMenu.Preview Inventory`로 확인한다. 저장된 베이스의 `PageClasses`에 여섯 콘텐츠 페이지를 등록하여 일반 `Show` 진입에서도 좌측 버튼으로 전환한다. 재방문 시 같은 페이지 인스턴스와 인벤토리 스크롤 위치를 유지한다.

**인벤토리 탭 (2026-09-28, 적용 완료):** `NUCharacterMenuBase`에 `Inventory=5`를 추가해 기존 섹션 값 0~4를 보존했다. 선택적 `NavInventoryButton` 바인딩, 선택 표시, 선택/등록 허용 범위와 여섯 페이지 미리보기를 지원한다. 인벤토리는 장비 다음 세 번째 행이며 버튼 Normal/Pressed Padding은 0이다. 탭 추가 당시 베이스는 188개 위젯이었고 검증·백업은 `Saved/UIInventoryTab_20260928/`에 보관한다. 이후 최신 시안의 현재 배치·269개 위젯 구성은 4.6절을 따른다. 저장된 `PageClasses` 여섯 항목은 유지한다.

### 4.9 캐릭터 스탯 페이지 — 디자인 단계

`/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterStatsPage`는 일반 `UserWidget`을 부모로 하는 독립 WBP다. 시안의 4분할 구성을 현재 `FNUCharacterStatsViewData`와 서버 스탯 스냅샷의 필드에 맞췄다. 기존 캐릭터 스탯창의 13개 표시 행에 서버 데이터에 정의된 보조/장비 항목 8개를 더해 **총 21행**으로 구성한다.

| 패널 | 표시 항목 |
|---|---|
| 주요 능력치, 5행 | 힘 STR, 민첩 DEX, 지능 INT, 행운 LUK, 체력 VIT |
| 전투 능력치, 6행 | 공격력, 마법 공격력, 방어력, 속도, 치명타 확률, 치명타 피해 |
| 생존·보조, 6행 | 현재/최대 HP, 현재/최대 SP, 패링 확률, 회피 확률, 피해 감소, 회복 보너스 |
| 장비 보너스, 4행 | 공격력 증가율, 마법 공격력 증가율, 최대 HP 증가율, 방어력 추가값 |

- 2026-09-28 `/Game/Developers/MinkiCho/ProjectM_Stat_Concept`를 반영했다. 위젯 316개, 디자인 크기 `1200×600`이다. 중복 페이지 제목을 제거하고 `584×284` 패널을 `(0,0)`, `(616,0)`, `(0,308)`, `(616,308)`에 배치한다. 좌우 간격 32, 상하 간격 24, 제목 띠 높이 40이며 네이비 띠가 패널 전체 폭을 덮지 않도록 줄였다. 공통 베이스는 변경하지 않았다.
- 네이비 제목 띠, 얇은 금색 테두리와 모서리, 아이보리 본문, 낮은 대비의 교차 행과 구분선을 사용한다. 이름은 왼쪽, 수치는 오른쪽 정렬하고 장비 추가값은 청색으로 구분한다. 주요 스탯 아이콘 5개는 기존 `zxcv1/CharacterProgression/UI/Icons/T_ProgressionNode_*`를 재사용했다.
- 주요 스탯 행 높이는 40, 전투·생존 행은 34, 장비 행은 48이다. 이름·수치 영역 높이는 32, 제목 영역은 34를 확보했다. 금색 테두리와 모서리는 실제 UMG 선이며 재사용 종이 질감을 낮은 대비로 더했다. 텍스트는 ClipToBounds와 말줄임을 사용한다.
- 항목 이름은 기존 데이터에 맞췄다. 시안의 속성 저항·마법 방어력·명중률·공격 속도·HP/SP 회복량은 현재 조사한 스탯 데이터에 없으므로 이 페이지 항목에 포함하지 않았다. `Speed`는 **속도**로 표기하며 필드 이동 속도로 단정하지 않는다.
- 모든 수치는 디자인 샘플이다. HP/SP의 현재·최대치는 각각 한 행에 함께 표시한다. 장비 보너스는 별도의 추가 효과 표시이며, 후속 바인딩에서 서버가 계산한 최종 능력치에 중복 가산하면 안 된다.
- 스탯 데이터·행동 바인딩과 기존 메뉴 진입점은 후속 작업이다. 2026-09-28 인벤토리 탭 작업에서 `PageClasses.Stats`를 저장해 베이스의 스탯 버튼으로 이 페이지를 연다. 표시 수치는 디자인 샘플로 유지한다.

### 4.10 캐릭터 특성 페이지 — 디자인 및 트리 탐색

`/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterTraitsPage`는 `UNUCharacterTraitsPage`를 부모로 하는 독립 WBP다. **기존 성장 트리의 방사형 그물 구조**를 유지하면서 2026-09-28 확대·축소와 드래그 이동을 연결했다. 이전에 만든 가로 15노드 배치는 현재 디자인이 아니다.

- 디자인 크기는 `1200×600`이다. 상단 페이지 제목·보유 포인트 영역 아래에 `800×536` 노드 패널과 `376×536` 상세 카드를 배치하고 간격은 24로 맞췄다. 공통 프로필·내비게이션·바깥 패딩은 베이스가 제공한다.
- 기존 `DT_ProgressionNode`의 나이아 나이트 그래프 1001에서 **기본 노드 252개, 연결선 512개, 줄기 9개**를 사용한다. 원본의 `LayoutRadius`, `LayoutAngleDegrees`, Offset으로 좌표를 계산하고 노드 종류별 크기도 기존 `WBP_ProgressionGraph`와 일치시켰다. 좌표·크기를 단일 비율로 축소하고 평행 이동해서 전체를 표시하며, 줄기 방향이나 갈래를 다시 배열하지 않는다.
- 중앙 전직 노드에는 기존 `NUProgressionGraphLinkWidget`과 같은 줄기별 3갈래 접합을 적용한다. 최종 선택 노드 4개는 기존 런타임에서도 특정 완주 조건과 마지막 줄기에 따라 생성·재배치되므로 이 정적 기본 화면에서는 제외한다.
- 원본처럼 노드 아래의 개별 텍스트 격자를 없애고 원형 아이콘을 사용한다. 획득 경로와 선택 노드는 금색, 잠긴 아이콘은 원본의 `(0.28,0.31,0.38)` 색으로 낮춘다. 선택 노드에는 얕은 금색 강조 면을 추가한다. 네이비 제목 띠·아이보리 바탕·금색 모서리는 통합 메뉴 디자인을 유지한다.
- 우측에는 선택한 `VIT +1`의 아이콘·효과·설명·필요 레벨·선행 경로·포인트와 획득 버튼 외형을 구성했다. 효과는 `DT_ProgressionNodeReward`의 해당 노드 보상과 일치한다. 선행 경로 `LUK +1`은 샘플에서 선택한 획득 경로다. 전체 후보 경로 목록을 의미하지 않는다.
- 노드의 획득/잠김 상태, 보유 포인트 1과 요구 조건 충족 상태는 정적인 디자인 샘플이며 실제 캐릭터 상태에 연결하지 않았다. 기존 ProgressionNode 아이콘을 재사용하며 원본 성장 트리 WBP와 데이터는 수정하지 않는다. 추가한 C++는 현재 그려진 트리의 확대·이동만 처리한다.
- 실제 획득·노드 선택·데이터 바인딩과 기존 메뉴 진입점 연결은 후속 작업이다. 저장된 `PageClasses.Traits`를 통해 베이스의 특성 버튼으로 이 페이지를 연다. 기존 노드 버튼의 좌클릭은 후속 선택 처리용으로 유지하고, 빈 배경의 좌클릭이나 우클릭/가운데 버튼 드래그로 트리를 이동한다.
- 마우스 휠로 커서 위치를 기준으로 확대·축소한다. 하단 `− / +`는 화면 중앙 기준이며 `전체 보기`는 배율과 이동을 초기화한다. 표시 범위는 전체 보기 기준 **100~800%**, 휠 한 칸과 버튼 한 번당 1.25배다. 확대·이동은 `GraphClip` 안에만 적용되고 오른쪽 설명과 제목·범례는 고정된다. DPI를 반영한 이동, 노드 범위에 따른 이동 제한, 페이지 재방문 시 배율·위치 유지와 마우스 해제 처리를 포함한다.
- 2026-09-28 `/Game/Developers/MinkiCho/ProjectM_Trait_SkillTree_Concept`를 반영해 짧은 제목 띠·금색 프레임·청회색 그래프 배경과 오른쪽 설명 영역만 폴리싱했다. **사용자 요청에 따라 `GraphViewport`와 모든 하위 위젯 1,018개는 슬롯·브러시·색상·버튼 상태를 포함해 변경 전과 동일함을 비교했다.** 내부 252개 노드와 512개 연결선의 디자인·좌표·크기는 변경하지 않았다. 상단 특성/성장 트리는 시안의 표시 외형이며 새 전환 로직은 연결하지 않았다.
- 현재 WBP는 **1,134개 위젯**이다. 확대를 위해 기존 `GraphViewport` 바깥에 `768×420` 클리핑 영역을 추가했으며 내부 하위 위젯 1,017개(노드 252개·연결선 512개 포함)의 슬롯·색·브러시·상태는 그대로다. 전체 보기 배율은 원본 좌표의 약 `0.1191`이다. 향후 동적 `WBP_ProgressionGraph`로 교체하면 기존 정적 트리와 탐색 처리도 함께 교체하여 그래프나 입력을 이중 생성하지 않는다.
- 디자이너의 제목 겹침은 이전 `SectionTitle`, `SectionSubtitle`, `GraphJobTitle`, `GraphStemLabel`을 런타임 `Collapsed`로만 숨긴 상태에서 새 제목을 추가한 것이 원인이었다. UMG 디자이너는 별도의 `bHiddenInDesigner`로 표시를 판단한다. 사용하지 않는 옛 제목·배경 16개와 확대 컨트롤 자리의 옛 줄기 수 문구를 제거해 편집 화면에서도 중복을 없앴다.
- 검증: Editor 빌드 성공, WBP 오류·경고 0, 기존 EventGraph 동일. 자동화 `ProjectM.UI.CharacterTraits.View`와 `Input`이 모두 통과했다. 커서 기준 확대·범위 제한, 그래프 바깥 휠 무시, 150% 배율에서 드래그 좌표 변환·버튼 해제 후 이동 정지를 검증한다. PIE에서는 실제 게임 창의 휠 메시지, 확대 버튼, 최소/최대 배율, 초기화, 여섯 페이지 전환과 재방문 시 배율 유지·중복 바인딩 방지를 확인했다. 노드/연결선 속성 비교와 화면 증거는 `Saved/UITraitsZoom_20260928/`의 `editor_audit.json`, `automation_result.json`, `qa_runtime.json`, `designer_after.png`, `zoom_final00000.png`에 있다.

### 4.11 나이아 구성 페이지 — 디자인 단계

**2026-10-02 파티 빈 슬롯 표시:** 파티 4칸의 `PartySlot0~3Empty`는 `빈 자리` 대신 `+`(Roboto Regular26)를 표시한다. `Align_PartySlotNEmpty`를 64×72 버튼 전체에 Stretch하고 사방4의 동일 여백을 두며, TextBlock의 SizeBoxSlot은 가로·세로 중앙/패딩0이다. 버튼 Normal/Pressed Padding도0으로 맞춰 클릭 시 위치가 밀리지 않는다. 마지막 칸에 별도로 남아 있던 `PartySlot3PlusH/V` 두 장식과 해당 GUID를 제거해 중복 `+`를 없앴다. 현재479개 위젯이며 다른 표시·바인딩은 동일하다. 컴파일0/0, 별도 프로세스 저장 파일 재로드 및 로컬 PIE 화면 확인 완료. 증거/백업: `Saved/NaiaPartyPlus_20261002/`.

`/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterNaiaPage`는 일반 `UserWidget`을 부모로 하는 독립 WBP다. 시안의 큰 초상화·가운데 스킬/능력치·오른쪽 목록 구성을 현재 나이아 로스터의 **파티 4칸, 보관함 6열/정원 500**에 맞췄다. 기존 `WBP_NaiaRosterWindow`와 나이아 시스템은 변경하지 않았다.

- 2026-09-28 `/Game/Developers/MinkiCho/ProjectM_Nia_Concept`를 반영했다. 디자인 크기 `1200×600`, 빈 상태 정리 후 위젯 459개다. 왼쪽 320, 가운데 352, 오른쪽 480 너비와 각 24 간격을 유지한다. 초상화 아치를 선명한 사각 프레임과 얇은 이름 띠로 정리하고, 스킬/능력치/파티/보관함에 짧은 네이비 제목 띠와 금색 모서리를 적용했다. 공통 베이스는 변경하지 않았다.
- 2026-09-28 사용자 요청으로 선택 초상화·파티·보관함의 샘플 나이아 이미지 16개와 샘플 패시브 아이콘을 비웠다. `NamedSlot_NaiaPreview`와 이미지 위젯은 유지하되 텍스처 참조를 해제하고 숨긴다. 이름 띠에는 `선택 없음`, 하단에는 `선택한 나이아 없음`을 표시한다. 원본 이미지·Unreal 텍스처 에셋은 삭제하거나 수정하지 않았다.
- 파티는 `PartyListBox` 아래 **4개 빈 자리, `0 / 4`**를 표시한다. 샘플의 이름·레벨·체력·선두 강조는 제거했으며 파티 1번이 전투에 참가한다는 고정 안내는 유지한다. 이 정원은 기존 로스터 컴포넌트와 일치한다.
- 보관함은 `StorageListBox` ScrollBox와 `StorageGrid` UniformGridPanel의 **6열×3행, 18개 빈 슬롯**이다. 셀은 `64×88`, 패딩은 사방 4, 격자 피치는 `72×96`이다. 수량은 `0 / 500`으로 표시하며 실제 정원 500은 그대로다. 샘플 이름·레벨·등급과 선택 강조를 제거했다.
- **보유 스킬**의 샘플 이름·비용·설명을 비우고 이름은 `—`로 표시한다. 패시브와 기존 두 스킬 행의 자리만 유지하며 시안의 스킬 칸 수를 게임 규칙으로 추가하지 않았다.
- 능력치 8개(최대 HP·최대 SP·공격력·마법 공격력·방어력·속도·치명타 확률·치명타 피해)는 항목을 유지하고 샘플 수치를 모두 `—`로 바꿨다. 빈 상태는 WBP 디자인 기본값이며 실제 보유 데이터나 서버 상태를 변경하지 않는다.
- 네이비·아이보리·금색 모서리와 초상화 하단 이름 띠, 배경 나침반 장식은 유지한다. 빈 슬롯은 작은 마름모로 표시한다. 보유 수량과 금색 상단선이 겹치지 않도록 선을 끊은 구조도 유지한다.
- 선택 대상이 없어 보관함 이동 버튼은 비활성화했다. 필터·정렬 버튼은 외형과 기본 시각 상태만 구현했다. 실제 선택·드래그/교환·빠른 이동·서버 바인딩, 플립북 애니메이션과 기존 메뉴 진입점 연결은 후속 작업이다. 저장된 `PageClasses.Naia`를 통해 베이스의 나이아 버튼으로 이 페이지를 연다.

### 4.12 통합 페이지의 임시 인게임 미리보기

베이스와 여섯 콘텐츠 페이지, WBP 7개의 현재 폴더는 `/Game/Developers/MinkiCho/UI/CharacterMenu/`다. 2026-09-22 폴더 이동 후 미리보기 코드에 남아 있던 `dbals` 경로를 수정했다. C++ 경로 문자열은 에디터의 에셋 이동으로 자동 갱신되지 않으므로, 경로를 변경하면 Editor를 다시 빌드하고 Standalone도 재시작해야 한다. 당시 이동 후 재빌드, Standalone 명령으로 창 열기, PIE 12개 동작 검증을 통과했다. 기록은 `Saved/UICharacterPreviewPathFix/`에 있다.

현재 P키와 기존 메뉴 진입점은 이전 UI를 연다. 새 페이지의 디자인은 **Standalone Game**에서도 확인할 수 있다. 최신 Editor 빌드로 실행한 뒤 게임 창에서 `~` 콘솔을 열고 다음 명령을 입력한다. PIE에서는 Output Log의 **Cmd** 입력란에서도 같은 명령을 사용할 수 있다.

```text
nu.CharacterMenu.Preview
```

명령 뒤에 `Character`, `Equipment`, `Inventory`, `Stats`, `Traits`, `Naia`를 붙이면 해당 페이지부터 연다. 예: `nu.CharacterMenu.Preview Inventory`. 실행 후 콘솔을 닫고 왼쪽의 6개 버튼으로 전환하며, 우측 상단 X 또는 ESC로 메뉴를 닫는다. 다시 열 때도 같은 명령을 사용한다. 이미 열린 통합 메뉴가 있으면 기존 창과 페이지 인스턴스를 재사용한다.

`UI/CharacterMenu/NUCharacterMenuPreview.cpp`의 개발용 명령은 기존 페이지를 재사용하고 없는 페이지를 생성·등록한다. 베이스에 저장된 `PageClasses`에도 여섯 페이지가 연결되어 있다. 캐릭터 정보와 실제 장비 초상화를 제외한 콘텐츠 데이터·행동은 디자인 단계다. 장비·인벤토리·나이아는 빈 상태이며, 스탯 샘플과 특성 트리는 유지한다. 기존 P키·게임 메뉴 진입점과 서버 데이터 연결은 변경하지 않는다. Python/Revvy 없이 동작하며 Shipping 빌드에는 명령이 포함되지 않는다. 패키징한 Development 게임에서는 관련 에셋의 별도 cook 포함이 필요하다. 기존 `Saved/UICharacterMenuPreview/show_all_pages.py`는 로컬 PIE 전용 보조 스크립트로 남긴다.

2026-09-22 검증: Editor 빌드 성공. 별도 `-game` 프로세스의 L_TestLevel에서 명령으로 5개 페이지를 준비하고 나이아 화면이 표시되는 것을 확인했다. PIE에서는 네이티브 명령을 통해 열고 5개 버튼 델리게이트의 NamedSlot 전환, 페이지 인스턴스 유지, 중복 창 방지, 잘못된 인수 거부, X 닫기·입력 복구, 재열기와 패드 뒤로 닫기 등 12개 검증을 통과했다. Standalone에서 물리적 마우스·키보드 조작은 확인하지 않았다. 기록: `Saved/UICharacterStandalone/qa.json`, `build_completed.log`, `build_no_uba.log`, `standalone.log`. 6,480개 기존 파일 비교에서 문서 외 변경은 없었다.

2026-09-28 인벤토리 탭 검증: 일반 Editor 빌드 성공 후 재시작했고, Base WBP 컴파일 오류·경고는 0이다. 새 PIE에서 일반 `Show`의 저장된 페이지 연결, 여섯 버튼 델리게이트의 NamedSlot 전환·단일 선택 표시, 인벤토리 인스턴스와 스크롤 200 유지, 닫기·입력 반환, `nu.CharacterMenu.Preview Inventory` 재열기를 확인했다. 선택 테두리 정렬과 여섯 라벨의 표시 크기도 검수했다. 물리적 마우스·게임패드와 별도 Standalone 프로세스는 이번에 다시 검사하지 않았다. 중간 Live Coding 패치 적용 시 에디터가 종료되어 일반 전체 빌드로 복구했으며 임시 UBT 설정은 원상복구했다. 증거: `Saved/UIInventoryTab_20260928/qa.json`, `fullbuild.log`, `inventory_tab_verified00000.png`.

2026-09-28 전체 화면 개편 검증: Base 200 / Info 105 위젯, 두 WBP 컴파일 오류·경고 0. 새 PIE에서 여섯 페이지의 실제 버튼 델리게이트·단일 선택 표시·인벤토리 인스턴스/스크롤 200 유지·라이브 초상화·닫기/입력 반환·콘솔 재열기를 확인했다. 표시 텍스트 26개가 영역 안에 들어가며 긴 이름·지역 말줄임, 19자리 ID·큰 골드·일곱 자리 EXP를 검사했다. 셸 미리보기는 16:9·16:10·21:9, 런타임은 임시 위젯 할당 영역을 넓고 높은 비율로 바꿔 프레임·내용 비율·선택/제목 표시를 확인했다. 실제 OS 해상도나 프로젝트 DPI 설정은 바꾸지 않았다. 이 임시 할당은 닫고 재열어 제거했고 최종 화면은 전체 뷰포트를 채운다. 물리적 포인터·패드는 이번 검수 범위가 아니다. 기록과 최종 화면: `Saved/UICharacterFullscreen_20260928/qa.json`, `text_qa.json`, `final_audit.json`, `final_fresh00000.png`.

2026-09-28 빈 슬롯 정리 검증: 장비 9칸·인벤토리 30칸·나이아 파티 4칸/보관함 표시 18칸을 유지하고 샘플 이미지 참조와 관련 정보를 비웠다. 세 WBP 컴파일 오류·경고 0, EventGraph 동일, 선택 대상 없는 작업 버튼 비활성화를 확인했다. 별도 Standalone의 장비·인벤토리·나이아 화면에서 빈 상태와 실제 캐릭터 표시를 확인했고 나이아 안내 문구의 말줄임도 수정했다. Base·Info·Stats·Traits와 소스/설정 등 나머지 보호 파일 740개 및 사용자 Git 인덱스는 유지했다. 증거: `Saved/UICharacterEmptySlots_20260928/editor_audit.json`, `final_audit.json`, `equipment00000.png`, `inventory00000.png`, `naia_final00000.png`. 실제 아이템/로스터 연결은 후속 작업이다.

### 4.13 잡화 상점 — 시안 기반 WBP 및 로컬 조작 미리보기

2026-09-29 `/Game/Developers/MinkiCho/상점_페이지`의 실제 이미지를 확인하고 `/Game/Developers/MinkiCho/UI/Shop/`에 다음 3개를 만들었다. 기존 `NUShopSettings.ShopWindowClass` 및 실제 NPC 상점 진입점은 변경하지 않았다.

| WBP | 역할 |
|---|---|
| `WBP_ShopPage` | 갱신된 시안의 네이비·금색 외곽, 긴 아이보리 깃발과 나침반, 대화한 NPC의 상반신·이름 배너, 상품 목록과 아이보리 소지품 패널, 별도 수량 선택 팝업. `UNUShopPageWidget` 부모, 1792×877 디자인을 화면 중앙·가로 80% 범위에 비율 유지하여 표시 |
| `WBP_ShopItemEntry` | 상품 행과 개별 `BuyButton`. `ItemName`, `ItemDescription`, `PriceLabel`, `StockLabel`, `ItemTexture`를 Instance Editable로 제공하고 PreConstruct에서 표시 갱신 |
| `WBP_ShopInventorySlot` | 네이비 모서리 절삭 슬롯과 작은 별 장식. 메인에서 5열×6행, 30칸을 스크롤로 표시. 사용하지 않던 숨김 아이콘·수량·선택 프레임과 이전 테두리 위젯은 삭제 |

**상점 피드백 반영: 2026-09-30.** ‘화면 3/5’는 면적 약 60%로 적용했다. `ResponsiveScale`의 Canvas 앵커는 (0.1,0.1)~(0.9,0.9), 여백 0이며 `DesignSize`는 가로·세로 중앙 정렬이다. 1792×877 비율을 유지해 일반적인 16:9 화면에서 가로 약 80%·세로 약 70~75%를 차지한다. 원본 배경 텍스처를 사용하는 `Feedback20260930/M_Shop_BackdropTrim`은 금색 외곽선 밖 네이비 여분만 투명하게 하고 깃발·금색 장식은 보존한다. 화면 딤은 중립 검정 22%로 변경했다.

버튼은 Navy/Paper/Gold 3계열의 Normal/Hover/Pressed MI를 사용하며 모서리 8·장식선·패딩을 고정하고 색으로 상태를 표현한다. 소지품 분류의 런타임 `SelectedInventoryTabStyle`/`IdleInventoryTabStyle`도 갱신했다. 금액과 재고는 같은 18pt·Y=27 기준선으로 정렬하고 구매 버튼 앞에 24 여백을 확보했다. 팝업 총액은 `PurchaseTotalValue` HorizontalBox 안에서 동전과 숫자를 10 간격으로 묶어 우측 정렬한다. `PurchaseFooter`는 RootCanvas 직계 자식으로 옮겨 상점 축소와 독립된 484×360 중앙 고정 팝업을 유지한다.

미사용 위젯은 상점 페이지 38개, 상품 행 2개, 빈 소지품 슬롯 8개, 총 48개를 삭제했다. 참조 없는 숨김 장식·이전 테두리·미사용 확장 버튼 등을 정리했으며, 실제 사용 시 나타나는 구매 팝업과 빈 분류 안내는 유지했다. 총액 정렬용 HorizontalBox 1개가 추가되어 최종 위젯 수는 Page 192 / ItemEntry 12 / InventorySlot 5이다. 그래프 노드와 연결 및 C++는 변경하지 않았다.

피드백 검증: WBP 3개 오류/경고 0, 배경 머티리얼 정상 렌더링. 6상품·수량 1~99/MAX·합계·취소/바깥 클릭·분류/빈 목록·소지품 선택 스타일·30칸·루간/티온 좌우 보존을 확인했다. 1920×1032와 1280×720에서 실제 구매/수량/취소/소지품 탭/X 클릭, 호버 외곽 유지, 중앙 팝업 드래그 무시, 네 자리 총액 간격도 검증했다. `Saved/ShopFeedback_20260930/`에 백업, 삭제 목록, QA와 최종 화면이 있다. 이 작업은 기존 로컬 디자인 미리보기 범위이며 서버 거래 연결은 변경하지 않았다.

**최신 시안 반영: 2026-09-30.** 업데이트된 `상점_페이지.상점_페이지`에 맞춰 전체 비율을 1792×877로 변경했다. 전체 높이의 깃발·나침반·잎사귀·금색 외곽은 새 `Concept20260930/T_Shop_PageBackdropV3`로 표시하고, 제목·가방 아이콘·상점 문구는 별도 위젯으로 둔다. 상품 영역은 (266,284)/872×524, 상품 행은 872×84와 행 간격 4, 소지품 패널은 (1150,60)/604×758, 슬롯은 104×94이다. 한글 UI는 Roboto 합성 글꼴의 한글 폴백으로 시안의 고딕체에 맞추고 영문 깃발 문구는 기존 명조체를 유지했다. 시안의 `재고 20`은 샘플 표시이며 실제 재고 조회가 아니다.

현재 NPC 표시 영역은 `MerchantPortraitClip` (400,48)/198×156이다. 기존 네이티브 상반신 표시·가로 전체 폭 맞춤을 유지했다. 분류 탭 이동 간격은 상점 218, 소지품 110, 상품 선택 표시의 세로 간격은 88로 함께 조정했다. 시안 반영 시점에는 EventGraph 노드 568개와 구매 패널 드래그 그래프를 보존했으며, 이후 아래 중앙 고정 변경에서 드래그 로직을 제거했다. C++와 HUD 채팅·퀘스트 위젯은 이번 변경 대상이 아니다.

새 장식 배경은 built-in `image_gen`으로 시안에서 텍스트·버튼·상품·NPC를 제거한 배경판을 제작했다. 원본은 `ArtSource/UI/Shop_20260930/T_Shop_PageBackdropV3.png`, 정확한 프롬프트는 같은 폴더의 `backdrop_prompt.txt`, 출처·해시는 `manifest.json`에 보존한다. 상인 배경은 기존 `T_Shop_MerchantBackground`를 계속 사용한다.

검증: WBP 3개 컴파일 오류·경고 0. 상품 6종의 구매 팝업·설명·가격·수량·합계, 1~99/MAX, 취소/바깥 클릭, 분류/빈 목록, 소지품 선택 스타일·30칸, 반복 열기의 단일 인스턴스를 확인했다. 루간·티온의 좌우 잘림 없음, 실제 마우스 구매 버튼·팝업 왼쪽 테두리 드래그/해제·이동 후 수량 2/합계 80·취소·X 닫기 및 커서/이동 입력 복귀를 확인했다. 1920×1032 창과 1280×720 창(실제 게임 뷰포트 1274×682)에서 전체 배치를 시각 검증했다. 결과는 `Saved/ShopRefresh_20260930/qa.json`, `final_main00000.png`, `small_window00000.png`에 있다. 에디터는 PIE를 중지하고 `WBP_ShopPage` 탭을 열어 두었다. 아래 날짜별 설명의 이전 치수는 당시 작업 이력이다.

디자인용 상품 6개는 현재 ShopItemTable의 하급 체력/마나 포션과 화상·둔화·중독·환각 치료제 이름·가격·기존 아이콘을 사용한다. 실제 재고나 보유품을 읽은 화면이 아니며 소지품은 `0 / 30`이다. 행 선택은 선택 표시만 바꾸고, 개별 구매 버튼은 해당 상품의 이름·설명·아이콘·단가를 채운 수량 팝업을 연다. 수량 1~99, MAX, 합계, 취소/바깥 클릭 닫기, 탭/빈 분류, X 닫기는 로컬 미리보기로 연결했다. 팝업의 최종 구매·판매·휴지통 버튼과 서버 거래, 보유 골드/아이템, 잠금 조건, NPC 상점 진입은 미연결이다. 샘플 값과 수량 상한을 실제 거래 규칙으로 사용하지 않는다.

수량 팝업은 별도 WBP를 추가하지 않고 기존 `PurchaseFooter` 가시성 그룹 안의 `PurchaseDialogPanel`로 구성해 수량/합계 바인딩을 유지했다. 기본 Collapsed이며 전체/소모품 탭으로 돌아와도 자동으로 열리지 않는다. 바깥 클릭을 받는 버튼과 팝업 안쪽 입력 차단 영역을 분리했다. 소지품 탭은 선택 배경·글자색·금색 프레임을 함께 갱신한다. 마지막 외곽 여백은 좌/상/우/하 60/68/60/18이다.

**현재 구매 팝업: 중앙 고정(2026-09-30 후속 요청).** `PurchaseDialogPanel`은 `PurchaseFooter`의 중앙 앵커/정렬 (0.5,0.5), 위치 (0,0), 크기 484×360을 사용한다. 마우스로 이동할 수 없으며 다시 열거나 창 크기를 바꿔도 중앙에 표시된다. 드래그용 마우스 함수 3개, 캡처 상실 이벤트/상태/주석 3노드, 상태 변수 3개, 투명 입력 영역 4개를 제거했다. 나머지 EventGraph 565노드와 연결은 그대로 보존했다. 팝업 내부 입력 차단과 수량·MAX·취소·바깥 클릭 동작도 유지했다. 컴파일 오류·경고 0, 상품 6종과 실제 네 테두리 드래그 무시·수량 클릭·취소·재열기, 1920×1032/1280×720 중앙 배치를 확인했다. 백업과 검증 기록/화면은 `Saved/ShopPopupFixed_20260930/`에 있다.

이전 구현 이력 — 2026-09-30 구매 패널 이동(위 중앙 고정 변경으로 제거됨): 상품 아이콘·이름이 있는 상단 영역과 네 면의 테두리를 마우스 왼쪽 버튼으로 끌면 팝업 전체가 이동했다. `PurchaseDragHandle`과 `PurchaseDragEdgeLeft/Right/Bottom`으로 입력을 받고, `OnPreviewMouseButtonDown`/`OnMouseMove`에서 로컬 좌표 변환과 화면 범위 제한을 처리했다. 버튼 해제·캡처 상실 때 이동을 종료하고 같은 상점 창에서는 이동 위치를 유지하는 WBP 구현이었다.

이동 검증: WBP 컴파일 오류·경고 0. PIE의 실제 마우스 입력으로 이동·버튼 해제 후 정지·오른쪽 드래그 무시·좌상단/우하단 범위 제한·이동 후 수량 증가와 합계·취소를 확인했다. 상품 6종·수량 1~99/MAX·탭·바깥 클릭·중복 창 방지 회귀 검사도 통과했다. 백업·스크립트·실행 캡처와 검사 기록은 `Saved/ShopDrag_20260930/`에 있다. 실제 거래 연결 상태는 위 설명과 같다.

같은 날 테두리 드래그 보완: 상·하·좌·우 테두리와 왼쪽 가장 바깥쪽에서 실제 마우스 드래그·해제를 확인하고 수량/합계·취소·바깥 클릭을 재검증했다. 컴파일 오류·경고 0이며 추가 기록은 `Saved/ShopDragBorder_20260930/qa.json`에 있다.

2026-09-30 NPC 상반신 배너: `WBP_ShopPage`의 부모를 `UNUShopPageWidget`으로 연결했다. `SetMerchantActor`로 실제 대화 대상을 넘기며 기본 생성과 반복 `nu.Shop.Preview`는 `ANUPlayerController.GetLastDialogueSource`를 사용한다. 성공한 대화/기능 선택 메뉴의 원본 액터를 약한 참조로 보존하고, 상점 Provider의 검증된 `ClientOpenShop(ShopId, SourceActor)`에도 원본 대상을 전달한다. 가까운 NPC를 추측해서 선택하지 않는다. 이는 표시용 문맥이며 거래 권한으로 사용하지 않는다.

현재 필드 NPC는 Paper2D이므로 해당 `PaperFlipbookComponent`의 현재 `PaperSprite`를 Slate atlas 브러시로 표시한다. `MerchantPortraitClip`(356,76 / 208×138)에서 원본 비율을 유지한 상단 60%를 보여 주며, 원본 애니메이션·알파·색상을 따라간다. 가로 전체 폭과 상반신 높이를 모두 만족하는 작은 배율을 사용해 좌우가 잘리지 않게 하고, 상반신 아래쪽을 카운터에 맞춰 배치한다. 별도 NPC 스폰/SceneCapture가 없고 필드 NPC의 위치·방향·애니메이션을 바꾸지 않는다. `PortraitVisibleFraction`으로 구도를 조절할 수 있다. 대상/프레임이 없으면 숨기고 `NativeDestruct`에서 표시 참조를 해제한다. `MerchantName`은 NPC 데이터의 표시 이름을 사용한다. 3D 메시나 여러 파츠로 구성한 새 NPC는 별도 렌더링 확장이 필요하다.

좌우 잘림 보완(2026-09-30): 기존 144 폭에 티온의 175 폭 이미지가 들어가 양쪽 약 15.6씩 잘리던 문제를 수정했다. 표시 영역을 208로 넓히고 너비 기준 축소도 적용했다. 전체 Editor 빌드 성공(9.89초), WBP 오류·경고 0. 루간/티온의 좌우 경계와 120 폭으로 강제 축소한 경우에도 전체 폭·상반신 하단 정렬을 PIE에서 검증했다. 기록과 수정 화면은 `Saved/ShopPortraitWidth_20260930/`에 있다.

배너 배경은 고정 인물을 제거한 `T_Shop_MerchantBackground`로 바꿨다. built-in `image_gen`으로 편집한 원본 `ArtSource/UI/Shop_20260929/T_Shop_MerchantBackground.png`와 정확한 프롬프트 `merchant_background_prompt.txt`를 보존한다. 이 페이지는 여전히 디자인 미리보기이며 실제 거래용 `ShopWindowClass` 연결 단계는 별도다.

검증: 전체 ProjectMEditor 빌드 성공(268.15초), WBP 컴파일 오류·경고 0. PIE에서 루간/티온 실제 대화 후 자동 대상·이름 교체, 애니메이션 프레임 갱신, 무대상/스프라이트 없는 대상 숨김, 반복 열기 갱신·중복 방지, 닫기 참조 해제를 확인했다. 저장한 위젯을 새 PIE에서 재검증했고 실제 마우스로 왼쪽 테두리 이동·해제, 수량 2/합계 80, 취소를 확인했다. 백업·빌드·검증·최종 화면은 `Saved/ShopMerchant_20260930/`에 있다. 패키지 빌드와 별도 네트워크 클라이언트는 미검증이다.

**개발용 열기 명령:** PIE 또는 Standalone의 게임 콘솔(`~`)에서 `nu.Shop.Preview`를 실행한다. PIE에서는 에디터 Output Log의 **Cmd** 입력란에서도 실행할 수 있다. 닫기는 X/ESC 또는 `nu.Shop.Preview Close`다. `Source/ProjectM/Shop/UI/NUShopPreview.cpp`에서 `UNUUIManager`에 창을 등록해 커서·이동 입력 차단·ESC 소유권을 기존 창 관리 경로로 처리한다. 반복 열기는 기존 창을 유지하고, X는 `nu.Shop.Preview Close`를 먼저 호출해 등록을 해제한 뒤 직접 생성된 미리보기도 제거한다. 이 명령은 Shipping에서 제외되며 Python/Revvy 없이 동작한다. 패키징한 Development 빌드는 별도 cook 포함이 필요하다.

2026-09-29 명령어 최초 검증: 일반 ProjectMEditor 빌드 성공(13.82초), 새 DLL의 별도 `-game` Standalone에서 `Preview ready` 로그 확인. 당시 승인 정책으로 미뤘던 X 닫기·이동 입력/커서 복귀·중복 열기는 아래 디자인 재수정 때 PIE에서 확인했다. 물리적 ESC 입력은 별도 검증하지 않았다. 최초 빌드/Standalone 로그와 백업: `Saved/ShopPreviewCommand_20260929/`.

최신 사람 상인 배너와 잎사귀 패널은 built-in `image_gen`으로 제작했다. 원본 `ArtSource/UI/Shop_20260929/T_Shop_MerchantStallV2.png`, `T_Shop_FoliagePanelV2.png`와 프롬프트 `merchant_stall_v2.json`, `foliage_panel_v2.json`을 보존한다. 픽셀 후처리 없이 배너의 표시 범위만 UMG Brush UV로 조절했고 말풍선/문구는 별도 위젯이다. 구형 고양이 배너와 V2 사람 상인 배너/manifest는 제작 이력으로 남아 있으며, 현재 MerchantBanner는 위의 인물 없는 MerchantBackground를 참조한다. UE 에셋은 `/Game/Developers/MinkiCho/Art/Shop/Concept20260929/`에 있으며 Gold/GoldHover/Slot/TabIdle MI 4개를 추가하고 상점 전용 Navy MI를 어둡게 조정했다. 공유 `M_HUDCutFrame`과 CharacterMenu 원본은 수정하지 않았다.

최신 디자인 검증: WBP 3개 컴파일 오류·경고 0. PIE에서 6종의 개별 구매 팝업 데이터, 수량 하한/상한/MAX/합계, 취소와 바깥 클릭, 행 선택과 팝업 열기의 분리, 탭/빈 분류, 소지품 선택 스타일, 30칸 빈 아이콘, 중복 열기 방지, X 닫기 후 이동 입력·커서 복귀를 델리게이트 호출로 확인했다. 1920×1032 실행 캡처의 메인/팝업을 시각 확인했다. 이후 상단 여백을 조정했으며 해당 여백과 1280×720 임시 할당 영역의 추가 캡처는 테스트 창 최소화로 완료하지 못했다. 물리적 포인터/패드·ESC·실제 거래는 미검증이다. 에디터는 L_TestLevel, PIE 중지, 기존 12개 탭/ShopPage 활성 상태를 유지했다. 증거·백업·스크립트: `Saved/ShopRedesign_20260929/`의 `qa.json`, `final_compile.json`, `final_main00000.png`, `final_dialog00000.png`. 이전 버전 기록은 `Saved/ShopConcept_20260929/`. Revvy `capture_preview`는 Slate prepass 생략 문제가 있어 실행 캡처로 판단한다.

### 4.14 NPC 대화 — 패널 2 시안 (2026-09-30)

`/Game/Developers/zxcv1/Dialogue/UI/WBP_DialogueWidget`을 `/Game/Developers/MinkiCho/대화_패널2` 기준으로 변경했다. 1742×903 기준 화면을 ScaleBox로 맞추고, 하단 (48,619)/1644×248에 네이비·금색 모서리·나침반 배경을 표시한다. 왼쪽 캐릭터 영역은 224×210, 금색 세로 구분선과 화자 이름/밑줄, 본문 스크롤, `F 다음` 버튼은 각각 실제 위젯이다. 기존 `UNUDialogueWidget` 부모와 필수 BindWidget 이름을 유지하며 C++·대사 데이터·퀘스트/거래 코드는 수정하지 않았다.

`DialogueActorPortrait`는 현재 대화 원본 NPC와 표시 중인 화자 이름이 일치하면 PaperFlipbook의 현재 스프라이트 전체를 표시한다. ScaleBox로 종횡비와 좌우 폭을 보존하며 별도 액터나 SceneCapture는 만들지 않는다. 원본 NPC가 없거나 화자가 다르거나 프레임이 유효하지 않으면 원래 `PortraitImage`를 사용한다. 테스트 데이터의 `/Engine/EngineResources/AICON-Red`를 수정하지 않고 실제 NPC 표시로 대체한다. 디자이너의 샘플은 티온 스프라이트이며 실행 중에는 위 조건에 따라 갱신된다. 새로운 3D NPC는 기존 초상화를 사용하며 별도 모델 렌더링은 구현하지 않았다.

선택지는 전용 `/Game/Developers/MinkiCho/UI/Dialogue/WBP_DialogueOptionEntryPanel2`로 분리했다. 공용 `WBP_InteractionOptionEntry`는 그대로 보존한다. 오른쪽 위 326 폭, 최대 높이 310의 스크롤 안에서 금색 선택 프레임·화살표·대화/직업/나가기 아이콘을 표시한다. 대화·직업·나가기의 중복 분류 접두사는 생략하고, 퀘스트 등 나머지 항목은 기존 상태 문구를 유지한다. 전체 원문은 툴팁으로 확인할 수 있다. 선택 실행·활성 여부·글자 출력·키보드/패드 이동·나가기는 기존 네이티브 코드가 담당한다. 다음 버튼은 기존 규칙대로 일반 대사/글자 출력 중에 나타나며, 선택을 기다리는 마지막 줄에서는 숨긴다.

배경은 built-in `image_gen`으로 시안의 장식만 분리 제작했다. `ArtSource/UI/DialoguePanel2_20260930/`에 PNG 원본·정확한 프롬프트·manifest를 보존하고, 에셋은 `Art/Dialogue/Panel2/T_Dialogue_Panel2`다. 원본 픽셀은 후처리하지 않았으며 투명 여백은 UMG Brush UV 범위로 제외했다. 패널 1/2 참고 이미지는 그대로 두었고 프레임·초상화·텍스트를 독립 구성했다. 패널 1의 완성 프리셋/원클릭 전환은 아직 추가하지 않았다.

검증: 두 WBP 컴파일 오류·경고 0. 실제 티온·루간 대화의 화자/스프라이트 전환, 다른 화자의 원래 초상화 복귀, 마우스 선택 강조·다음·나가기, 글자 출력 첫 입력 완성, 긴 본문 줄바꿈과 1920×1032/1280×720 배치를 확인했다. 오프라인 테스트 월드에서 선택 버튼의 네이티브 전달/메뉴 닫기는 확인했으나 서버 후속 퀘스트/거래 실행은 검증하지 않았다. 백업·스크립트·QA·화면은 `Saved/DialoguePanel2_20260930/`에 있다.

### 4.15 HUD 알림 패널 (2026-09-30)

기존 필드 HUD의 네이비 면·얇은 금색 이중선·작은 잘린 모서리·아이보리 글자를 알림에도 적용했다. 공용 `M_HUDCutFrame`과 나침반/해 문양을 재사용하며, 알림 전용 인스턴스 6개는 `/Game/Developers/MinkiCho/Art/FieldHUD/Alerts20260930/MI_Alert_*`에 둔다. 공용 마스터, 일반 HUD, 대화, 상점, C++ 동작은 이번 작업에서 변경하지 않았다.

| 대상 WBP (`/Game/Developers/` 기준) | 변경 내용 |
|---|---|
| `dbals/UI/WBP_NoticeBanner` | 하단 중앙 680 폭 배너, 해 문양, 본문 19pt, 여러 줄 높이 자동 확장, 내부 여백 24/16. 기존 2.5초 숨김과 클릭 통과 유지 |
| `dongh/UI/WBP_PartyPrompt` | 상단 중앙 520×260 카드, 파티 요청 제목·본문·응답 시간·가는 진행선·수락/거절 버튼. 기존 34개 그래프 노드와 타이머/클릭 연결 유지 |
| `dbals/UI/WBP_ConfirmDialog` | 중앙 560×306 카드, 스크롤 본문, 명확한 확인/취소. 순정 Button과 네이티브 `ConfirmLabelText`/`CancelLabelText` 계약으로 동적 라벨을 표시하고 Positive/Destructive 색 구분 유지. 안내 전용은 취소가 접히며 확인만 중앙 정렬 |
| `zxcv1/Quest/UI/WBP_RewardPopup`, `zxcv1/UI/WBP_RewardPopup` | 두 보상창을 중앙 560×390 카드로 통일. `TitleText` 네이티브 계약으로 실제 보상 제목 표시, 본문 스크롤·하단 확인 분리. 실제 QuestWidget은 Quest/UI 쪽을 사용 |
| `dongh/UI/WBP_ActionBanner` | 상단 기존 위치와 등장 애니메이션 유지, 네이비·금색 배너와 아이보리 22pt. 스킬 이름·레벨·연타 수 표현 유지 |

검증: WBP 6개 모두 오류/경고 0, 기존 이벤트 그래프 노드·연결 유지. L_TestLevel PIE에서 일반 알림의 여러 줄/자동 숨김, 파티 초대·가입 문구 및 카운트다운/버튼 바인딩, 전투 배너 등장/레벨·연타 표시, 확인창 실제 마우스 취소·ESC·안내 단일 버튼 중앙 배치, 실제 퀘스트 UI 경로의 보상 제목·확인 클릭·대기열 다음 팝업·Enter 닫기를 확인했다. 12개 긴 보상 항목 스크롤 끝과 1920×1032/1280×720 배치를 검수했다. 파티 서버 수락/거절과 실제 보상 지급은 검증 범위 밖이며 테스트는 표시 경로만 실행했다. 백업·최종 컴파일/런타임 QA·화면은 `Saved/HUDAlerts_20260930/`에 있다. 원본 적용 스크립트는 최초 변환용이며 최종 에셋에 그대로 재실행하지 않는다.

후속 버튼 통일: `WBP_ConfirmDialog` 확인/종료 버튼도 취소 버튼과 같은 잘린 모서리·금색 이중선·호버/눌림 패딩을 사용한다. `ConfirmButtons20260930/M_ConfirmIntentButton`과 Normal/Hover MI는 기존 의도색(종료 빨강·수락 초록)을 면에 유지하고 금색 테두리의 Slate 색 곱만 보정한다. 기존 `PanelIntro`와 그래프 5노드/연결·취소 스타일은 보존했다. 컴파일 오류/경고 0, 실제 PIE 색·호버·콜백 없는 확인/취소 클릭 검증과 백업은 `Saved/ConfirmButtonMatch_20260930/`에 있다.

### 4.16 설정·키 설정 — 시안 적용 (2026-10-06)

`설정_패널_디자인`·`키설정_패널_디자인`을 기준으로 네이비 바탕, 금색 프레임·탭 밑줄, 나침반 리본과 적용 버튼을 반영했다. 대상은 dbals/UI의 OptionsWindow, KeyBindingsWindow, KeyBindingRow, KeyBindingGroup, OptionRadioGroup WBP 5종이다. 기존 컨트롤 이름·바인딩을 유지하며, 키보드·마우스 통합 열과 게임패드 열은 실제 입력 시스템 구성을 따른다. 미구현 항목은 준비 중으로 표시한다.

배경은 독립 에셋 `Art/Settings/Textures/T_Settings_Frame`을 사용한다. 재적용 스크립트와 C++ 연결 규약은 [Tools/UISettingsReference](../Tools/UISettingsReference/README.md), 원본은 `ArtSource/UI/SettingsReference`에 있다. C++ 빌드 및 WBP 5종 컴파일 오류·경고 0, PIE에서 4개 탭·선택 표시, 상단 닫기, 키보드·게임패드 입력 대기, 입력 중 취소·재열기를 확인했다. 실제 키 매핑 저장이나 전체 화면비·패드 탐색 QA는 별도다.

### 4.17 설정 기준 패널 색상 통일 (2026-10-06)

**허브 최신 배치는 아래 4.18절을 따른다.** 이 절의 허브 중앙 패널·5열·85% 축소 내용은 이전 작업 기록이며, 재적용 시 `Tools/UIHubSidePanels/apply_edge_fade.py`를 마지막에 실행한다.

- 허브 버튼 패널의 왼쪽 잘린 문양은 제거하고 오른쪽 나침반은 약 90% 보이도록 왼쪽으로 이동했다. 메뉴는 5열 유지·왼쪽 정렬, 하단 시스템 버튼 3개는 오른쪽 정렬이다. `hub_personalize.py`를 허브 관련 스크립트 중 마지막에 적용한다.
- 왼쪽 인사말은 클릭 편집 가능: Enter/포커스 이동으로 저장, Esc로 취소, 빈 입력은 기본 인사말로 복원한다. 최대 32 UTF-16 단위, 한글 조합 중에는 제한하지 않는다. `Saved/Config/HubProfiles.ini`에 서버·계정·캐릭터별 로컬 저장하며 서버 동기화는 하지 않는다.

- 허브 크기 축소 요청: `HubFit`의 화면 점유 가로·세로를 기존 85%로 조정했다. 글자 상대 크기 +10%, 주요 아이콘 86→96으로 보완해 가독성을 유지한다. 게임 해상도는 변경하지 않는다. 재적용은 `Tools/UIPanelConsistency/compact_hub.py`, 실행 검증은 `Saved/UIPanelConsistency/hub_compact_runtime.json` 및 스크린샷 참조.

- 허브 오른쪽 메뉴 버튼 패널도 후속 요청으로 원래 아이보리 바탕과 글자·아이콘 색상을 복구했다. 왼쪽 프로필과 새 나침반은 유지한다. `M_Hub_OriginalRight`가 두 원본 텍스처를 구역별로 표시하며 허브 WBP 3개도 일괄 색상 재적용에서 제외했다.

- 후속 요청으로 **퀘스트 창 5개 WBP와 `MinkiCho/UI/CharacterMenu` 8개 WBP는 색상 통일 직전 디자인으로 복구**했다. 아래 최초 적용 범위에서 두 그룹은 제외하며, 재적용 스크립트도 제외 처리했다. 허브·설정·그 외 메뉴 변경은 유지한다.

- 설정 창의 남색 바탕·금색 강조를 공통 기준으로 사용한다. 메인 메뉴 허브는 배경 색상과 나침반 문양을 함께 교체하고, 캐릭터·퀘스트·상점·대화 및 기타 메뉴는 기존 문양과 배치를 유지한 채 색상을 교체했다.
- 전용 아트와 색상 변환 머티리얼: `/Game/Developers/MinkiCho/Art/UITheme/SettingsNavy`. 원본 PNG: `ArtSource/UI/PanelConsistency`. 상점은 탭 전환 때 원래 브러시를 다시 적용하므로 전용 MI의 표면 색상도 맞춘다.
- 재적용: `Tools/UIPanelConsistency/apply.py`. 기존 페이지 생성 스크립트보다 나중에 실행한다. 위젯 계층·슬롯·GUID·바인딩 보존을 검사하며, 백업과 검증 기록은 `Saved/UIPanelConsistency`에 남긴다.

### 4.18 메인 메뉴 허브 — 시안2 좌우 패널 (2026-10-06)

**Depth follow-up (2026-10-06):** Final pass is `Tools/UIHubSidePanels/apply_depth.py`. Neutral polished colors remain, with mild multiplicative shading on paper/books/desk and a transparent near-camera leaves overlay. Left backdrop uses static noise and a smooth finite fade toward the center, while button artwork stays legible. Overlay is hit-test invisible and the game center remains clear. Sources/prompts: `ArtSource/UI/HubSidePanels/DEPTH.md`; runtime evidence: `Saved/UIHubDepth`.

**Illustrated polish (2026-10-06):** The polish pass is `Tools/UIHubSidePanels/apply_polish.py` (includes the edge/interaction passes). Clean illustrated navy/ivory texture siblings remove distressed realism, and a shared warm-hue correction produces neutral champagne accents rather than orange. Hover atlases remain independent per row. A transparent illustrated books/quill/desk cutout replaces photographic texture; clean contour masks remove background fragments. Native behavior is unchanged. Sources and built-in image-generation prompts: `ArtSource/UI/HubSidePanels/POLISH.md`; evidence: `Saved/UIHubPolish`.

**Previous edge follow-up (2026-10-06):** Edge pass is `Tools/UIHubSidePanels/apply_edge_fade.py`, which includes the interaction pass. The profile art and live fields align to the upper-right corner (source Y offset -139); X stays inside the paper. Hub `OpenGameMenu` disables the global dim and temporarily hides/restores the field HUD so its top icons cannot peek through. Three finite black gradients fade behind the left rail, top-right card and bottom-right desk; the central character/map view remains fully clear. Build and runtime evidence: `Saved/UIHubEdgeFade`.

**최신 배치·호버 수정:** `apply_interaction.py`가 원본 적용을 포함한 최종 스크립트다. 왼쪽 아트와 버튼은 별도 ScaleToFitY 그룹으로 화면 왼쪽·아래에 밀착한다. 오른쪽 상단 정보창 배치는 유지하고, 우측 하단에 원본 책·깃펜·나침반·책상 RGB와 세부 투명 마스크를 합성한다. 9개 버튼은 기본 남색, 호버 시에만 해당 행의 아이보리 상태가 표시되며 행별 동적 머티리얼을 독립 사용한다. 하단 설정·캐릭터 선택·게임 종료에는 금빛 반투명 배경과 얇은 테두리 호버를 적용한다. 검증 기록은 `Saved/UIHubInteraction`.

**후속 수정:** 첫 생성형 패널은 시안과 달라 사용자가 수정을 요청했다. 현재는 `apply_reference.py`를 최종 적용한다. 원본 시안과 바이트가 같은 텍스처에서 왼쪽 타이틀·9개 버튼의 한글/영문·아이콘·윤곽·하단 버튼 및 오른쪽 종이 패널을 직접 표시한다. 실시간 이름·레벨·경험치·수치 영역만 정리된 종이 위에 UMG로 표시하며, X와 인사말 편집을 유지한다. 장비 항목은 통합 캐릭터 메뉴의 장비 페이지에 연결했다. 중앙·오른쪽 하단은 다각형 마스크로 비운다. 아트 경로는 `Art/UITheme/HubReference2`, 기록은 `Saved/UIHubReferenceExact`이다. 아래 내용은 최초 근사 버전 기록이다.

`메인_메뉴_허브_시안2`를 기준으로 왼쪽에 기존 메뉴 8개를 세로 목록으로 배치하고 하단에 시스템 버튼 3개를 둔다. 오른쪽에는 상단 캐릭터 요약 카드와 X 닫기 버튼만 표시한다. 기존 블러·캐릭터 미리보기·대체 초상 위젯을 제거하고 중앙 및 오른쪽 하단을 투명하게 비워 실제 게임 화면이 보이도록 했다. 인사말 편집·캐릭터별 로컬 저장 기능은 오른쪽 요약 카드로 옮겨 유지했다.

독립 아트는 `Art/UITheme/HubSidePanels`, 원본 PNG는 `ArtSource/UI/HubSidePanels`에 있다. 참고 시안의 캐릭터 그림과 배경은 런타임에 사용하지 않는다. 전용 UI 머티리얼로 패널 외부 알파를 차단한다. 기존 메뉴 등록·동작·바인딩은 유지하며 이 배치 변경에 새로운 C++ 코드는 필요하지 않다. 재적용 및 검증 기록은 [Tools/UIHubSidePanels](../Tools/UIHubSidePanels/README.md) 참조.

## 5. 이미지 원본과 에셋 관리

`ArtSource/UI`는 제작한 PNG 원본과 프롬프트·SHA-256·치수·임포트 기록이다. 게임이 실행할 때 읽는 파일은 `Content`의 Unreal 에셋이다. 원본은 수정·재임포트·출처 확인을 위해 보존한다.

2026-09-21 원본 정리에서 교체된 원본·중간 결과 41개를 삭제하고 혼합 manifest 3개를 정리했다. 참조가 확인된 Texture2D 원본 32개, 사용자 정리 대상인 전체 시안 PNG 5개와 관련 기록을 포함하여 57개 파일을 남겼다. 참조가 있다는 사실이 모두 현재 화면에 표시된다는 뜻은 아니므로 숨겨진 브러시·간접 참조도 보수적으로 보존했다.

후속 요청으로 `Content`의 미사용 채팅 텍스처 9개와 퀘스트 텍스처 7개도 삭제했다. 대상은 ChatConceptV4 3개, ChatRoot의 구형 Panel/Control/Input 및 V2 6개, Quest_BookmarkTab_V5, QuestMist 3개, QuestTheme 3개다. 삭제 직전 에셋·코드 참조가 없음을 확인하고 신규 추가 스테이징도 해제했다. 목록과 로컬 복구 사본은 `Saved/UIUnusedTextureCleanup`에 있다. 현재 사용하는 ChatJoined·FrameV3·PolishV3·CategoryRail 등의 텍스처와 아래 표의 원본 32개는 유지한다.

추가로 구형 FieldHUD 텍스처 8개(`T_FHUD_Character`, `Growth`, `Inventory`, `MenuBase`, `Naia`, `Panel`, `Quest`, `Stats`)와 `MI_FHUD_Menu`를 삭제했다. 이 묶음 밖의 에셋·코드 참조가 없음을 재확인한 뒤, MenuBase를 참조하던 MI부터 제거하고 신규 추가 스테이징도 해제했다. 현재 사용하는 `MI_FHUD_ChatTab`, `T_FHUD_Control`, `T_FHUD_QuestMarker`와 IconsV2 계열은 유지한다. 복구 사본과 검증 기록은 `Saved/UIFieldHUDUnusedCleanup`에 있다.

| 원본 폴더: `ArtSource/UI/` 아래 | 보존된 사용 원본 | 현재 Unreal 위치 |
|---|---|---|
| `ChatJoined_20260918` | Frame / TabIdle / TabSelected, 3개 | `MinkiCho/Art/FieldHUD/Chat_New/Textures` |
| `ChatRootTheme_20260917` | General / Local / World / Party / Whisper / System, 6개 | 같은 Chat_New/Textures |
| `FieldIconsV2_20260918` | 상단 메뉴 아이콘 6개 | `MinkiCho/Art/FieldHUD/IconsV2` |
| `FieldButtonBlue_20260918` | MenuBase_LightBlue, 1개 | 같은 IconsV2 |
| `FieldHUDTheme_20260917` | FHUD_Control / QuestMarker, 2개 | `MinkiCho/UI/FieldHUDTheme/Textures` |
| `QuestFrame_20260921` | Navy / Reading / TabLine, 3개 | `MinkiCho/Art/Quest/FrameV3/Textures` |
| `QuestPolish_20260921` | QuestPolish_Control, 1개 | `MinkiCho/Art/Quest/PolishV3/Textures` |
| `QuestRail_20260921` | QuestRail_Icons, 1개 atlas | `MinkiCho/Art/Quest/CategoryRail` |
| `QuestJournalV4_20260917` | Paper / Navy / Control / Compass / Feather, 5개 | `MinkiCho/Art/Quest/JournalV4/Textures` |
| `NaiaFolio_20260917` | Journal / Motif, 2개 | `MinkiCho/Art/UITheme/NaiaFolio/Textures` |
| `GameMenuHub_20260921` | backplate / cards, 2개 | `dbals/UI/GameMenuHub/Textures` |

표의 Unreal 경로는 `/Game/Developers/` 기준이다. 각 원본 폴더의 `manifest.json`이 생성 기록을 보유한다. 이전 manifest의 경로나 참조 입력은 생성 당시의 이력일 수 있다. 특히 ChatRoot는 최초 `dongh/UI/Chat/RootTheme`에서 현재 `MinkiCho/Art/FieldHUD/Chat_New`로 이동했으므로 basename과 해시, 현재 Asset Registry 참조를 함께 확인한다. 제거된 이전 생성 입력의 역사적 경로를 현재 필수 원본으로 해석하지 않는다.

신규 공통 UI 텍스처의 기본 위치는 `/Game/Developers/MinkiCho/Art` 하위 기능 폴더다. 기존 FieldHUDTheme와 게임 메뉴 전용 GameMenuHub처럼 현재 참조가 유지되는 예외 경로를 임의로 이동하지 않는다. 에셋 이동은 Unreal 이동 기능과 참조·리다이렉터 정리, 관련 WBP 컴파일을 통해 수행한다.

현재 원본들은 내장 image generation 결과를 픽셀·알파 후처리 없이 보관했다. 이미지 모델 식별자는 도구가 제공하지 않았으므로 작업 에이전트 모델과 혼동하지 않는다. UI 텍스처는 개별 기록에 따라 UI 그룹, sRGB, UI 압축, NoMipmaps, Clamp, NeverStream을 사용한다. 최대 크기와 Box margin은 실제 9-slice 선 두께에 영향을 주므로 원본 해상도만 보고 일괄 변경하지 않는다. 필요한 투명 여백은 Brush UVRegion과 표시 크기로 조절한다.

이번 참조 조사에서 원본 32개의 Texture2D import filename 배열은 비어 있었다. 따라서 그 배열만으로 ‘원본 미사용’을 판정하지 않으며 manifest의 매핑·해시와 Asset Registry의 직접·간접 참조를 같이 사용한다.

## 6. 검증 기록과 범위

2026-10-01 전투 UI: ProjectMEditor 빌드 성공 후 에디터를 다시 열어 네이티브 변경 로드를 확인했다. 변경한 WBP 6개와 아바타/표식 BP 4개는 컴파일 오류·경고 0, 신규 마스터 머티리얼 5개도 최종 셰이더 오류 0이다. L_TestLevel PIE의 실제 위젯과 소환한 전투 아바타에 임시 표시 데이터를 넣어 HP 72%/86%, 아군 SP 62.5%, 적 SP 숨김, 숫자 위젯 제거, 이름·레벨 바인딩, 선택 배경 전환, 패리 비용 4→7과 마커 0.82, 회피 SP 6 표시 및 패리 바 전환을 확인했다. 표식의 실제 머리 앵커 부착과 다른 아바타로의 재부착도 확인했다. 1914×994 및 1280×720 뷰포트에서 배치를 검토했다. 실제 서버 전투 입장·명령 전송/결과 처리·패키징은 이번 검증 범위가 아니다. 원본 백업·스크립트·빌드 로그·PIE 화면은 `Saved/BattleUIRefresh_20261001/`에 있으며 테스트 액터는 PIE 종료로 제거했다.

2026-09-28 장비·인벤토리 분리: 장비/인벤토리 WBP 컴파일 오류·경고 0, 각각 EventGraph 3개 노드/0개 연결. 장비 페이지의 인벤토리 위젯 제거, 인벤토리 페이지의 착용 영역 제거, 착용 9칸·인벤토리 5열/30칸을 검사했다. PIE에서 실제 장비 캐릭터 표시, 페이지 이동 시 캡처 반환·재사용, 인벤토리의 임시 NamedSlot 조립과 스크롤 끝, 고정 상세 영역·닫기 입력 복귀를 확인했다. 텍스트 32개(장비)와 인벤토리 표시 텍스트를 슬롯 크기로 검사했다. 증거·백업·스크립트는 `Saved/UIEquipmentSplit_20260928/`에 있다. 공통 베이스·다른 페이지·소스·설정 등 727개 보호 파일은 동일하다. 메뉴 영구 연결·아이템 조작·서버·전체 패키징은 이번 검증 범위가 아니다.

2026-09-28 캐릭터 메뉴 새 시안 4종: Info/Stats/Traits/Naia WBP 컴파일 오류·경고 0, 기존 EventGraph 유지. 특성 `GraphViewport` 및 하위 1,018개 위젯의 전체 스냅샷이 변경 전과 동일하며 노드 252개/연결선 512개를 유지했다. PIE에서 5개 NamedSlot 페이지 전환·캐시 재사용, 캐릭터 정보의 성장/골드/직업 갱신, 실제 캐릭터 표시, 긴 텍스트 정책·19자리 ID·큰 골드 수치, 닫기 후 입력/커서 복귀와 재열기 11개 검사를 통과했다. 현재 표시 텍스트 163개(Info 16/Stats 51/Traits 25/Naia 71)의 슬롯 크기를 측정했다. 증거와 변경 전 백업은 `Saved/UICharacterConcepts_20260928/`에 있다. 이번 범위는 WBP 디자인이며 신규 게임 데이터 바인딩·서버 호출·C++·공통 베이스·장비 화면은 변경하지 않았다. 전체 패키징과 물리 포인터 클릭 검사는 포함하지 않는다.

2026-09-27 공통 프로필 직업 로고: Live Coding 컴파일/패치 성공, 베이스 WBP 컴파일 오류·경고 0 및 기존 EventGraph 연결 유지. L_TestLevel PIE에서 6개 기본 직업, 5개 전직, 미등록 직업/직업 해제, 5개 NamedSlot 페이지 이동을 실제 직업 변경 델리게이트로 검사했다. 원래 로컬 직업 상태를 복구했으며 서버 호출은 하지 않았다. 닫기 후 게임 입력·커서 복귀를 확인했다. 모든 로고의 실제 표시 화면과 결과는 `Saved/UIJobEmblems_20260927/`의 캡처 및 `functional_qa.json`에 있다. 전체 패키징 검증은 포함하지 않는다.

2026-09-27 재업로드 시안 Revision 2: 퀘스트 저널·항목·목표 행·보상 타일 WBP 컴파일 오류/경고 0, 네이티브 표시 변경 Live Coding 컴파일 및 패치 적용 성공. L_TestLevel PIE에서 실제 지역명/목표 수량, 잠금 조건, 경험치·골드·아이템 표시를 확인했다. 임시 런타임 데이터로 선행 목표 숨김/해제, 완료 체크·수량 상한, 선택 목표, 지역 없음, 보고 안내를 검증하고 원복했다. 상단/레일 5쌍 연동, 빈 분류, 상태 그룹, 추적과 선택 갱신도 통과했다. 자료는 `Saved/UIQuestReference_20260927_Revision2/`의 `functional_qa.json`, `row_qa.json`과 화면 캡처에 있다. 서버 수락/보상 지급이나 패키징 테스트는 포함하지 않는다. 아래 첫 시안 검증의 ‘소스 무변경’은 당시 범위이며 이번에는 퀘스트 UI 표시 소스 3개와 공통 표시 헬퍼 1개를 수정/추가했다.

2026-09-27 퀘스트 새 시안 적용: `WBP_QuestJournal`과 `WBP_QuestJournalEntry` 컴파일 오류·경고 0. L_TestLevel PIE에서 상단/좌측 5개 카테고리 동기화, 빈 분류의 상세 숨김·추적 비활성화, 3개 상태 그룹 접기/펼치기, 실제 퀘스트 3개 선택 및 상세 갱신, 목표 표시 토글/복원, 닫기 입력 차단/반환과 재열기를 확인했다. 긴 제목·10행 설명·7행 목표·4행 보상은 임시 런타임 텍스트로 시험해 스크롤 끝(780.61)에 도달하고 하단 행동 영역이 유지됨을 확인한 뒤 복원했다. 테스트는 실제 버튼 델리게이트 호출이며 물리 마우스 클릭이나 서버 수락/보상 지급은 포함하지 않는다. 기존 네이티브 소스·캐릭터 메뉴·MCP 설정 등 676개 보호 파일은 작업 전과 동일하다. 자료: `Saved/UIQuestReference_20260927/functional_qa.json`, `motion_and_long_text_qa.json`, `final_verified00000.png`. 위젯 재구성 중 도구의 이름/GUID 등록에 대한 handled ensure가 있었고 최종 후속 컴파일에서는 재발하지 않았다. 최종 위젯 수는 저널 238개·항목 8개이며 기존 EventGraph의 노드 수와 연결(87/106, 8/6)은 동일하다.

다음은 각 변경을 완료했을 때의 기록이다. 숫자는 해당 작업의 비교 범위이며 전체 프로젝트의 무변경을 보증하는 숫자가 아니다.

| 작업 | 확인한 결과 | 로컬 근거 폴더 / 대표 자료 |
|---|---|---|
| 필드 HUD 최초 정리 | WBP 5개 0 오류/0 경고, 추적기 조작·긴 한글·채팅 채널/포커스 | `Saved/UIHUDPolish`, `Saved/UIHUDTextures` |
| HUD 시안 적용 (2026-09-29) | WBP 6개 0 오류/0 경고. 실제 리베르그로브 PIE에서 지도·장소 이름, 메뉴 호버와 퀘스트 저널 진입, 추적기 접기·닫기·재열기 확인. 채팅은 로컬 검증 인스턴스로 6개 탭·채널 전환·드롭다운 확인; 서버 송수신은 미검증. 캡처의 대화 3줄은 저장·전송하지 않은 임시 표시 | `Saved/UIHUDConcept_20260929/qa.json`, `final_audit.json`, `final_hud_clean00000.png` |
| HUD 숨김 토글 (2026-09-29) | ProjectMEditor 전체 빌드 성공, WBP_FieldHUD 0 오류/0 경고. PIE 5회 숨김·복원, 숨김 중 지역/퀘스트 갱신, 추적기 접힘·닫힘 보존, 나중에 생성된 채팅의 숨김, 채팅 입력 시 복귀와 숨김 시 포커스 해제, 맵 재로드 뒤 상태 유지 확인. 채팅은 오프라인 검증 인스턴스이며 온라인 송수신·실제 전투 진입은 이번 검증에 포함하지 않음 | `Saved/HUDVisibility_20260929/build.log`, `qa.json`, `visible00000.png`, `hidden00000.png` |
| 상단 아이콘·연파랑 버튼 | WBP 0/0, 6개 런타임 참조·크기·상태 확인, 실제 화면 검토 | `Saved/UIFieldIconsV2/verification.json`, `Saved/UIFieldButtonBlue/verification.json` |
| 패널·테두리 그라데이션 | WBP 3개 0/0, 마스터 셰이더 정상, 밝고 어두운 화면 확인 | `Saved/UIHUDGradient`, `Saved/UIHUDBorderFade` |
| 연결형 채팅 | WBP 3개 0/0, 6개 탭과 페이지·채널·모달·포커스, 긴 한글 스크롤 | `Saved/UIChatJoined/verification.json`, `final_chat00000.png` |
| 채팅 미세 그라데이션 | 밝고 어두운 배경·드롭다운·입력 포커스, 11개 버튼의 33개 상태 머티리얼 확인 | `Saved/UIChatSubtleGradient/verification.json`, `after_bright00000.png` |
| 채팅 불투명도 +30% | 최종 MI 5개 값 독립 재조회·저장, 다른 기존 에셋 193개 동일. **이 수치 조정 후 PIE 재실행은 하지 않음** | `Saved/UIChatOpacity30/applied.json` |
| 퀘스트 프레임 | WBP 0/0, 분류·상태·추적·긴 한글/스크롤·고정 행동 영역, 다른 기존 에셋 162개 동일 | `Saved/UIQuestFrame/readback.json`, `runtime_checks.json`, `final_frame00000.png` |
| 양쪽 카테고리 | 좌측 5개·상단 5개의 실제 OnClicked 델리게이트로 동일 필터·목록·상세·단일 선택 확인 | `Saved/UIQuestRail/runtime_checks.json`, `final_rail00000.png` |
| 퀘스트 On/Off | Editor 빌드 성공, WBP 0/0, 16개 단계의 18개 실행 검사. Q/ESC 주입·재열기·일시정지·입력 반환·카테고리 동기화 포함 | `Saved/UIQuestMotion/runtime_motion_checks.json`, `final_motion00000.png` |
| 메뉴 허브 | Editor 빌드 성공, WBP 4개 0/0, 10개 메뉴/닫기 델리게이트와 프로필 확인, 기존 에셋 309개 동일 | `Saved/UIGameMenuHub/final.png` 및 같은 폴더의 검증·빌드 기록 |
| 캐릭터 통합 베이스 | Editor 빌드 성공, WBP 0/0, 5개 선택·페이지 교체/상태 유지·프로필 확인, 닫기 버튼/ESC/패드 B 입력 복귀, 패드 탐색/A 선택, 실제 PIE 화면 중앙 정렬 확인. 개별 페이지와 서버 데이터 변경 중 갱신은 후속 범위 | `Saved/UICharacterBase/final00000.png`, `behavior.json`, `final_asset.json`, 입력 QA·빌드 기록 |
| 통합 베이스 여백 조정 | WBP 0/0, 5개 선택과 변경된 NamedSlot 계층의 페이지 교체 확인. PIE에서 임시 본문 면과 긴 한글 이름·직업으로 간격을 시각 확인한 뒤 제거. Border/BorderSlot 패딩 readback 일치. Python Geometry가 0으로 반환되어 수치 검증에 사용하지 않음 | `Saved/UICharacterSpacing`, `final_asset.json`, `live_verification.json`, `content_margin_probe00000.png` |
| 통합 메뉴 공통 간격 규격화 (2026-09-23) | 베이스+5페이지 WBP 컴파일 오류·경고 0. 헤더 144·패딩 48/24/48/40·본문 1200×600 구조 readback, 5페이지 전환/캐시/닫기 등 12개 동작 검증 통과. PIE 전체 화면 5종과 현재 표시 텍스트 228개의 실제 desired size/할당 박스 비교에서 넘침 0. 기존 9개 착용 슬롯·4×4 인벤토리·4개 파티/6열 보관함·방사형 트리 보존. 캐시 Geometry가 0이므로 본문 크기는 슬롯 산식과 화면으로 검증. C++·데이터 바인딩 변경 없음 | `Saved/UIMajorMenuMetrics_20260923/final_audit.json`, `qa.json`, `*_metrics.json`, 페이지별 PNG |
| 캐릭터 요약 패널 폴리싱 (2026-09-23) | 정보 페이지 WBP 0/0. 3단 카드→단일 요약 패널, 중복 이름/직업 제거, 열 간격32. 기존 성장·인벤토리 변경 알림의 UI 갱신, 실제 초상화·전환 시 풀 반환/재사용·닫기 입력 복구 확인. 긴 한글 지역명·19자리 ID·10억 단위 골드 표시 검수. 공통 상단 간격16 안을 다섯 페이지에서 비교 후 기존24 유지 | `Saved/UICharacterSummary_20260923/qa_results.json`, `long_values.json`, `decision.json`, `final_audit.json`, 최종 PNG |
| 캐릭터 정보 카드 제거·초상화 확대 (2026-09-23, 후속) | 정보 페이지 WBP 0/0, 80→39 위젯. 카드 외곽/문구/장식 축소, 초상화 RenderScale1.25 및 받침대 정렬, 이름·직업/레벨·EXP·3행 정보로 재구성. 실제 값·성장/직업/인벤토리 변경 알림·페이지 재방문과 초상화 풀 반환/재사용·닫기 입력 복구 검증. 현재 텍스트12개와 긴 이름/직업/지역/19자리ID/큰 골드의 배치 확인. 베이스·다른4페이지·C++·공용 미리보기 렌더러 유지 | `Saved/UICharacterFlat_20260923/final_audit.json`, `qa_results.json`, `metadata_metrics.json`, `long_text.json`, `final00001.png` |
| 캐릭터 정보 페이지 | Editor 빌드 성공, 페이지/베이스 WBP 0/0, 별도 페이지 자동 삽입·동일 인스턴스 재방문·실제 미리보기·풀 반환/재대여·닫기 입력 복구 확인. 기존 변경 알림으로 페이지와 헤더 동시 갱신 확인. 서버 접속 상태의 ID/지역과 실제 장비 변경은 미검증 | `Saved/UICharacterInfoPage/final00000.png`, `qa_results.json`, `final_assets.json`, `build.log` |
| 장비 페이지 디자인 | WBP 0/0, 9개 착용 슬롯·4×4 격자·22개 아이콘 참조와 설명 높이 확인. 베이스 크기의 임시 PIE 조합에서 최종 여백·텍스트·선택 표시 검수. 기능 바인딩 없음, 베이스/기존 정보 페이지/C++ 무변경 | `Saved/UICharacterEquipmentPage/equipment00002.png`, `asset_verification.json`, `visual_verification.json`, `final_verification.json` |
| 스탯 페이지 디자인 | WBP 0/0, 데이터 정의와 21행 대조, 4개 패널의 24 간격·아이콘 5개 확인. 임시 PIE 조합에서 글자 크기를 측정하여 잘림 수정 후 전체 텍스트 재확인. 기능 바인딩과 기존 WBP/C++ 변경 없음 | `Saved/UICharacterStatsPage/stats00001.png`, `visual_verification.json`, `asset_verification.json`, `final_verification.json` |
| 특성 페이지 디자인 | 최초 가로 15노드 배치는 후속 요청으로 원본 방사형 그물 구조로 교체. WBP 0/0, 252개 노드 좌표·512개 연결·9개 중앙 접합 대조. 임시 PIE 조합에서 텍스트 25개·아이콘 252개·24 간격·모든 노드의 영역 내 배치 확인. 상태는 디자인 샘플, 기능 바인딩과 원본 성장 트리/기타 WBP/C++ 변경 없음 | `Saved/UICharacterTraitsRadial/traits00000.png`, `visual_verification.json`, `asset_verification.json`, `final_verification.json` (이전 가로안: `Saved/UICharacterTraitsPage`) |
| 나이아 구성 페이지 디자인 | WBP 0/0, 실제 파티 4칸·보관함 6열/500 정원·스킬과 8개 기본 스탯 대조. 임시 PIE에서 텍스트 71개·초상화 16개·24 간격을 확인하고 아치/이름표 겹침 수정. 기능 바인딩과 기존 로스터/기타 WBP/C++ 변경 없음 | `Saved/UICharacterNaiaPage/naia00001.png`, `visual_verification.json`, `asset_verification.json`, `final_verification.json` |
| 메뉴 TileView 후속 변경 | Editor 빌드 성공, 변경 WBP 2개 0/0, 5+2 시각 확인, 6개로 축소·순서 반전 시 재배치와 타일 재사용 확인, 주요 메뉴와 옵션의 실제 버튼 델리게이트·닫기·입력 반환 확인 | `Saved/UIGameMenuTileView/five_columns00000.png`, `runtime_layout.json`, `qa_results.json`, `build.log` |
| main UI 병합 | Editor 최종 빌드 성공, 관련 BP 7개 오류·경고 0, 지도 포함 5+3 배치·M 토글·퀘스트/인벤토리 진입·입력 반환 확인. 새 PIE에서 패드 첫 포커스·방향키·A 선택·B 닫기·재열기 16단계 통과. main 단독 변경 761개 및 UI 단독 변경 127개 경로 보존 | `Saved/UIMergeIntegration/merged_menu00000.png`, `asset_validation.json`, `qa_results.json`, `git_validation.json`, `build_final.log` |

공통으로 실제 PIE 화면의 밝은/어두운 배경, 긴 한국어와 스크롤, 빈 목록·최초 표시·재열기 상태를 확인했다. UI 변경 후 임시 문구·조명은 복원하고 PIE를 종료했으며 원래 `L_LoginLevel`을 유지했다.

검증의 한계는 다음과 같다.

- 내부 핸들러·델리게이트 호출과 Enhanced Input 주입은 실제 데스크톱 마우스·키보드 테스트와 구분한다. 여러 작업에서 Orca 런타임 부재 또는 창 가림으로 물리 입력을 시험하지 않았다.
- 채팅 서버 송수신, 모바일 실기기 성능과 모든 화면비는 검증 범위 밖이다. 퀘스트 실제 수락·보상 지급도 수행하지 않았다.
- TileView 후속 검증에서 캐릭터 정보·스탯·스킬·인벤토리·나이아·퀘스트·옵션 진입을 확인했다. 성장 트리는 직업 미선택 로컬 캐릭터의 기존 안내 문구까지 확인했고, 직업 선택 이후 화면은 시험하지 않았다. Quit/Character Select는 델리게이트 연결만 확인했으며 실제 종료·전환은 실행하지 않았다.
- TileView 배치는 PIE 화면과 저장된 크기·간격으로 확인했다. Python의 캐시 Geometry 읽기는 0을 반환해 픽셀 좌표의 자동 검증 근거로 사용하지 않았다. 마이그레이션 중 제거된 위젯 GUID에 대한 UE handled ensure는 엔진이 정리했고, 후속 컴파일에서는 재발하지 않았다.
- main 병합 시 `WBP_QuestWidget`을 공통 조상과 에셋 텍스트·위젯 속성으로 비교했다. 기능상 변경은 `TrackerSizeBox.bOverride_HeightOverride=False`뿐이며 우리 디자인에도 이미 적용돼 있어 우리 에셋을 유지했다. `WorldLocation`은 main의 새 경로와 에셋을 사용한다. 지도 검증은 `L_TestLevel`에서 창 진입·M 토글·닫기 및 패드 경로를 확인했으며, 지도 이미지가 등록된 지역의 줌·팬이나 서버 연동 전체를 재검증한 것은 아니다.
- `Saved/UIChatSubtleGradient`의 이미지는 30% 증가 전이다. 최신 채팅 수치는 `Saved/UIChatOpacity30` 기록을 사용한다.
- 퀘스트 모션의 비동기 Python 진단·GC/정리 과정에서 PythonScriptPlugin 충돌이 두 번 있었다. 근본 원인은 확정되지 않았다. 재시작 후 비동기 콜백 없는 열기·닫기·입력 반환·PIE 종료 검사는 통과했다.

## 7. 유지보수 시 지켜야 할 연결

1. **네이티브 바인딩 보존:** WBP 이름·클래스·부모·슬롯, BindWidget 이름, 기존 EventGraph 연결을 작업 전후 비교한다. 시각 수정이 데이터 처리·공유 버튼으로 전파되지 않도록 전용 에셋을 사용한다.
2. **퀘스트 버튼 라벨:** `ApplyStateButtonText`와 `SetTrackButtonLabel`은 `UButton.GetContent()`를 `UTextBlock`으로 읽는다. 상태 버튼 3개와 TrackButton의 라벨은 직접 자식이어야 하며 장식은 버튼 바깥에 둔다.
3. **슬롯과 Hit Test:** Overlay로 ScrollBox를 옮길 때 가로·세로 Fill을 명시한다. 장식은 HitTestInvisible, 상세 컨테이너는 네이티브와 일치하는 SelfHitTestInvisible을 유지한다. 전환용 입력 shield만 의도적으로 입력을 받는다.
4. **실제 델리게이트:** 확인한 Revvy 빌드의 `WidgetService.bind_event`는 기록만 하는 stub이었다. 카테고리 rail은 `BlueprintService.add_delegate_bind_node`와 custom event로 바인딩했다. API 응답만 보고 클릭 연결을 완료로 판단하지 않는다.
5. **위젯 재배치:** 이미 자식이 있는 SizeBox에 다른 자식을 바로 추가하거나 UserWidget을 분리한 상태에서 구조 컴파일하지 않는다. 임시 부모 아래 새 컨테이너를 만든 뒤 기존 위젯을 옮기고 구조를 확인한다. `set_property`가 중간 저장할 수 있으므로 전체를 원자적 트랜잭션으로 가정하지 않는다.
6. **에디터 복구:** 실패 후 무조건 트랜잭션을 취소·재실행하지 않는다. 이름이 남아도 invalid UObject일 수 있어 유효성을 확인한다. 2026-09-10의 rename 도구는 WidgetVariableNameToGuidMap 갱신 문제를 보였으므로 이름 변경 후 컴파일과 바인딩을 확인한다. 삭제·재생성은 참조를 보존하는 검토된 복구 방법으로만 사용한다.
7. **수치 확인:** 일부 Material setter가 false를 반환해도 실제 값이 바뀐 사례가 있다. 별도 readback으로 검증한다. `get_cached_geometry()`가 0일 때는 정상 좌표로 간주하지 말고 유효한 런타임 크기·최종 화면으로 확인한다.
8. **초상화 수명주기:** 메뉴 프로필 갱신, DrawAs Image와 미리보기 액터 반환을 함께 유지한다. 닫기 경로별 입력 소유권 복구도 확인한다.
9. **일회성 스크립트:** `Tools/UIHUDPolish`의 inspect/stage/import/apply/refine 계열은 당시 스냅샷·에셋 생성·구조 변경을 포함할 수 있다. 완성된 계층에 일괄 재실행하지 않는다. 향후 단순 농도 조정은 저장된 전용 MI를 수정한다.
10. **비동기 진단:** `quest_motion_qa.py`, `quest_motion_visual.py`는 충돌 이력이 있는 진단 기록이다. 정기 검증에 그대로 재사용하지 않으며 가능하면 콜백 없는 독립 조회와 수명주기 검사를 사용한다.

## 8. 보류 사항과 후속 작업

| 항목 | 남은 판단 / 작업 |
|---|---|
| SP 초기화 | 2026-09-10 기록에서 dongh의 공용 `BPGE_InitializeVitals`에 SP/MaxSP 모디파이어를 넣는 작업을 사용자 요청으로 보류. 기본 데이터 50/50과 SetByCaller 태그만으로 실제 초기화가 완성된 것은 아님 |
| SP·방어 기획 | 당시 MaxSP의 STR+INT 파생, 본인 턴 종료 리필, 기본 공격 SP 전량 소모·반복, LUK 기반 패링 범위, 패링 불가 구분, DEX 회피/INT 실드 및 높은 스탯 자동 선택이 후속 백엔드 과제였음. 현재 구현을 재조사한 뒤 UI 연결 |
| HP/Mana 바 | 당시 C++ HealthBar/ManaBar와 Percent 배선은 작성했으나 전체 빌드·WBP Overlay 연결·PIE 확인 미완료 기록. 후속 완료 여부 재확인 필요 |
| 캐릭터 위 바 | 당시 `ANUBattleAvatar.VitalBarWidget` → `UNUCombatantVitalWidget.SetVitals` 경로와 WBP_CombatantVital은 존재했으나 테스트 아바타 3종 WidgetClass가 None. 화면 공간 바와 HUD 상태 목록 병행 여부는 디자인 결정 대기였음 |
| 전투 확장 | 데미지 팝업 Fade, 결과 배너 연출, 구 시스템의 Capture 재도입 여부, BattleCommand의 BP 유지/C++ 기반 여부 |
| 통합 대형 메뉴 | 공통 베이스 및 캐릭터 정보 페이지 연결 완료. 장비·스탯·특성·나이아 구성 페이지는 WBP 디자인 완료. 특성은 원본 방사형 성장 트리, 나이아는 파티 4칸/보관함 6열 구성. 데이터·조작·NamedSlot 등록과 기능·기존 메뉴/P키 진입점 통합은 후속 작업 |
| 퀘스트 선택 연출 | 콘텐츠 Cross Fade·카테고리 Indicator 이동 등은 제안 단계. 현재 On/Off 모션과 구분하여 필요 시 작업 |
| 추가 QA | 실제 포인터/키보드, 서버 채팅, 모바일·여러 화면비, 최신 채팅 농도의 시각 재확인 |

기존 아키텍처 문서의 ‘필드 HUD 아이콘 4개가 없음/PIE 미확인’ 기록은 2026-09-18의 6종 아이콘·버튼 적용과 검증으로 대체됐다. 그 외 전투 보류 사항을 완료된 것으로 추정하지 않는다. 공용 부품·ViewModel·CommonUI 도입도 실제 필요에 맞춰 별도로 결정한다.

## 9. 통합한 기록과 변경 이력

기존 26개 문서의 출처와 현재 해석을 아래에 남긴다. 중간 디자인의 ‘최종’이라는 표현은 당시 작업의 최종을 뜻하며 현재 화면 기준은 4절이다. 세부 원문 사본은 로컬 `Saved/UIProjectDocumentation_20260921/originals`에 보존했다.

| 기존 문서 | 통합 내용 / 현재 해석 |
|---|---|
| `UI_STRUCTURE_ANALYSIS.md` | 2·8절. 2026-09-10 아키텍처·전투 진행 기록, 이후 필드 아이콘 완료 반영 |
| `UI_ToneAndManner_Guide.md` | 3절. 중복 규칙 정리, v4 표면·Floating 구성과 화면별 최신 시안 우선 적용 |
| `UI_FieldHUD_Polish_20260917.md` | 4.1·6절. 최초 크기·여백·긴 한글·조작 검증 |
| `UI_FieldHUD_Textures_20260917.md` | 5·6절. 초기 10종 중 현재 Control/QuestMarker 원본 보존, 아이콘·바탕은 후속 버전으로 교체 |
| `UI_ChatRoot_Textures_20260917.md` | 4.2·5절. 전용 탭/채널 클래스와 아이콘 매핑 유지 |
| `UI_ChatRoot_ToneV2_20260917.md` | 더 밝은 표면을 탐색한 이전 단계. 연결형 채팅과 최신 MI 설정으로 대체. 근거 `Saved/UIChatToneV2` |
| `UI_Texture_Relocation_Analysis_20260917.md` | 5절. 당시 19종 이동 제안은 분석 기록이며 현재 경로와 구분. 근거 `Saved/UITextureRelocationAnalysis` |
| `UI_Quest_Tone_20260917.md` | 초기 카드형 저널·트래커·의뢰 정리. 메인 화면은 후속 시안으로 대체. 근거 `Saved/UIQuestTone` |
| `UI_NaiaFolio_ChatQuest_20260917.md` | 기록서·모티브 및 통합 메뉴 설계 이력. Journal/Motif 참조 원본 보존. 근거 `Saved/UINewLanguage` |
| `UI_ModernRecord_ChatQuest_20260917.md` | 회색/아이보리 면을 탐색한 중간 단계로 현재 디자인 아님. 근거 `Saved/UIModernRecord` |
| `UI_Quest_ReferenceV4_20260917.md` | 4.1·5절. 종이·나침반·깃펜 등 5종, 트래커·의뢰 이력. 메인 배치는 후속 교체. 근거 `Saved/UIQuestV4` |
| `UI_Quest_JoinedLayout_20260917.md` | 이후 거부된 큰 사각 바탕 버전. 현재 적용 기준 아님. 근거 `Saved/UIQuestJoined` |
| `UI_Quest_BookmarkTabs_20260918.md` | 하단선 없는 책갈피 중간안. 현재 금색 밑줄·rail로 대체. 근거 `Saved/UIQuestBookmark` |
| `UI_Quest_MistJournal_20260918.md` | 전체 화면 블러·열린 배치의 출발점. 번지는 면은 FrameV3로 교체. 근거 `Saved/UIQuestMist` |
| `UI_Chat_ConceptV4_20260918.md` | 연결형 채팅 이전 버전. 근거 `Saved/UIChatConceptV4` |
| `UI_Chat_Connected_20260918.md` | 4.2·6·7절. 현재 연결 프레임·탭 구획·구조 수정 주의사항 |
| `UI_FieldHUD_IconsV2_20260918.md` | 4.1·5·6절. 현재 아이콘 6종과 표시 크기 |
| `UI_FieldHUD_LightBlue_20260918.md` | 4.1·5·7절. 연파랑 바탕과 독립 readback 검증 |
| `UI_FieldHUD_Gradient_20260918.md` | 4.1·4.2절. 추적기/지역 그라데이션 유지, 채팅 값은 최신 조정으로 대체 |
| `UI_Chat_Translucent_20260918.md` | 4.2절. 전용 버튼 마스터·중복 바탕 제거·보조 컨트롤 값 유지, 주요 알파는 최신값 사용 |
| `UI_Quest_Polish_20260921.md` | 4.3·7절. 명조 제목·목록 계층·Control 텍스처·라벨 직접 자식 제약. 번지는 패널 해석은 후속 수정. 근거 `Saved/UIQuestPolish` |
| `UI_Quest_FramedPanels_20260921.md` | 3.1·4.3절. 현재 선명한 Navy/Ivory 면과 금색 경계 |
| `UI_Quest_CategoryRail_20260921.md` | 4.3절. 양쪽 5종 카테고리 동기화 |
| `UI_Quest_Motion_20260921.md` | 4.4·6·7절. 0.38초 열기/0.20초 닫기와 진단 한계 |
| `UI_Chat_SubtleGradient_20260921.md` | 4.2·6절. 미세 그라데이션 및 최종 불투명도 +30% |
| `UI_GameMenu_Hub_20260921.md` | 4.5·6절. 현재 메뉴 허브와 실제 프로필 연결 |

2026-09-21 저장소 정리에서는 Python 캐시 제외 규칙을 추가했고, `.mcp.json`은 **팀 공용 추적 파일로 유지**하도록 추적 해제를 취소했다. `.mcp.json`에 ignore·skip-worktree·assume-unchanged를 적용하지 않는다. 로컬 변경은 정상적인 Git 수정사항으로 표시되므로 UI 커밋에 넣지 않을 때는 파일별로 스테이징하며, 팀 변경과 겹치면 일반 Git 병합 절차로 처리한다. 이 문서 통합은 UI 에셋·C++ 코드 변경이나 새로운 기능 검증을 포함하지 않는다.


### Hub profile/copy cleanup (2026-10-07)
Final apply_depth.py removes the header/book slogans using CleanHeaderProfile and CleanDesk source siblings, makes the top-right profile almost rectangular and slightly larger, remaps live fields, and raises the editable motto to design y214. Native greeting persistence is unchanged. Sources/prompts: ArtSource/UI/HubSidePanels/COPY_CLEANUP.md; runtime evidence: Saved/UIHubCopyCleanup.
