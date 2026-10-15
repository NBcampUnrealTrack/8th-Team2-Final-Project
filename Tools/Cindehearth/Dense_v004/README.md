# 신데하스 v004 — 조밀한 마을과 건축 리워크

2026-09-05. 적용 맵 `/Game/Maps/L_Cindehearth`. 기존 v003의 넓은 빈 공간과 부분 교체에 그쳤던 주막·대장간 형태를 수정한 작업입니다. **최종 검증 상태는 `signoff.json` 참조**. 이 파일만으로 검증 완료를 판단하지 않습니다.

묘지 비탈과 주막 전면의 추가 수정까지 적용했습니다. **14개 실제 이동 경로·72개 지점, 퀘스트 상태·장부고·에필로그·카메라 검증 통과**. 마지막 수정 후 주막·묘지·딥베인도 다시 통과했습니다. 낮/Q0/PIE 종료 상태로 저장했습니다.

## 적용 내용

- 마을 부지 150×180 → **111×120.6 설계 m**, 면적 **49.58%**. 프로젝트의 설계 1m=45 UE cm 관례를 유지하므로 실제 UE 부지는49.95×54.27m입니다. 북쪽+Y와 단 높이0/157.5/315cm는 유지했습니다.
- 지형·연결로는 XY 압축, 화덕·가구·건물 부속품은 묶음 단위로 이동했습니다. 17개 주택 묶음은0.9 균일 축소하고 겹치는 집5곳 위치를 별도로 조정했습니다. 95개 소나무는 입체 메시와 균일 스케일을 유지합니다.
- 주막, 도리크 대장간, 하역장: 기존 `Tavern_Stone`/`Smithy_Stone` 본체·지붕·외벽 조각을 숨기고, **새 Tripo P2 스마트 메시**로 완성형 건물3곳을 교체했습니다. 대장간 지붕만 실내 진입 시 가려지도록 분리했습니다.
- 단순 작업장3곳을 새 입체 모델로 교체하고, 광부 준비실·등잔 창고·감정소 포장 창고3동과 소품18개를 배치했습니다. 주막 술통14개는 벽 쪽 적치장으로 모았습니다.
- 계단4곳, 레일 경사로2곳, 애쉬슬로프·묘지·파인할로우 진입부: 낮은 석축·기초석·작업 소품106개. 모든 연결부 신규 장식은NoCollision이며 보행로는 비워 두었습니다.
- 실제 플레이 검수에서 발견한 애쉬슬로프·파인할로우 지형 틈에는 바위·흙 둔덕14부품을 추가했습니다. 주막 입구·다락·반사광의 강도를 낮추고 표지판 위치를 조정했습니다(`verification/tavern_lights_toned.json`). 집하장 카메라를 가리던 장식 암석2개는 보행면 아래로 낮추고, 바닥 충돌과 숨겨진 원본 상태를 유지했습니다(`verification/depot_view_fixed.json`).
- 떠 보이던 묘지 경사길 아래와 사각 플랫폼 가장자리에 **매립 코어2개·P2 암석14개, 총16개**를 추가했습니다. 모두NoCollision이며, 암석 상단은 기존 보행면 아래에 놓았습니다. 길·묘지 바닥·무덤·조망 지점은 옮기지 않았습니다(`verification/cemetery_bank_applied.json`; `verification/player_cemetery.png`에서 최종 화면 확인).
- 감정소 장부고 이동은 월드의 `CH.Archive.Entry`/`CH.Archive.Lower` 위치를 읽도록 C++을 변경했습니다. Q7 봉쇄와 컷신 중 조작 제한은 유지했습니다. 고름 집 서신 소품과 **직렬화된** 에필로그 카메라 경로도 함께 옮겼습니다.

## Tripo → Blender → Unreal

| 새 원형 | P2 task | UE 조립 삼각형 수 |
|---|---|---:|
| 주막 | `4caa7db6-bf5c-462b-a2f7-2b6fc1428808` | 3,190 |
| 대장간 | `dda91e9f-07fe-4fb0-b1fa-683ddbfe6b0a` | 3,226 |

두 생성에220크레딧 사용, 확인 잔액1,560. 신규 생성은 native P2 quad이며 렌더 시 삼각형으로 변환됩니다. 디테일은256 픽셀 격자 텍스처와 낮은 노멀 강도로 표현합니다.

Blender에서는 느슨한 버텍스·비다양체 접합·퇴화 면을 정리하고, 누락 벽과 처마 틈을 선택적으로 보수했습니다. 크게 늘어지는 보수 면의 UV는 원본 본체에서 분리하고 기존 월드 좌표 석재 재질을 적용했습니다. **본체+벽+지붕을 동일 transform으로 조립**해야 합니다. 부품별 bounds 정규화는 금지합니다. 모든 최종 부품의 비다양체·느슨한 버텍스·퇴화 면은0이지만 원본 표면 경계와 부품 접합부가 남으므로 전체 밀폐 메시라고 주장하지 않습니다.

주막의 마지막 보수벽은 `TavernWallsInset`(전면12tri)과 `TavernWallsFrame`(나머지36tri)입니다. 전면벽을 로컬X로14cm 넣어 원본 나무문과 1층 창틀이 드러나도록 수정했습니다. 이전 통합 `TavernWalls` 액터는 숨겼습니다. Blender 수정은 `Architecture/verification/TavernWallsInset_fix.json`, UE 부품 교체는 `verification/tavern_walls_inset.json`에 기록되어 있습니다. 두 부품은 이전 벽의 동일 transform을 사용합니다.

Blender 원본·정리본·기하 진단·GLB 경로는 `Architecture/verification/architecture_handoff.json`. UE 에셋은 `/Game/Cindehearth/Dense_v004`에 별도로 저장했습니다. 기존 P2 에셋은 참조 재사용합니다.

## 검증과 복구

- 최종 화면: [마을 전경](verification/final_village.png), [화덕 광장·대장간](verification/final_square.png), [주막 야경](verification/player_tavern_night.png), [묘지 연결부](verification/player_cemetery.png), [집하장](verification/player_depot.png).
- 최종3376액터, 입체 소나무95개, 연결부 장식106개, 누락 머티리얼0. 장부고 앵커·주막 분리벽·집하장 장식 충돌과 위치도 직접 재조회했습니다. `verification/final_scene_audit.json`과 `verification/gameplay_composite.json`이 증빙입니다.
- 원본 맵 백업: `Saved/CindehearthDenseBackup/20260905_v004/L_Cindehearth.umap`.
- 원본 액터3195개 스냅샷: `verification/level_before.json`; 카메라 원본: `director_before.json`.
- 배치 계산: `layout_transform.py`, 적용 기록: `layout_applied.json`. 반복 적용하지 않고 원본 기준 계산을 사용합니다.
- 실제 이동 QA: `QA/run.py`; 매 실행은 별도 폴더에 저장합니다. 첫 실행에서 애쉬슬로프 연결 바닥의 잘못된 스케일을 발견했고 `fix_ash_bridge.py`에서 저장된7.2/3.6/.135 스케일을 기준으로 수정했습니다. 실패 기록을 보존합니다.
- C++ 실제 전체 빌드 성공 및 맵 재개방 확인: `Runtime/full_build.json`, `Runtime/reopen_verified.json`. 구문 검사만을 런타임 반영으로 취급하지 않습니다.
- 실제 PIE 스크린샷: `verification/player_*.png`. `CH_Dense_review_*`는 편집기 검토용입니다. ScreenshotService PNG의 알려진 B/R 순서만 기술적으로 바로잡았고, PIE 이미지는 보정하지 않았습니다.
- 최종 감사는 `audit_final.py`에서 저장된 에셋을 읽기만 합니다. 에디터 충돌을 유발했던 강제 머티리얼 재컴파일 진단은 호출하지 않습니다.

전체 퀘스트 데이터의 실제ID 연결, 최종 NPC 동작/대사, 전투·저장·쿠킹·성능 인증은 이 환경 리워크의 완료 판정에 포함하지 않습니다. 새 작업 창고는 별도 퀘스트나 NPC를 추가하지 않습니다.
