# 뒷숲 포탈 16037 — 서버 요청서

작성 2026-10-14 · 대상: 서버 담당 · 관련 클라 작업은 main 에 병합됨 (PR #251 포탈, #258 넝쿨)

## 한눈에

리베르그로브 허시베일 폭포 뒤 바위틈과 뒷숲(지역 11008)을 잇는 포탈 **16037** 을 클라 포탈 표에 추가했습니다.
포탈 이동은 서버가 `PortalMove.portal_id` 를 서버 포탈 표에서 찾아 승인하므로, 서버 표에 이 행이 없으면 **로그인 상태에서는 이 포탈로 이동할 수 없습니다** (에디터 오프라인 플레이에서는 이동됨).

부탁드리는 것은 하나입니다.

1. 서버 포탈 표에 아래 한 행을 추가해 주세요. 다른 표는 바꿀 것이 없습니다. 지역 11008, 장소 12008 · 12026 은 기획 표에 원래 있던 행입니다.

## 추가할 행

클라 `Scripts/Data/PortalTable.csv` 38행과 같은 값입니다.

| 칸 | 값 |
|---|---|
| PortalId | 16037 |
| PortalKey | ST_PORTAL_16037 |
| DisplayName | 뒷숲 바위틈 |
| SourceAreaId / SourceLocationId / SourceEntryPointId | 11001 / 12008 / EP_12008_Crack |
| DestinationAreaId / DestinationLocationId / DestinationEntryPointId | 11008 / 12026 / EP_12026_Passage |
| PortalType / ActivationMode | AreaTransition / Overlap |
| bTwoWay / bLoadingScreen | TRUE / TRUE |
| RequiredLevel | 9 |
| UnlockQuestId | 10015 (회당이 열리다, 완료해야 열림) |
| RequiredItemId | 0 |
| bEnabledByDefault | FALSE |

## 서버 판정에 쓰이는 값

- 양방향 포탈입니다. `source_side` 0 = 리베르그로브 쪽(12008 에서 출발), 1 = 뒷숲 쪽(12026 에서 출발).
- 열리는 조건: 레벨 9 이상, 퀘스트 10015 완료. 클라도 같은 조건으로 막고, 10015 완료 전에는 폭포 앞이 넝쿨로 덮여 있습니다 (`ActorPresenceTable` Backgrove_Vines_10 / _20).
- `entry_point_id` 는 클라가 보내는 값을 그대로 돌려주시면 됩니다 (EP_12026_Passage, 돌아올 때 EP_12008_Crack).

## 확인 방법

1. 퀘스트 10015 를 완료한 캐릭터로 로그인해 리베르그로브 허시베일 폭포 앞 바위틈(파란 빛의 상자)에 들어가면, 뒷숲 뿌리터 입구로 이동합니다.
2. 뒷숲 통로 입구의 빛의 상자로 들어가면 폭포 앞 EP_12008_Crack 으로 돌아옵니다.
3. 10015 를 완료하지 않은 캐릭터는 넝쿨이 덮여 있고, 이동이 거부되어야 합니다.
