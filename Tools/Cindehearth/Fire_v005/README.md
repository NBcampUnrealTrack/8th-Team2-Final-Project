# 신데하스 v005 — 고정 정면 불 이펙트

2.5D 플레이 카메라(pitch−25°, yaw90°, FOV32°)에서 자연스럽게 읽히도록 화덕·대장간 불을 수정했습니다. 카메라 설정과 낮·밤 조명은 유지했습니다.

- 교차하던 불꽃 면5장을 숨기고, 정면용 면2장으로 교체했습니다.
- 불꽃 다섯 갈래의 높이와 흔들림을 다르게 만들고, 노란 중심·주황빛 몸체·반투명 가장자리로 표현했습니다. 64×80 UV 격자, 16Hz 애니메이션입니다.
- 작은 불티3개는 위로 올라가며 사라집니다. 바닥 불씨는 유지됩니다.
- 화덕은 장작 교차점에, 대장간은 상단 화로의 어두운 입구 하단에 불 뿌리를 맞췄습니다.
- 모두 NoCollision이며 그림자를 투사하지 않습니다. 추가 조명·Niagara 시뮬레이션·Tripo 크레딧 사용은 없습니다.

실제 플레이 화면: [화덕 낮](verification/final_hearth_day.png), [화덕 밤](verification/final_hearth_night.png), [대장간 최종](verification/forge_final_forge.png). `before_*`는 변경 전, `after_*`와 `final_forge`는 위치 보정 중 기록입니다. 대장간 최종 증거는 반드시 `forge_final_forge.png`를 사용합니다.

머티리얼은 `/Game/Cindehearth/Fire_v005/Materials/M_CH_FrontFire`, 인스턴스는 `MI_CH_FrontFire_Hearth`, `MI_CH_FrontFire_Forge`입니다. 원본 PixelFire 머티리얼은 수정하지 않았습니다. `verify`에서 원본 그래프·저장 배치·실제 카메라·낮/밤 조명 상태를 재조회합니다. 머티리얼 생성 시 컴파일 요청과 실제 렌더를 확인했으며, 과거 충돌을 일으킨 강제 진단 재컴파일은 사용하지 않았습니다.

최종 판정은 `signoff.json` 참조. 이 작업은 정면 화면의 환경 이펙트 개선이며, 회전 카메라용 입체 이펙트나 플랫폼별 성능 인증을 의미하지 않습니다. 사용자 미감 승인은 별도입니다.

복구용 맵: `Saved/CindehearthFireBackup/20260905_v005/L_Cindehearth.umap`. 이전 면은 삭제하지 않고 숨겼습니다.
