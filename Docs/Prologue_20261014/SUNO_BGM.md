# 프롤로그 BGM — Suno 요청서

게임 시작 프롤로그(약 66초)에 깔리는 곡. 버려진 아기를 나이아 여섯(아쿠퐁·볼토·푸루나·불코이·토그라·휘리)이 키우고,
아이가 그들과 뛰놀며 자라, 어른이 되어 사람의 마을로 걸어 나가는 이야기다. 그림책 4장 → 실제 게임 화면 → 마을 전경 → 타이틀 순서.
곡이 오면 박자에 맞춰 `Tools/Prologue_20261014/timeline.json` 의 컷 시각을 다시 맞춘다(곡을 화면에 억지로 맞추지 않아도 됨).

## 생성 설정

| 항목 | 값 |
|---|---|
| 모드 | Custom, **Instrumental 켬** |
| 모델 | 가장 최신 버전 |
| 길이 | 70초 안팎이면 충분. 길게 나오면 Suno 의 Crop 으로 0:00~1:08 만 남기고 자연스럽게 끝나는 버전을 고른다 |
| 받을 파일 | WAV (없으면 MP3 320k) |
| 넣을 곳 | `ArtSource/Prologue_20261014/audio/BGM_Prologue_Spirit.wav` — 넣으면 `build_prologue.py` 한 번으로 임포트·연결된다(WAV 임포트 시험 통과). 박자 맞춤은 제가 합니다 |

## Style of Music (그대로 붙여넣기)

```
instrumental, cozy fantasy JRPG prologue, storybook lullaby that grows into a warm hopeful theme, music box and celesta lullaby, playful pizzicato strings, ocarina and wooden flute, marimba water-drop accents, concert harp, soft solo piano, warm cello, gentle strings swell, light hand percussion and woodblock, glockenspiel bells, 76 BPM, D major, whimsical, tender, nostalgic, heartwarming, hand-drawn animation film score, intimate small orchestra, clean mix, no vocals
```

## Exclude Styles

```
vocals, choir, lyrics, rap, EDM, trap, dubstep, rock drums, electric guitar, distortion, heavy bass, synth lead, epic trailer, dark, horror, sad minor
```

## Lyrics 칸 (구조 지시만, 가사 없음)

```
[Instrumental]

[Intro - night forest lullaby: music box and celesta alone, a soft low cello hum like a small campfire, 0:00-0:14]

[Verse - playful chase with little creatures: pizzicato strings, ocarina, marimba water drops, woodblock taps, bouncy and cheeky, 0:14-0:24]

[Interlude - reading together at golden hour: harp and solo piano play the main lullaby melody, cello answers softly, 0:24-0:34]

[Build - dawn, growing up: sustained strings rise slowly, flute holds a long note, 0:34-0:40]

[Lift - first step into the human world: the lullaby melody opens up in warm strings and wooden flute, 0:40-0:46]

[Chorus - a village where people and creatures live together: gentle folk orchestra plays the main theme, harp arpeggios, light hand drum, bright but restrained, 0:46-0:58]

[Climax - title: glockenspiel and bell accent, held major chord, 0:58-1:04]

[Outro - the last chord rings out with a music box echo of the lullaby, 1:04-1:08]

[End]
```

## 간단 모드(Song Description)로 할 때 (200자 이내)

```
Instrumental cozy fantasy RPG prologue: a music-box lullaby in a night forest grows into a playful, warm folk-orchestra theme as a child raised by cute spirit creatures steps into a village.
```

## 화면과 맞출 지점

| 시각(초) | 화면 | 음악이 해 줬으면 하는 것 |
|---|---|---|
| 0 ~ 2 | 검은 화면 | 숨 고르기(거의 무음) |
| 2 ~ 14 | 그림 1: 고목 뿌리 사이 요람, 나이아 여섯이 아기를 지킴, 불코이의 불빛 | 오르골·첼레스타 자장가, 낮은 첼로(모닥불 같은 온기) |
| 14 ~ 24 | 그림 2: 아이가 나이아들과 술래잡기(구름 탄 볼토, 물장구 아쿠퐁, 굴에서 튀어나온 토그라) | 피치카토·오카리나·마림바 물방울·우드블록, 통통 튀게 |
| 24 ~ 34 | 그림 3: 아이가 요람의 책을 읽고, 나이아들이 둘러앉음 | 하프·피아노로 자장가 선율, 첼로가 화답 |
| 34 ~ 40 | 그림 4: 뿌리 아치 아래 어른이 된 여행자, 나이아들이 배웅 | 현이 천천히 부풀어 오름(새벽) |
| 40 ~ 46 | 그림책이 걷히고 **실제 게임 화면**, 캐릭터가 남문으로 걸어 들어감 | 자장가 선율이 활짝 열림 |
| 46 ~ 58 | 마을 광장, 사람과 나이아가 함께 | 포크 오케스트라 주제, 밝지만 절제 |
| 58 ~ 64 | 타이틀 로고 | 글로켄·종 한 번, 장화음으로 머묾 |
| 64 ~ 66 | 검게 닫힘 → 게임 시작 | 잔향 + 오르골 메아리로 끝 |

악기와 나이아를 살짝 짝지어 두었다: 아쿠퐁(물) 마림바 물방울 · 볼토(번개 구름) 첼레스타·글로켄 반짝임 · 푸루나(풀) 오카리나·나무피리 · 불코이(불) 따뜻한 첼로 · 토그라(땅) 우드블록·핸드드럼 · 휘리(바람) 플루트의 긴 음.

## 변주안 (첫 결과가 안 맞을 때)

- **B — 켈틱 포크**: Style 앞부분을 `instrumental, cozy fantasy prologue, celtic folk, tin whistle and harp lullaby, playful fiddle pizzicato, warm cello, bodhran near the end, storybook, hopeful` 로 바꾼다.
- **C — 오르골·피아노 미니멀**: `instrumental, music box and felt piano lullaby, soft string quartet, slow build to a warm major resolve, storybook, intimate, tender` 로 바꾼다.
- 결과가 너무 웅장하면 Style 에 `small ensemble, quiet, no big percussion` 을 더한다. 너무 처지면 `76 BPM` 을 `84 BPM` 으로.
- 타이틀 화면 곡(`BGM_Title_Dawn_Sacred_Breath`)과 같은 조로 맞추고 싶으면 Style 의 `D major` 를 그 곡의 조로 바꾼다.
