# 고름의 집 — 메시 보수 v006

2026-09-05. 사용자가 표시한 지붕 구멍·박공 틈·떠 있는 측면 조각을 보수하고 `/Game/Maps/L_Cindehearth`의 `CH_GormHouse_P2`에 적용했다. 추가 Tripo 생성 및 크레딧 사용은 없다. 이전 v001의 ‘Blender 정리 완료’ 판정은 철회했다. 당시 검사는 변환·UV·퇴화 면 위주여서 실제 외관 결함을 걸러내지 못했다.

## 변경과 확인

- 구멍과 비다양체 모서리가 있는 지붕 연결 부품을 제거하고 두께가 있는 **닫힌 10쿼드 지붕**으로 교체했다. 널판 디테일은 기존 지붕의 컬러를 1,024px 텍스처로 재베이크했다. 조명은 굽지 않았다.
- 떠 있는 측면 창문판 6면, 새 지붕을 뚫고 나오던 작은 부품 61면을 선택 삭제했다. 몸체·문·곡괭이·굴뚝은 유지했다.
- 유지하는 면은 기존 방향과 UV를 보존한다. 열린 판재 전체에 노멀 재계산을 적용하면 뒷벽 방향이 뒤집히는 것을 확인해 해당 처리를 제거했다.
- Unreal에서는 몸체의 기존 픽셀 재질과 새 지붕 재질을 분리했다. 지붕은 PixelGrid 512 / NormalStrength 0, 몸체는 기존 0.12. Nanite는 끄고 실제 렌더 삼각형 수를 확인했다.
- 원본 `.blend`, 이전 에셋, 교체 전 액터 정보와 맵을 보존했다. 배치 위치·회전·스케일은 그대로다. 지붕 돌출부 제거로 외곽 폭은 315cm에서 309cm로 줄었다.

| 검사 | v001 | v006 |
|---|---:|---:|
| 버텍스 | 2,129 | 1,365 |
| 편집 면 | 1,907 | 1,062 |
| 쿼드 / 삼각 면 | 1,456 / 451 | 932 / 130 |
| 렌더 삼각형 | 3,363 | 1,994 |
| 면에 연결되지 않은 버텍스 | 0 | 0 |
| 비다양체 모서리(열린 경계 별도) | 12 | 0 |
| 열린 경계 모서리 | 803 | 634 |
| UV 퇴화 / 비유한값 | 0 / 0 | 0 / 0 |

**한계:** v006도 집 전체가 완전히 밀폐된 메시라는 판정은 아니다. 원래의 분리된 벽체·장식 부재에 열린 경계 634개가 남는다. 지붕은 닫힌 구조로 만들었으며 표시된 외관 결함을 앞·뒤 텍스처/무채색 렌더와 Unreal 화면에서 확인했다. 경계 수나 자동 quality 점수만으로 외관 검수를 대체하지 않는다. 창문과 굴뚝의 의도된 개구부는 유지했다.

## 현재 사용할 파일

교환 루트: `%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth`

- `Repair/CH_GormP2_Clean_v006.blend` — 편집본, 오브젝트 `CH_GormP2_Clean_v006`, 텍스처 포함.
- `Export/CH_GormP2_Clean_v006.fbx` — 쿼드 편집용 교환본.
- `Export/CH_GormP2_Clean_v006.glb` — Unreal에 실제 가져온 텍스처 포함 파일.
- `Repair/CH_GormRoof_Color_v002.png` — 새 지붕 컬러 베이크.
- Unreal: `/Game/Cindehearth/Tripo/GormHouse_P2_Repair_v006/CH_GormP2_Clean_v006/StaticMeshes/CH_GormP2_Clean_v006`.

현재 Unreal에서 이 수정본을 열어두었다. 실행 중이던 Blender 장면은 유지했으며, 수정본은 위의 별도 파일에 저장했다. v003·v004는 검수 탈락한 실험본이다. `accepted_*`, `final_*`라는 예전 실험 렌더 이름을 완료 판정으로 해석하면 안 된다. 채택 버전은 **v006**뿐이다. v005 패치 시도는 가드에서 거부되어 저장하지 않았다.

## 검증 자료

- [Blender 텍스처 검수](verification/gorm_repaired_v006_blender_textured.png), [무채색 검수](verification/gorm_repaired_v006_blender_clay.png), [뒷면](verification/gorm_repaired_v006_blender_back.png).
- [Unreal 지붕 근접 화면](verification/gorm_repaired_v006_day.png), [플레이 카메라 구도](verification/gorm_repaired_v006_player_camera.png). 플레이 카메라 구도는 실제 PlayerCameraManager에서 측정했던 위치·회전·FOV를 이용한 에디터 캡처다.
- [실제 PIE 보행 8/8](verification/repair_walk.json), [Q12 카메라·입력 복귀](verification/repair_epilogue_runtime.json), [Unreal 재조회](verification/repair_unreal_readback.json).
- [보수 검수 기록](verification/repair_pipeline_signoff.json), [교체 전 액터](verification/repair_unreal_replacement_before.json).
- 맵 백업: `Saved/CindehearthAutomation/L_Cindehearth_before_mesh_repair.umap`.

## 이후 에셋의 Blender 완료 조건

1. P2 native quad 원본을 보존하고 연결된 면 묶음·열린 경계·비다양체 모서리·UV를 검사한다. loose vertex 0이어도 떠 있는 면 묶음이 있을 수 있다.
2. 보이는 구멍과 떠 있는 조각을 식별해 **선택한 부분만** 보수한다. 결함이 많은 판재는 단순한 닫힌 형상으로 교체하고 텍스처를 옮긴다. 작은 수치의 전체 병합만 실행하고 정리 완료로 처리하지 않는다.
3. 보존한 면의 방향과 UV를 유지한다. 새 면의 방향·퇴화·UV를 확인하고 원본 fingerprint와 파일 해시를 대조한다.
4. 앞·뒤·양쪽 사선에서 텍스처와 무채색 렌더를 확인한다. 구멍·떠 있는 면·빛샘·돌출·음영 이상을 검수 기록에 남긴다.
5. Unreal의 새 에셋에서 삼각형 수·재질·스케일·충돌을 재조회하고 실제 카메라, 이동, 관련 컷신을 확인한 뒤 배치한다. 남은 결함과 미검증 항목을 명시한다.

이번에 추가한 `Blender-Repair-Cindehearth`는 선택한 부품 삭제·경계 보수·닫힌 박공지붕 생성/컬러 베이크·내보내기·검수 렌더에 한정한 typed MCP다. 임의 코드 입력을 받지 않으며 별도 Blender 프로세스와 새 파일에서 작업한다. 검사한 fingerprint, 선택 ID, 안정적인 job_id가 필요하다. `repair_step.py`는 해당 MCP 세션을 유지하며 결과를 폴링한다. 결과가 불확실하면 새 작업을 제출하지 않고 기존 영수증을 확인한다.

실제 Blender 고정 테스트에서 선택 조각 삭제·구멍 보수·기존 면 방향 보존·오래된 fingerprint 거부·원본 보존·10쿼드 지붕 밀폐를 확인했다. MCP의 경로 제한·덮어쓰기 거부·작업 ID 충돌·완료 작업 재조회 등 오프라인 검사 5개도 통과했다.
