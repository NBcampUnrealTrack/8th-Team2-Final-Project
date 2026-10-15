# VillageLayout — `L_VillageEntrance` (아랫마을) 재생성 도구

2026-09-03 세션에서 HD-2D(옥토패스 트래블러풍) 마을 레벨을 절차적으로 만든 스크립트 모음입니다.
레벨 자체(`Content/Maps/L_VillageEntrance.umap`)와 에셋(`Content/Village/**`)은 이미 완성되어 있으며,
이 폴더는 **배치를 바꾸거나 처음부터 다시 만들 때** 씁니다.

## 스케일 규칙 (중요)

플레이어 스프라이트는 **88 cm**(14×22 px, PPU 0.25), 캡슐은 96 cm 다. 배경은 사람 크기(레이아웃 미터)로 설계하고 `layout_village.json` 의 **`world_scale` (현재 0.45)** 로 줄여서 배치한다.

- `gen_placements.py` 가 모든 좌표를 `layout_m × 100 × world_scale` cm 로 변환하고, 오브젝트 스케일도 같은 비율을 곱한다.
- **랜드스케이프 액터도 같은 비율**이어야 한다: 스케일 `(45,45,45)`, 위치 `(-2857.5,-2857.5,0)`. 이러면 랜드스케이프 버텍스 1개 = 레이아웃 1 m 가 되어 페인트·높이 래스터를 그대로 쓸 수 있다.
- **랜드스케이프 머티리얼의 `TileCm_*` 도 같은 비율로** 줄여야 지면 텍스처가 같이 작아진다 (Grass/Dirt 540, Cobblestone 450, Moss 630, Cliff 360, Macro 2700, EdgeNoise 225).
- 소품 머티리얼은 UV 기반이라 메시가 줄면 무늬도 같이 줄어든다 — 손댈 필요 없다.
- 결과 비율: 집 3.9×3.5×3.5 m, 문 1.2 m, 문기둥 1.9 m, 울타리 0.53 m, 나무 3.3 m, 큰길 폭 2.0 m. 캐릭터(0.88 m) 기준으로 문이 1.35배, 나무가 3.7배다.
- `heights` 스테이지는 **절대값**(BASE_Z + delta)이라 여러 번 실행해도 낙차가 누적되지 않는다.

## 구성

| 파일 | 역할 |
|---|---|
| `layout_village.json` | 마을 설계 (미터 단위). 도로·광장·집 11채·소품·울타리·돌담·생울타리·등불·나무 존·이끼 존·절벽 림 |
| `gen_placements.py` | 설계 → `placements.json`(cm 좌표, 127×127 페인트 래스터, 높이 델타) + `plan_topdown.png` 미리보기. `python gen_placements.py` |
| `assemble_village.py` | 언리얼 에디터 안에서 실행하는 배치 스크립트. 맨 앞에 `STAGE='...'` 를 붙여 전송. 스테이지: `heights`, `paint`, `houses`, `props`, `linear`, `lanterns`, `rim`, `foliage`, `stats`, `clear_<stage>`. 각 스테이지는 자기 폴더의 액터를 먼저 지우므로 재실행해도 중복되지 않음 |
| `revvy-call.ps1` | Revvy 풀액세스 JSON-RPC 헬퍼 (`http://127.0.0.1:8091/mcp`, 사용자 환경변수 `REVVY_FULL_ACCESS_TOKEN`) |
| `HOWTO_REVVY.md` | 에디터 자동화 시 검증된 주의사항 (Rotator 인자 순서, BGR 스크린샷, 크래시 안전 임포트 경로 등) |
| `blender/blend_build.py` | Blender 5.0 헤드리스로 소품 25종 FBX 생성. `"C:/Program Files/Blender Foundation/Blender 5.0/blender.exe" -b --python blend_build.py -- <출력폴더>` |
| `blender/blend_verify.py`, `blend_contact.py` | FBX 재임포트 검증 + 미리보기 컨택트시트 |
| `texgen/texgen_gen.py` | 소품용 256px 픽셀 텍스처 10종(BC/N/ORM) 절차 생성 (PIL+numpy). `python texgen_gen.py <출력폴더> dx` |
| `reference_village.png` | 목표 레퍼런스 이미지 |

## 재생성 순서 (배치만 바꿀 때)

1. `layout_village.json` 수정 → `python gen_placements.py` → `plan_topdown.png` 확인.
2. `assemble_village.py` 상단 `PLACEMENTS` 경로를 새 `placements.json` 위치로 바꾸기.
3. 에디터에서 `L_VillageEntrance` 열고, 아래처럼 스테이지별 실행 (헬퍼 사용):
   ```powershell
   # 예: houses 스테이지
   # code = "STAGE='houses'`nFRONT_YAW_OFFSET=180.0`nLINEAR_YAW_OFFSET=0.0`n" + (assemble_village.py 내용)
   powershell -File revvy-call.ps1 -Tool execute_python_code -ArgsFile args.json
   ```
   순서: `paint` → `heights` → `houses` → `props` → `linear` → `lanterns` → `rim` → `foliage`.
4. 레벨 저장.

## 하늘 (M_HD2D_Sky)

옥토패스 트래블러류 HD-2D 는 물리 대기 대신 **그려진 스카이 돔**을 쓴다. 이 레벨도 같은 방식이다.

- `SkyAtmosphere` 는 숨김(`visible=False`). 대신 액터 `SkyDome_HD2D` (엔진 `BasicShapes/Sphere`, 스케일 2000 = 반지름 1 km, TwoSided, 그림자 끔)에 `MI_HD2D_Sky` 를 입혔다.
- 머티리얼 `M_HD2D_Sky` 는 Unlit + `IsSky` 이며, 전체 하늘을 Custom HLSL 노드 하나로 그린다. 방향은 `CameraVectorWS * -1` 이라 시차 없이 무한히 먼 하늘로 보인다.
  - 3단 수직 그라디언트 (`HorizonCol` → `MidCol` → `ZenithCol`)
  - `T_Sky_Clouds` R/G 채널 = 큰 적운 / 얇은 권운, 돔 평면 투영으로 흘러감 (`Tune` = x 커버리지, y 스케일, z 속도)
  - 같은 텍스처 B 채널 = **열별 능선 높이 프로파일**(풀레인지 0~1). 셰이더에서 `smoothstep` 으로 실루엣을 만들어 원경 산맥 3겹을 그린다. 텍스처에 실루엣을 굽지 않는 이유는 8비트 양자화로 계단이 생기기 때문.
  - 태양 디스크 + 글로우 (`SunDir` 은 `Sun_Directional` 의 forward 를 뒤집은 값), 지평선 난반사 밴드, 지평선 아래 `GroundCol` 로 어두운 원경.
- 색·구름 밀도·산 높이는 전부 `MI_HD2D_Sky` 파라미터로 조정. 텍스처 재생성은 `texgen/gen_sky_roof.py`.

### 옥토패스풍 보정 (2차)

레퍼런스 조사 결과 HD-2D 하늘의 특징은 ① 채도 낮은 파스텔 그라디언트 ② 부드럽게 흐르는 painterly 구름 ③ 지평선 근처의 강한 대기 원근(원경일수록 하늘색에 녹아듦) ④ 따뜻한 지평선 글로우 였다. 이에 맞춰:

- 팔레트를 파스텔로: 천정 `(0.24,0.42,0.66)`, 중간 `(0.56,0.70,0.82)`, 지평선 `(0.98,0.82,0.63)`.
- 지평선 위에 `exp(-h*42)` 헤이즈 밴드를 덮어 원경 산맥이 하늘로 녹아들게 함. 먼 산맥은 헤이즈 색과 70% 섞는다.
- 구름은 태양 방향 면이 따뜻해지도록 `SunDir` 기준 틴트를 넣었다.
- 구름 페이드는 `smoothstep(0.16, 0.78, h)`, 돔 평면 투영 분모는 `max(h, 0.11)`. **페이드 시작이 클램프 값보다 높으면 그 고도에 직선 이음선이 보인다.**
- 구름 텍스처는 **가로세로 모두** 타일링되어야 한다. 처음엔 가로만 wrap 시켜서 하늘 중턱에 수평 직선이 나왔다.

### 하늘 작업 시 함정 (실제로 겪음)

- 돔 컴포넌트의 `bounds_scale` 를 키우면 바운드가 20 km 를 넘어 **통째로 컬링**되어 하늘이 검게 나온다. 1.0 유지.
- 돔의 `affect_dynamic_indirect_lighting` 을 끄면 실시간 캡처 스카이라이트가 돔을 못 봐서 "IsSky 메시가 필요합니다" 경고와 함께 검게 나온다. 켜 둘 것.
- 셰이더 컴파일은 **비동기**다. `compile_material` 직후 캡처하면 검은 화면 + "skydome does not cover" 경고가 찍힌다. 40~60초 기다린 뒤 캡처할 것.

## 광장 분수

- `SM_Fountain` (Tripo 스캔 모델 정리본) + `SM_Fountain_Water` (수면 전용 메시). 둘 다 `Village/Props` 폴더, 같은 트랜스폼 (0, 1620, -2) / 스케일 1.9 → 폭 190 cm, 높이 105 cm (캐릭터 88 cm 기준).
- 수면 메시(`WATER_v3` 방식)는 **수반 안쪽 벽을 각도마다 레이캐스트해서 그 반지름 그대로** 만든다. 원판으로 만들면 팔각형 모서리에 마른 틈이 생기고, 반대로 키우면 림 위로 삐져나온다. 96 분할 링, z 0.148, 안쪽 반지름 0.10, 바깥은 측정값 + 0.012(돌 속으로 물림).
- 수반 실측값(Blender, SM_Fountain.fbx 기준): 바닥 z 0.0391, 림 윗면 z 0.199, 안쪽 벽 반지름 0.338(면)~0.381(모서리), 기둥 받침 단 윗면 z 0.1518.
- 수면 z 는 받침 단(0.1518)보다 **낮게** 둔다. 위로 두면 단 위에 얇은 물막이 생겨 밝은 고리로 빛나 보인다.
- 상부 소반은 실제로 비어 있지 않다(z 0.40 에서 기둥 반지름 0.055~0.068). 옛 버전의 상부 디스크는 지오메트리 안에 묻혀 있었고 지금은 제거했다.
- 물 머티리얼 `MI_HD2D_Water`: Translucent + DefaultLit, 월드 좌표 UV, 128px 6단계 코스틱 텍스처 2겹. **RippleScale 은 크기 배수라 값이 클수록 물결이 커진다** (빈도가 아님).
- 우물물은 잔잔·맑게 맞췄다: `RippleSpeed` 0.08(기본 1.0), `RippleScale` 0.6, `BandStrength` 0.30, `HighlightStrength` 0.30, `OpacityBase` 0.46(바닥이 비쳐야 맑아 보인다), `SpecularBase` 0.12(높으면 하늘을 거울처럼 비춰 뿌옇게 뜬다), `RoughnessBase` 0.34, `ShallowDistance` 14, `FoamWidth` 1.2.
- `layout_village.json` 의 분수 스케일은 4.2222 다. `gen_placements.py` 가 `layout_scale × world_scale(0.45)` 로 계산하므로, 실제 액터 스케일 1.9 를 얻으려면 1.9/0.45 를 적어야 한다.

## 픽셀 밀도 기준

"도트 느낌이 안 난다"는 텍셀 크기 문제다. 게임플레이 카메라(10 m, FOV 30, 2038 px)에서 화면 1 px ≈ 2.6 mm 이므로, **텍셀 하나가 1.5~3 cm(화면 5~11 px)** 여야 도트로 읽힌다.

- 지형: 텍셀 = `TileCm / 텍스처 가로`. 예전 540/512 = 1.05 cm 라 뭉개졌고, 512→256 으로 낮춰 2.1 cm 로 맞췄다.
- 소품: 한 번 반복 = `4 / Tiling × 0.45` m. 예전 Tiling 4 / 256 px = 1.8 mm(화면 0.67 px, 서브픽셀 노이즈)였다. 텍스처 64 px + Tiling 2.0 으로 1.4 cm.
- `max_texture_size` 를 낮추는 것은 원본 데이터를 지우지 않으므로 값을 되돌리면 원상복구된다.
- 주의: `Texture2D.blueprint_get_size_x()` 는 현재 스트리밍된 밉 크기를 돌려주므로 설정 검증에 쓸 수 없다.

## 픽셀 격자를 월드에 고정 (`M_PixelPropWA2`)

메시 UV 로 타일링하면 텍셀 격자가 면마다 제각각 기울어, 45도 면에서 픽셀이 대각선으로 뭉개진다. 소품 머티리얼 17종을 전부 **월드 좌표 삼면 투영** 마스터 `M_PixelPropWA2` 로 옮겨, 텍셀이 항상 월드 축에 정렬되고 옆면에서는 열이 수직으로 선다.

- UV 는 `월드좌표 / TileCm`. 옆면은 V 를 월드 −Z 에 물려(`float2(P.y,-P.z)` / `float2(P.x,-P.z)`), 윗면은 `float2(P.x,-P.y)`.
- **축 블렌드를 쓰지 않는다.** 엔진의 `WorldAlignedTexture` 는 세 투영을 부드럽게 섞어서, 정확히 45도인 면(분수 팔각형이 그렇다)에서 두 장이 반반 겹쳐 뭉개진다. 대신 `step` 으로 지배축만 1인 가중치를 만들어 하드로 고른다.
- **UV 를 분기(if/else)로 고르면 안 된다.** UV 가 불연속이 되어 하드웨어 파생값이 튀고, 이 프로젝트에서 D3D12 접근 위반으로 에디터가 죽었다. 축마다 **연속인 UV 로 따로 샘플**(맵당 3장, 총 9장)한 뒤 가중치로 더하는 형태여야 안전하다.
- TileCm = 텍스처 가로 × 목표 텍셀(1.4 cm). 64 px → 90, 128 px → 180, 256 px → 360. 절벽 480 / 바위 320 은 덩치가 커서 그대로 뒀다.
- **얇은 소품은 예외.** 월드 투영은 물체 크기와 무관하므로 기둥·난간처럼 얇은 것은 텍스처의 좁은 조각만 걸려 밋밋해진다. `Wood` `Timber` `Metal` `Bark` `Cloth` 는 TileCm 60 으로 좁혀 결을 살렸다.

### `MaterialNodeService` 함정

- `connect_expressions` 의 핀 이름은 **표시명**이다. `TextureSample` 은 `Tex` / `UVs` 로 연결해야 하고, `get_expression_pins` 가 돌려주는 프로퍼티명(`TextureObject` / `Coordinates`)으로는 실패한다. `ComponentMask` 는 입력 이름이 빈 문자열(`''`)이다.
- 반환값이 `False` 여도 예외가 안 나므로, 연결마다 결과를 모아서 확인해야 한다. 안 그러면 컴파일 에러 메시지("Missing input")만 보고 원인을 못 찾는다.
- `MaterialExpressionInfo` 의 id 필드는 `expression_id` 가 아니라 `id` 다.
- `export_material_graph` 의 expression 키도 `name` 이 아니라 `id` 다. `name` 으로 순회하면 조용히 아무것도 안 지운다.

### 캡처 함정

- `capture_from_pose` 는 **에디터 뷰포트 카메라 위치 기준으로 거리 컬링**한다. 뷰포트가 멀면 건물이 통째로 빠지고 그림자만 남는다. 캡처 전에 `ViewportService.set_camera_location/rotation` 으로 같은 지점에 옮길 것.
- 에디터를 새로 띄우면 `set_game_view(True)` 와 `set_exposure_game_settings()` 가 풀린다. 안 하면 기즈모가 찍히고 노출이 달라진다.

## 스프라이트 라이팅 (옥토패스식)

도트 캐릭터가 빛을 받도록 노멀맵 파이프라인을 붙였다.

- **머티리얼**: `/Game/Village/Materials/M_PixelSprite_Lit` + 인스턴스 `MI_PixelSprite_Lit`. Masked / DefaultLit / TwoSided, `SpriteTexture` → BaseColor·OpacityMask, `SpriteAdditionalTexture_0` → Normal. 파라미터: `NormalStrength`(1.0), `Roughness`(0.85), `Specular`(0.25), `Tint`, `AmbientLift`(0.12 — 그늘에서도 실루엣이 읽히게 하는 미세 자체발광). 원본 `M_PixelSprite_Lit_N` 은 건드리지 않았다.
- **노멀맵**: 스프라이트 원본 텍스처마다 같은 해상도로 생성해 `<이름>_N` 으로 옆에 저장(TC_NORMALMAP, sRGB off, TF_NEAREST, 밉 없음). 실루엣 거리장(둥근 볼륨) 0.75 + 아트 명도 0.25 를 8배 슈퍼샘플로 합성.
- **연결**: `PaperSprite.additional_source_textures[0]` = 노멀맵, `PaperSprite.default_material` = `MI_PixelSprite_Lit`. **PaperFlipbook 의 `default_material` 도 같이 바꿔야 한다** — PaperFlipbookComponent 는 스프라이트가 아니라 플립북에서 머티리얼을 가져오므로 스프라이트만 바꾸면 화면에 반영되지 않는다. 현재 스프라이트 77/77, 플립북 26/26 적용.
- **노멀맵 규약**: 이 프로젝트의 스프라이트는 DirectX(green down) 기준이 맞다. 위쪽 가장자리 G < 128, 왼쪽 가장자리 R < 128 이 정상. 반대로 구우면 태양과 반대쪽이 밝아진다. 규약 정정 스크립트는 `texgen/fix_sprite_normal_convention.py`.
- **런타임 강제 해제 (C++ 1곳)**: `Source/ProjectM/Field/NUSpriteVisualSettings.cpp` 가 `Settings.Material` 에 Paper2D 언릿 머티리얼을 하드코딩해서 `NUSpriteVisualComponent::RefreshAppearance()` 가 BeginPlay 마다 스프라이트를 언릿으로 되돌렸다. 이 대입을 제거해 **Material 을 null 로 두었다** — 헤더 주석대로 null 이면 스프라이트/플립북 애셋의 머티리얼을 그대로 쓴다. 덕분에 노멀맵이 붙은 스프라이트만 라이팅을 받고, 아직 준비 안 된 전투 아바타·필드 적 등은 종전 그대로다. 여기에 라이팅 머티리얼 경로를 하드코딩하면 노멀맵 없는 스프라이트가 머티리얼 기본 노멀맵을 뒤집어쓴다.
- **그림자 · 태양 각도(중요)**: 스프라이트는 카메라를 향해 빌보드하므로 태양 방위에 따라 결과가 갈린다.
  - 정면광(yaw 45): 스프라이트는 밝지만 그림자가 전부 캐스터 뒤로 떨어져 안 보인다.
  - 역광(yaw ~200): 그림자는 보이지만 스프라이트 정면이 그늘로 죽는다.
  - **측면광(yaw 170, pitch -48) 채택**: 그림자가 화면 오른쪽으로 길게 떨어져 또렷하고, 노멀맵 덕분에 스프라이트는 왼쪽이 밝고 오른쪽이 어두운 입체 음영을 얻는다. 옥토패스의 조각된 느낌이 이 조합에서 나온다. 그레이징 각도라 광량이 줄어 태양 9.5 / 스카이라이트 6.5 / 노출 -0.7 로 보정했고, `light_source_angle` 1.6 으로 그림자 가장자리를 부드럽게 했다.
  - 태양을 다시 돌릴 때는 `MI_HD2D_Sky` 의 `SunDir` 도 라이트 forward 의 반대 벡터로 같이 갱신해야 하늘의 해 위치가 어긋나지 않는다.
- `Village/NPCs` 10명 전원과 플레이어 폰 스프라이트 모두 `cast_shadow` 켜짐(플레이어는 `MakeFieldCharacter()` 의 `bCastShadow=true` 경유).
- **발밑 블롭 그림자 (C++)**: 빌보드 스프라이트는 태양이 옆에서 오면 판이 빛과 나란해져 실제 그림자가 거의 안 생긴다. 그래서 `UNUSpriteVisualComponent::RefreshBlobShadow()` 가 `MI_HD2D_BlobShadow` 데칼을 발밑(`Settings.SpritePivotZ`)에 깔고 `Settings.BlobShadowRadius`(NPC·플레이어 26 cm, 정령·적·전투 아바타는 0)로 크기를 정한다. 애셋이 없으면 조용히 건너뛴다.
- **데칼 함정**: Deferred Decal 은 `Unlit + Translucent` 로 두면 이미시브 **가산** 모드가 되어 어두운 색을 더해도 아무 변화가 없다. 어둡게 하려면 `DefaultLit + Translucent` 로 두고 **BaseColor** 에 어두운 색, Opacity 에 알파를 물려 알베도를 덮어써야 한다. 그리고 `DecalComponent.DecalSize` 는 파이썬에서 읽기 전용이라 액터 스케일로 조절해야 한다(기본 128×256×256).

비교 이미지: `preview/sprite_lighting_ab.png` (왼쪽 NormalStrength 0, 오른쪽 1.0), `preview/sprite_player_lit.png`, `preview/sprite_npc_lit.png` (측면광에서의 음영 + 바닥 그림자).

## 캐릭터 음영량 (옥토패스 기준)

빌보드 스프라이트에 실루엣 거리장 노멀맵을 100%로 물리면 측면광에서 대비가 폭주한다.
흰 옷이 회색 얼룩으로 덮이고 그늘 쪽은 뭉갠다. 레퍼런스 조사 후 세 군데를 고쳤다.

### 1. 라이팅 받는 비율 분리 (`M_PixelSprite_Lit`)

라이브 어 라이브 HD-2D 강연(UNREAL FEST WEST '22, 슬라이드 33)에 문서화된 방식이다.
스프라이트 색을 **라이팅 받는 몫과 안 받는 몫으로 나누고 비율을 고정**한다.

    BaseColor     = SpriteTex x Tint x (1 - AmbientLift)
    EmissiveColor = SpriteTex x Tint x AmbientLift

합이 항상 아트 원색이라 `AmbientLift` 를 올려도 전체가 밝아지지 않고 대비만 줄어든다.
그늘 쪽 하한이 아트 자기 색이므로 어두워져도 색을 잃지 않는다.
**이전에는 BaseColor 가 그대로였고 이미시브만 더해서, 올릴수록 그냥 밝아지기만 했다.**

### 2. 파라미터

| 파라미터 | 이전 | 지금 | 이유 |
|---|---|---|---|
| `NormalStrength` | 1.0 | **0.30** | `lerp((0,0,1), N, s)` 라 FlattenNormal 과 같다. 0.3 이면 최대 기울기 45도 → 17도 |
| `AmbientLift` | 0.12 (가산) | **0.25 (비율)** | 그늘 하한을 아트색의 25%로 고정 |
| `Specular` | 0.25 | **0.0** | 빌보드는 V 가 화면 전체에서 일정해서 균일한 무채색 베일이 되고 채도를 깎는다 |
| `Roughness` | 0.85 | **0.90** | 스페큘러를 없앤 것과 일관 |
| `Tint` | (1,1,1) | 유지 | 맵별 밝기 보정용으로 남겨둠 |

### 3. 노멀맵 재생성 (`texgen/sprite_normal_gen.py`)

이전 레시피(실루엣 0.75 + **아트 명도 0.25**)가 얼룩의 진짜 원인이었다. 아트에는 이미
화가가 그린 음영이 들어 있어, 명도를 높이로 쓰면 같은 음영이 지오메트리로 한 번 더 들어간다.
두 램프가 서로 상관이 있어서 오차가 상쇄되지 않고 겹친다.

    python sprite_normal_gen.py --slope 0.40 --detail 0.0 --green dx --ss 6 --maxtilt 22 --nblur 0.14

- `--detail 0.0` — 아트 명도 항 제거
- `--maxtilt 22` — 기울기 상한 22도 (이전 45도)
- `--nblur 0.14` — 네이티브 블러 시그마 = 짧은변 x 0.14. 텍셀 단위 요철을 지운다
- 스프라이트 85장 재생성 후 TC_NORMALMAP / sRGB off / TF_NEAREST / 밉 없음 으로 재임포트

### 4. 측정값

같은 카메라·같은 조명에서 노멀맵 없는 상태 대비 변조 폭:

| 항목 | 이전 | 지금 | 목표 |
|---|---|---|---|
| 노멀맵 변조 폭 (p95/p05) | 5.71 : 1 | **1.46 : 1** | 옥토패스 실측 얼굴 스캔라인 1.14 : 1 |
| 캐릭터 / 발밑 지면 (선형) | — | **0.84** | < 1.0 (캐릭터가 지면보다 밝으면 안 된다) |
| 그늘 10% 채도 | 0.366 | **0.641** | 그늘이 회색이 아니라 아트 색을 유지해야 한다 |
| 전 채널 245 이상 픽셀 | 0 | **0** | 순백으로 클리핑되면 안 된다 |

미리보기: `preview/sprite_shading_ab.png`

### 하체가 어두운 원인은 블롭 그림자 데칼이었다

`UNUSpriteVisualComponent::RefreshBlobShadow` 가 `DecalSize = (Radius, Radius, Radius)` 를
발밑에 두고 있었다. 데칼 박스는 **위치 기준 양쪽으로** 퍼지므로 발밑에서 위로도 26 cm 올라간다.
스프라이트는 Masked 라 뎁스를 쓰기 때문에 자기 정강이·허벅지에 그림자가 찍혔다.
80 cm 캐릭터에서 26 cm 면 무릎까지다.

측정(같은 카메라, 하체/상체 밝기비):

| 상태 | 하체/상체 |
|---|---|
| 아트 원본 (`idle_front`) | 0.915 |
| 데칼 없음 | 0.823 |
| **데칼 26 cm (이전)** | **0.536** |
| 데칼 12 cm + 발밑 아래로 (지금) | 0.820 |

수정: 투영 깊이 12 cm, 박스 윗면이 지면보다 2 cm 위에 오도록 컴포넌트를 내린다.
지면 블롭은 오히려 더 또렷해진다 (투영이 얕은 깊이에 집중되므로).

    const float ProjectionDepth = 12.0f;
    BlobShadow->DecalSize = FVector(ProjectionDepth, Radius, Radius);
    BlobShadow->SetRelativeLocation(FVector(0, 0, Settings.SpritePivotZ - ProjectionDepth + 2.0f));

미리보기: `preview/blob_decal_ab.png`

**남은 관측(미수정)**: AO 가 스프라이트 전체를 26% 어둡게 한다(`ShowFlag.AmbientOcclusion 0` 대비).
`PPV_Village` 의 `ambient_occlusion_radius` 는 150 cm 로 캐릭터 키(88 cm)보다 크지만,
이 레벨은 GI 를 Screen Space 로 오버라이드한 탓에 **반경을 30~150 으로 바꿔도 렌더가 전혀 안 바뀐다**.
줄이려면 AO 강도(0.5) 를 건드려야 하는데 마을 전체에 영향이 가므로 그대로 뒀다.

### 함정

- 마스터 머티리얼을 고치면 스프라이트가 잠깐 **회색 체커**로 렌더된다. 셰이더 재컴파일 중이며,
  머티리얼이 깨진 것이 아니다. 조금 뒤 다시 캡처하면 정상이다.
- 노멀맵 파일명은 반드시 **에셋 전체 경로** 기준으로 만든다. `walk_down_0` 처럼 폴더만 다르고
  이름이 같은 텍스처가 12쌍 있어서, 파일명만 쓰면 서로 다른 그림에 같은 노멀이 들어간다.

## 플레이 세팅 (게임모드 · NPC)

개발용 `L_TestLevel` 과 같은 구성으로 맞춰서 바로 플레이할 수 있다.

- **World Settings → GameMode Override** = `/Game/Developers/dongh/GameMode/BP_TestGamemode`
  (폰 `BP_TestFieldCharacter`, 컨트롤러 `BP_NUPlayerController`, 스테이트 `BP_TestPlayerState`).
- **KillZ = -20000.** `ANUGameMode::ArenaCenter` 가 (0,0,-10000) 이라 전투가 그 아래에서 벌어진다. KillZ 를 그보다 얕게 두면 전투 액터가 사라진다.
- **PlayerStart**: 남문 안쪽 (0, -3800, 지면+70). 캡슐이 r24/h48 이라 지면에서 충분히 띄워야 한다. **더 남쪽(-4800 등)에 두면 추격 카메라(폰 뒤 1000 cm)가 절벽 림 바깥으로 나가 바위가 화면을 가린다.** 시작 위치를 옮길 때는 카메라가 평지(|y| < 5600) 안에 남는지 확인할 것.
- **NPC 접지**: NPC 스프라이트는 피벗이 `BOTTOM_CENTER`, `pixels_per_unreal_unit = 1` 이라 **액터 원점이 곧 발 위치**다. 지면 높이에 그대로 놓아야 하며 올려 두면 그만큼 공중에 뜬다. 배치 후 `line_trace_single` 로 지면과의 차이를 재서 검증할 것 (기준 1 cm).
- **남쪽 절벽 림 간격**: `layout_village.json` 의 `cliff_rim.gaps` 가 큰길 회랑(|x| < 8 m, y < -50 m)에서 블록을 건너뛴다. 길이 바위벽에 막히지 않고 고원 가장자리로 이어진다.
- **소품 충돌**: 나무·덤불·바위·생울타리·절벽 블록은 `CTF_USE_COMPLEX_AS_SIMPLE` 로 바꿔 캐노피 아래로 지나갈 수 있게 했다. 집·울타리·돌담은 컨벡스 그대로.
- **DOF**: 게임 카메라가 스프링암 1000 cm / FOV 30 이라, PPV 의 시네마틱 DOF 를 focal 1300 · f/8 · sensor 36 으로 낮췄다. 히어로 샷용 강한 틸트시프트가 필요하면 그때만 값을 올릴 것.
- NPC 는 NavMesh 가 필요 없다 (SphereComponent 루트의 정지형 스프라이트 액터).

### 배치된 NPC (`Village/NPCs` 폴더, 지면+60 cm)

| 라벨 | 블루프린트 | 위치 (cm) | 자리 |
|---|---|---|---|
| Guide_Gate | BP_NPC_TutorialGuide | (700, -2600) | 남문 안쪽 |
| JobMaster_Plaza | BP_NPC_JobSelection | (-900, 3150) | 광장 서편 |
| Elladon_Plaza | BP_NPC_Elladon | (1050, 3350) | 광장 동편 |
| Rugan_Inn | BP_NPC_Rugan | (2200, 1150) | 여관 앞 |
| Tion_Well | BP_NPC_Tion | (4450, 350) | 우물가 |
| Child_Storehouse | BP_NPC_Q3003_HiddenChild_01 | (-1500, -950) | 창고 뒤 |
| Child_Garden | BP_NPC_Q3003_HiddenChild_02 | (4000, -1250) | 텃밭 |
| Child_Forest | BP_NPC_Q3003_HiddenChild_03 | (-4600, -2300) | 남서쪽 숲 |
| Villager_Market | BP_TestNPC | (-2700, -1400) | 시장 통 |
| Villager_NW | BP_TestNPC | (-3000, 3900) | 북서 주택가 |

### 함정: 스카이 돔 충돌이 플레이어 스폰을 밀어낸다

스카이 돔(반지름 3 km 구체)에 충돌이 남아 있으면 `AGameModeBase` 의 스폰 보정이 폰을 구체 밖으로 밀어내고, 폰은 허공에서 떨어져 죽는다. 로그에 경고가 남지 않고 "폰이 안 생긴다"로만 보인다. 돔 컴포넌트를 `NoCollision`(프로파일과 enum 둘 다)으로 둘 것.

에디터 뷰포트와 게임 화면은 노출이 다르다. 실제 게임 색감을 보려면 뷰포트를 `ViewportService.set_exposure_game_settings()` + `set_game_view(True)` 로 맞춘 뒤 캡처할 것.

## 주의

- 메시 정면: Blender +Y 가 언리얼 -Y 로 들어오므로 `FRONT_YAW_OFFSET=180`.
- 랜드스케이프 페인트는 반드시 `LandscapeService.add_layer` 로 타겟 레이어(Grass/Dirt/Cobblestone/Moss)를 등록한 뒤 `set_weights_in_region` 으로 써야 저장 후에도 남는다 (에디트 레이어 경유).
- 랜드스케이프에 할당된 머티리얼(`M_VillagePixelLandscape2`)에 `get_material_diagnostics` / `compile_material` 을 호출하지 말 것 — 라이브 리컴파일로 에디터가 D3D12 크래시를 냈다.
- 이 레벨의 `PPV_Village` 는 GI/Reflection 을 Screen Space 로 오버라이드한다 (Lumen 카드 캡처 크래시 회피).
