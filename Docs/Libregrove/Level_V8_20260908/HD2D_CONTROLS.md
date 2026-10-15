# 리베르그로브 V8 — 그래픽 조절 가이드

2026-09-09: 머티리얼 적용, 실제 카메라 동작, 6개 주요 보행 경로 검수 완료. Windows 에디터 PIE 기준이며 모바일 빌드/성능 인증은 별도다.

## 전체 색감과 도트

콘텐츠 브라우저에서 `/Game/Libregrove/Level_V8/HD2D/MPC_LG8_HD2D`를 열고 기본값을 조절한다. 공통 색 함수와 6종 활성 마스터를 공유하는 38개 인스턴스를 V8 레벨의 건물·소품·수목·지형에 연결했다. 원본 텍스처·Static Mesh의 기본 재질은 바꾸지 않았다. 지형 작업 백업용 마스터와 인스턴스 각 1개는 별도로 남겨두었다.

| 설정 | 기본값 | 역할 / 조절 시작 범위 |
| --- | ---: | --- |
| StyleStrength | 1 | 공통 색보정·픽셀 샘플링 강도, 0–1 |
| PixelBlend | 0.90 | 픽셀 형태를 드러내는 강도, 0–1 |
| TextureDensity | 1 | 클수록 세밀한 픽셀, 0.5–2 |
| WorldPixelSizeCm | 3 | 지형·월드 정렬 소품의 픽셀 간격, 2–6 cm |
| PaletteSteps | 24 | 채널별 색 단계, 12–48; 실제 색상 개수 제한 팔레트는 아님 |
| PosterizeStrength | 0.55 | 색 단계 효과 혼합량, 0–1 |
| Saturation | 0.93 | 채도, 0.8–1.1 |
| Contrast | 1.07 | 명암 대비, 0.9–1.15 |
| ShadowLift | 0.025 | 어두운 색을 살리는 정도, 0–0.06 |
| GlobalTint | 1.02 / 1 / 0.95 | 공통 RGB 색조 |
| RoughnessBias | 0.02 | 건물·지형·소품 거칠기 보정 |
| NormalStrength | 0.55 | 연결된 노멀맵 강도; 선택 기능을 켠 경우에만 적용 |
| EmissiveMultiplier | 1 | 발광 배율; 등불 소품·하늘·폭포도 영향 받음 |

`StyleStrength=0`은 새 스타일 효과를 비교하기 위한 값이다. 원래 모든 머티리얼 설정을 복원하는 스위치는 아니다. 원본 할당은 `HD2D/assignment_backup.json`에 보존되어 있다.

건물·나무 인스턴스는 `/HD2D/Instances/MI_HD2D_LG_*`에서 찾는다. `BaseColorTexture`는 사용자 Tripo 텍스처를 그대로 사용한다. `PixelGrid`가 작을수록 더 굵은 도트다(건물·나무 256, 일반 UV 소품 128부터 시작). `StyleInfluence`, `PixelInfluence`, `MaterialTint`, `Roughness`, `Metallic`은 개별 재질 조절이다. `UseNormalMap`은 기본 꺼짐이며, 기존 소품 노멀맵을 선택적으로 켤 수 있다.

현재 지형은 `M_LG8_HD2D_Landscape_Stable` / `MI_HD2D_Terrain_Stable`를 사용한다. 접미사 없는 Landscape 마스터/이전 인스턴스는 작업 백업이다. 월드 정렬 소품은 `M_LG8_HD2D_WorldProps`, 물·하늘·폭포는 각각 별도 마스터다. 원래 지형 레이어·물 흐름·하늘 구성을 보존한다. 물과 하늘에는 건물용 UV 픽셀 크기가 적용되지 않는다. 공통 색보정의 기본 영향도는 하늘 0.3, 물·폭포 0.65로 낮췄다.

픽셀 샘플 위치와 별도로 원래 UV의 미분값을 사용해 밉 선택을 유지한다. 화면 전체를 저해상도로 만드는 필터는 없으므로 UI를 도트화하지 않는다. 다만 최종 움직임·대상 플랫폼에서의 앨리어싱 검증은 필요하다.

## 카메라

`/Game/Libregrove/Level_V8/Gameplay/BP_LG8_FieldCharacter`의 Class Defaults:

- `LG_CameraRotation`: Pitch -30°, Yaw 90°, Roll 0°
- `LG_CameraDistance`: 1900 cm
- `LG_CameraFOV`: 38°

새 마을 전용 GameMode가 이 캐릭터를 사용한다. BeginPlay에서 실제 캐릭터 컴포넌트에 적용하며, 원본 캐릭터 Blueprint와 네이티브 컴포넌트 템플릿은 수정하지 않는다. 이동 중 시점 회전·카메라 충돌 줌·지연은 꺼져 있다. 실제 PIE에서 각도·거리·FOV를 읽어 검증했다. 기존 테스트 캐릭터 외형은 그대로다.

## 포스트 프로세스

월드 아웃라이너의 `Libregrove/Environment`:

- `LG8_PP_ArtGrade`: 노출 고정, 노출 보정 0.30, Bloom 0.40 / Threshold 1.10, Vignette 0.13. 모션 블러·색수차·필름 그레인 0. 색보정과 조명 조화용이다.
- `LG8_PP_DOF_Optional_Windows`: 선택형 심도. Focus Distance 1900 cm, F-stop 2.4, Sensor Width 120 mm. 큰 센서는 의도적인 미니어처 연출 값이다. `Blend Weight=0`이면 끈다.

카메라 거리를 바꾸면 DOF의 Focus Distance도 함께 맞춘다. 너무 흐리면 F-stop을 올리거나 Sensor Width를 줄인다. 이 두 값은 실제 물리 카메라 재현보다 길·캐릭터 가독성을 우선해 조절한다.

`Game_SquareNight_Final_003.png`는 런타임 광원·하늘 값을 잠시 바꿔 검수한 야간 미리보기다. 촬영 후 낮 설정과 MPC 기본값으로 복구했다. 자동 낮/밤 순환이나 게임 내 프리셋 전환 UI를 구현한 것은 아니다.

## Windows 우선 / 추후 모바일

프로젝트 전체 렌더러 설정은 변경하지 않았다. V8 레벨은 기존 Screen Space GI/반사 설정을 사용한다. DOF를 별도 볼륨으로 분리하고 UV 표면 마스터는 기본 노멀맵을 끄도록 구성했다.

모바일 빌드가 이미 검증되었다는 의미는 아니다. 모바일 대상이 정해지면 DOF 비활성화, 물 투명도·반사 경량화, 지형 레이어/샘플 수 축소 또는 베이크, 광원·그림자·LOD·텍스처 압축/메모리 예산을 별도로 조정하고 실제 기기에서 Cook/성능 검증해야 한다. Windows PIE에서 시각·동선을 검수했으며 FPS나 메모리 예산을 인증한 것은 아니다.

## 검수 이미지 구분

`Preview/Game_*.png`는 실제 플레이어 컨트롤러로 요청한 게임 화면이다. `HD2D_Square_001.png` 등은 에디터 구도 검수다. `PIE_Entry_001.png`는 자동 캡처가 에디터 시점으로 찍힌 진단 자료이며 플레이어 시점 증거로 사용하지 않는다. `*_bridge_color_unreliable.png`는 색 채널 문제가 있는 보조 진단 캡처라 색감 평가에 사용하지 않는다.
