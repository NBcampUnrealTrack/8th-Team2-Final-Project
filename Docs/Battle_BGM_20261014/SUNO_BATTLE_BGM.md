# 전투 BGM — Suno 요청서 (일반 전투 / 보스전)

프롤로그 곡 'Nia Unbound'(D장조, 약 57 BPM, 오르골·하프·포크 오케스트라)와 같은 세계의 곡으로 들리게 맞췄다.
- **일반 전투**: B단조(프롤로그 D장조의 나란한조, 같은 음을 쓴다). 경쾌한 모험감, 오래 들어도 덜 피곤하게.
- **보스전**: D단조(프롤로그와 같은 D를 어둡게). 무겁고 웅장하게, 일반 전투와 확실히 구분.

둘 다 **반복 재생(루프)** 용이다. Suno 는 보통 끝을 페이드아웃·마무리로 만드니, 아래 구조 지시로 "끝이 처음으로 다시 이어지게" 유도하고,
받은 곡은 제가 박자 분석으로 루프 구간을 잘라 끊김 없이 돌게 맞춘다.

## 생성 설정 (공통)

| 항목 | 값 |
|---|---|
| 모드 | Custom, **Instrumental 켬** |
| 길이 | 1:30 ~ 2:30 (짧으면 Extend 로 한 구간 더) |
| 받을 파일 | WAV (없으면 MP3 320k), 마음에 드는 후보 2개씩 |
| 넣을 곳 | `ArtSource/Audio/Battle_20261014/BGM_Battle_Normal.wav`, `BGM_Battle_Boss.wav` |

---

## 1) 일반 전투

### Style of Music

```
instrumental, upbeat fantasy JRPG battle theme, adventurous and heroic but light, 150 BPM, B minor, driving string ostinato, staccato strings, punchy snare and taiko, bright brass stabs, tin whistle and fiddle lead melody, pizzicato and marimba accents, energetic piano runs, small orchestra with celtic folk colors, catchy memorable hook, tight rhythm, seamless loop, no fade out, clean mix, no vocals
```

### Exclude Styles

```
vocals, lyrics, choir, rap, EDM, dubstep, trap, lo-fi, ambient, slow ballad, heavy metal, distorted guitar, horror, sad
```

### Lyrics 칸 (구조 지시)

```
[Instrumental]

[Intro - 2 bars: snare roll and brass hit, the battle begins]

[Main Theme A - fiddle and tin whistle play a catchy heroic melody over driving staccato strings and taiko]

[Theme B - brass answers, marimba and pizzicato bounce, playful creature-companion energy]

[Bridge - short breakdown: strings and piano build tension, drums drop to half time]

[Main Theme A - full orchestra returns, bigger]

[Loop - end exactly on the downbeat that leads back into the Main Theme, no ending, no fade]
```

### 간단 모드(Song Description)

```
Instrumental upbeat JRPG battle theme in B minor: driving strings, taiko and snare, brass stabs, fiddle and tin whistle hook, light heroic creature-adventure feel, seamless loop.
```

---

## 2) 보스전

### Style of Music

```
instrumental, epic dark fantasy JRPG boss battle, intense and dramatic, 168 BPM with half-time heavy sections, D minor, thunderous taiko and timpani, low brass and horns, aggressive string ostinato, wordless dramatic choir aahs, pipe organ swells, rock drum kit and overdriven guitar riffs underneath the orchestra, urgent piano, rising brass fanfares, menacing yet heroic, climactic, seamless loop, no fade out, powerful wide mix, no lyrics
```

### Exclude Styles

```
lyrics, sung words, rap, EDM, dubstep, trap, lo-fi, ambient, cute, playful, ballad, comedic, jazz
```

### Lyrics 칸 (구조 지시)

```
[Instrumental]

[Intro - ominous low drone, timpani pulse, the boss appears, then a huge orchestral hit]

[Main Theme A - aggressive strings and low brass riff, pounding taiko and rock drums, overdriven guitar doubles the bass]

[Theme B - wordless choir aahs and horns carry a dark heroic melody, organ swells]

[Breakdown - half-time, heavy and menacing, just drums, choir and guitar chugs]

[Climax - a minor-key echo of the gentle lullaby theme, played huge by full orchestra and choir, hope against despair]

[Main Theme A - returns at full power]

[Loop - end exactly on the downbeat that leads back into Main Theme A, no ending, no fade]
```

### 간단 모드(Song Description)

```
Instrumental epic JRPG boss battle in D minor: thunderous taiko and timpani, low brass, wordless choir, organ, rock drums and guitar under the orchestra, dark heroic and climactic, seamless loop.
```

---

## 둘을 확실히 다르게 가져가는 기준

| | 일반 전투 | 보스전 |
|---|---|---|
| 조 | B단조(밝은 단조, 프롤로그와 같은 음) | D단조(프롤로그의 D를 어둡게) |
| 빠르기 | 150 BPM, 계속 달림 | 168 BPM + 하프타임 무거운 구간 |
| 선율 악기 | 피들·틴 휘슬(포크 색) | 합창(아아)·호른·오르간 |
| 리듬 | 스네어·타이코, 가볍게 튐 | 타이코·팀파니·록 드럼, 일렉 기타 리프 |
| 분위기 | 모험, 나이아와 함께 싸우는 경쾌함 | 위협, 결전, 마지막에 희망 |
| 프롤로그 연결 | 같은 음을 써 자연스럽게 이어짐 | 클라이맥스에 자장가 선율을 단조로 크게 인용 |

## 결과가 안 맞을 때

- 일반 전투가 너무 무거우면: Style 에 `bright, playful, major-key lift in chorus` 추가, `taiko` 를 `hand drums` 로.
- 일반 전투가 너무 가벼우면: `150 BPM` → `160 BPM`, `brass stabs` 를 `bold brass fanfares` 로.
- 보스전에 록이 과하면: `rock drum kit and overdriven guitar riffs underneath the orchestra` 를 지우고 순수 오케스트라로.
- 보스전 합창이 가사를 부르면: Exclude 에 `words, singing` 추가, Style 의 `choir aahs` 를 `choir pads` 로.

## 게임 연결 메모

- **일반 전투 (완료, 2026-10-14)**: 'Nia Unbound Battle'(148.96초)을 끊김 없이 도는 90.1초 루프로 잘라
  `/Game/Audio/BGM/BGM_Battle_NiaUnbound`(반복 재생, SC_BGM)로 넣고 `DA_BGMConfig` 의 `DefaultBattle` 이 가리키게 했다.
  - 루프 구간: 원곡 20.86초 ~ 110.97초(154 BPM, 231박). 같은 음악이 다시 나오는 지점을 마디 단위로 찾고, 끝점은 박이 들어오는 시각으로 맞췄다(43.5ms 보정).
    이음매는 마지막 한 박(0.39초)을 시작점 직전 한 박과 겹쳐 잇는다 — 이음매 앞뒤 박 간격 편차 최대 11ms(곡 안 다른 곳과 같은 수준).
  - 음량: 파일은 원곡 수준(-16 LUFS), 설정 음량은 다른 BGM 과 같은 0.3 그대로 → 실효 약 -26.5 LUFS, 마을 곡(약 -23.6)보다 약 3dB 낮다.
  - 파일: `ArtSource/Audio/Battle_20261014/BGM_Battle_Normal_Loop.wav`, 이음매 확인용 `BGM_Battle_Normal_Loop_seam_check.wav`(끝 8초 → 처음 8초를 두 번), 근거 `BGM_Battle_Normal_Loop.json`.
  - 만든 도구: `Tools/Audio_20261014/render_loop.py`(루프), `apply_battle_bgm.py`(임포트·설정). 설정 백업 `Saved/Audio_20261014/backup/`.
- **보스전 (완료, 2026-10-14)**: 'Nia Unbound Boss Battle'(148.80초, 약 173 BPM)을 61.3초 루프로 잘라
  `/Game/Audio/BGM/BGM_Battle_Boss_NiaUnbound`(반복 재생, SC_BGM)로 넣고 `DA_BGMConfig` 에 `BossBattle` 항목(음량 0.3, 페이드 1초)을 더했다.
  - 루프 구간: 원곡 33.81초 ~ 95.13초(176.5박). 리듬이 같은 모양으로 돌아오는 지연을 찾은 뒤, 이음매 앞 4초와 뒤 4초가 **둘 다** 맞는 시작점을 골랐다
    (먼저 찾은 27.1초 시작은 이음매 앞만 맞고 뒤가 어긋나 버림). 이음매 박 간격 편차 최대 12.5ms(곡 안 다른 곳 p90 12.5ms와 같음).
  - 음량: 원곡이 이미 -16.4 LUFS 라 -16 으로만 맞춤(일반 전투곡과 같은 기준) → 설정 0.3 으로 실효 약 -26.5 LUFS.
  - 파일: `ArtSource/Audio/Battle_20261014/BGM_Battle_Boss_Loop.wav`, 확인용 `BGM_Battle_Boss_Loop_seam_check.wav`, 근거 `BGM_Battle_Boss_Loop.json`.
  - 연결: `DefaultGame.ini` 의 `BossBattleBGMId=BossBattle`. 전투가 시작되면 `UNUBGMSubsystem` 이 적 아바타가 생길 때까지(보통 몇 프레임) 기다렸다가
    적 중에 **보스 나이아(나이아 표 Category=Boss, 지금은 1701 브램블코어)** 가 있으면 보스곡, 없으면 기존 순서(전투 세트 구역 BGM → `DefaultBattle`)로 고른다.
    보스곡이 전투 세트 구역보다 우선이라 어느 전투 배경에서 싸워도 보스곡이 나온다. 적이 1.5초·20프레임 넘게 안 뜨면 일반곡을 먼저 틀고 보스가 나오면 바꾼다. PIE 확인: 1701 → 보스곡 바로(일반곡 안 섞임), 1004 → 일반곡.
  - 코드: `Source/ProjectM/Audio/NUBGMSubsystem.*`(보스 판정·곡 선택), `NUBattleAvatar.h`(`GetNaiaId()` 한 줄), `NUAudioSettings.h`(주석).
  - 루프 도구: `render_loop.py` 에 BPM 힌트(6번째 인자)와 `LOOP_EXACT=1`(찾아 둔 시작·길이를 그대로 씀) 추가. `apply_battle_bgm.py` 는 `BGM_WAV/BGM_NAME/BGM_KEY` 로 곡을 바꿔 쓴다.
- 참고: `L_Battle_Village` 의 BGM 구역은 `BattleBGM` 라는 Id 를 쓰는데 설정 표에 그 항목이 없다(그 세트에서는 무음이 될 수 있음 — 확인 필요).
