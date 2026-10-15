# 신데하스 불 이펙트와 고정 카메라의 런타임 계약

2026-09-05. 프로젝트 C++와 저장된 배치·PIE 검증 자료만 읽어 확인했다. UE 호출, 에디터 변경, 소스 변경, 빌드를 수행하지 않았다. 현재 에디터의 불 머티리얼 세부 값은 root의 라이브 조사 결과를 우선한다.

## 실제 게임 카메라

최신 저장 증거 `Dense_v004/QA/receipts/routes_v004b/player_camera.json`의 실제 PIE 카메라는 다음과 같다. JSON의 회전 배열 순서는 **roll, pitch, yaw**다.

| 항목 | 값 |
|---|---:|
| 월드 pitch | −25° |
| 월드 yaw | 90°: 남쪽에서 북쪽(+Y)을 봄 |
| 월드 roll | 약0° |
| FOV | 32° |
| SpringArm 길이 | 2,100cm |
| SpringArm 상대회전 | pitch−25°, yaw약0° |
| SpringArm 충돌 검사 | 꺼짐 |

`NUCindehearthDirector.h:48–54`에도 같은 마을 카메라 기본값이 있다. `ConfigureVillageCamera()` (`.cpp:303`)는 기존 상대회전에서 **pitch만** 교체하고 길이·FOV·충돌 검사를 설정한다. yaw를 매 프레임 강제로90°로 설정하지 않는다. 현재 월드 yaw90°는 플레이어/부모 회전에서 상속된다. 같은 pawn의 SpringArm을 이미 설정했다면 바로 반환하므로 매 tick마다 저장된 pitch/FOV를 덮어쓰는 구조도 아니다. EndPlay에서 원래 카메라 설정을 복원한다.

`FieldPlayerCharacter.cpp:46–47`은 컨트롤러 yaw와 이동 방향에 따른 캐릭터 회전을 둘 다 끈다. 카메라 구성은 `:131–149`: Root→CameraAnchor→SpringArm→Camera, anchor Z10cm, arm 상대 Z24cm, camera lag 켜짐/속도5다. 생성자 기본값 pitch−5°/길이1,000cm/FOV30°는 마을 Director 적용 전 값이므로 현재 신데하스 연출 기준으로 사용하면 안 된다.

예외는 Q12 에필로그다. `PlayEpilogue()` (`Director.cpp:207`)가 별도 CameraActor를 생성해 **FOV38°**, 저장된 `EpilogueCameraRoute` 회전을 사용한다. 현재 작업의 정면 개선은 일반 플레이 카메라를 기준으로 하되 에필로그 첫 광장 컷에서 심한 카드 옆면 노출만 확인하면 된다. 카메라 렌즈나 전체 레벨 방향을 바꿀 이유는 없다.

## 불 카드의 방향·크기·머티리얼을 덮어쓰는가

**확인한 C++에는 기존 불 카드의 actor rotation, scale 또는 material을 덮어쓰는 처리가 없다.**

저장된 `Dense_v004/verification/level_before.json`에서 불은 Blueprint가 아닌 `StaticMeshActor`이며 다음 태그를 사용한다: `Cindehearth`, `CH.HD2D`. 충돌은 `NoCollision`이다.

| 기존 카드 | 저장된 회전 roll/pitch/yaw | 저장된 scale | 머티리얼 |
|---|---|---|---|
| `CH_HD2D_HearthFire0` | −90/0/0 | 1.296/1.296/1 | `MI_CH_PixelFire0` |
| `CH_HD2D_HearthFire1` | −90/0/60 | 1.296/1.296/1 | `MI_CH_PixelFire1` |
| `CH_HD2D_HearthFire2` | −90/0/120 | 1.296/1.296/1 | `MI_CH_PixelFire2` |
| `CH_HD2D_ForgeFire0` | −90/0/0 | 약0.790/0.6/1 | `MI_CH_PixelFire0` |
| `CH_HD2D_ForgeFire1` | −90/0/90 | 약0.790/0.6/1 | `MI_CH_PixelFire1` |

이는 v004 작업 전 기준 자료이며 현재 라이브 transform을 대신하지 않는다. 다음 코드들이 실제 런타임 덮어쓰기 범위를 정한다.

| 코드/태그 | 런타임 동작 | 현재 불 카드에 적용되는가 |
|---|---|---|
| Director `CH.Resident`, `.cpp:253` | 카메라 쪽으로 actor yaw 회전 | 아니오: 해당 태그 없음 |
| Director `CH.Crowd`, `.cpp:270` | 위치를 저장된 기준점 주변으로 흔듦 | 아니오 |
| Director `CH.InteriorRoof`, `.cpp:259–265` | pawn의 XY/Z 위치로 actor hidden 상태 변경 | 아니오 |
| Director `CH.Sky`, `.cpp:91–96` | 낮/밤 sky material을 슬롯0에 지정 | 아니오 |
| Director `CH.Window`, `.cpp:79` | 낮/밤 표시 여부 변경 | 아니오 |
| `NUSpriteVisualComponent` | 캐릭터 PaperSprite/PaperFlipbook 방향·크기·머티리얼 적용 | 현재 불은 해당 컴포넌트가 없는 StaticMeshActor |
| `NUCameraOcclusionFadeComponent` | `CameraFade` 태그가 있고 sweep에 맞은 컴포넌트의 custom data index0에 fade값 기록 | 현재 불은 태그 없음+NoCollision. 머티리얼 교체나 actor transform 변경도 하지 않음 |

따라서 불 카드 정면 방향·크기·재질 교체는 에디터에 저장한 값으로 유지된다. 기존 일반 태그를 유지하면 별도 C++ 변경이나 매 tick billboard 기능이 필요하지 않다.

## 낮/밤 전환에서 유지해야 할 부분

`ApplyPresentation()` (`Director.cpp:99–103`)는 **`CH.Light.Hearth` 태그의 PointLight intensity를 낮240 / 밤580으로 설정**한다. 저장 자료에서 이 태그가 있는 액터는 `CH_HearthLight`, `CH_HearthBounce` 두 개다. 값은 BeginPlay, `SetNight`, `SetChapterProgress`, 직접 `ApplyPresentation()` 호출 때 다시 적용된다. 해당 light의 rotation, scale, 위치, light color, attenuation radius는 이 분기에서 변경하지 않는다.

이 밝기는 불 **카드의 emissive multiplier와 별개**다. 카드 자체의 낮/밤 material 교체나 emissive parameter 업데이트는 발견되지 않았다. 기본 해·하늘광·sky material은 낮/밤에 바뀌므로 주변 화덕·광장 표면은 따뜻해지고, 불의 화면 밝기는 tonemapping/bloom/exposure 영향을 받을 수 있다. 불 모양만 자연스럽게 만드는 이번 작업 때문에 밤 조명 분기를 수정할 필요는 없다.

## 머티리얼 애니메이션과 정면 배치에 대한 판단

기존 제작 스크립트 `Restyle/build_effects.py`는 Fire를 **TwoSided / Masked / Unlit**, UV44×52 셀, Time을10Hz로 양자화한 custom HLSL, 카드별 `Phase`로 생성했다. Fire의 움직임은 mask/color 변화이며 **Fire에는 WPO 연결이 없다**. 위로 이동하는 WPO는 별도 Smoke에만 있다. `Restyle/refine_flames.py`가 HLSL 높이 계산을 변경했고 `fix_flame_orientation_v3.py`가 카드 roll을270°(−90°)로 교정했으므로, 원본 생성 스크립트의 `h=1-q.y`나 roll90°를 그대로 다시 적용하면 기존 UV 상하가 뒤집힐 수 있다.

일반 카메라에서는 yaw0° 카드가 넓은 정면을 보여준다. yaw60/120° 또는90° 교차 카드는 현재 고정 시점에서 겹친 칼날 같은 실루엣이나 옆면을 만들 수 있다. 정면을 중심으로 여러 불 혀를 배치하고 깊이·높이·Phase 차이를 주는 방식은 런타임 코드와 충돌하지 않는다. 교차 카드 전체를 카메라 회전 기능으로 전환할 필요는 없다.

기하학적으로 카메라 pitch−25°에 완전히 맞춘 카드 평면의 법선은 대략 `(0,−0.9063,+0.4226)` 또는 반대 방향이다. 기존 roll−90°의 UV 상하 관례를 그대로 유지하며 기울이는 후보는 **roll−115°(245°), yaw0°**다. 이는 소스와 축 계산에서 얻은 후보이며 실제 UV·화덕 바닥 접점은 root의 라이브 미리보기에서 검증해야 한다. TwoSided이므로 정면 법선 부호보다 UV 상하와 바닥에 붙는 불의 뿌리가 중요하다.

기존 스크립트 일부에는 `MaterialNodeService.get_material_diagnostics()` 호출이 남아 있다. 이전 세션의 실제 강제 재컴파일 충돌 원인이므로 이 문서의 조사는 해당 스크립트를 실행하지 않았으며, 불 작업 검증에도 그 호출을 재사용하지 않는다.
