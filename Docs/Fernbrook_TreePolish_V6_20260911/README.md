# 펀브룩 나무 톤·스케일 폴리싱 V6

2026-09-11 · 대상: `L_Fernbrook_Entrance` · 88cm 플레이어 기준.

상태: 적용·저장·재로드 검증 완료. PIE 종료, 미저장 패키지 0개. 다리 왕복, 숲길 왕복, 나이아 접근 총 87/87개 이동 지점 통과. 기존 마을 관련 709개 파일과 V5 원본 에셋의 SHA256은 변경되지 않았습니다.

사용자가 표시한 FB13 작은 나무의 밝은 회백색 잎과 가는 실루엣을 주변 FB12 숲에 맞춘 작업입니다. 기존 마을 두 곳, 꽃·풀·덤불, 다리, 수면, 지형 및 게임 카메라 설정은 수정 대상이 아닙니다.

## 변경 내용

| 대상 | 수량 | 크기 변경 | 배치된 높이 중앙값 | 색상 방향 |
|---|---:|---:|---:|---|
| FB12 큰 나무 | 513 | 92% | 4.57 → 4.21m | 기존 녹색을 유지하며 약한 따뜻한 톤 |
| FB13 작은 나무 | 311 | 130% | 1.69 → 2.20m | 밝은 회녹색을 짙은 녹색으로, 줄기를 따뜻한 갈색으로 |

균일한 메시 빌드 스케일을 사용했습니다. 나무를 삭제하거나 다시 심지 않았으며 전체 8,981개 식생의 위치·회전·배치 스케일을 유지합니다. 작은 나무는 여전히 어린 나무로 읽히되 큰 나무와 높이 차이를 완화했습니다.

원본 V5 모델·텍스처·머티리얼 대신 별도 V6 파생 에셋을 사용합니다. UV, 노멀, 삼각형 인덱스와 정점 수는 유지하며 줄기 충돌 캡슐도 동일 비율로 조정됩니다. 확대된 나무에는 렌더링 경계를 보수적으로 확장하여 화면 가장자리의 잘림을 방지했습니다.

## 조절할 머티리얼

폴더: `/Game/Fernbrook/Polish_V6/Materials/`

- `M_FB6_HD2D_Tree`: 기존 트리 셰이더에서 파생한 나무 전용 마스터. 텍스처의 색상으로 잎/줄기를 부드럽게 구분합니다. 수작업 ID 마스크가 아니므로 녹색 이끼 등에도 잎 보정이 일부 적용될 수 있습니다.
- `MI_FB6_Tree_Controls`: 공통 `MaterialTint`, `PixelGrid`, `PixelInfluence`, `TreeVariation`, `Roughness` 조절.
- `MI_FB6_Canopy`, `MI_FB6_Sapling`: 수종별 `LeafTint`, `BarkTint`, `LeafSaturation`, `LeafContrast` 조절. 이 네 값은 자식 인스턴스에서 별도 지정되어 있습니다.

`StyleInfluence=0`, `TreePaletteBlend=0` 유지. 적은 색상 수로 잘라내는 포스터화/CRT 효과는 사용하지 않습니다. 원본 텍스처의 음영과 색상 변화를 유지하고 픽셀 UV 스냅을 적용합니다. 공유 식생 머티리얼은 수정하지 않아 꽃·풀에는 영향을 주지 않습니다.

## 실제 화면

[사용자 시점 수정 전](Preview/Trees_Before_Current_001.png) · [동일 시점 수정 후](Preview/Trees_After_Current_001.png) · [전체 배치](Preview/Trees_Overview_V6_001.png)

[실제 플레이: 입구](Preview/Game_Entry_V6_001.png) · [실제 플레이: 다리](Preview/Game_Bridge_V6_001.png) · [나이아 가림 확인](Preview/Game_Naia_V6_001.png) · [88cm 높이 숲길 시점](Preview/Trees_TrailEye_V6_001.png)

PNG는 언리얼에서 직접 촬영한 무보정 이미지입니다. `_UNRELIABLE_COLOR` 이름의 파일은 색상 검토에서 제외합니다. `Trees_BridgeSide_V6_001`은 앞쪽 잎이 카메라를 가린 실패 구도이며 결과 대표 이미지로 사용하지 않습니다.

검증 수치·완료 여부의 최종 근거는 `final_signoff.json`, `final_readback_handoff001.json`, `GameplayQA/summary_V6_001.json`입니다. `meshes_001.json`의 `size`는 빌드 직후 UE의 갱신되지 않은 소스 경계이므로 실제 크기는 최종 readback의 `render_size_cm`을 사용합니다. 최초 두 `Current` 캡처의 큐 로그에는 기본 다리 카메라 수치가 출력되었지만 실제 캡처는 사용자 시점을 유지했습니다. 실제 시점은 `baseline.json`의 `view`와 최종 readback에 기록되어 있습니다.

원본 복구 스냅샷: `Saved/FernbrookRecovery/V6_TreePolish_Before_20260911`.

나이아 쉼터의 전경 나뭇잎에 의한 캐릭터 가림은 별도 가시성 폴리싱 항목입니다. 이번 톤·스케일 조정이 그 문제까지 해결했다는 뜻은 아닙니다. 자동 LOD, 모바일 성능, 패키징 검증은 포함하지 않습니다. Git 커밋/푸시는 하지 않았습니다.
