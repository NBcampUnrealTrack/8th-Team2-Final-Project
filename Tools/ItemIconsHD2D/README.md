# 아이템 아이콘 HD-2D 톤 다시 그리기 (2026-10-14)

지금 있는 아이템 아이콘 90종(`/Game/Developers/dbals/UI/ItemIcons/T_Item_*`)을 게임 캐릭터 도트와 같은 톤으로 새로 그려 같은 이름의 텍스처로 덮어썼다.
인벤토리·상점·장비·보상 등 아이콘을 쓰는 모든 화면에 그대로 반영되고, 참조는 하나도 바뀌지 않는다.

## 톤 규칙 (`style.txt`)
- 캐릭터 필드 스프라이트(`_ref/style_player.png`, 노비스 몸)를 스타일 기준 그림으로 붙인다: 1px 검은 외곽선, 차분한 따뜻한 팔레트,
  3~4단 셀 음영, 왼쪽 위 빛, 작은 하이라이트 하나.
- 아이템 하나, 가운데, 살짝 3/4 구도, 마젠타 단색 배경(빼기 쉬움).
- 무엇을 그릴지는 dbals 님 `Scripts/BuildIconPrompts.py`의 아이템별 문구(INDIVIDUAL, 장비 계열+등급)를 그대로 쓰고, 거기 없는 26종은
  `make_prompts.py` 의 EXTRA 문구. 옛 아이콘 그림도 "무엇인지"만 알려 주는 두 번째 참고로 붙인다.

## 순서
1. `python Tools/ItemIconsHD2D/make_prompts.py` --- 아이콘마다 프롬프트 (`prompts/`)
2. `bash Tools/ItemIconsHD2D/gen_all.sh 3` --- GPT(Codex 이미지 도구)로 3개씩 그림. 이미 있는 것은 건너뜀 (`ArtSource/ItemIcons_HD2D/_gen`, 커밋 제외)
3. `python Tools/ItemIconsHD2D/finish_icons.py` --- 게임 아이콘으로 정리
   - 마젠타를 가장자리에서 흘려 지움(외곽선 안의 분홍 아이템은 남는다)
   - 그림의 실제 도트 크기 = 한 색 구간 길이의 최빈값(자기상관은 2배 크기에 잘못 잡히는 일이 있었다), 그 격자의 가운데 색을 하나씩 뽑아 원래 도트로 되돌림
   - 거리 22 안의 비슷한 색만 묶음(빈도순 median cut 은 하이라이트·그늘을 먹는다)
   - 48칸 격자 가운데에 1:1로 놓음 → 모든 아이콘 도트 크기가 같다. 48칸을 넘는 긴 검·활·지팡이만 칸마다 많은 색으로 줄임
   - 3배 최근접 확대 = 144px (`ArtSource/ItemIcons_HD2D/T_Item_*.png`), 48칸 원본 `_native/`, 검수용 `_sheet.png`
4. 언리얼 임포트: `UnrealEditor-Cmd ProjectM.uproject -run=pythonscript -script=Tools/ItemIconsHD2D/import_icons.py`
   --- 옛 .uasset 은 `Saved/ItemIconsHD2D/backup` 에 한 번 복사, 옛 텍스처 설정 유지(144는 2의 거듭제곱이 아니라 밉 없음).

## 참고
- 생성 그림은 "32x32"를 시켜도 약 47칸(1254px에서 칸 약 26px)으로 그린다. 32칸에 억지로 줄이면 줄이 빠져 깨진다 --- 48칸에 그대로 둔다.
- 옛 아이콘 원본은 `ArtSource/ItemIcons_Pixel` 에 그대로 있다(되돌릴 때). 아이콘이 없는 아이템(표에는 있으나 그림 없음)은 이번 범위가 아니다.
