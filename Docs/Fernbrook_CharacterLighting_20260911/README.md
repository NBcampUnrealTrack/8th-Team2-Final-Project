# 펀브룩 캐릭터 밝기 조정 — 2026-09-11

저장·레벨 재로드·실제 플레이 검증 완료. 물은 실행 시 정상이라는 사용자 확인에 따라 변경하지 않았다.

## 적용

펀브룩 전용 `MI_FB_Player_Lit`에서 원본 색을 유지하며 밝기 배율과 그늘 보정을 조절했다. Revvy 머티리얼 스킬의 기존 그래프 검토를 통해, 이미 있는 lit/emissive 혼합 구조를 재사용했다. 공유 마스터나 원본 스프라이트 수정 없이 자식 머티리얼 인스턴스만 추가했다.

| 조절 항목 | 이전 | 최종 |
|---|---:|---:|
| Tint RGB — 선형 색 밝기 배율 | 1 / 1 / 1 | 1.3 / 1.3 / 1.3 |
| AmbientLift — 그늘 보정 비율 | 0.25 | 0.45 |
| NormalStrength | 0.30 | 유지 |
| Roughness / Specular | 0.90 / 0 | 유지 |

`Tint`는 화면 밝기 30% 증가를 뜻하지 않는다. 톤매핑 전 선형 RGB 배율이다. `AmbientLift`만 올리면 밝은 곳에서는 변화가 작아, 최종값에는 Tint 보정도 포함했다. 알파·도트 원본·노멀·발밑 그림자는 유지한다. 새 광원이나 포스트 프로세스, 추가 Tick 로직은 없다.

## 에디터에서 조절하기

`/Game/Fernbrook/CharacterLighting/Materials/MI_FB_Player_Lit`

- 전체 캐릭터 밝기: `Tint`의 R/G/B를 같은 비율로 조절. A는 1 유지.
- 그늘에서 묻히는 정도: `AmbientLift`. 현재 0.45, 원래 값은 0.25.
- 같은 MI가 본체·장비 5개·머리·얼굴의 8개 스프라이트 슬롯에 적용된다.

펀브룩 맵은 전용 `BP_FB_GameMode` → `BP_FB_FieldCharacter`를 사용한다. 기존 리베르그로브 BP를 복사하고, 기존 BeginPlay 카메라 설정 다음에 8개 `SetMaterial` 호출을 연결했다. 카메라 -30° / 90°, 거리 1900cm, FOV 38°, 캡슐 half-height 48cm / radius 24cm는 그대로다. 원본 게임 모드·플레이어 BP·다른 마을은 변경하지 않았다.

## 검증

- Blueprint 두 개 컴파일 성공, 오류·경고 0. 그래프 연결 29개 readback 확인: `graph_verified.json`.
- 저장한 펀브룩 맵 재로드 후 실제 게임에서 새 게임 모드/캐릭터 및 8개 MI 슬롯 확인.
- 실제 이동 입력으로 278.21cm 이동, `idle_front → walk_down → idle_front` 전환 동안 8개 슬롯 유지: `movement_qa_002.json`.
- 최종 캡처: [입구](Preview/Character_Entry_Final_002.png), [다리](Preview/Character_Bridge_Final_001.png). 비교용: [입구 이전](Preview/Character_Entry_Before_001.png), [다리 이전](Preview/Character_Bridge_Before_001.png).
- 기존 에셋 1,013개 중 펀브룩 맵만 변경; 나머지 1,012개 SHA-256 동일. 신규 에셋 3개. 기존 241개 액터 transform 동일.
- 종료 상태: PIE 정지, 저장 안 된 맵/콘텐츠 0. `final_signoff.json` 참조.

## 범위와 증거 주의사항

현재 PIE 기본 플레이어의 회색 원본 아트는 교체하거나 재색칠하지 않았다. 장비 슬롯의 머티리얼 할당은 검증했지만, 해당 세션에는 장비/머리/얼굴 플립북이 비어 있으므로 모든 장비 조합의 외형 테스트를 완료한 것은 아니다. 모바일 패키징/성능 테스트는 별도다.

`Character_Shade_TrialTint130_001.png`는 전경 수관에 캐릭터가 가려져 밝기 비교에서 제외했다. 전경 나무 가림 현상은 이번 밝기 조정의 해결 범위가 아니다. `Character_Entry_Final_001.png`는 레벨 재로드 직후 지형 표시가 아직 안정되지 않은 캡처로, 최종 판정은 `Final_002`를 사용했다.

첫 `movement_qa.json`은 종료 직전 별도 캡처용 순간이동이 섞여 총 이동 거리 증거로 무효다. 이를 수정하고 순간이동 없는 `movement_qa_002.json`으로 재검증했다. 초기 PIE 중 material graph export 실패는 PIE 정지 후 정상 재조회로 해소했으며 공유 그래프는 수정하지 않았다.

## 복구

기존 맵 백업: `Saved/FernbrookRecovery/CharacterLighting_Before_20260911/L_Fernbrook_Entrance.umap`.

빠른 적용 해제는 펀브룩 World Settings의 GameMode Override를 `/Game/Libregrove/Level_V8/Gameplay/BP_LG8_GameMode`로 되돌리는 것이다. 맵 파일 전체 백업 복원은 이후 사용자 변경을 덮어쓸 수 있으므로 자동 실행하지 않는다. Git 커밋/푸시는 하지 않았다.
