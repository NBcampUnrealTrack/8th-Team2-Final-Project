# 펀브룩 V5 — 텍스처 모델 적용 및 다리 보정

2026-09-11. **반영·저장·재로드 검증 완료 / 추가 아트 폴리싱 항목 있음.**

대상 레벨: `/Game/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance`

공식 Reddy 프로젝트 경로의 Revvy **Full** 인증을 확인한 뒤 직접 편집했다. 별도 프록시, GUI 실행 우회, 인증 필터 변경은 하지 않았다. 과거 연결 대기 기록보다 이 문서와 `final_signoff.json`이 우선한다.

## 이번 반영

- Blender 텍스처 모델 FB07–13 7종을 기존 마을·88cm 플레이어 기준의 게임용 크기로 적용. 실제 성인 인체 비례가 아니다. 원본 25오브젝트와 텍스처는 보존했다.
- 새 나무 2종으로 큰 나무 513개·작은 나무 311개를 교체했다. 기존 식생 8,480개의 위치·회전·스케일·인덱스는 그대로다.
- 흰 야생화 143개, 연노랑 야생화 55개, 넓은잎 잡초 A 197개·B 106개를 군락으로 추가했다. 총 501개, 충돌 없음, 거리 컬링 65m, 밀도 스케일링 사용.
- 다리를 기존 방향 그대로 24cm 올려 목재가 흙에 묻히지 않게 했다. 바닥 높이는 Z=124cm이며, 지형 샘플 기준 목재 최하단 외접 범위도 지면보다 최소 약 4.17cm 위에 있다.
- 긴 쐐기형 목재 진입판 2개를 짧은 돌 진입부로 재사용하고, 추가 디딤돌 4개와 하부 돌 받침 4개를 연결했다. 기존 임시 다리 81개 파트는 삭제하지 않고 숨김·충돌 해제했다.
- 기존 두 마을, 지형, 물 머티리얼, 게임 카메라와 포스트 프로세싱은 이번 적용에서 변경하지 않았다. 실제 게임용 나이아 Aqupong 1010도 보존했다.

## 확인한 결과

- 최종 레벨: **241 액터 / 14종 식생 타입 / 8,981 인스턴스**.
- 7종 UE LOD0의 위치·삼각형·UV·노멀 데이터가 최초 임포트와 정확히 일치한다. 비정상 좌표, UV 범위, 노멀 길이 검사 통과. 치수와 렌더 정점 수는 [asset_runtime_stats.csv](asset_runtime_stats.csv).
- 원본 Blender 합계 4,462 정점 / Quad 3,053개 + 기존 Triangle 1,919개. UE 렌더 합계 10,481 정점 / 8,025 삼각형. UV·노멀 경계 분리로 렌더 정점 수가 늘어난 것이며, 원본 Quad를 임의로 삼각분할해 덮어쓰지 않았다.
- 신규 지피식생 전부 실제 지형 히트 확인. 기준 높이와 오차 최대 0.83cm 미만. 첫 배치에서 오브젝트 위에 놓인 후보는 해당 신규 배치 트랜잭션만 되돌린 후 제외했다. 최종 계획은 `planting_plan_v5_003.json`.
- 실제 이동 입력 검사: 다리 왕복·필드 왕복·나이아 접근 **87/87**. 최종 식생 적용·저장·재로드 후 다리 왕복·나이아 접근 **39/39** 재통과. 경로 시작점만 텔레포트했으며 검사 중 경로는 실제 이동했다.
- 플레이 카메라 Pitch -30° / Yaw 90° / FOV 38° 유지. 편집 카메라와 임시 캡처용 변경은 복원했다. 최종 PIE 종료, 미저장 패키지 0개.
- 보호 대상 기존 마을·라이브러리 파일 709개 SHA256 동일. Git 커밋·푸시 없음.

## 실제 화면

각 PNG는 Unreal의 원본 캡처이며 이미지 생성·보정 결과가 아니다.

- [다리 플레이 화면](Preview/Game_Bridge_V5_Final_001.png)
- [동쪽 받침·진입부](Preview/Bridge_East_Final_005.png)
- [서쪽 연결부](Preview/Bridge_West_Final_005.png) — 일부 전경 나무가 가린다.
- [입구 플레이 화면](Preview/Game_Entry_V5_Final_001.png)
- [나이아 쉼터](Preview/Game_Naia_V5_Final_001.png) — 아래의 미완료 시야 문제 확인용.
- [레벨 배치 개요](Preview/Overview_V5_001.png) — 개요 촬영 중에만 거리 컬링을 늘렸으며 이후 복원했다.

`*_UNRELIABLE_COLOR.png`는 기존 Revvy 캡처의 색 채널 문제 때문에 최종 색 판단에 사용하지 않는다. `Bridge_Seated_002`는 중간 상태이며 최종 화면이 아니다.

## 일괄 조절용 머티리얼

위치: `/Game/Fernbrook/Polish_V5/Materials/`

| 에셋 | 대상 / 주요 값 |
|---|---|
| `MI_FB5_Vegetation_Controls` | 신규 나무·꽃·잡초 공통. PixelGrid 1024, StyleInfluence 0, TreePaletteBlend 0, TreeVariation 0.28, Roughness 0.94 |
| `MI_FB5_Wood_Controls` | 신규 다리. PixelGrid 1024, StyleInfluence 0, Roughness 0.91 |
| `MI_FB07_Textured_V5`–`MI_FB10_Textured_V5` | 각 원본 식물 텍스처. PixelGrid 512 개별 재정의 |
| `MI_FB11_Textured_V5`–`MI_FB13_Textured_V5` | 다리·나무 원본 텍스처와 공통 컨트롤 상속 |
| `MI_FB5_BridgeStone` | 다리 받침·디딤돌. 기존 Rock MI의 자식, StyleInfluence 0, TileCm 32 |

기존 HD2D 마스터를 재사용하는 **독립 자식 MI 계층**이다. 기존 마스터 그래프를 덮어쓰지 않았다. 팔레트 색 단계 축소는 꺼 두었으며, 픽셀 UV 스냅은 유지한다. 작은 식물 4종은 PixelGrid를 재정의했으므로 공통 PixelGrid만 바꿔도 그 값은 바뀌지 않는다.

## 남은 항목 / 적용 범위

- **나이아 쉼터 전경 수관이 플레이어와 나이아를 가리는 문제는 남아 있다.** 통행 성공과 시야 확보는 별개다. 수관 배치 조정 또는 제한적인 카메라 가림 처리가 다음 폴리싱 대상이다. 전체 아트 완성·승인으로 보지 않는다.
- 자동 생성한 LOD1/2가 UV 경계를 외삽해 제외했다. 현재는 원본 저폴리 LOD0 1단계와 거리 컬링만 사용한다. `game_assets_001.json`의 최초 3LOD 수치는 과거 기록이며 `autolod_rejected_001.json`과 `final_readback_handoff001.json`이 최종이다.
- Windows 에디터/PIE 검증이며 모바일 패키징·실기 성능·최종 LOD 최적화는 하지 않았다.
- 물은 기존 V3 Single Layer Water를 유지했다. 이번 다리 화면에서 이어져 보이지만, 과거 보고된 모든 수면 끊김을 재현·해결했다는 뜻은 아니다.
- 신규 게임플레이, 퀘스트·계약 기능, 런타임 카메라 가림 시스템은 추가하지 않았다.

## 원본·복구·검증 파일

- Blender 원본: `Blender/Source/Fernbrook_UserTextured_Source.blend`
- 게임 크기 작업본: `Blender/Fernbrook_Textured_GameScale_V5.blend`
- 7FBX·7GLB 및 텍스처: `Blender/Export/`, `Blender/Textures/`
- 원본 보존/14개 재임포트: `Blender/source_preservation.json`, `Blender/roundtrip_qa.json`
- 엔진 전체 변경 전: `Saved/FernbrookRecovery/V5_Resume_Before_20260911/` (프로젝트 루트 기준)
- 다리 추가 피드백 직전: `Saved/FernbrookRecovery/V5_BridgeFeedback_20260911/`
- 다리 기준: `bridge_seated_002.json`, 지형·방향 사전 검사: `bridge_feedback_before_001.json`
- 저장 후 독립 검사: `final_readback_reload001.json`, 최종 검사: `final_readback_handoff001.json`
- 파일 해시·적용 범위·이동 검증: `final_signoff.json`, `GameplayQA/`

`Tools/Fernbrook_Textured_V5_20260911/`는 단계별 작업 이력이다. 기존 산출물을 덮어쓰거나 전체 스크립트를 처음부터 일괄 재실행하지 않는다.
