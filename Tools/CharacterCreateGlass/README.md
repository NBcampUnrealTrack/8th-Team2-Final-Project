# 캐릭터 생성 화면 — 여관 문 앞 무대 + 글래스 시트 UI + 머리 색 (2026-10-13)

시안: `Docs/CharacterCreate_Proposal_20261013/` (무대 제안 `charcreate_proposals.html`, UI 시안 `charcreate_ui_concepts.html` 1번, 종족 아이콘 시안 `charcreate_race_icons.html`). 같은 폴더의 `ingame_*.png`는 게임에서 찍은 결과입니다.

스크립트가 원본입니다. 에디터에서 손으로 고친 위젯·레벨 값은 스크립트를 다시 돌리면 덮입니다. 고칠 것은 스크립트에 넣으세요.

## 실행

```powershell
.\Tools\CharacterCreateGlass\run.ps1 -Steps build,fx,hair,stage,ui,dialog,select
.\Tools\CharacterCreateGlass\run.ps1 -Steps shot -States .\Tools\CharacterCreateGlass\states.json
```

| 단계 | 하는 일 | 스크립트 |
|---|---|---|
| (그림) | UI 그림 26장 렌더 → `ArtSource/UI/CharacterCreateGlass_20261013/kit` | `make_ui_kit.py` (Playwright 필요) |
| (그림) | 무대 배경 굽기 3840×2160(그림자 있는 것·없는 것) + 발밑 그림자 → `ArtSource/UI/CharacterCreateGlass_20261013` | `bake_backdrop.py` (웹 아트 세트 필요) |
| build | 에디터 모듈 빌드 | — |
| fx | 시간으로 움직이는 UI 머티리얼 2종(무대 분위기, 버튼 빛)을 새로 만든다 | `apply_fx.py` |
| hair | 머리 색 머티리얼 2종을 새로 만든다 | `apply_hair.py` |
| stage | 배경 임포트, 생성 레벨 두 곳 재배치 | `apply_stage.py` |
| ui | UI 그림 임포트, 버튼 WBP 3종, 화면 WBP, 게임모드 2곳 연결 | `apply_ui.py` |
| dialog | 확인 창 WBP 4종(창 2, 버튼 2), 생성 게임모드 2곳 연결 | `apply_dialog.py` |
| select | 선택창 WBP 2종, 선택 게임모드 2곳 연결 | `apply_select.py` |
| shot | 플레이 화면을 상태별로 캡처 → `Saved/CharacterCreateGlass/<이름>_win.png` | `pie_capture.ps1` |

- build·hair·ui는 ProjectM 에디터가 떠 있으면 실패합니다(DLL·애셋 파일이 잠깁니다). 에디터를 닫고 돌리세요.
- hair·ui는 만든 애셋을 지우고 처음부터 다시 만듭니다. 있는 위젯 안에서 다시 짜면 지운 변수의 흔적이 남아 엔진이 경고를 냅니다.
- shot은 에디터 창을 하나 띄웠다가 닫습니다. 상태 목록은 `states.json`(이름, 그 상태를 만드는 도우미 스크립트, 대기 초)입니다.
  결과는 상태마다 세 가지입니다: `_win.png`(화면에 보이는 그대로), `_view.png`(엔진 스크린샷), `.layout.json`(위젯마다 원하는 크기).
- 화면 스크립트 둘(`apply_ui.py`, `apply_select.py`)이 쓰는 경로·색·브러시·위젯 도우미는 `ccg_kit.py`에 있습니다.
- 필드에서 머리 색 확인: `run.ps1 -Steps shot -States .\Tools\CharacterCreateGlass\states_field.json -Map /Game/Libregrove/Level_V8/Maps/L_Libregrove`
  (팀 디버그 명령 `ember.FakeRemote`로 가짜 캐릭터 셋을 세웁니다).

## 만든 것

- `/Game/Art/UI/CharacterCreate/Glass/WBP_CharacterCreate_Glass` — 화면. 부모는 `UNUCharacterCreateWidget`.
- `/Game/Art/UI/CharacterCreate/Glass/WBP_CCG_SegButton`, `WBP_CCG_RaceChip`, `WBP_CCG_Swatch` — 이 화면용 버튼. 공용 버튼과 같은 C++ 부모(`UNU_CommonButtonWidget`).
- `/Game/Art/UI/CharacterCreate/Glass/Kit/T_CCG_*` — UI 그림. `Stage/T_CCG_InnDoor_Backdrop`, `Stage/MI_CCG_InnDoor_Backdrop` — 무대 배경.
- `/Game/Art/Materials/HairTint/M_NUCharacterHairTint`(필드·전투·생성창 스탠드), `M_NUHairTint_UI`(선택창 초상·머리 썸네일) — 머리 색.

## 바꾼 것 (되돌리는 법)

- 생성 레벨 두 곳(`/Game/Maps/L_CharacterCreate`, `/Game/Developers/dbals/Map/L_CharacterCreate`): 배경 평면의 그림과 크기, 인간 자리의 위치·크기·머티리얼(캐릭터 머티리얼 `M_NUCharacterSprite`), 나머지 세 자리 숨김, 카메라 노출 고정.
- 게임모드 두 곳(`BP_CharCreateGameMode`, `BP_dbals_CharCreateGameMode`): `CharacterCreateWidgetClass`를 새 화면으로.
- C++는 아래 목록. 화면 쪽 추가는 전부 "WBP에 그 이름의 위젯이 있을 때만" 동작해 기존 `WBP_CharacterCreate`는 그대로입니다.
- 원본은 `Saved/CharacterCreateGlass/Backup`에 있습니다(맵 2, 게임모드 2, 소스, 옛 값 영수증 json). 기존 `WBP_CharacterCreate`는 건드리지 않았으니, 게임모드 값만 되돌리면 옛 화면으로 돌아갑니다.

## C++에 추가한 것

화면(`NUCharacterCreateWidget`) — 전부 선택 사항:

- `bAutoPickFirstRace` — 열려 있는 첫 종족을 고른 채 시작(첫 화면부터 폼이 보임).
- `bCollapseUnusedThumbs` — 안 쓰는 썸네일 칸을 자리째 접음. `ThumbBoxSize` — 썸네일 칸 한 변(머리 확대 계산의 기준, 기본 64).
- 이름으로 찾는 위젯: `SkinIndexText`, `GenderFemaleSelected`/`GenderMaleSelected`, `RaceSelected0..N`, `RaceLock0..N`,
  `HairOptions`·`HairEmpty`(Face·Skin도 같음), `LookEmptyNote`·`LookRows`(세 줄 모두 고를 것이 없으면 안내 한 줄),
  `HairThumb0Selected`·`HairThumb0Cell`(칸 이름 + Selected / Cell), `NameOkMark`,
  `HairColorButton0..N`·`HairColorFill0..N`·`HairColorSelected0..N`·`HairColorCell0..N`·`HairColorRow`.
- 고친 것 하나: 서버 없이 실행하면 이름 확인 답이 요청 안에서 바로 와서 "사용 가능한 이름입니다."를 "확인 중..."이 덮었습니다. 기다리는 문구를 요청 앞으로 옮겼습니다.

머리 색:

- `NUAppearance::PackHairId / SplitHairId / GetHairTint` (`NUCharacterCreateTypes`), 견본과 머티리얼 경로는 `UNUAppearanceDataSettings`(프로젝트 세팅 > NU Appearance Data > Hair Color).
- `UNUSpriteVisualComponent::RefreshAppearanceSkins`가 번호를 풀어 머리 레이어에 입힙니다 — 필드(내 캐릭터·남의 캐릭터)·전투·정보창 미리보기가 모두 이 함수를 지납니다.
- `ANUCharacterCreatePreview`(생성창 스탠드), `UNUCharacterSlotWidget` + `UNUFlipbookImage`(선택창 초상).

## 머리 색이 저장되는 방식

서버에 머리 색 칸이 없고 `hair_id`는 서버가 저장·에코만 하므로, 한 숫자에 같이 싣습니다.

```
hair_id = 머리 파츠 번호(PartId) + 색 번호 × 1000
```

- 색 번호 0은 "그림 원래 색"입니다. 색을 고르기 전에 만든 캐릭터(`hair_id` = PartId)는 그대로 읽힙니다.
- 견본 순서가 곧 저장되는 번호입니다. **순서를 바꾸거나 중간에 끼워 넣지 말고 뒤에만 보태세요.** 머리 파츠의 PartId는 1000보다 작아야 합니다.
- 이 변경이 없는 빌드는 색이 실린 번호를 모르는 머리로 보고 그 몸의 첫 머리를 그립니다(색도 모양도 틀림). 팀에 같이 들어가야 합니다.
- 서버가 `hair_id`를 표와 대조하기 시작하면 깨집니다. 서버에 `hair_color` 칸이 생기면 Pack/Split 두 함수만 걷어내면 됩니다.

생성 화면 스탠드의 머리는 처음부터 물들임 머티리얼의 인스턴스를 쓰고 `HairTintOn`(0/1)으로만 켜고 끕니다. 머티리얼을 갈아 끼우면 UE가 새 셰이더가 준비될 때까지 그 부품을 그리지 않아, 처음 색을 고를 때 머리가 몇 프레임 사라졌습니다(`HairTintOn` 0 = 일반 캐릭터 머티리얼과 같은 그림).

물들이는 법: 머리 그림은 밝은 금발 + 검은 외곽선이라 밝기만 남겨도 음영이 살아 있습니다. 밝기 × 고른 색으로 칠하고, 기준보다 밝은 붓질은 흰 쪽으로 올립니다(`HairRefLuma` 0.60, `HairHighlight` 0.35). 월드에서는 색을 머리 레이어의 정점 색으로 보내고, UI에서는 머티리얼 파라미터로 보냅니다. 필드용 머티리얼은 `M_NUCharacterSprite`의 **사본**이라 그쪽 그래프를 고치면 `hair` 단계를 다시 돌려야 합니다.

## 연출 (2026-10-14)

정적이지 않게, 그러나 조작을 방해하지 않을 만큼만 넣었습니다. 전부 끌 수 있습니다(WBP 클래스 기본값).

| 무엇 | 어디서 | 끄는 법 |
|---|---|---|
| 열릴 때: 검은 막이 걷히고(0.6초), 시트가 오른쪽에서 들어오고(0.5초), 시트 안 줄이 위에서부터 차례로 뜸 | 생성·선택 화면 C++ (`bPlayIntro`) | `bPlayIntro` 끄기 |
| 선택창 목록 줄이 처음 찼을 때 차례로 들어옴, 다른 캐릭터를 고르면 무대 캐릭터가 살짝 올라오며 나타남 | 선택 화면 C++ | 같음 |
| 외형(종족·성별·머리·머리 색·외모)을 바꾸면 머리·어깨 둘레에서 작은 도트 반짝임 7개가 차례로 터졌다 사라짐(0.5초, 그림 1칸 = 5px 격자) | 생성 화면 C++ (`LookSparkle0..6`, `FNUScreenMotion::AddPulse`) | WBP에서 `LookSparkle*` 지우기 |
| 등불·창문이 숨 쉬듯 밝아졌다 어두워지고, 불씨(그림 1칸 크기)가 피어오름 | UI 머티리얼 `M_CCG_StageAmbience` (위젯 틱 없음) | 파라미터 `GlowStrength`·`MoteStrength` |
| 생성하기·시작하기가 눌릴 수 있을 때 4.5초마다 빛이 한 번 지나감 | UI 머티리얼 `M_CCG_Shine` | 파라미터 `Strength`, 또는 `CreateShine`·`EnterShine` 지우기 |
| 확인 창: 뒤가 어두워지고 창이 살짝 커지며 나타남 | `UNUConfirmDialog` (`bPlayOpenMotion`, 기본 끔) | `bPlayOpenMotion` 끄기 |

C++의 움직임은 `FNUScreenMotion`(`Source/ProjectM/UI/NUScreenMotion.h`) 하나가 맡습니다. 위젯의 렌더 변환·불투명도만 바꾸고 배치는 건드리지 않으며, 끝나면 원래 값으로 돌아갑니다.

## 해상도

- 화면 크기는 프로젝트 기본(짧은 변 기준, 1080 = 1배)을 따릅니다. 1280x720이면 0.67배, 2560x1440이면 1.33배로 같은 모양입니다.
- 무대 그림을 2560:1080(21:9)까지 넓게 구웠습니다. 가운데 16:9는 승인된 시안과 픽셀까지 같습니다.
  - 16:9: 가운데를 봅니다.
  - 21:9처럼 넓은 화면: 거리를 옆으로 더 보여 줍니다(확대하지 않음).
  - 16:10·4:3: 양옆이 잘립니다. 캐릭터·시트는 그대로입니다.
  - 2560:1080보다 넓은 화면(32:9)은 양옆에 그림이 없습니다.
- 생성 화면 카메라는 세로 시야를 고정합니다(`AspectRatio_MaintainYFOV`). 선택 화면은 같은 규칙의 ScaleBox(`ScaleToFitY`)라 두 화면의 무대·캐릭터 위치가 같습니다.
- 시트는 오른쪽에 붙고 높이만 화면을 따릅니다. 가로 1440(4:3)까지 캐릭터와 겹치지 않습니다.
- 캡처: `run.ps1 -Steps shot -States ...states_res_create.json -W 1440 -H 1078` 처럼 창 크기를 바꿔 찍습니다.
- 확인한 크기(2026-10-14, 생성·선택 둘 다): 1280x720, 1680x1050(16:10), 1440x1080(4:3), 1920x1080, 2520x1080(21:9). 결과는 `Saved/CharacterCreateGlass/res/`. 같은 이름 접두어의 캡처는 다음 실행이 지우므로 크기마다 `res/`로 옮겨 둡니다. 32:9와 실제 고해상도(1440p 이상) 창은 찍지 않았습니다.

## 확인 창 (2026-10-14)

생성·선택 화면의 확인 창을 같은 유리 카드로 바꿉니다. 시안: `Docs/CharacterCreate_Proposal_20261013/dialog_mock.png`.

```powershell
.\Tools\CharacterCreateGlass\run.ps1 -Steps dialog
```

- `/Game/Art/UI/CharacterCreate/Glass/Dialog/WBP_CCG_ConfirmDialog`(확인 버튼 아이보리 — 생성하기·계속) — 생성 게임모드 두 곳의 `ConfirmDialogClass`가 이것을 가리킵니다.
- `WBP_CCG_ConfirmDialog_Danger`(확인 버튼 빨강 — 삭제) — 선택창의 삭제 확인에 씁니다(`select` 단계보다 먼저 돌릴 것).
- 둘 다 `UNUConfirmDialog` 자식이라 부르는 코드는 그대로입니다. 게임 전체 공용 확인 창(프로젝트 세팅의 `WBP_ConfirmDialog`)은 바꾸지 않습니다 --- 선택창의 [타이틀로] 확인은 공용 설정을 쓰므로 옛 모양으로 나옵니다.
- 확인 버튼은 공용 버튼(`UNU_CommonButtonWidget`)입니다. 순정 Button이면 C++가 팔레트의 초록·빨강을 곱해 아이보리가 초록이 됩니다.

## 선택창 (2026-10-14, 적용 전)

생성 화면과 같은 여관 문 앞 무대와 글래스 시트로 선택창을 다시 짭니다. 시안: `Docs/CharacterCreate_Proposal_20261013/select_mock.png`.

```powershell
.\Tools\CharacterCreateGlass\run.ps1 -Steps build,select
.\Tools\CharacterCreateGlass\run.ps1 -Steps shot -States .\Tools\CharacterCreateGlass\states_select.json -Map /Game/Developers/dbals/Map/L_CharacterSelect
```

- 무대는 레벨이 아니라 위젯 안 그림입니다(`T_CCG_InnDoor_Backdrop_NoFoot` + 발밑 그림자 `T_CCG_FootShadow`). 골라 둔 캐릭터를 생성 화면 스탠드와 같은 자리·같은 배율(그림 1칸 = 5px)에 세웁니다. 선택 레벨은 바꾸지 않습니다.
- 오른쪽 시트: 골라 둔 캐릭터 이름과 "Lv · 직업 · 종족", 내 캐릭터 목록(줄마다 얼굴 칩, 6줄 넘으면 스크롤), 점선 "새 캐릭터 만들기", 시작하기. 삭제는 골라 둔 줄에만 나옵니다.
- 열자마자 첫 캐릭터(삭제 예약이 아닌 것)를 골라 둡니다.
- 만드는 것: `/Game/Art/UI/CharacterCreate/Glass/Select/WBP_CharacterSelect_Glass`, `WBP_CharacterSlot_Glass`. 바꾸는 것: 선택 게임모드 두 곳(`BP_CharSelectGameMode`, `BP_dbals_CharSelectGameMode`)의 `CharacterSelectWidgetClass`. 옛 `WBP_CharacterSelect`·`WBP_CharacterSlot`은 그대로입니다.
- C++ (전부 선택 사항, 기본값은 지금 동작): 선택창 `bAutoSelectFirst`·`HeroPixelScale`과 이름으로 찾는 `HeroPortrait`·`HeroShadow`·`SelectedNameText`·`SelectedSubText`·`SlotCountText`·`EmptyNote`. 칸 `PortraitPixelScale`·`bPortraitAlignTop`·`bWalkWhenSelected`·`bSingleLineSub`·`bDeleteOnlyWhenSelected`. 칸과 큰 캐릭터가 같은 그림을 쓰도록 외형 그리기를 `UNUCharacterSlotWidget::BuildPortrait`로 묶었습니다.
- 서버 없이 확인: 콘솔 `nu.Char.Dummy [이름] [종족] [성별] [머리] [외모] [직업] [레벨] [삭제예약시간]`(개발 빌드만). 서버에 붙어 있으면 아무것도 하지 않습니다.

## 무대 숫자

카메라는 배경 평면 앞 1030, 가로 화각 90 → 16:9 화면을 꽉 채우려면 평면이 2060 × 1158.75. 배경은 384×216칸(1칸 = 1080p에서 5px). 깊이 530에 선 자리는 `960 × 배율 / 530` px로 보이므로 배율 2.7604가 같은 5px입니다. 배경이 언릿 그림이라 카메라에서 노출을 고정하고 필름 곡선·블룸·비네트·로컬 노출을 껐습니다(프로젝트 기본 로컬 노출 0.8이 어두운 색을 3분의 1쯤 밝게 만듭니다).

## 만들면서 확인한 것 (다음에 UI를 스크립트로 짤 때)

- **배경 흐림(BackgroundBlur)에 모서리 반경을 주면 아무것도 안 그려집니다.** 반경이 있으면 흐린 그림을 백버퍼 알파로 섞는데 3D 장면 위의 알파가 0입니다. 반경 0으로 두면 됩니다(둥근 유리 모서리 밖으로 흐린 조각이 최대 12px 남지만 어두운 배경에서는 안 보입니다).
- **9분할(Box) 브러시의 모서리 크기는 텍스처의 실제 픽셀 수에서 나옵니다.** 2배로 그린 그림을 쓰면 모서리가 두 배가 됩니다. 9분할 그림은 1배로 만듭니다.
- 파이썬에서는 위젯의 화면 위치를 읽을 수 없습니다(`FGeometry`에 읽을 수 있는 칸이 없음). 원하는 크기(`get_desired_size`)만 읽힙니다.
- 폰트의 줄 높이가 브라우저와 다릅니다. 혼자 서 있는 글줄은 시안의 줄 높이만큼의 SizeBox에 넣어야 쌓임이 시안과 같아집니다.
- 테두리 있는 CSS 상자는 적힌 높이보다 2px 큽니다(테두리가 바깥). 버튼 안쪽 자리(ButtonSlot)는 기본 여백(4,2)이 있습니다.
- 밝은 글자는 브라우저보다 가늘게 그려집니다(같은 폰트·같은 굵기, 어두운 글자는 같음). 엔진의 글자 그리기 차이라 손대지 않았습니다.
- 처음 쓰는 머티리얼은 에디터가 뒤에서 컴파일하는 동안 회색 덩어리로 그려집니다. 캡처는 그 상태에 `wait`를 넉넉히 주세요.
- 마스크드 스프라이트는 시간 누적 안티에일리어싱을 지나 가장자리가 흐려집니다. 캐릭터 머티리얼은 그 뒤에 그려져 또렷합니다.

## 여성 머리 색

여성 기본 몸(직업 없음)은 머리가 몸 그림에 같이 그려져 있고, 머리색이 피부와 같은 살구색 계열이라 색으로 따로 떼어 낼 수도 없습니다. 그래서 생성 화면에서 여성은 머리 색 줄이 접힙니다.

- 여성 직업 몸(드루이드·트래커·법사·갬블러)은 머리가 따로 된 레이어라, 직업을 고른 뒤에는 머리 색이 그대로 먹습니다.
- 생성 화면에서도 쓰려면 여성 기본 몸을 머리 없는 몸 + 머리 레이어로 나눠 그려야 합니다(남캐 `MCh_Body_*` + `MCh_Hair1_*`처럼 대기·걷기 × 앞·옆·뒤). 그림이 오면 머리 파츠 표에 여성·직업 0 줄을 하나 넣는 것으로 끝나고 코드는 바꿀 것이 없습니다.
