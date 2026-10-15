# 신데하스 — Tripo → Blender → Unreal

2026-09-05. 현재 적용본은 **CH_GormP2_Clean_v006**입니다. 사용자가 지적한 지붕 구멍·박공 틈·떠 있는 조각을 Blender에서 보수해 Unreal에 교체했습니다. 이전 v001의 ‘정리 완료’ 판정은 철회했습니다. [보수 결과·현재 파일·완료 조건](REPAIR_REPORT.md)을 먼저 확인하세요. 사용자 기준은 [ART_DIRECTION.md](ART_DIRECTION.md): P2 native quad, 단순한 형태, 도트 텍스처 중심입니다. 마을 전체의 최종 아트 교체는 후속 작업입니다.

## 현재 연결 상태

| 구성 | 상태 |
|---|---|
| `Tripo-Cindehearth` | 프로젝트 전용 MCP 등록 및 stdio 초기화·도구 조회·상태 조회 통과 |
| Tripo 인증 | 실제 API 인증 성공. 총 140크레딧 사용(H3.1 비교용 30 + P2 110), 잔액 4,860 / 동결 0 |
| `Blender-Cindehearth` | Blender 5.0 / Revvy Bridge 1.2. 원본·작업본·최종 FBX/GLB/.blend 저장 완료 |
| `Blender-Repair-Cindehearth` | 선택 부품 삭제·경계 검사/보수·닫힌 지붕 생성/컬러 베이크·검수 렌더·내보내기용 typed MCP 추가. 별도 Blender 프로세스/새 파일 사용 |
| 설치된 Tripo 애드온 | Tripo Bridge 1.0.21, Studio 결과 전송용, 포트 60600 |
| 추가 모델링 애드온 | Quad Remesher 1.4 설치 폴더 확인; 활성·라이선스 상태는 미검증. 추가 설치 없음 |
| Unreal | `/Game/Maps/L_Cindehearth`의 `CH_GormHouse_P2`에 v006 적용. 새 에셋 `/Game/Cindehearth/Tripo/GormHouse_P2_Repair_v006`, 이전 v001 보존 |

이 MCP는 **공식 Tripo SDK `tripo3d==0.4.2`를 사용하는 프로젝트 전용 어댑터**입니다. 공식 `tripo-mcp` 배포판 자체를 설치했다고 주장하지 않습니다. 공식 서버는 구형 Blender 애드온 프로토콜과 포트 9876에 의존하며, 현재 그 포트는 인증된 Revvy 편집 브리지가 사용 중입니다. 생성은 Tripo API, 모델 수정은 기존 typed Blender MCP로 연결했습니다.

## API 키 설정

Tripo 플랫폼에서 발급한 API 키가 필요합니다. Studio 로그인과 API 인증은 별도 준비 항목입니다. 키를 채팅이나 프로젝트 파일에 붙여 넣지 마세요.

로컬 PowerShell에서 아래 파일을 실행하면 숨김 입력으로 Windows 사용자 환경변수에 등록합니다.

```powershell
& .\Tools\Cindehearth\Pipeline\set_tripo_key.ps1
```

서버가 호출 시 사용자 환경변수를 다시 읽으므로 에디터를 재시작할 필요는 없습니다. MCP 목록이 이미 캐시된 클라이언트에서는 서버 새로고침이 필요할 수 있습니다. 아래 클라이언트는 현재 프로젝트 설정으로 즉시 호출할 수 있습니다.

```powershell
Saved/CindehearthTripoEnv/Scripts/python.exe Tools/Cindehearth/Pipeline/mcp_client.py tripo tripo_status
Saved/CindehearthTripoEnv/Scripts/python.exe Tools/Cindehearth/Pipeline/mcp_client.py blender blender_bridge_status
```

## 샘플과 아트 기준

[기준 이미지](reference/GormHouse_v001.png), [P2 생성 입력](sample_gorm_p2.json), [최종 플레이 카메라 구도](verification/gorm_p2_clear_player_camera.png), [밤 질감 확인](verification/gorm_p2_night.png).

플레이 카메라에서 문을 가리던 앞줄 집 `MinersHome_06`의 15개 부품을 서쪽으로 157.5cm 옮겼고, 고름의 집 옆 계단에 옹벽·덮개 4개를 보완했습니다. 새 외벽 문 옆에는 기존 감독관 곡괭이 표식의 별도 복사본 3개를 배치했습니다. 변경 전 위치와 최종 값은 `verification/p2_sightline_changes.json`, `verification/p2_supervisor_tool.json`에 기록했습니다. 계단 보완 이후에도 실제 PIE 이동 8지점을 다시 통과했습니다. 이 보완은 기본 배치 생성 후 적용되는 Pipeline 단계이며, 기본 레벨을 재생성할 때 함께 복원해야 합니다.

최종 플레이 카메라 구도는 PIE에서 읽은 PlayerCameraManager의 위치·회전·FOV를 사용한 에디터 캡처입니다. 이전 `gorm_p2_player_camera.png`는 앞줄 지붕이 가리던 수정 전 비교 화면입니다. 밤·서신 캡처는 옹벽·표식 보완 전의 조명과 컷신 검증 자료입니다. 현재 에디터는 `L_Cindehearth`, 낮, 챕터 진행 0, PIE 종료 상태로 저장했습니다. 전체 마을은 아직 블록아웃이며 P2 아트 교체는 고름의 집 외관 샘플까지입니다.

기준 이미지는 외관 가이드입니다. 실제 P2 결과는 `f79424fa-dbd0-4137-95c2-3e7a2ff0a14f`. 이전 `sample_gorm_image.json`과 `sample_gorm_house.json`은 H 시리즈 비교용 기록이며 최종 생성에 사용하지 않습니다. P2는 공식 v3 API `P2-20260801`, `quad=true`를 사용합니다. H 시리즈의 `smart_low_poly`와 다릅니다.

| 항목 | 신데하스 샘플 기준 |
|---|---|
| 비례 | 설계 7×6m, 벽 높이 3.5m. 기존 레벨에서 폭 315cm·깊이 270cm |
| 실루엣 | 낮고 묵직한 석조 몸체, 각진 급경사 지붕, 짧고 굵은 굴뚝 |
| 재료 | 거친 돌·그을음·놋쇠 보수판, 최소한의 목재 |
| 질감 | 축소 화면에서 읽히는 큰 색 덩어리, 과도한 미세 노멀·광택 억제 |
| 조명 | 텍스처에 강한 그림자를 굽지 않고 Unreal에서 낮·밤 및 화덕 조명 적용 |
| 생성 예산 | P2 face_limit 2,000, quad=true. 실제 1,907면 = 쿼드 1,456 + 삼각형 451. 렌더 3,363삼각형, 재질 1개 |
| 교체 위치 | 고름의 집 `(-25,-36,0)` 설계 m / `(-1125,-1620,0)` UE cm |

옥토패스의 공식 개발 인터뷰가 설명하는 2D 픽셀과 3D 환경의 결합, 조명과 캐릭터의 관계를 참고합니다. 위 수치와 신데하스 재료 규칙은 이 프로젝트를 위한 제작 판단입니다. 게임 카메라에서 낮·밤으로 비교해 질감 밀도와 그림자를 판단합니다.

## 생성과 정리 순서

아래는 **v001 생성·전송의 이력**입니다. 5~8단계의 변환·UV 검사만으로 메시 정리가 완료된 것은 아닙니다. 현재 수정본의 경로, 지붕 재구성, 텍스처 베이크, 검수 조건과 1,994삼각형 결과는 [v006 보수 기록](REPAIR_REPORT.md)을 따릅니다. 기존 v001 파일로 현재 액터를 다시 교체하지 마세요.

1. `tripo_status(check_balance=true)`로 인증과 잔액 확인. 생성은 크레딧을 사용합니다.
2. `tripo_generate_p2_image`에 `sample_gorm_p2.json`으로 한 요청만 제출하고 `request_id`, `task_id`를 기록. 같은 요청 ID 재호출은 기존 기록을 반환합니다. 전송 결과가 불확실하면 자동 재생성하지 않습니다.
3. `tripo_task_status(api_version="v3")`로 완료 확인 후 `tripo_download_result(api_version="v3")`로 전용 교환 폴더에 다운로드.
4. Blender 장면 검사 → 새 `.blend` 백업 → `CH_GormHouse_REF_` 접두어로 가져오기 → 메시·UV 기준 상태 기록.
5. 새 작업 복사본에서 필요한 비례·방향·바닥 피벗 조정만 수행합니다. P2 쿼드와 UV를 보존하며 무조건 병합/decimate하지 않습니다. H 비교본의 병합 실험은 교차 모서리를 만들어 채택하지 않았습니다.
6. Tripo의 텍스처와 UV0를 보존. UV를 새로 펼쳐 기존 텍스처를 망가뜨리지 않습니다. 재베이크가 필요하면 별도 지원 도구를 확인해야 합니다.
7. 315×270×260cm, 바닥 피벗 확인 후 `CH_GormP2_Game_v001`만 내보냈습니다. 쿼드 편집본은 FBX/.blend에 보존하고, 텍스처가 함께 저장되는 GLB를 Unreal 렌더용으로 사용합니다.
8. `/Game/Cindehearth/Tripo/GormHouse_P2_v001`에 가져와 배치 yaw=180으로 문을 남쪽으로 맞췄습니다. 낮은 메시 밀도를 그대로 사용하도록 Nanite를 끄고 3,363삼각형을 검증했습니다. 재질은 PixelGrid=512, NormalStrength=0.12, Roughness=0.92. 장작과 문패 위치도 조정했습니다.

파일 교환 루트: `%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth`. 생성 요청 기록: `Saved/CindehearthTripo`. 기존 파일과 원본 오브젝트는 덮어쓰지 않습니다.

교환 폴더의 `Blender/CH_GormP2_REF_v001.blend`, `Blender/CH_GormP2_Final_v001.blend`, `Export/CH_GormP2_Game_v001.fbx`, `Export/CH_GormP2_Game_v001.glb`가 실제 결과물입니다. 원본 fingerprint는 `4c4ce9fd1dc88904d46245c27feffb3b1a72c2d0378501fab5dff070f92891ba`, 최종은 `18c220125260c20be6db8054d3f2b0d573457432472428bebac52d8949825b7a`입니다.

P2 원본과 최종 모두 UV/노멀/유한값/퇴화 면 오류가 없습니다. 경계 803개, 비다양체 모서리 12개는 원본에 있는 경고로 유지됩니다. 완전 밀폐된 메시로 판정하지 않습니다. 면 수와 UV 상태를 그대로 보존했으며 무리한 자동 병합을 하지 않았습니다.

검증: [집 앞·계단 8지점 실제 보행 통과](verification/p2_walk.json), [Q12 종료 후 카메라·입력 복귀](verification/p2_epilogue_runtime.json), [서신 클로즈업](verification/gorm_p2_seal.png). 원래 집 12개 구성 액터는 삭제하지 않고 컴포넌트 표시와 충돌을 껐습니다. 복구 값은 `verification/p2_replaced_shell.json`, 원래 맵 파일은 `Saved/CindehearthAutomation/L_Cindehearth_before_P2.umap`에 보존했습니다.

## 근거와 검증

- [공식 Tripo MCP](https://github.com/VAST-AI-Research/tripo-mcp)
- [공식 Tripo SDK v0.4.2](https://github.com/VAST-AI-Research/tripo-python-sdk/tree/v0.4.2)
- [Tripo API 시작 가이드](https://developers.tripo3d.ai/en/docs/quick-start)
- [옥토패스 HD-2D 개발 인터뷰](https://www.unrealengine.com/spotlights/octopath-traveler-s-hd-2d-art-style-and-story-make-for-a-jrpg-dream-come-true?lang=en-US)

`test_submission.py`는 외부 요청 없이 중복 제출 방지·응답 유실 후 재제출 방지·인증 누락·요청 ID 충돌·P2 엔드포인트와 quad 옵션을 검증합니다. 실제 P2 생성·Blender 수정·Unreal 가져오기·플레이 검증도 완료했습니다.

MCP stdio 초기화 및 두 서버 연결, 오프라인 검사 7개와 Python 구문 검사가 통과했습니다. `verification/`에 결과를 보관합니다. Blender 클라이언트는 실행마다 48비트 무작위 요청 ID 시작값을 사용하여, 새 프로세스의 첫 편집이 이전 작업 ID와 충돌하지 않도록 합니다. 응답 유실 시 출력된 요청 ID를 `blender_job_status`로 조회하고 편집을 재제출하지 않습니다. P2는 SDK 0.4.2의 저수준 v3 전송 함수에 공식 문서의 입력을 전달합니다. Unreal 가져오기는 MCP 호출 스택 밖의 Slate 콜백에서 수행합니다.

`Saved`를 정리하여 Python 환경이 없어졌을 때는 프로젝트 루트에서 다음 명령으로 복구합니다.

```powershell
uv venv Saved/CindehearthTripoEnv --python 3.12
uv pip install --python Saved/CindehearthTripoEnv/Scripts/python.exe -r Tools/Cindehearth/Pipeline/requirements.txt
```
