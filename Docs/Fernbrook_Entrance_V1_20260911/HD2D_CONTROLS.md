# 펀브룩 HD2D 조정 안내

## 전체 스타일

`/Game/Fernbrook/Entrance_V1/Materials/MPC_FB1_HD2D`

기존 마을의 MPC를 독립 복제해 펀브룩 마스터 5개가 이 컬렉션을 참조하도록 연결했습니다. 이 에셋을 조정하면 펀브룩의 재질군에 공통 변화가 적용되며 기존 두 마을의 컬렉션 값은 바뀌지 않습니다. 공통 색 처리 Material Function은 기존 것을 읽기 전용으로 재사용하므로 그 Function 자체를 편집하면 기존 마을에도 영향을 줄 수 있습니다. 조정은 이 MPC와 펀브룩 MI에서 하세요.

| 매개변수 | 현재 값 | 역할 |
|---|---:|---|
| StyleStrength | 1 | 전체 스타일 영향도 |
| PixelBlend | 0.90 | 픽셀 샘플링 혼합 강도 |
| TextureDensity | 1 | UV 픽셀 그리드 밀도 배율 |
| WorldPixelSizeCm | 3 | 월드 좌표형 재질의 픽셀 크기 |
| PaletteSteps | 24 | 색 단계 수 |
| PosterizeStrength | 0.55 | 색 단계화 혼합 강도 |
| Saturation | 0.93 | 채도 |
| Contrast | 1.07 | 대비 |
| ShadowLift | 0.025 | 어두운 텍스처 값 보정 |
| RoughnessBias | 0.02 | 거칠기 공통 가산 |
| NormalStrength | 0.55 | 노멀맵을 활성화한 Surface의 노멀 강도 |
| EmissiveMultiplier | 1 | 발광 공통 배율 |
| GlobalTint | 1.02, 1.00, 0.95 | 공통 색조 |

원본 2K 텍스처 파일을 다시 작게 굽지 않고 셰이더에서 픽셀 샘플링합니다. 텍스처 원본과 UV는 그대로입니다. 6개 Tripo 모델은 Base Color만 사용하고 `UseNormalMap=0`인 상태입니다. 노멀맵이 없는데 NormalStrength만 조정해도 효과가 생기는 것은 아닙니다.

## 개별 재질

마스터는 `Materials/M_FB1_HD2D_Surface`, `Landscape_Stable`, `WorldProps`, `Water`, `Sky`입니다. 해당 MI는 `Materials/Instances`에 있습니다.

- `MI_FB01_Textured` … `MI_FB06_Textured`: `MaterialTint`, `PixelGrid`, `PixelInfluence`, `Roughness`, `EmissiveGain`. 큰 나무/그루터기 그리드 256, 나머지 192.
- `MI_FB1_Undergrowth`: 불투명 양면 Surface. 고사리·풀을 검은 뒷면처럼 보이지 않도록 별도 연결. PixelGrid 128, EmissiveGain 0.045.
- `MI_FB1_ForestFloor`: Grass/Dirt/Moss/Cobblestone의 타일 크기, 매크로 얼룩, 가장자리 노이즈. 현재 TileCm_Grass 155, Dirt 125, Moss 165, Cliff 190.
- `MI_FB1_Creek`: 색상, 물결 속도/크기, 불투명도, 하이라이트, 물가 거품. 최종 FoamWidth 1.7, HighlightStrength 0.28.
- `MI_FB1_NaiaGlow`: EmissiveStrength 0.7. `FB1_Naia_SoftGlow` 점광원은 강도 2 / 반경 125cm이며 그림자를 계산하지 않습니다.

## 카메라와 후처리

현재 맵만 기존 `BP_LG8_GameMode`를 참조합니다. 해당 캐릭터의 BeginPlay가 고정 카메라를 설정합니다: Pitch -30°, Yaw 90°, 스프링암 1900cm, FOV 38°. 실제 PIE 카메라로 확인했습니다. 기존 Blueprint를 직접 수정하지 않았습니다. 펀브룩만 다른 각도로 바꾸려면 캐릭터/게임모드를 별도 복제하여 연결한 후 조정해야 기존 마을이 보존됩니다.

- `FB1_PP_ArtGrade`: 전체 색감/노출/반사. Screen Space GI와 Reflection, 노출 min=max 1 / bias 0.35, bloom 0.32, vignette 0.14. 색수차/필름 그레인/모션 블러는 0.
- `FB1_PP_DOF_Optional_Windows`: 별도 DOF 볼륨. F-stop 3.2, sensor width 100, 초점 거리 1900cm. 끄려면 Blend Weight를 0으로 변경하고 재검사하세요.
- `FB1_ForestHaze`: 지수 안개 및 Windows용 볼류메트릭 안개. 볼류메트릭을 별도로 끌 수 있습니다. 자동 낮밤 시스템은 없습니다.

## 추후 모바일 검토 지점

아래는 남겨둔 조정 지점이지 성능 합격값이 아닙니다.

1. 대상 장치에서 DOF·볼류메트릭 안개·SSR을 끈 상태부터 비교합니다. 프로젝트 렌더러 설정을 이번에 변경하지 않았습니다.
2. Foliage 폴더의 유형별 표시 거리/밀도를 조정합니다. 현재 최대 거리: 큰 나무 8500, 어린 나무 6500, 관목 4700, 바위 5500, 고사리 4200, 풀 3700 게임 cm.
3. 큰 나무/어린 나무/바위는 충돌을 유지하고, 관목/고사리/풀만 밀도 스케일링 대상으로 지정했습니다. 카메라 자체가 캐릭터에서 1900cm 떨어져 있으므로 잔디 표시 거리를 그보다 짧게 잡으면 플레이어 주변의 잔디까지 사라질 수 있습니다.
4. 6개 주요 모델의 LOD는 100% / 약 55% / 약 25% 삼각형, 화면 크기 전환 1 / 0.22 / 0.09입니다. 실제 수치는 CSV에 기록했습니다. Nanite는 비활성입니다.
5. Dynamic 조명 경로이며 라이트맵 UV/베이크 검증은 하지 않았습니다. 메모리·드로콜·프레임 시간과 모바일 셰이더 쿠킹을 실제 빌드에서 확인해야 합니다.

## 유지보수 주의

현재 할당된 랜드스케이프 마스터에 Revvy의 강제 머티리얼 diagnostics/compile을 호출하지 마세요. 이 프로젝트에서 이전에 렌더링 충돌이 발생했던 경로입니다. 이번 마스터는 지형에 할당하기 전에 컴파일 검증했고, 이후에는 MI 값/실제 화면/저장·재로드로 확인했습니다.

Unreal 에디터의 CPU 메시 조회는 `Allow CPU Access`가 꺼졌다는 쿠킹 관련 경고를 남길 수 있습니다. 이번 검수는 에디터에서만 수행했으며 런타임 ProceduralMesh 변환 기능을 추가하지 않았습니다. 경고를 없애려고 전체 메시의 CPU Access를 켜지는 않았습니다.
