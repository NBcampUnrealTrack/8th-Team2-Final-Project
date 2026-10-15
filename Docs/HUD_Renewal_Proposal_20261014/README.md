# 인게임 HUD 리뉴얼 시안 (2026-10-14, 제안 단계)

`hud_proposal.html` 하나에 모두 들어 있습니다(그림·글꼴 포함, 브라우저로 열기).

- 01 지금 HUD에서 고칠 점: 필드·전투 캡처에 번호를 매겨 정리했습니다.
- 02 설계 원칙: 색, 2px 격자 아이콘, 해상도 규칙.
- 03 시안 3종을 같은 장면에서 비교합니다(A 글래스 엣지 추천, B 모험가 수첩, C 투명 오버레이).
- 04 A안의 상황별 화면: 사냥터 진입, 채팅 열기, 전투 행동 선택, 패링.
- 05 A로 갈 때의 구성과 순서.
- 06 정해 주실 것.
- 07 HUD 밖에서 함께 본 것.

`png/`에는 시안마다 1920x1080 그림이 있고, `_page.png`는 페이지 전체입니다.

## 다시 만들기

1. 캡처: `Tools/CharacterCreateGlass/run.ps1 -Steps shot -States <states_hud_*.json> -Map <맵>`
   - `states_hud_village.json`: `/Game/Libregrove/Level_V8/Maps/L_Libregrove`
   - `states_hud_town.json`(전투 포함): 같은 맵
   - `states_hud_forest.json`: `/Game/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance`
   - 상태에 `"noui": true`를 주면 위젯 없이 장면만 찍습니다.
2. 지도 그림: `Tools/HUDRenewal/export_map_textures.py`(커맨드렛, 읽기만 함)
3. 아이콘: `python Tools/HUDRenewal/make_icons.py`
4. 페이지: `python Tools/HUDRenewal/build_proposal.py`
5. PNG: `python Tools/HUDRenewal/render_png.py`

중간 재료(캡처, 지도, 초상, 게임 글꼴에서 꺼낸 TTF)는 `Saved/HUDRenewal/`과 `Saved/CharacterCreateGlass/hud_*`에 있습니다.

## 알아 둘 것

- 시안의 이름·수치·퀘스트 문구·키 표시는 자리 채움입니다.
- 게임 에셋과 코드는 바꾸지 않았습니다.
- 유리 패널을 `transform`으로 가운데 정렬하면 배율이 걸린 캔버스 안에서 흐림이 엉뚱한 곳을 집으므로, 가운데 정렬은 감싸는 줄(`.crow`)로 합니다.
