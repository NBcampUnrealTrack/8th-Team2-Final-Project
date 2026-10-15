# 조절 안내

## 나무: 깨끗한 현대 HD2D

`/Game/Fernbrook/Polish_V2/Materials/MI_FB2_Tree_Canopy` 및 `MI_FB2_Tree_Sapling`을 엽니다. 메시/폴리지의 재질 연결을 바꾸지 않고 이 두 인스턴스를 조절하면 각각의 나무에 일괄 적용됩니다.

| 파라미터 | Canopy / Sapling 최종값 | 의미 |
|---|---:|---|
| PixelGrid | 512 / 384 | UV 한 축의 샘플 격자 수. 클수록 작고 섬세한 픽셀 |
| TreePaletteBlend | 0 / 0 | 강제 저색상 양자화 비활성. CRT 느낌 방지를 위해 0 유지 |
| StyleInfluence | 0 / 0 | 공통 포스터화 효과를 나무에서 우회 |
| TreeVariation | 0.3 / 0.3 | 인스턴스 간 색 변화. 지나친 대비/색 반전 억제 |
| PixelInfluence | 1.12 / 1.12 | 공통 강도와 곱해 최종 clamp. 격자 감도 유지 |
| MaterialTint | (0.78,0.84,0.91) / (0.83,0.89,0.88) | 원본의 강한 황록색을 완화하는 승산색 |
| Roughness | 0.97 / 0.97 | 젖은 플라스틱 같은 광택 억제 |

TreePaletteSteps=9는 아직 노출되지만 TreePaletteBlend=0이므로 비활성입니다. 원본 텍스처 자체의 저해상도 정보가 복원되는 것은 아닙니다. 더 풍부한 잎 디테일은 새 FB12/FB13 텍스처 제작으로 보완합니다.

## 물

활성 MI: `/Game/Fernbrook/Polish_V3/Materials/MI_FB3_ClearCreek_Windows`

마스터: `/Game/Fernbrook/Polish_V3/Materials/M_FB3_ClearCreek`

적용 액터: `FB1_CreekSurface`. 마스터 기본값이 아니라 **현재 MI 오버라이드**가 아래 최종 외형 기준입니다. 새 프리셋은 이 MI를 복제해 만드세요.

| 파라미터 | 최종값 | 조절 효과 |
|---|---:|---|
| FlowSpeedCm | 12 | 흐르는 패턴 속도(cm/s 기준 설계), 유속 시뮬레이션 아님 |
| FlowDirection | (−0.08, −1) | 월드 XY 방향, 내부에서 약하게 방향을 휘게 함 |
| WaveLengthCm | 100 | 파형 반복 크기 |
| NormalStrength / DetailNormalStrength | 0.048 / 0.023 | 큰 물결 / 미세 물결 |
| PixelSizeCm / PixelBlend | 2 / 0.1 | 아주 약한 월드 격자 영향. 큰 모자이크가 필요 없으면 Blend=0 |
| FoamWidthCm / FoamStrength | 5 / 0.4 | 물가 깊이 띠 / 거품 강도 |
| CausticStrength | 0.18 | 물 아래 밝기 무늬. 실제 투사 코스틱 아님 |
| SurfaceOpacity | 0.04 | 표면/수중 기여 조절. 일반 Alpha Blend 투명도와 다름 |
| Roughness / Specular | 0.26 / 0.5 | 표면 반사의 부드러움 / 강도 |
| PhaseG | 0.15 | 산란 방향성 |
| Scattering RGB | (0.002, 0.0055, 0.006) | 수중 산란색/강도 |
| Absorption RGB | (0.025, 0.008, 0.005) | 채널별 흡수 |
| SurfaceColor RGB | (0.025, 0.10, 0.115) | 표면색 |
| FoamColor RGB | (0.55, 0.72, 0.64) | 거품색 |

현재 수면 위치 (0,500,−8), Scale (46,97,1), bounds X ±2300 / Y −4350..5350 cm. 지형 아래로 가려진 부분은 정상입니다. 표면이 사라지면 먼저 위치·bounds·재질 슬롯·컴파일 로그를 확인하세요. 표시 문제를 해결하려고 포스터화 강도를 높이지 마세요.

## 포스트프로세스 / 카메라

`FB1_PP_ArtGrade`: Saturation W 0.86, Contrast W 1.055, Bloom 0.38 / Threshold 1.1, Vignette 0.22, Exposure Bias 0.2. 밝은 곳은 약간 따뜻하게, 그림자는 약간 서늘하게 조정했습니다. `FB1_Sun` 8, `FB1_SkyLight` 1.9.

`FB1_PP_DOF_Optional_Windows`: f/2.8, Sensor Width 105, Focal Distance 1900cm, Blend Weight 1. 흐림이 과하면 먼저 Blend Weight를 낮추세요. 단순히 낮은 렌더 해상도로 화면 전체를 흐리게 만드는 방식은 사용하지 않습니다.

공통 카메라 BP는 수정하지 않았습니다. 현재 실제 게임: Pitch −30° / Yaw 90° / Arm 1900cm / FOV 38°. 나이아 공터의 전경 가림을 DOF만으로 해결할 수는 없습니다.

## 모바일 검토 계획 — 미적용/미검증

모바일은 볼류메트릭 물/SSR 지원 차이를 전제로 별도 저비용 셰이더 및 기기별 프로파일을 검증해야 합니다. Windows용 MI의 NormalStrength를 0으로 내리는 것만으로 노드 계산이 제거되지는 않습니다. 정적 분기/별도 마스터로 코스틱·보조 노멀·깊이 버퍼 의존성을 실제 제거하고, SSR/DOF를 끈 기기용 프로파일을 비교하는 후속 작업이 필요합니다. 현재 모바일 자동 전환이나 모바일 지원 완료를 주장하지 않습니다.
