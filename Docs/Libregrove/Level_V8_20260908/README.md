# 리베르그로브 V8 — 전달 안내

2026-09-09. 마을 환경 구성과 조절형 도트 머티리얼, 플레이 카메라 및 포스트 프로세스 적용을 완료했다. Unreal Engine 5.8.1 / Windows DX12 에디터에서 검수했으며, 저장한 맵을 다시 열어 결과가 유지되는지 확인했다.

## 열기와 조절

콘텐츠 브라우저에서 `/Game/Libregrove/Level_V8/Maps/L_Libregrove`를 열고 Play 한다. 다른 레벨의 기본 게임 모드와 원본 캐릭터 Blueprint는 바꾸지 않았다. 현재 에디터에는 이 맵을 열어두었고 PIE는 종료했다.

| 조절 대상 | 에셋 / 아웃라이너 이름 |
| --- | --- |
| 전체 도트 크기·채도·색 단계·명암 | `/Game/Libregrove/Level_V8/HD2D/MPC_LG8_HD2D` |
| 개별 건물 텍스처·도트 강도 | `/Game/Libregrove/Level_V8/HD2D/Instances` |
| 카메라 각도·거리·FOV | `/Game/Libregrove/Level_V8/Gameplay/BP_LG8_FieldCharacter`의 `LG_CameraRotation`, `LG_CameraDistance`, `LG_CameraFOV` |
| 노출·Bloom·Vignette·색보정 | `Libregrove/Environment/LG8_PP_ArtGrade` |
| 선택형 피사계 심도 | `Libregrove/Environment/LG8_PP_DOF_Optional_Windows` |

자세한 기본값·조절 범위·모바일 준비 항목: [그래픽 조절 가이드](HD2D_CONTROLS.md).

## 결과 이미지

- [전체 배치 조감도](Preview/Final_Overview_001.png) — 에디터 구도. 구조 확인을 위해 촬영 중에만 DOF 표시를 껐다가 복구했다.
- [광장 실제 플레이 화면](Preview/Game_Square_Final_002.png)
- [남쪽 진입](Preview/Game_Entry_Clear_001.png) / [공방 구역](Preview/Game_Workshops_Final_002.png) / [성역 계단](Preview/Game_Shrine_Final_002.png)
- [폭포와 물가](Preview/Game_Waterfront_Final_003.png) / [물가 건물과 서쪽 다리](Preview/Game_Watermill_Final_004.png)
- [스타일 효과 해제 비교](Preview/Game_StyleOff_Final_002.png) / [도트 강하게 비교](Preview/Game_StyleStrong_Final_002.png)
- [야간 가독성 미리보기](Preview/Game_SquareNight_Final_003.png) — 촬영 후 낮 설정으로 복구. 자동 낮/밤 순환 기능은 아님.

모든 이미지는 엔진 원본 캡처다. `Game_` 접두사는 실제 플레이어 카메라 화면이다. 과거 진행 이미지와 실패 진단 자료도 이 폴더에 보관되어 있으므로 전달 이미지는 위 링크를 기준으로 확인한다.

## 구성과 검증

남쪽 주거지, 동서 하천과 세 다리, 중앙 분수 광장, 서북 물가·폭포, 동쪽 공방과 경작지, 북쪽 성역을 구성했다. 낮 조명·스카이·안개·등불, 길과 지형 레이어, 건물 기단 및 충돌, 생활 소품과 숲을 함께 배치했다. 88 cm 테스트 캐릭터에 맞춘 게임 스케일이며 실제 성인 건축 치수 기준 맵은 아니다.

최종 저장 상태: 레벨 액터 785개, 건물·가판 배치 34개, foliage 2,347개(큰 나무 283 / 어린 나무 191 / 수풀 1,564 / 바위 309). 공통 색 함수·MPC와 활성 마스터 6종, 활성 인스턴스 38개를 사용하고 909개 컴포넌트 재질 슬롯 및 지형에 연결했다. 지형 마스터/인스턴스 작업 백업 각 1개는 활성 수치에서 제외했다.

| 실제 이동 테스트 | 통과 지점 | 최종 증거 |
| --- | ---: | --- |
| 남쪽 진입 → 중앙 다리 → 성역 | 19 | `GameplayQA/central_shrine_002.json` |
| 아랫마을 → 서쪽 다리 → 물가 | 15 | `GameplayQA/west_bridge_mill_005.json` |
| 아랫마을 → 동쪽 다리 → 공방 | 12 | `GameplayQA/east_bridge_workshop_002.json` |
| 아랫마을 순환 | 8 | `GameplayQA/lower_loop_001.json` |
| 서쪽 출구 | 6 | `GameplayQA/west_exit_004.json` |
| 남동 출구 | 4 | `GameplayQA/southeast_exit_001.json` |

경로 시작 위치만 이동시킨 뒤 각 지점 사이는 실제 CharacterMovement 입력으로 이동했다. 총 64개 지점 통과이며 전체 맵의 모든 위치를 검사한 것은 아니다. 세부 판정 기준과 위치 샘플은 JSON에 있다.

[재로드 후 최종 검사](final_audit_002.json): 건물 수, foliage 수, 모든 표면 재질의 V8 경로, 4개 Foliage Type의 재질 오버라이드, 활성 지형 부모, 전용 게임 모드, 카메라 Blueprint 컴파일 상태, MPC 기본값 복구, 원본 V7 메시 기본 재질 보존을 확인했다. 기존 `L_VillageEntrance.umap` SHA256도 작업 전 값과 같다.

## 보존·범위·향후 작업

원본 Tripo 텍스처와 V7 모델은 유지하고 새 맵에서 재질을 오버라이드했다. 기존 지붕 형태·메시 디테일을 새로 리토폴로지한 작업은 아니다. 이 레벨은 사용자 확인된 병합 V7 모델을 사용하므로, 이전 별도 문 애니메이션 모델의 여닫기 기능을 새로 이식한 것은 아니다. 기존 테스트 캐릭터 외형을 사용하며 신규 NPC·퀘스트·실내 출입 시스템은 추가하지 않았다.

Windows용 선택형 DOF와 표면 노멀맵 기본 비활성화를 마련했다. 모바일이 목표로 확정되면 지형 샘플 수·물 반사/투명도·광원/그림자·LOD·텍스처 메모리를 기기 예산에 맞춰 별도 조정해야 한다. Windows 패키징 및 모바일 Cook/실기기 성능 측정은 수행하지 않았다.

복구 자료는 `HD2D/assignment_backup.json`, `HD2D/default_preset.json`, `Saved/LibregroveV8Backup`의 변경 전 맵에 있다. 입구 가림을 해소하기 위해 큰 나무 3개를 제거했고 서쪽 길 보완 시 foliage 36개를 추가 제거했다. 모두 새 V8 맵 내부 변경이며 백업으로 복구할 수 있다.

`Tools/Libregrove/level_v8_*.py`는 단계별 제작·검수 기록이다. 이미 수행한 생성/배치 스크립트를 일괄 재실행하면 안 된다. 초기 도로 설계 이후 수정분은 `west_roads_patch.json`, 최종 기단은 `mill_foundation.json`이 기록한다. 실사용 결과의 기준은 저장된 새 UE 맵과 재로드 후 최종 검사다.
