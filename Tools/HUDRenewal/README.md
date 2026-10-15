# 인게임 HUD 리뉴얼 (글래스 엣지)

시안: `Docs/HUD_Renewal_Proposal_20261014/hud_proposal.html` 의 A안. 생성·선택 화면과 같은 남색 유리 판, 모서리 역할 고정
(왼위 나·파티, 오른위 지도·지역·퀘스트, 왼아래 채팅, 오른아래 메뉴), 2px 격자 아이콘.

## 원칙

- 새 위젯은 전부 `/Game/Art/UI/HUD2026` 에 새로 만든다. 부모 C++ 클래스는 예전 WBP와 **같은 클래스**라 로직은 그대로이고,
  모양만 새로 짠다. 예전 WBP(`/Game/Developers/dongh/UI/...` 등)는 손대지 않는다.
- 게임이 새 위젯을 쓰게 하는 것은 클래스 지정뿐이다: `BP_NUPlayerController` 기본값 10곳(+ 옛 대상 인디케이터 끔 1곳) + `Config/DefaultGame.ini` 의
  `[/Script/ProjectM.NUUISettings]` 키 6개(WorldLocationWidgetClass, NoticeBannerClass, NameplateClass, BattleAllyTagClass,
  BattleEnemyTagClass, bEnableLocationEntryNotice=False --- 새 지역 이름 위젯이 진입 배너를 띄우므로 '○○에 진입했습니다' 알림은 끈다). `run.ps1 -Steps revert` 가 원래 값으로 돌린다(원래 값은 `Saved/HUDRenewal/classes_original.json`,
  백업 `Saved/HUDRenewal/Backup`).
- C++ 에 넣은 것은 전부 선택(BindWidgetOptional) 위젯·기본 꺼짐 기능이라 예전 WBP는 그대로 동작한다.
  고친 원본 파일 사본은 `Saved/HUDRenewal/Backup/Source`.
- WBP 는 스크립트가 매번 지우고 새로 만든다. 에디터에서 손으로 고친 것은 다음 실행 때 사라지므로 수정은 `apply_*.py` 에 한다.

## 실행

```
.\run.ps1 -Steps kit,fx,field,battle,apply     # 그림 가져오기 → 머티리얼 → 필드 위젯 → 전투 위젯 → 클래스 지정
.\run.ps1 -Steps revert                         # 클래스 지정만 원래대로 (새 에셋은 남는다)
.\wait_free.ps1 -Then build,battle              # ProjectM 에디터가 다 닫힐 때까지 기다렸다가 실행 (다른 세션과 겹치지 않게)
```

- `kit`: `make_hud_kit.py` 가 만든 `ArtSource/UI/HUD2026/kit/*.png`(그림자, 스크림, 바, 표시, 2px 아이콘)를 가져온다.
  그림을 고치면 `python make_hud_kit.py` 후 `kit`.
- C++ 빌드와 플레이 캡처는 생성 화면 파이프라인을 쓴다: `..\CharacterCreateGlass\run.ps1 -Steps build` /
  `-Steps shot -States .\Tools\HUDRenewal\qa\states_field.json -Map /Game/Libregrove/Level_V8/Maps/L_Libregrove`.
  전투 캡처는 **전투 배경 존이 있는 맵**에서 해야 한다(`-Map /Game/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance`,
  `qa/states_battle.json`). 리베르그로브 마을에서 `nu.Battle.Fight` 를 쓰면 배경 없이 시험 인카운터 기본 배치(아군 오른쪽)로 뜬다.
- `qa/layout_overlay.py`: 전투 진형 축마다 4명씩 섰을 때 머리 위 표시 자리를 스크린샷에 그려 겹침을 센다
  (`qa_layout.py` 가 PIE에서 `Saved/HUDRenewal/battle_layout.json` 을 쓴 뒤).

## 화면별 메모

- 필드 HUD(`WBP_HUD_Field`): 왼위 상태 판, 오른위 미니맵(252), 오른아래 메뉴 독 + 눈(HUD 숨김) 버튼. 전투 중에는 부모가 접는다.
- 퀘스트 추적·지역 이름 줄은 필드 HUD 밖 위젯이라 `FNUHudBattleFold`(UI/HUD/NU_HudBattleFold) 로 전투 동안 접힌다.
- 전투 명령 판(`WBP_HUD_BattleCommand`, C++ `UNU_BattleCommandWidget`): 행동하는 캐릭터 몸 그림 한가운데 옆에 뜬다
  (오른쪽 먼저, 화면이 모자라면 왼쪽). 금색 점·선으로 캐릭터와 잇고, 열릴 때 캐릭터 쪽에서 커지며 줄이 차례로 들어온다.
  대상 고르기: 대상 머리 위 표시 위에 금색 커서(위아래로 흔들림), 발밑에 숨쉬는 금색 고리가 뜨고, 대상을 바꾸면
  커서·고리가 미끄러져 가며 한 번 튄다. 고른 대상의 표시는 금색 테두리 + 조금 커지고(`SetTargeted`), 같은 편 다른 후보 표시는
  흐려진다(`SetDimmed`). 팀의 옛 대상 인디케이터 액터는 겹치지 않게 컨트롤러의 TargetIndicatorClass 를 비워 끈다(revert 로 복원).
  개발용 `nu.UI.MotionScale 0.1` 이면 등장 연출이 열 배 느려진다(프레임 확인용).
- 머리 위 표시: 아군 124x34(HP 숫자 + HP·SP 바), 적 116x30(이름·레벨 + 바). 4인 파티 + 나이아 4 + 적 4 배치에서 서로 안 겹침
  (축 간격 150cm). 전투 카메라는 4명 줄도 화면에 들어오게 화각을 넓힌다(`NUFieldDotZoom` 맞춤 화각, `nu.Cam.BattleFit*`).
- 개발용 콘솔(Shipping 제외, `UI/HUD/NU_HudDebug.cpp`): `nu.HUD.Notice`, `nu.HUD.PartyInvite`, `nu.HUD.ChatSample`,
  `nu.HUD.ChatOpen`, `nu.HUD.Location`. 서버 없이 실행하면 채팅·파티 창이 안 만들어지므로 미리보기용 한 장을 띄운다.

## 아이콘 (2026-10-14 v2, 듀오톤)

- 원본은 벡터 `hud_icons.py`(24 격자, 17개). 스타일 셋: line / duo(추천·적용) / medal. `make_hud_kit.py` 가 Chromium으로
  52px(독 26px의 2배) PNG를 뽑아 `T_HUD_Icon_*` 를 덮어쓴다(밉 생성, 필터 기본). 모양을 고치면 `make_hud_kit.py` → `run.ps1 -Steps kit`.
- 옛 2px 픽셀 아이콘(`make_icons.py`)은 더 쓰지 않는다. 비교 시안: `Docs/HUD_Refine_Proposal_20261014/hud_refine.html`(`build_refine.py`).
- **함정**: UE 5.8 에서 ROUNDED_BOX 브러시의 외곽선은 위젯 렌더 불투명도를 무시한다 --- 불투명도로 페이드하는 표시(독 호버 테두리 등)에
  외곽선 있는 rbox를 쓰면 채움만 사라지고 금색 선이 늘 남는다. 페이드하는 테두리는 9분할 텍스처(`T_HUD_HoverFrame`)로 그린다.
- 독 이름표의 단축키 칸은 `UNU_HudButtonWidget::ShortcutAction` 에 지금 묶인 키보드 키를 읽어 보여 준다(키 설정 변경을 따라감).
- 미니맵이 어두웠던 원인: `Img_Map` 자리표시 브러시의 색조(#2B4636)가 `SetBrushFromTexture` 뒤에도 남아 지도 그림에 곱해졌다 --- 흰 브러시로 바꿈.

## 퀘스트 추적 · 퀘스트 띠 · 미니맵 마커 (2026-10-14 v3)

시안: `Docs/HUD_QuestMap_Proposal_20261014/quest_map.html` (`build_questmap.py`, 실제 화면·킷 그림·마을 지도로 만든 한 파일).

- 추적 창(`WBP_HUD_Quest` 의 TrackerPanel): 판 없는 옅은 그늘 위 글자라 밝은 지붕 위에서 "퀘스트" 제목·▲× 버튼이 안 보였다 →
  미니맵과 같은 유리 판(300 폭). 제목 줄 = 퀘스트 아이콘 + 15px 상아, 26px 버튼. 줄(`WBP_HUD_QuestEntry`)은 보고 가능이면
  금 제목 + "보고" 칩 + 금빛 바탕(C++ `UNUQuestTrackerEntryWidget`: `EntryTurnInBadge`/`EntryReadyBack`/`EntryFlash`/`EntryDiamond`,
  `bApplyReadyStyle`). 수락한 줄은 오른쪽에서 들어오며 한 번 빛나고(`PlayIntro`), 보고 가능으로 바뀌면 빛나고 칩이 튄다(`PlayReadyFlash`).
- 퀘스트 띠(`FNUQuestFx`, UI/HUD/NU_QuestFx): 화면 위 32% 자리에 수락("새 의뢰"·첫 목표) / 목표 달성("목표 달성"·보고 대상, 청록) /
  완료("의뢰 완료"·받은 보상, 도장 + 빛살 + 고리 + 반짝이 8). 조각은 `WBP_HUD_Quest` 안 `QuestFx*` 이름. 차례로 띄우고 전투 화면이
  떠 있는 동안 생긴 것은 끝난 뒤에. 완료 때 보상 창은 띠가 뜬 뒤 `RewardPopupDelay`(1.3초) 뒤에 뜬다(띠 조각 없는 옛 WBP는 바로).
  미리보기: `nu.HUD.QuestFx <accept|ready|done> [제목]`.
  지역 진입 배너와 겹치지 않게: 줄에 들어온 띠는 0.3초 지켜본 뒤, 진입 배너가 떠 있으면 끝날 때까지 기다린다(도착 목표는 둘이 같은 순간에 생김).
  자리도 화면 위 32%(진입 배너 아래). 보상 창은 완료 띠가 실제로 뜬 뒤부터 1.3초를 센다.
- 지역 줄: 장소 이름이 없으면 "·"·"—" 를 숨긴다(`UNUWorldLocationWidget`, `LocDot` 이름이 있을 때만).
- 미니맵 내 위치(`WBP_HUD_MapView` 의 Marker_Player): 부채꼴 + 그늘 + 고리 + 두 면 화살표를 통째로 이동 방향으로 돌린다
  (`UNU_MapImageViewWidget::bRotatePlayerMarker`, 속도 20cm/s 넘을 때 방향 갱신, 처음엔 캐릭터 Facing). 지도 오른쪽 = -X, 위 = +Y.
- 퀘스트 표시(`WBP_HUD_MapMarker`): 태그별 마름모 배지 26px (`T_HUD_Badge_*`: Offer 금!, TurnIn 밝은 금?, Target 청록 빈 마름모,
  InProgress 회청 …, Locked 남색 회색!, Portal 청록 문). TurnIn·Target 은 고리(`PulseTags`). 미니맵 밖이면 `EdgeTags`(TurnIn·Target·Offer)는
  테두리 안 26px에 붙고 꼭지(`MarkerEdgePointer`)가 실제 방향을 가리킨다(`EdgeInset`). 옛 `T_HUD_Marker_*` 그림은 남겨 둠.

## 상호작용 키 · NPC 머리 위 퀘스트 표시 (2026-10-14 v4)

- 상호작용 안내(`WBP_HUD_Interact`): "상호작용" 글자를 빼고 키만. 남색 테두리 안 금빛 키캡(36px) + 키 위로 지나가는 반짝임
  (`MI_HUD_KeyShine`, `M_HUD_Shine` 키 크기 인스턴스) + 키에서 퍼지는 상아 고리 두 겹(`M_HUD_KeyRing`, 고리 바로 바깥에 남색 그늘이
  붙어 밝은 잔디에서도 보임) + 아래 남색 그늘·금빛 무리. 고리 판 때문에 위젯 자리는 108x108 고정. 나타날 때 톡 튀어나오는 것은
  C++(`NU_InteractPromptWidget` PopMotion) 그대로. 키 글자는 입력 설정에서 읽는다.
- NPC 머리 위 퀘스트 표시: 시안 A(미니맵과 같은 마름모 배지)를 더 크고 또렷하게 적용(`Docs/HUD_NpcMarker_Proposal_20261014/npc_marker.html`).
  - NPC BP 39곳이 모두 zxcv1 `WBP_WorldMarker`를 가리키므로 BP는 그대로 두고 UI 설정 한 줄로 바꾼다:
    `[/Script/ProjectM.NUUISettings]` `HeadMarkerClass`(= `WBP_HUD_HeadMarker`), `HeadMarkerMaterial`(= `M_HUD_WorldMarker`),
    `HeadMarkerDrawSize`(160px), `HeadMarkerWorldSize`(80cm), `HeadMarkerPivot`(0.5, 0.6) --- `UNUWorldMarkerComponent::ApplyHeadMarkerSettings`.
    비우면 BP에 적힌 옛 표시 그대로. `run.ps1 -Steps apply`가 넣고 `revert`가 뺀다.
  - 그림 `T_HUD_Head_{Offer,TurnIn,Target,Locked}`(128px, `make_hud_kit.py head_badge`): 굵은 남색 테두리 + 안쪽 상아 선 + 큰 글자.
    받을 퀘스트 금 "!", 보고 밝은 금 "?", 목표 청록, 조건 미달 남색. 진행 중은 퀘스트 판정이 표시를 안 띄운다(그대로).
  - 재질 `M_HUD_WorldMarker`: zxcv1 `M_WorldMarker`(카메라를 향함·깊이 가림) 사본을 마스크드 대신 반투명(AlphaComposite)으로 ---
    위젯 렌더 타깃이 미리 곱한 색이라 이 섞기가 맞다. 가장자리가 매끈하고 그늘이 부드럽다. 빛 안 받는 색이 맵 노출(마을 약 0.5배)을
    받아 칙칙하던 것은 `EyeAdaptationInverse`로 되돌림(캐릭터 재질과 같은 방식), 밝기는 재질 `Brightness`.
  - 위젯 `WBP_HUD_HeadMarker`: 어두운 그늘 + 퍼지는 고리(`MI_HUD_HeadRing`, 받을 퀘스트에도 켬 `bPulseOnOffer`) + 배지 124px.
    통째로 천천히 떠다니고(2.4초, 5px) 표시가 생기거나 바뀌면 톡 튀어나온다(`UNUWorldMarkerWidget` `MarkerFloat`, 0.4초).
    지도 마커는 `MarkerFloat`가 없어 그대로. 상호작용 F 키는 표시 꼭대기 위에 자동으로 붙는다(플레이어 컨트롤러 계산 그대로).
  - 확인: 리베르그로브 PIE 원경·근접(F 함께) `Saved/CharacterCreateGlass/hm_idle/hm_near/hm_near2_view.png`. 미확인: 실제 서버 퀘스트
    상태 변화 때 튀어나오기, 신데하스 NPC(BP 크기 36이었으나 설정이 덮음), 건물 뒤 가림의 반투명 정렬.

## 상호작용 키 자리 · NPC 크기·밝기 (2026-10-14 v5)

- 상호작용 키: 시안 A(`Docs/HUD_Interact_Proposal_20261014/interact_prompt.html`) 적용. F 키 칩이 머리 위 배지 위에 쌓이지 않고
  마름모 오른쪽 아래 모서리에 붙어 배지 앞(화면 UI)에 그려진다 --- `ANUPlayerController::InteractionIndicatorMarkerAnchor`(판 가운데에서
  오른쪽 0.27·아래 0.20). 배지 없는 NPC·물건은 전처럼 대상 위. 색은 상아(퀘스트는 금), 고리·빛 대신 1.6초마다 키가 눌렸다 올라온다
  (`UNU_InteractPromptWidget` KeyCap·KeyLip, PressPeriod/PressDepth).
- 머리 위 표시 높이: `HeadMarkerGap`(10cm) --- NPC 스프라이트 꼭대기 위 10cm 에 배지 아래 꼭지(`HeadMarkerPivot` 0.82)를 둔다
  (`UNUWorldMarkerComponent::PlaceAboveOwnerSprite`, 0.5초마다 다시 잼). NPC BP 마다 다른 높이(90~120)·크기 보정과 상관없이 머리 바로 위.
- NPC 크기·밝기: 프로젝트 세팅 NU NPC Visual(`UNUNPCVisualSettings`, DefaultGame.ini). NPC 그림은 플레이어와 같은 도트 크기(1.04cm)·
  비슷한 키(87~96 대 94 도트)지만 평균 밝기가 0.15~0.37(플레이어 0.41)로 어둡다 → NPC 별 ToneGamma/ToneGain 으로 ~0.33 에 맞춤
  (캐릭터 재질 `M_NUCharacterSprite` 에 ToneGamma·ToneGain 추가, 기본 1 = 그대로, `apply_char_tone.py`). 드워프(4017)는 0.9배.
  표는 `apply_npc_visual.py`(--revert), 그림 측정은 `export_npc_art.py`, 게임 안 측정은 `qa/qa_npc_survey.py`.
- 멀리 선 NPC 도트 깨짐: 원근 때문에 도트 한 칸이 1.2~1.6 화면 픽셀(플레이어는 2)이라 칸 폭이 들쭉날쭉. 재질에 있던 칸 가장자리 섞기
  (`NUA_PixelAA`)가 0.7 로 반만 켜져 있어 NPC 는 1.0(완전)으로 --- 정수 배율에서는 원래 도트 그대로.

## NPC 대화창 시안 (2026-10-14, 제안만)

`Docs/HUD_Dialogue_Proposal_20261014/dialogue.html` --- 지금 대화창(실제 캡처)과 시안 3종: A 글래스 하단 창 + 이름 탭 + 도트 흉상(추천),
B 머리 위 말풍선, C 판 없는 자막형. 에셋·코드는 그대로. 그림은 `build_dialogue.py` 가 HUD 없이 찍은 실제 화면(`dlg_plate`) 위에 그린다
(캡처: `qa/states_dialogue.json` 지금 대화창, `qa/states_dialogue_plate.json` 배경; 대화 열기 `qa/qa_open_dialogue.py`, 다음 줄 `qa/qa_dialogue_next.py`).
초상은 NPC 대기 스프라이트 머리·어깨를 3배 정수 배율로 키운 것. 대화 넘김 키는 NUUISettings DialogueAdvanceKeys(F·Enter·Space·패드 A).

## NPC 대화창 --- 자막형 적용 (2026-10-14 v6)

사용자가 시안 C(판 없는 자막형)를 골랐다. 투명도는 가시성을 확보하는 선에서 맞춤. `run.ps1 -Steps dialogue,apply`(되돌리기 `-Steps revert`).

- 새 위젯 `Field/WBP_HUD_Dialogue`(부모 `UNUDialogueWidget`, 옛 zxcv1 WBP_DialogueWidget 과 같은 C++ --- 대화 로직 그대로)와
  선택지 줄 `Field/WBP_HUD_DialogueOption`(부모 `UNUInteractionOptionEntryWidget`). 연결은 DefaultGame.ini `DialogueWidgetClass` 한 줄
  (`apply_classes.py`, 옛 값은 `Saved/HUDRenewal/classes_original.json`). 원본 `apply_dialogue.py`.
- 화면 아래 340px 를 위는 투명·이름 줄 ~0.6·대사 줄 ~0.85 로 어둡게(`T_HUD_DialogueScrim`, `make_hud_kit.py` vgrad). 대사 상아 25px
  + 진한 그림자, 이름 금 26px + 마름모 + 금 줄. 실제 화면에서 잰 대비: 대사 뒤 최저 11.5:1, 이름 뒤 최저 8.1:1(상아 기준).
- 대사 칸 전체가 다음 버튼(NextButton), 오른쪽 아래 금 꺾쇠 + F 키는 문장이 다 나오면 천천히 3px 움직인다(`NextIndicator`).
- 선택지: 가운데 오른쪽 위 유리 줄. 퀘스트 줄은 "[진행 가능]" 머리말 대신 머리 위 표시·미니맵과 같은 마름모 배지(`GroupIcons`),
  대화·상점 등은 작은 마름모, 나가기는 빈 마름모(`SystemGroupIcon` --- System 그룹은 숨김 enum 이라 표 키로 못 씀). 고른 줄은 금 테 + 금 막대.
- 대화 중에는 필드 HUD(상태·지도·퀘스트 추적·지역 이름·채팅·하단 메뉴)를 접는다 --- 대화 창을 열 때 `bHideFieldHUD`
  (위젯 `bHideFieldHUDWhileOpen`). 열릴 때 12px 떠오르며 0.2초(`bPlayOpenMotion`), 대사 없이 선택지만 열면 자막 띠 접음.
- C++ 추가는 모두 선택 사항(기본값이면 옛 WBP 그대로): `UNUDialogueWidget` SpeakerNameRow(나레이션이면 접힘)·NextIndicator·
  bHideFieldHUDWhileOpen·bPlayOpenMotion·bCollapseDialoguePanelWithoutLines, `UNUInteractionOptionEntryWidget` bShowCategoryPrefix·
  GroupIcons·DefaultGroupIcon·SystemGroupIcon·GroupIcon·FocusHighlight.
- 확인: 리베르그로브 엘라던 --- 질문+선택지, 대화하기 선택 뒤 일반 대사·넘김 표시 흔들림, 닫은 뒤 HUD 복귀 (`qa/states_dialogue_c.json`,
  `states_dialogue_c2.json`, `states_dialogue_bob.json`). 미확인: 나레이션 줄(이름 없음), 선택지 6개 넘는 스크롤, 패드 입력, 다른 화면비.

## 상점창 시안 (2026-10-14, 제안만)

`Docs/HUD_Shop_Proposal_20261014/shop.html` --- 리베르그로브 상점 NPC 시안(BP_NPC_Q3003_HiddenChild_02, NpcId 4002)과 대화 →
"상품 보기"로 연 실제 화면(dbals WBP_ShopWindow + 옛 소지품 창)과 민기 님 디자인 페이지(MinkiCho WBP_ShopPage, nu.Shop.Preview)를 찍고,
시안 3종: A 상아 상점 페이지(민기 님 디자인을 다듬어 실제 연결, 판매도 같은 창 --- 추천) / B 남색 유리 사이드 시트 / C 대화 이어지는 진열대.
그림 `build_shop.py`(아이콘·상인 얼굴은 실제 캡처·스프라이트에서), 캡처 `qa/states_shop.json`(qa_shop_go·talk·pick·preview).
에셋·코드 무변경. A 로 가면 민기 님 WBP_ShopPage 작업과 범위 조율 필요.

## 상점 --- 유리 사이드 시트 적용 (2026-10-14 v7)

사용자가 상점 시안 B(유리 사이드 시트)를 골랐다. `run.ps1 -Steps shop,apply`(되돌리기 `-Steps revert` --- NUShopSettings ShopWindowClass 도 복원).

- 새 위젯 `Field/WBP_HUD_Shop`(부모 `UNUShopWindowWidget`, dbals WBP_ShopWindow 와 같은 C++), 줄 `WBP_HUD_ShopEntry`(`UNUShopEntryWidget`),
  줄 전체를 덮는 투명 버튼 `WBP_HUD_ShopRowButton`(`UNU_CommonButtonWidget`). 원본 `apply_shop.py`. 연결은 DefaultGame.ini
  `[/Script/ProjectM.NUShopSettings] ShopWindowClass` 한 줄(`apply_classes.py` SHOP_INI, 옛 값은 classes_original.json `ini_shop`).
- 화면 오른쪽 40px 안쪽 남색 유리 시트: 상점 이름(금)·소지금·닫기, 구매/판매 탭(금 밑줄), 목록(아이콘·이름·보유 수 또는 설명·가격,
  레벨 잠김은 자물쇠 칩), 고른 품목 칸(아이콘·이름·보유/재고·설명·수량 -/+/최대·합계·금색 구매/판매 버튼 + F). 판매도 같은 시트 안에서 ---
  옛 소지품 창을 같이 띄우지 않는다. 열려 있는 동안 필드 HUD 접힘·캐릭터 이동 막음(대화창과 같은 흐름).
- C++ 추가(모두 선택, 기본값이면 dbals WBP_ShopWindow 그대로): `UNUShopWindowWidget` bOpenInventoryWithShop·bHideFieldHUDWhileOpen·
  bBlockGameInputWhileOpen + 고른 품목 칸 바인딩(DetailPanel·DetailIcon·DetailNameText·DetailDescText·DetailInfoText·QuantityText·
  QuantityMinus/Plus/MaxButton·TotalText·ConfirmButton·ConfirmLabel) --- DetailPanel 이 있으면 줄 버튼은 고르기만 하고 수량·실행은 칸에서
  (나누기 창 없이, 좌우 키 = 수량, F·패드 X = 실행). `UNUShopEntryWidget` SelectedHighlight·SubText·bPriceNumberOnly.
  `UNUInventoryComponent::OpenShopUI` 가 창 클래스 기본값을 읽고, `ToggleSheetWidget` 에 bHideFieldHUD·bBlockGameInput(기본 false) 추가.
- 확인(리베르그로브 시안 NPC → "상품 보기"): 시트·HUD 접힘·옛 소지품 창 없음, 수량 3 → 구매 120골드(잔액 9,880, 보유 5→8, 수량 1로 돌아감),
  판매 탭(되팔기 목록·개당 가격·판매하기). `qa/states_shop2.json`. 미확인: 실제 키보드·패드 입력, 서버 온라인 거래, 다른 화면비, 상인 대사 자막(시안의 왼쪽 아래 자막은 미구현).
- 아이템 아이콘 90종은 같은 날 HD-2D 톤으로 다시 그림 --- `Tools/ItemIconsHD2D/README.md`.

## 직업 선택 · 시작 나이아 선택 · 의뢰 팝업 시안 (2026-10-15, 제안)

`Docs/HUD_Selection_Proposal_20261015/selection.html` (`build_selection.py`) --- 세 화면의 지금 캡처(`qa/states_selection*.json`)와 시안
직업 A 무대 / B 카드 / C 목록+상세, 나이아 A 무대 / B 카드 / C 넘기기, 의뢰 A 대화 옆 의뢰서 / B 대화 안 의뢰서. 사용자가 전부 A(추천)를 골랐다.

## 직업 선택 · 시작 나이아 선택 무대 · 의뢰 팝업 그림 보상 (2026-10-15 v8)

`run.ps1 -Steps kit,select,quest,apply` (되돌리기 `-Steps revert` --- JobSelectionWidgetClass 복원, StarterNaiaSelectionWidgetClass 줄 삭제).

- 새 위젯 `/Game/Art/UI/HUD2026/Select`(`apply_select.py`): `WBP_HUD_JobSelect`(부모 `UNUJobSelectionWidget`, zxcv1 WBP_JobSelection 과 같은 C++),
  `WBP_HUD_JobCard`(`UNUJobSelectionCardWidget`), `WBP_HUD_JobSkillRow`·`WBP_HUD_SelChip`(이름으로 채우는 줄·칩),
  `WBP_HUD_NaiaSelect`(부모 `UNUStarterNaiaSelectionWidget` --- 지금까지 WBP 없이 C++가 회색 카드를 만들던 창), `WBP_HUD_NaiaCard`(`UNUStarterNaiaCardWidget`).
  연결은 DefaultGame.ini NUUISettings `JobSelectionWidgetClass`(교체)·`StarterNaiaSelectionWidgetClass`(추가).
- 화면: 뒤 장면 흐림 + 비네트, 제목, 무대 한 줄, 아래 남색 유리 판, 키 안내(← → 고르기 · F 정하기). 필드 HUD 접힘. 확인 창은 생성 화면 글래스 확인 창(WBP_CCG_ConfirmDialog).
  - 직업: 내 캐릭터(고른 캐릭터 외형 --- 성별 그대로, 시험 플레이면 nu.Char.TestGender)가 다섯 직업 차림으로 정면을 보고 선다(사용자 요청).
    고른 칸은 정면 대기를 재생하며 직업 색 빛 기둥 안에 --- 머리·얼굴 층은 선택창 초상과 같은 `BuildPortrait`(바운스 표를 따라가는
    `UNUFlipbookImage::FollowBodyBounce`, dbals 님 10/14)로 몸을 따라 오르내린다. 나머지는 첫 그림에 멈춰 어둡게. 그 성별의 직업 몸이 아직 없는 직업(남자 엘리멘탈리스트·여자 나이아 나이트)은
    노비스 옷으로 보인다 --- 그림이 들어오면 몸 표 한 줄로 바뀐다.
    판: 문장·이름·직업 문장(1인칭 대사), 시작 스킬 3종(직업 스킬 표 Class = JobKey · NodeType 시작스킬, 패시브→액티브→시너지, 설명은 한 줄 + 마우스 말풍선),
    Lv.20 전직 두 갈래(GetAdvancementsByJob), "트래커로 정하기"(로/으로 자동).
  - 나이아: 대기 플립북(PreviewFlipbook) --- 고른 것 4배·나머지 3배 어둡게, 이름 위에 속성 그림 아이콘. 판: 속성·타고난 힘(PassiveSkill1Id)·
    처음 능력치 6종(여섯 중 최고값은 금색 막대)·진화(지금 그림 실루엣 + Lv·다음 이름), "아쿠퐁과 함께하기"(와/과 자동).
  - 직업 색은 차림에서 뽑은 값(JobAccents: 나이아 나이트 7E9BE0 · 트래커 D9A74A · 엘리멘탈리스트 7FC8EE · 갬블러 D06A7E · 드루이드 93B567).
- 의뢰 팝업(`apply_field.py` build_quest_rows·build_quest, `run.ps1 -Steps quest` 는 퀘스트 위젯만 다시 만든다): 오른쪽으로 옮겨 NPC·캐릭터를 비움,
  떠 있는 동안 필드 HUD 접힘(`UNUQuestWidget::bHideFieldHUDDuringOffer`), 머리말 "새 의뢰 · 의뢰인"과 의뢰인 얼굴(QO_GiverMedal·QO_GiverPortrait ---
  NPC 정의 Portrait, 없으면 월드의 그 NPC 그림 / 마을 NPC 대기 플립북 이름 규칙에서 머리 칸을 잘라 씀). 보상은 그림 칸(경험치 결정·골드 동전·
  실제 아이템 아이콘 + 수량 배지 + 이름), 아이템 칸에 마우스를 올리면 이름·분류·설명 말풍선(`RewardTooltipClass` = `WBP_HUD_RewardTip`). 수락 버튼에 F.
- 새 그림: 경험치·골드·속성 6종 아이콘(GPT, 아이템 아이콘 톤 --- `ArtSource/UI/HUD2026/reward_icons`, 정리 `finish.py`),
  직업 문장 256px 흰색판(`make_select_art.py`), 키트 T_HUD_SelBeam·SelGlow·SelShadow·SelVeil(`make_hud_kit.py`). 모두 `/Game/Art/UI/HUD2026/Icons`·`Kit`.
- C++ 추가(모두 선택, 기본값이면 옛 WBP 그대로): `UNUJobSelectionWidget` JobCardClass·JobEmblems·JobAccents·bSelectFirstOnOpen·bHideFieldHUDWhileOpen·
  SkillRowClass·AdvancementChipClass·ConfirmDialogClass·ConfirmLabelFormat + 선택 바인딩(DetailEmblem·AccentBar·SkillList·AdvancementList·ConfirmLabel), 키 처리.
  `UNUStarterNaiaSelectionWidget` NaiaCardClass·ElementIcons·같은 옵션들 + 바인딩(DetailElementIcon·ElementChip·ElementText·PassiveNameText·EvolutionFigure·
  EvolutionLevelText·EvolutionNameText·ConfirmLabel, 능력치는 이름 StatValue0~5·StatBar0~5). `UNUFlipbookImage::StopAtKeyFrame`.
  `UNUFlipbookImage::StopAtKeyFrame`·`GetFlipbook`. `UI/NUKoreanText.h`(와/과·로/으로). 나이아 확인 문구도 와/과를 맞춤("아쿠퐁과 함께하시겠습니까?").
  `NUUIController.h` 에 PlayerController include (유니티 빌드 밖에서 컴파일되게).
- 개발 콘솔: `nu.HUD.JobSelect`, `nu.HUD.NaiaSelect` --- 실제 여는 경로(UI 매니저 창)로 연다.
- 확인(리베르그로브 엘라던 곁, `qa/states_selection_after.json`·`states_selection_female.json`): 직업 창 열기·트래커 고르기(남), 드루이드 고르기(여, 정면·성별 맞춤),
  나이아 창·불코이 고르기, 의뢰 10004(의뢰인 루간 얼굴·경험치·골드·마을 약도).
  미확인: 실제 키보드·패드 입력, 서버 온라인 확정 흐름(전직 요청·나이아 계약), 아이템 말풍선(마우스), 다른 화면비.
- 10/15 퀘스트 추적 창 롤백(사용자 요청 --- 유리 판이 시선을 너무 끈다): v3 유리 판을 걷고 10/14 오전 모습(오른쪽 끝 그늘 위 금색 "퀘스트" 제목·▾ ×,
  `Docs/HUD_QuestMap_Proposal_20261014` 1번 화면)으로 되돌림. 그늘(`T_HUD_ScrimR`)만 조금 진하게 .40/.66 → .52/.78. 줄 모양(보고 칩 등)은 v3 그대로.
  `run.ps1 -Steps kit,quest`. 비교 `Docs/HUD_Selection_Proposal_20261015/png/tracker_rollback.png`.
  같은 날 추적 창 글자(제목·▾×·퀘스트 이름·목표)에 1px 어두운 외곽선(#09121C 85%) --- `apply_field.py` TRACKER_OUTLINE(0이면 끔), `run.ps1 -Steps quest`
  (이제 WBP_HUD_QuestEntry 도 같이 다시 만든다). 비교 `Docs/HUD_Selection_Proposal_20261015/png/tracker_outline.png`.

## 전투 결과창 · 대화 말풍선 시안 (2026-10-15, 제안)
`Docs/HUD_ResultBubble_Proposal_20261015/result_bubble.html` (`build_result_bubble.py`). 결과창 A 승리 제목 + 오른쪽 정산 시트 / B 가운데 정산서 / C 무대 위 정산,
말풍선 A 유리(이름표와 한 벌) / B 도트 종이 / C 이름표 일체형. 사용자가 두 화면 모두 A 채택.

## 전투 결과창 · 대화 말풍선 (2026-10-15 v9)
`apply_result.py` (`run.ps1 -Steps result`, 매번 새로 만든다) → `run.ps1 -Steps apply` 가 BP_NUPlayerController BattleResultWidgetClass 와
DefaultGame.ini SpeechBubbleClass·PartySpeechBubbleClass·WorldSpeechBubbleClass(새 키) 를 바꾼다. 옛 dongh WBP_BattleResult(+Member·Entry)·
dbals WBP_SpeechBubble(_Party) 는 그대로 --- `revert` 로 돌아간다.
- 결과창(`Battle/WBP_HUD_BattleResult`·`WBP_HUD_ResultMember`·`WBP_HUD_ResultEntry`): 왼쪽 무대 위 "전투 결과 / 승리"(진 전투에서 잡기만 했으면 "포획 성공"),
  내 캐릭터 레벨이 오르면 내 전투 아바타 머리 위 "레벨 업!"(로컬 전투는 ASC 소유자, 넷 전투는 플레이어 아바타가 하나일 때만 --- 파티 넷 전투는 접힘).
  오른쪽 유리 시트: 보상(경험치 결정·골드 동전 그림, 0 에서 차오름) → 파티(얼굴 --- 캐릭터는 선택창과 같은 겹친 플립북, 나이아는 PreviewFlipbook 1배 ·
  레벨 · 경험치 막대가 차오르고 레벨이 오르면 끝까지 찼다가 새 값으로 + "레벨 업 1 → 2" 칩, 나이아는 경험치가 없어 막대 대신 속성 그림) →
  획득(아이템 그림 칸 + 수량) / 포획(나이아 대기 그림 + 금테 + "새 동료"). 제목 → 시트 → 줄 → 칸 차례 등장, 첫 키(또는 계속 버튼)는 연출 건너뛰기, 다음 키가 닫기.
- 말풍선(`Field/WBP_HUD_SpeechBubble`·`_Party`·`_World`): 이름표와 같은 남색 유리 · 상아 글(17px), 파티는 하늘색 "파티", 전체는 금색 "전체" 칩.
  줄은 C++ 가 나눈다 --- 두 줄이면 문장 끝(. ! ? ~ , …)에서 먼저, 아니면 고른 길이로, 3줄 넘으면 "…"(폭 360). 아래에서 살짝 튀어 오르며 뜬다(0.22초), 5초 표시.
- **UE 5.8 ROUNDED_BOX 외곽선은 렌더 불투명도를 무시** --- 등장·퇴장 연출로 흐려지는 테두리(시트·칸·칩·말풍선)는 전부 9분할 선 그림
  (`T_HUD_Line8/10/14/22`, 원 `T_HUD_CircleLine52`, 꼬리 `T_HUD_BubbleTail`, `make_hud_kit.py`). 새 것만 `import_kit_part` 로 넣는다(키트 전체 재임포트는 모든 .uasset 을 다시 씀).
  얼굴 원의 모서리 가림(`T_HUD_PortraitMask72`)은 색조 없이(흰색) --- 그림 자체 남색이 시트 유리가 실제로 보이는 색(#101B26)과 거의 같다.
- C++ (모두 선택 · 기본값이면 옛 WBP 그대로): `FNUBattleResultMember` PreviousLevel·NaiaId(+ 고른 캐릭터가 없으면 플레이어 이름).
  `UNUBattleResultWidget` ResultTitleText·ContinueButton·TitleRoot·SheetRoot·LevelUpCallout 바인딩, Victory/Capture/EndTitle·Experience/GoldFormat·
  RevealStagger·CountUpSeconds·CalloutOffset·bFillSingleColumn·EntryPadding. `UNUBattleResultMemberWidget` PortraitBox·NaiaFigure·ElementImage·ExperienceRoot·
  LevelUpText 바인딩, PortraitPixelScale·NaiaPixelScale·ElementIcons·LevelUpFormat·BarFillSeconds. `UNUBattleResultEntryWidget` NaiaFigure·NewNaiaRoot·CountRoot.
  `UNUNoticeBannerWidget` BalancedWrapWidth·MaxLines·EnterOffset·EnterFromScale·EnterSeconds. `UNUUISettings::WorldSpeechBubbleClass`.
- 개발 콘솔: `nu.HUD.BattleResult [levelup 0|1] [capture 0|1]`(이기는 치트가 없어 실제 PlayerController 경로로 예시 결과창, 명단은 BuildMembers),
  `nu.Bubble.SayWorld`(+ 기존 Say·SayParty).
- 확인(`qa/states_result_new.json` 펀브룩 전투, `qa/states_bubble_new.json` 리베르그로브): 결과창 레벨 업+포획 / 둘 다 없음, 등장 중간(테두리 남지 않음),
  말풍선 근처·긴 말(문장 끝 줄바꿈)·파티·전체·튀어 오르는 중간. 옛 WBP 6개 새 C++ 로 컴파일 오류 0.
  미확인: 실제 전투 승리 흐름 끝까지(이기는 치트 없음 --- 결과창을 띄우는 C++ 경로는 그대로), 넷 전투·파티 전투의 "레벨 업!" 위치, 실제 키 입력의 건너뛰기,
  다른 화면비, 서버 채팅으로 남이 말한 말풍선.
