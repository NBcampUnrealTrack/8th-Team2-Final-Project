# GPT 그림 — 프롤로그 그림책 4장 + 뿌리 문 참고 그림

## 상태 — 완료 (2026-10-14 07:25~07:45)

- 4장 모두 GPT(Codex `gpt-6.1-sol` 내장 image_gen)로 생성, 1장씩 확인 후 채택(재생성 0회). 원본 `ArtSource/Prologue_20261014/gen/p1~p4*.png`(1672×941),
  게임용 `final/T_Prologue_0N_*.png`(704×396). PIE 캡처 `Saved/Prologue_20261014/qa4/`.
- P1 을 먼저 만들고, P2~P4 는 P1 을 세 번째 참조로 붙여 같은 화풍으로 만들었다.
- 도트 처리: 704×396 로 줄여 격자만 고정하고 색 수는 줄이지 않았다(48~96색으로 묶으면 정령 빛의 분홍·청록이 베이지로 뭉개짐).
- Tripo 뿌리 문 참고 그림 4장(정면·오른쪽·뒤 회색 + 색 정면): `ArtSource/Prologue_20261014/tripo_refs/`. 뒤쪽 그림은 왼쪽·오른쪽 굵기 차이가 약하다(근사 참고).
- 막혔던 원인: Codex 로그인 세션. 토큰은 유효(Pro, 10/23까지)·모델 목록에도 Sol 이 있었지만 요청이 400 "not supported when using Codex with a ChatGPT account" 로 거부됐다.
  `codex logout` → `codex login` 으로 다시 로그인하자 바로 풀렸다.
- 생성 이미지는 `CODEX_HOME`(Reddy 계정 폴더 `.../reddy-ide/codex-accounts/<id>/home`)의 `generated_images/<thread id>/` 에 저장된다. `gen_image.sh` 가 거기서 가져온다.

## 다시 만들 때

```
bash Tools/Prologue_20261014/gen_image.sh ArtSource/Prologue_20261014/prompts/full_p1.txt <출력 PNG> refs/libregrove_game_ref.png refs/naia_sprites_ref.png
```

P1 을 먼저 만들고, 확정된 P1 을 세 번째 참조로 붙여 P2~P4 를 같은 화풍으로 만든다.

## 직접 ChatGPT 에서 만드실 경우

1. 새 대화에 첨부: `ArtSource/Prologue_20261014/refs/libregrove_game_ref.png`, `refs/naia_sprites_ref.png` (P2~P4 는 확정된 P1 도 함께).
2. 붙여넣을 글: `prompts/_style.txt` 전체 + 그림별 `prompts/p1_cradle.txt` … `p4_threshold.txt`. (index.html 에 합쳐진 글과 복사 버튼이 있다.)
3. 가로 그림(3:2 또는 16:9)으로 받는다. 위아래는 잘려도 되게 지시해 두었다.
4. 받은 원본을 `ArtSource/Prologue_20261014/gen/` 에 `p1_cradle.png`, `p2_child.png`, `p3_book.png`, `p4_threshold.png` 로 저장.

## 받은 뒤 (제가 실행)

```
python -I Tools/Prologue_20261014/process_art.py ArtSource/Prologue_20261014/gen/p1_cradle.png T_Prologue_01_Cradle
python -I Tools/Prologue_20261014/process_art.py ArtSource/Prologue_20261014/gen/p2_child.png T_Prologue_02_Child
python -I Tools/Prologue_20261014/process_art.py ArtSource/Prologue_20261014/gen/p3_book.png T_Prologue_03_Book
python -I Tools/Prologue_20261014/process_art.py ArtSource/Prologue_20261014/gen/p4_threshold.png T_Prologue_04_Threshold
UnrealEditor-Cmd.exe ProjectM.uproject -run=pythonscript -script=Tools/Prologue_20261014/build_prologue.py
```

`process_art.py` 는 가운데 16:9 로 잘라 704×396 도트 격자·48색으로 다시 앉힌다(게임은 정수 배율로만 확대해 도트가 고르게 보임).
`build_prologue.py` 는 `final/` 에 그림이 있으면 임시 카드 대신 그것을 쓴다.

## 그림 목록

| ID | 문장 | 그림 | 프롬프트 |
|---|---|---|---|
| P1 | 태어난 지 얼마 되지 않아 버려진 아이. / 그런 당신을 키운 것은 정령들이었습니다. | 밤, 고목 뿌리 사이 요람(아기 손만), 모여드는 정령의 빛, 요람 옆 낡은 책 | `prompts/p1_cradle.txt` |
| P2 | 사람 하나 없는 정령들의 세계에서 당신은 자랐죠. | 아침 숲 공터, 뒷모습의 다섯 살 아이가 정령들과 뛰논다. 사람·건물 없음 | `prompts/p2_child.txt` |
| P3 | 인간의 언어도 배웠습니다. | 늦은 오후, 열 살 아이가 요람의 그 낡은 책을 펼치고 정령들이 등불처럼 비춘다 | `prompts/p3_book.txt` |
| P4 | 그리고 어른이 된 당신은, / 처음으로 사람의 세계로 나왔습니다. | 새벽, 뿌리 아치 아래 두건 쓴 여행자 뒷모습, 아래로 사람 마을. 정령들은 숲에 남아 배웅 | `prompts/p4_threshold.txt` |
| G1~4 | (Tripo 참고) 뿌리 문 정면·오른쪽·뒤 회색 + 색 정면 | | `prompts/tripo_gate_*.txt` |

연출 의도: 정령은 실제 게임 나이아 6종(아쿠퐁·볼토·푸루나·불코이·토그라·휘리)을 스프라이트 디자인 그대로 그린다(사용자 요청, 2026-10-14).
참고 시트 `refs/naia_sheet_large.png`(게임 스프라이트 첫 프레임 6배 확대). 그림마다 나이아가 아이를 키우는 가족처럼 등장하고, 마지막 컷에서 휘리가 여행자를 따라 나선다.
'인간의 언어를 어떻게 배웠나'는 대사로 설명하지 않고, P1 요람 옆의 책이 P3 에서 다시 나오는 것으로 보여 준다.
첫 버전(정령을 형체 없는 빛으로 그린 안)은 `gen/v1_wisps/`, `final/v1_wisps/`, `prompts/v1_wisps/` 에 보관.
