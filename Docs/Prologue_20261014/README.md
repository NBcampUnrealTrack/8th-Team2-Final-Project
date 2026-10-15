# 게임 시작 프롤로그 — "정령이 키운 아이"

새 캐릭터가 처음 필드(리베르그로브)에 들어오면 한 번 재생되는 약 64초짜리 프롤로그. Suno 곡 'Nia Unbound'(64.9초)에 맞춰 편집했다.
**도트 그림책 4장(크로스페이드) → 새벽빛 전환 → 실제 게임 화면(레터박스, 내 캐릭터가 남문으로 걸어 들어감) → 마을 3컷(와이드·클로즈·타이틀) → 타이틀 → 바로 조작**.

> 상태 (2026-10-14): 엔진 구현·재생·자동 재생 조건·스킵·복귀 동작. 그림 4장은 GPT(실제 나이아 6종 등장), 음악은 사용자 Suno 곡 연결.
> 기존 zxcv1 님의 아르카디아/그리드폴 프롤로그 시안(`/Game/Developers/zxcv1/Cinematics/Prologue`)은 건드리지 않았다 — 두 설정 중 무엇을 정본으로 할지는 결정 필요.

## 곡에 맞춘 편집 (timeline.json v2)

| 시각(초) | 곡 | 화면 |
|---|---|---|
| 0 ~ 11.2 | 오르골 도입, 6.5초부터 선율 | 검은 화면 → P1(요람과 나이아) 밝아짐, 도트 반짝이 |
| 11.2 | 첫 프레이즈 | P2(술래잡기)로 크로스페이드 |
| 19.0 | 둘째 프레이즈 | P3(책)로 크로스페이드 |
| 25.0 | 셋째 프레이즈(커짐) | P4(문턱)로 크로스페이드 |
| 29.3 ~ 30.9 | 숨 고르기(30.4) | 따뜻한 새벽빛이 그림을 덮음 |
| 30.9 ~ 32.6 | 큰 섹션 시작(31.4) | 빛이 걷히며 실제 게임 화면, 레터박스 내려옴, 캐릭터가 남문으로 걸어 들어감(31.8~35.6) |
| 38.2 | 박자 | 검게 잠겼다 풀리는 컷(37.55→38.1 잠김, 38.3→38.9 풀림) → 마을 와이드 |
| 46.0 | 2절 | 마을 클로즈(사람과 나이아) |
| 56.45 | 클라이맥스 | 빛 번쩍임 + 위로 물러나는 카메라, 타이틀이 살짝 줄어들며 자리 잡음, 반짝이 |
| 61.8 ~ 63.6 | 잦아듦 | 검게 닫힘 → 게임 화면이 밝아지며 조작 |

자막은 문장 전체가 0.6초에 걸쳐 살짝 떠오르며 나타나고 가라앉으며 사라진다(한 글자씩 쓰는 효과 없음). 레터박스 구간에서는 아래 검은 띠 위에 앉는다.
연출 장면 캡처는 `index.html` 의 "전환·모션그래픽" 절.

## 대본

| # | 화면 | 자막 |
|---|---|---|
| 1 | 그림 P1 — 밤, 고목 뿌리 사이 요람, 나이아 여섯(아쿠퐁·볼토·푸루나·불코이·토그라·휘리)이 아기를 지킴 | 태어난 지 얼마 되지 않아 버려진 아이. / 그런 당신을 키운 것은 정령들이었습니다. |
| 2 | 그림 P2 — 나이아들과 술래잡기하는 아이(뒷모습) | 사람 하나 없는 정령들의 세계에서 당신은 자랐죠. |
| 3 | 그림 P3 — 요람의 그 낡은 책을 읽는 아이, 둘러앉은 나이아들 | 인간의 언어도 배웠습니다. |
| 4 | 그림 P4 — 숲의 경계, 두건 쓴 여행자 뒷모습, 배웅하는 나이아들(휘리는 따라 나섬) | 그리고 어른이 된 당신은, |
| 4' | 그림책이 걷히고 **실제 게임 화면**, 내 캐릭터가 몇 걸음 걸어 나감 | 처음으로 사람의 세계로 나왔습니다. |
| 5 | 마을 광장 전경, 주민과 나이아(시퀀스가 잠시 띄우는 스프라이트) | 사람들은 정령을 '나이아'라 불렀습니다. / 이곳은 나이아와 사람이 함께 살아가는 세계. |
| 6 | 타이틀 로고(로그인 화면의 `T_TitleText`) → 검게 닫힘 → 게임 화면이 밝아지며 조작 시작 | — |

연출 장치(위젯): 그림 크로스페이드(`bCrossfadePhotos`), 도트 반짝이 입자(`MoteIntensity`, 그림 한 칸 격자에 맞춘 '+' 모양), 화면 빛(`FlashOpacity`·`FlashColor`), 레터박스(`LetterboxAmount`·`LetterboxFraction`), 타이틀 배율(`TitleScale`), 자막 페이드(`CaptionOpacity`). 모두 기본값이 꺼져 있어 기존 퀘스트 연출은 그대로다.

원문에서 고친 곳: "태어난지" → "태어난 지", "이 곳" → "이곳", "어른이 된 당신은, 당신은" 중복 제거.
"사람 없는 세계에서 인간의 언어를 어떻게 배웠나"는 대사로 설명하지 않고, P1 요람 옆의 책이 P3 에서 다시 나오는 것으로 보여 준다.

## 재생 규칙

- **자동 재생**: 캐릭터 선택을 거쳐 필드에 들어왔고, 캐릭터 레벨 1 이하이고, 이 PC 에서 그 캐릭터로 아직 안 봤을 때.
  대상이면 **로딩 화면이 떠 있는 동안** 그 아래에 검은 덮개를 깔고, 입력·HUD·레벨 BGM 을 막고, 그림·시퀀스·음악을 미리 읽는다.
  로딩 화면이 걷히면(서버 접속 중이면 필드 입장 확정 뒤) 덮개 아래에서 0.15초 뒤 시작하고, 프롤로그 화면(검은 화면으로 시작)이 한 번 그려지면 덮개를 걷는다.
  그래서 로딩 화면 → 검은 화면 → 첫 그림으로 바로 이어지고, 그 사이에 필드·캐릭터·HUD 가 비치지 않는다.
  입장 실패 화면이 뜨면 덮개를 걷고 재생하지 않는다. 로딩이 끝나고 20초 안에 시작하지 못해도 덮개를 걷고 포기한다.
  맵을 직접 연 PIE 에서는 자동 재생하지 않는다.
- **마을 컷 데우기**: 시작 검은 화면(0~30.45초) 아래에서 마을 컷 카메라 3개로 5초씩 미리 찍고(0.5/5.5/10.5초) 15.5초에 내 캐릭터 카메라로 돌아온다(`village_shot.json` 의 `warmup`).
  화면에는 보이지 않고, 38.2초 마을 컷에서 텍스처·셰이더·그림자를 처음 그리느라 몇 초 동안 끊기던 것을 줄인다(내 캐릭터가 펀브룩처럼 마을에서 먼 곳에 있을 때 특히).
- **본 기록**: 끝까지 보거나 스킵하면 `Saved/SaveGames/NUPrologue.sav` 에 캐릭터별로 남는다. 서버에 새 캐릭터 표시가 없어 레벨로 가르고 PC 에 남긴다
  (서버에 저장할 칸이 생기면 옮기는 게 맞다).
- **스킵**: 오른쪽 아래 "길게 눌러 건너뛰기", 또는 Esc / 패드 B·메뉴를 1초 누르고 있기. 한 번 눌러 실수로 넘기지 않게.
  모양은 자막과 같은 마루부리 글꼴 + 남색·샴페인 금 키 칩(`Esc`, 패드를 쓰면 `B`) + 아래 금빛 줄(누르는 동안 차오름). 첫 그림과 함께 떠오르고 평소에는 62% 로 흐리게, 가리키거나 누르면 또렷해진다.
  레터박스 구간에서는 자막과 같은 줄(아래 띠)에 앉는다. 짧게 누르는 스킵을 쓰는 기존 퀘스트 연출의 버튼 모양은 그대로다.
- **재생 중**: 필드 입력·HUD 를 막고, 3·4번 컷에서는 시퀀스가 실제 캐릭터에 걷기 입력을 넣는다(서버에도 보통 이동으로 반영).
- **끝**: 검은 화면으로 닫히고 게임 화면이 1.2초 동안 밝아지며 조작이 돌아온다. 레벨 BGM 은 재생 중에만 꺼진다(`SMix_StoryMuteBGM`).
- **콘솔**: `nu.Prologue.Play`(언제든 다시 보기), `nu.Prologue.ResetSeen`(본 기록 지우기), `nu.Prologue.Auto 0`(자동 재생 끄기).
- **메뉴 '프롤로그 다시 보기'**: `UNUPrologueSubsystem::PlayPrologue()`(BlueprintCallable) 을 버튼에 연결하면 된다. 메뉴 WBP 는 팀 소유라 연결하지 않았다.

## 무엇이 어디에

| 구분 | 위치 |
|---|---|
| 재생 시스템 확장 | `Source/ProjectM/Cinematics/NUStorySlideshowData.h` (도트 그림 정수 배율, 타이틀, 길게 눌러 스킵, 자막 위치) |
| | `NUStorySequenceCue.h/.cpp` (시퀀스 트랙: PhotoPanX/Y, PhotoScale, TitleOpacity, PawnMoveX/Y) |
| | `NUStorySlideshowWidget.h/.cpp` (위 기능의 화면 처리) — 기존 퀘스트 연출은 기본값 그대로라 동작이 바뀌지 않는다 |
| 자동 재생 | `Source/ProjectM/Cinematics/NUPrologueSubsystem.h/.cpp`, `NUPrologueSettings.h` |
| | `WorldTransition/World/NUFieldLoadingSubsystem.h` 에 `IsLoadingField()`·`HasFailedField()` 추가 |
| 설정 | `Config/DefaultGame.ini` `[/Script/ProjectM.NUPrologueSettings]` |
| 에셋 | `/Game/NUA/Prologue/` — `DA_Prologue_Spirit`, `LS_Prologue_Spirit`, `SCue_Prologue_BGM_Temp`, `Textures/` |
| 빌드 스크립트 | `Tools/Prologue_20261014/build_prologue.py` (+ `timeline.json`, `village_shot.json`) — 에셋을 표에서 통째로 다시 만든다 |
| 그림 처리 | `Tools/Prologue_20261014/process_art.py` (GPT 원본 → 704×396 도트·48색) |
| GPT 프롬프트 | `ArtSource/Prologue_20261014/prompts/` (공통 `_style.txt` + 그림별) |
| 검증 캡처 | `Saved/Prologue_20261014/qa*/` (PIE 타임라인 캡처, `pie_capture.py` + `qa/*.py`) |

## 수정하는 법

- **문장·시각·팬 이동**: `timeline.json` 을 고치고 `build_prologue.py` 를 다시 돌린다(에디터를 닫고 헤드리스로).
  `UnrealEditor-Cmd.exe ProjectM.uproject -run=pythonscript -script=Tools/Prologue_20261014/build_prologue.py -unattended -nosplash`
- **마을 컷 카메라·주민·나이아 위치**: `village_shot.json` (L_Libregrove 좌표, 레벨은 건드리지 않음 — 시퀀스가 잠깐 띄웠다 지운다).
- **그림 교체**: `GPT_IMAGES.md` 의 순서대로 `process_art.py` → `build_prologue.py`.
- **연출 수치**: `timeline.json` 의 `tracks`(Cue 속성별 [초, 값] 키, 부드러운 곡선), 마을 카메라는 `village_shot.json` 의 `shots`.
- **음악 교체**: Suno 곡을 `ArtSource/Prologue_20261014/audio/BGM_Prologue_Spirit.wav` 로 넣고 `build_prologue.py` 를 돌리면 `/Game/NUA/Prologue/Audio/BGM_Prologue_Spirit` 로 임포트되고 `SCue_Prologue_BGM_Final`(SC_Master)로 바뀐다. 그다음 박자에 맞춰 `timeline.json` 시각을 다듬는다.

## 검증 (2026-10-14, PIE · ProjectMEditor Win64 Development 빌드 성공)

| 항목 | 결과 | 근거 |
|---|---|---|
| 전체 재생(L_Libregrove, `nu.Prologue.Play`) | 그림책 4장 → 남문 기둥 사이로 내 캐릭터가 걸어 들어감(약 3.4m) → 광장 전경(주민 3·나이아 5) → 타이틀 → 검게 닫힘 → 1.2초 밝아지며 HUD 복귀. 65.5초, 끝까지=1 | `Saved/Prologue_20261014/qa3/` 캡처 17장 + `stamps.jsonl`(시퀀스 시각·컷 상태) |
| 새 캐릭터 자동 재생 | 더미 캐릭터 선택 0.66초 뒤 `재생 시작 (offline:1)`, 끝까지 본 뒤 `NUPrologue.sav` 기록 | `qa_suite/1_new_character/editor.log` |
| 이미 본 캐릭터 | 자동 재생 안 함 | `qa_suite/2_seen_character/editor.log` |
| 필드 입장 실패 화면(오프라인) | 실패 화면 아래에서 재생하지 않음(처음엔 재생돼서 `IsLoadingField()` 가 실패 화면도 포함하도록 고침) | `qa_suite/3_offline_entry_failed/editor.log` |
| 길게 눌러 건너뛰기 버튼 | 글자 잘림 고친 뒤 표시 확인 | `qa_suite/1_new_character/skip_crop.png` |

| (10/14 오후) 마을 컷 끊김 | 펀브룩에서 재생, 38~44초 구간: 데우기 전 프레임 평균 88ms·최악 1093ms → 데운 뒤 77ms·최악 406ms. 에디터 PIE 라 기계 부하에 따라 흔들림(사용자 실행 로그에서는 39~43초에 약 9fps) | `trace_before/`, `trace_after/` (`qa/trace.py` + `qa/analyze_trace.py`) |
| (10/14 오후) 데우기 컷 | 0.5초 마을 와이드 → 5.8초 클로즈 → 10.5초 타이틀 → 15.5초 내 캐릭터 카메라 → 38.2초 마을 와이드 | `trace_after/trace.jsonl` 의 `vt` |
| (10/14 오후) 건너뛰기 모양 | 그림 위·레터박스·누르는 중(금빛 줄 절반)·스킵 뒤 게임 화면 | `skip_v2/k01~k04*.png` |

검증하지 못한 것: 서버 접속 상태의 실제 신규 캐릭터 흐름(필드 입장 확정 뒤 시작하는 분기), 키보드·패드를 실제로 길게 눌러 스킵(자동화로 키 입력을 넣을 수 없음), Esc 가 다른 UI(메뉴 등)에 먼저 먹히는지, 소리(캡처는 -NoSound).
오프라인으로 캐릭터 선택 → 필드에 들어가면 서버가 없어 원래 '입장에 실패했습니다'가 뜬다(기존 동작) — 그래서 1번 검증은 필드 맵에서 더미 캐릭터를 고른 상태로 했다.

## 남은 일

1. (완료) GPT 그림 4장 — `ArtSource/Prologue_20261014/gen/`(원본 1672×941) → `final/`(704×396) → `/Game/NUA/Prologue/Textures/T_Prologue_0N_*`.
2. (완료) Suno 곡 'Nia Unbound' 연결, 곡 구간에 맞춰 재편집.
3. (선택) Tripo 뿌리 문 `TRIPO_ROOT_GATE.md` → 새 캐릭터 도착 지점에 배치, 걷는 방향 조정.
4. (선택) Grok 미세 움직임 `GROK_VIDEO.md`.
5. 서버 쪽 '새 캐릭터/프롤로그 본 기록' 칸(지금은 레벨 1 + PC 세이브로 대신).
6. zxcv1 님 아르카디아 시안과의 설정 정리.
