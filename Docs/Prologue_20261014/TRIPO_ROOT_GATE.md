# Tripo 요청 — PR01 정령 숲의 뿌리 문 (Spirit Root Gate)

프롤로그 4번 그림(숲의 경계, 뿌리가 엇갈려 만든 아치)이 걷히고 **실제 게임 화면**으로 넘어갈 때,
그림 속 그 아치가 게임 안에도 서 있게 하려는 소품이다. 새 캐릭터가 이 문 아래에서 처음 걸어 나온다.
없어도 프롤로그는 동작한다(지금은 캐릭터가 도착 지점에서 마을 쪽으로 몇 걸음 걷는다). 모델이 오면 배치와 걷는 방향만 바꾼다.

## 치수 (게임 cm — 캐릭터 키 88cm 기준, 사람 실측 아님)

| 항목 | 값 |
|---|---|
| 전체 | 폭 380 × 깊이 170 × 높이 330 cm |
| 아치 안쪽 통로 | 폭 180 × 높이 230 cm (캐릭터가 머리 위로 여유 있게 지나감) |
| 바닥 | 뿌리 밑동이 지면 아래로 20cm 묻히도록(접지 이음매 숨김) |
| 피벗 | 통로 바닥 가운데, Z-up, 정면 = +Y (통로를 지나는 방향이 Y축) |

## 형태

- 거대한 고목 두 그루의 뿌리가 양쪽에서 솟아 위에서 서로 엇갈리며 자연스러운 아치를 만든다. 대칭이 아니게, 왼쪽이 조금 더 굵고 높다.
- 뿌리 표면: 굵은 껍질 결, 큰 덩어리 이끼 3~4군데, 뿌리 사이로 작은 틈.
- 아치 위쪽 뿌리 마디에 작은 버섯 무리 2곳(빛은 머티리얼에서 넣으므로 모델은 형태만).
- 단순하고 큰 덩어리 위주(도트 텍스처로 결을 보완). 잎·가는 잔가지·덩굴 줄기는 메시로 만들지 않는다.
- 닫힌 메시(뒷면·밑면 막힘). 통로 안쪽 면도 막혀 있어야 한다(카메라가 정면 위에서 내려다봄).

## 예산

| 항목 | 목표 |
|---|---|
| 소스 정점 | 1,800 ~ 3,000 |
| Quad 면 | 1,500 ~ 2,500 (Tripo Smart Mesh / Quad 켬) |
| 게임 삼각형 | 5,000 이하 |
| 텍스처 | 2K Base Color 하나(PBR 불필요), 게임에서 기존 HD-2D 소품 머티리얼로 도트·톤을 맞춤 |
| 충돌 | 뿌리 기둥 2개만 단순 박스(통로는 지나갈 수 있게) — 임포트 후 제가 만듭니다 |

## Tripo 프롬프트 (영문, 그대로)

```
A natural archway formed by the giant mossy roots of two ancient trees that rise from both sides and cross over each other at the top, stylized fantasy game prop, chunky simple shapes, thick bark grooves, a few large moss patches, two small mushroom clusters on the upper root knots, asymmetric (left root thicker and taller), walkable opening in the middle, closed watertight mesh, flat base, front view facing the camera, no leaves, no thin twigs, no vines, no text
```

- 이미지 → 3D 참고 그림(GPT 생성): `ArtSource/Prologue_20261014/tripo_refs/` — `tripo_gate_front.png`(회색 정면, 주 입력), `tripo_gate_right.png`, `tripo_gate_back.png`, `tripo_gate_textured_front.png`(같은 형태에 색).
  뒤쪽 그림은 좌우 굵기 차이가 약하게 나왔다(근사). 형태 기준은 정면이다.

## 받은 뒤 할 일 (제가 합니다)

1. Blender 에서 치수·피벗·법선·UV 확인, Y-up/Z-up 축 확인.
2. `/Game/NUA/Prologue/Models/SM_PR01_SpiritRootGate` 로 임포트, 기존 HD-2D 소품 톤 머티리얼 적용, 뿌리 기둥 충돌 박스.
3. 리베르그로브 새 캐릭터 도착 지점 옆에 배치(배치는 팀 맵 수정이라 확인 후).
4. `timeline.json` 의 `pawn_walk` 방향을 "문 아래에서 마을 쪽으로"로 바꾸고 다시 빌드.

## 받을 파일

`ArtSource/Prologue_20261014/tripo/PR01_SpiritRootGate.glb` (또는 FBX), Tripo 작업 링크.
