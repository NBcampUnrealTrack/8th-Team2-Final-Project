# 펀브룩 초입 — 현재 레벨 작업본

2026-09-11. 기존 두 마을과 같은 88cm 플레이어 기준의 숲 필드. 펀브룩 기반 에셋이 아직 미등록이어서 캐릭터 밝기 수정과 함께 Git에 정리한다.

## 실행 / 현재 상태

UE 5.8 프로젝트에서 `/Game/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance`를 열고 Play한다. Git LFS 에셋이 내려받아져 있어야 한다. 게임 모드는 이 맵에만 지정된 `BP_FB_GameMode`, 플레이어는 `BP_FB_FieldCharacter`다.

- 241 액터, 식생 타입 14종, 인스턴스 8,981개.
- V5: 텍스처가 적용된 꽃·잡초·다리·나무 반영, 다리 받침과 진입부 보정.
- V6: 큰 나무 513개와 작은 나무 311개의 크기·색감 조화. 기존 식생 transform 유지.
- 캐릭터: 원본 아트와 lit 머티리얼을 유지한 채 Tint RGB 1.3, AmbientLift 0.45. 본체·장비·머리·얼굴 8개 슬롯에 BeginPlay에서 한 번 적용.
- 카메라: Pitch -30°, Yaw 90°, 거리 1900cm, FOV 38°. 물은 실행 시 정상이라는 사용자 확인을 받았으며 마지막 밝기 조정에서 변경하지 않았다.

## 조절 위치 / 최종 검증 자료

| 항목 | 현재 에셋 또는 안내 |
|---|---|
| 캐릭터 밝기 | `/Game/Fernbrook/CharacterLighting/Materials/MI_FB_Player_Lit`, [설정·검증](../Fernbrook_CharacterLighting_20260911/README.md) |
| 나무 색·크기 | `/Game/Fernbrook/Polish_V6/`, [V6 최종 문서](../Fernbrook_TreePolish_V6_20260911/README.md) |
| 다리·꽃·잡초 | `/Game/Fernbrook/Polish_V5/`, [V5 문서](../Fernbrook_Textured_V5_20260911/README.md) |
| 물 / 레벨 색보정 / DOF | `/Game/Fernbrook/Polish_V3/Materials/MI_FB3_ClearCreek_Windows`, [조절값](../Fernbrook_Polish_V3_20260911/CONTROLS.md) |
| 기본 HD2D 공통 파라미터 | [V1 공통 컨트롤](../Fernbrook_Entrance_V1_20260911/HD2D_CONTROLS.md) |

현재 나무에는 **V6**, 물에는 **V3**, 캐릭터에는 **CharacterLighting** 문서를 우선 적용한다. V1/V3의 이전 나무·물 설정은 이력이며 현재 에셋 연결을 이전 버전으로 되돌리지 않는다. 각 단계 문서의 ‘Git 미커밋’ 문구는 해당 작업 종료 당시의 상태다.

실제 Windows PIE에서 V6 보행 87/87 지점 통과 후, 밝기 조정에서는 저장 맵 재로드·입구/다리 화면과 278.21cm 실제 이동, 대기→걷기→대기 전환 중 8개 머티리얼 슬롯 유지까지 검증했다. 새 게임 모드/캐릭터 BP 컴파일 오류·경고는 0이다. 에디터 최종 상태는 PIE 정지, 미저장 패키지 0개였다. 이번 Git 정리에서는 엔진/에셋을 다시 편집하지 않았다.

## 커밋 범위

`Content/Fernbrook/` 전체는 현재 맵을 여는 데 필요한 기반·파생 에셋을 포함한다. 공유 리베르그로브 셰이더와 기존 스프라이트는 이미 저장소에서 관리 중이며 변경하지 않았다. 바이너리와 검증 PNG는 기존 Git LFS 규칙을 사용한다.

최종 문서·검증 JSON·직접 연결된 비교 이미지와 캐릭터 밝기 작업/검증 스크립트를 포함한다. 로컬 MCP 인증 설정, raw MCP 요청·응답, 캐시, 실패/중복 캡처, Blender 원본·복구본, ZIP 패키지와 관계없는 마을 작업은 포함하지 않는다. 파일을 삭제하지는 않는다.

`Tools/Fernbrook_CharacterLighting_20260911/`의 스크립트는 **단계별 작업 이력**이다. 이미 반영된 레벨에 생성·배치·복구 단계를 일괄 재실행하지 않는다. MCP 클라이언트는 사용자 환경의 공식 프로젝트 연결과 환경변수 인증을 사용하며 인증정보를 포함하지 않는다. 검증 로그와 로컬 백업 경로는 작성 머신의 기록이다.

Git 정리 당시 에디터는 닫혀 있었다. 저장 파일의 UE 5.8 패키지 이름 테이블과 문자열 참조를 오프라인 검사했다. `SM_FB1_Bush`에서 발견된 `/Game/Village/Meshes/MAT_Leaf` 문자열은 헤더 밖의 이전 Interchange 임포트 데이터에 남아 있으며, 실제 패키지 이름 테이블은 포함된 `MI_FB1_Leaf`를 참조한다. 이 검사는 에디터 Asset Registry 재검증이나 Cook 테스트를 대신하지 않는다. 선별·보안 검사·구문 검사·스테이징 로직은 `Tools/FernbrookDelivery/prepare_git.py`에 있다.

## 남은 항목

나이아 쉼터와 일부 다리 끝의 전경 수관 가림은 별도 폴리싱 항목이다. 현재 플레이어의 회색 기본 아트는 재색칠하지 않았고, 빈 장비 슬롯은 연결만 검증했다. 모든 장비 조합 외형, 모바일·패키징·최종 LOD/성능, 신규 퀘스트·전투·맵 전환 검증은 완료한 것으로 보지 않는다. 신데하스의 기존 런타임 소스 누락 문제도 이 커밋의 수정 범위가 아니다.
