# 신데하스 — 플레이 가능한 1차 레벨

> 2026-09-09 Git 전달 주의: 아래는 최초 제작 당시 기록이다. 최신 아트는 [Dense v004](Dense_v004/README.md) + [Fire v005](Fire_v005/README.md). 현재 작업 트리에서 전용 `NUCindehearthDirector` C++ 소스가 빠진 상태를 발견했으므로, 새 빌드 전에 [두 마을 전달 안내](../../Docs/Villages/README.md)의 복구 주의사항을 먼저 확인한다.

2026-09-05 / Naia Unbound / UE 5.8

레벨은 `/Game/Maps/L_Cindehearth`, 전용 에셋은 `/Game/Cindehearth`에 있습니다. 콘텐츠 브라우저에서 `L_Cindehearth`를 열고 Play를 누르면 기존 테스트 캐릭터로 남쪽 입구에서 시작합니다. 기존 프로젝트의 작은 HD-2D 캐릭터에 맞춰 **설계 1m = 엔진 45cm**를 적용했습니다.

## 공간 구성

- 마을 설계 크기 150×180m, 북쪽 +Y, 아래·중간·위 단 높이 0 / 3.5 / 7m.
- 하나의 남북 레일과 나란한 보행로, 두 경사로와 돌계단으로 세 단 연결.
- 30×24m 화덕 광장, 지름 10m 화덕, 식탁 5개, 교대 게시판.
- 북쪽 갱도·하역장, 개방형 대장간, 감정소와 지하 장부고, 2층 주막, 의무소, 주택 17채.
- 주거 계단 옆 고름의 집, 도구 표식·빨랫줄·굴뚝 연기, 공동묘지 30기.
- 서쪽 애쉬슬로프 5단, 남쪽 파인할로우와 벌목 장비, 북쪽 딥베인의 집하장·유기적 경계·지름 25m 공동.

[배치도](plan.svg), [구조·배치 원본](layout.json), [프리뷰](preview/).

주민과 나이아는 기존 스프라이트 및 임시 조형으로 배치했습니다. 소나무·동물·사슬 고치도 교체 가능한 임시 에셋입니다. 이 단계는 공간, 시야, 통행과 진행 상태를 검토하는 레벨이며 최종 배경 아트가 아닙니다.

## 진행 상태 확인

`CH_LevelDirector`의 `Completed Chapter Quest`, `Night`를 변경하고 `Apply Presentation`을 누르거나, PIE 콘솔에서 다음 명령을 실행합니다.

```text
nu.Cindehearth.Phase 0 0
nu.Cindehearth.Phase 0 1
nu.Cindehearth.Phase 7 1
nu.Cindehearth.Phase 8 1
nu.Cindehearth.Phase 9 1
nu.Cindehearth.Phase 11 1
nu.Cindehearth.Phase 12 1
```

| 완료 단계 | 표현·통행 변화 |
|---|---|
| 0 | 장부고와 깊은 갱도 봉쇄. 낮 광장 4명, 밤 광장 24명 |
| 7 | 장부고 뚜껑 및 충돌 해제. 사다리 근처 **F**로 출입 |
| 8 | 갱도 바리케이드 및 충돌 해제 |
| 9 | 밤 인원 24명 유지, 주민 움직임을 줄여 침묵 표현 |
| 11 | 할로우코어 고치가 사라지고 흩어진 빛 표시 |
| 12 | 차분한 광장 상태, 광장→계단→고름의 문→서신 18초 카메라 연출 |

Q12 연출 동안 이동·시점 입력을 잠그고 종료 후 복원합니다. 같은 PIE 세션에서는 한 번 재생됩니다. 재검토하려면 PIE를 다시 시작하세요. 사다리 출입은 오르기 애니메이션 전 단계의 위치 전환입니다. 실내 진입 시 주막·의무소·감정소 지붕을 숨겨 내려다보는 시야를 확보합니다.

`ANUCindehearthDirector`는 기존 `UNUQuestComponent`의 완료 이벤트에 연결됩니다. **실제 신데하스 퀘스트 ID 자료가 없으므로 `QuestStages` 매핑은 비워 두었습니다.** 실제 ID→완료 단계 매핑을 채워야 정식 퀘스트 진행에 자동 반응합니다. Q7의 두 문서 단서는 장부고에만 지정했으며 딥베인에는 중복 배치하지 않았습니다.

## 다음 제작 단계

- 실제 Q1~Q12 데이터·대사·상호작용 연결, 이전 지역에서의 진입 및 저장 상태 연동.
- 드워프 NPC·나이아·소나무·동물의 최종 아트, 사냥·보스·광차 밀기 동작.
- 망치·레일·교대 종·사슬·바람·대화 음원과 Q9/Q12 대화 음량 변화. 현재 분위기 전환은 시각 표현입니다.
- 에필로그 전령의 걷기·노크·서신 전달 애니메이션, 정식 연출 시퀀스와 영구 재생 기록. 현재는 카메라 경로와 전령/서신 배치입니다.
- 공동묘지의 서사상 5분 접근로는 현재 짧게 압축되어 있습니다. 외곽 지형 연결과 마감, 주막 2층 내부 접근, 주변 필드 전체 전투·내비게이션 검증도 후속 범위입니다.

## 제작 원본과 재현

`generate_layout.py`는 결정적 모듈 형상·배치 JSON과 SVG를 생성합니다. `build_assets.py`는 검증된 GeometryScriptService를 사용해 메쉬를 생성하고, `assemble_level.py`가 새 레벨에 배치합니다. `polish_level.py`, `finish_details.py`, `install_signs.py`, `install_director.py`, `refine_access.py`, `refine_fields.py`, `finish_lighting.py`, `finish_people.py`, `finish_epilogue.py` 순서로 마감·연출을 적용합니다. 한국어 표지판은 `render_signs.py`로 현지 폰트에서 생성한 텍스처입니다. `refine_fields.py`는 높은 애쉬슬로프 단의 경사로 진입부를 파내어 통행 공간을 확보합니다.

`call.py`는 로컬 Revvy 연결 도우미입니다. `--stage`를 항상 명시하세요. 기존 메쉬는 재생성하지 않고 기존 배치도 건너뛰므로, JSON 수정만으로 저장된 메쉬가 자동 갱신되지는 않습니다. 레벨을 다시 만들 때는 새 복제 경로를 사용하거나 의도한 대상만 갱신하세요. `inspect_*`, `repair_smoke.py`, `restart_editor.py`는 작업 중 진단 도구이며 전체 재생성 절차가 아닙니다.

```powershell
python Tools/Cindehearth/generate_layout.py
python Tools/Cindehearth/call.py --file Tools/Cindehearth/assemble_level.py --stage audit
python Tools/Cindehearth/call.py --file Tools/Cindehearth/test_play.py --stage start
python Tools/Cindehearth/call.py --file Tools/Cindehearth/qa_runtime.py --stage states
```

`validation/`에 상태·동선 검사 결과를 보관합니다. 기존 레벨·에셋과 분리된 새 콘텐츠이며 기본 시작 맵 설정은 변경하지 않았습니다.

검증 결과: `ProjectMEditor Win64 Development` 빌드 성공. 남북 보행로, 주거 계단, 공동묘지 길, 딥베인 통로, 애쉬슬로프 5단을 PIE 이동 입력으로 통과했습니다. 단계별 차단 충돌·밤 인원 24명·주민 스프라이트 실제 렌더 데이터, 장부고 출입 및 에필로그 종료 후 조작 복귀를 확인했습니다. 패키징·전투·음향·멀티플레이·저장 연동 검사는 포함하지 않습니다.
