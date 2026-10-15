# 지도 텍스처 굽기 --- 열려 있는 레벨을 톱다운 직교로 캡처하고, 건물·물 마스크와 나무 자리를 함께 떠서
# 지도풍으로 스타일화(stylize_map_image.py, 시스템 파이썬)한 뒤 /Game/Art/UI/Maps 에 임포트한다.
# 에디터 콘솔에서 py "C:/UnrealProjects/ProjectM/Scripts/bake_map_image.py" 한 줄로 끝.
#   py "<이 파일>" --probe <폴더> : 캡처 변형들과 나무 자리만 <폴더>에 남기고 스타일화·임포트는 하지 않는다 (비교용)
#
# 방향 규약: 게임 카메라가 +Y 를 봐서(LG_CameraRotation yaw 90) 캡처도 yaw 90.
#   => 이미지 위 = +Y, 오른쪽 = -X. UV 변환은 UNUMapImageSettings::WorldToMapUV 한 곳에만 둔다.
# 범위: DefaultGame.ini [NUMapImageSettings] 에 이 레벨 줄이 있으면 그 범위를 그대로 쓴다 (지도 UV 가 안 바뀜).
#   없으면 스태틱메시 합집합(지평선·분위기 서브레벨과 거대 메시 제외)으로 정하고, 결과를 ini 에 새로 넣는다.
# 결과: T_Map_<레벨이름> 텍스처(UI 그룹, 알파 제거) + Saved/map_bounds.json
#
# HD-2D 맵 규칙 (10/15): 나무·풀은 카메라를 보는 세로 카드(ISM Plane)라 위에서는 안 보이고 그림자만 찍힌다.
#   => 세로 카드는 캡처에서 숨기고, 나무 카드 자리는 원으로 넘겨 스타일화가 숲으로 칠한다.
#   색 캡처는 바탕색(SCS_BASE_COLOR) --- 그림자·색보정·안개가 빠져 땅 색 분류가 맵 조명에 흔들리지 않는다.
#
# 주의 둘: (1) 새 재질을 받은 직후라면 셰이더 컴파일이 끝난 뒤 실행 --- 컴파일 중인 재질은 회색 체크무늬로 찍힌다
# (2) 캡처 액터를 넣었다 빼서 레벨이 수정됨으로 남는다. 저장하지 말고 new_blank_map(False)로 버린다.
#     그래서 월드·액터를 전역 변수에 남기지 않게 전부 함수 안에서 다룬다 (남으면 레벨을 바꿀 때 GC 실패로 에디터가 죽는다)
import unreal, json, io, os, re, sys, time, subprocess

OUT_DIR = "/Game/Art/UI/Maps"
TEX_SIZE = 2048
MARGIN = 1.05
SAVED = unreal.Paths.project_saved_dir()
INI = os.path.join(unreal.Paths.project_config_dir(), "DefaultGame.ini")
STYLIZER = "C:/UnrealProjects/ProjectM/Scripts/stylize_map_image.py"
# 건물로 칠할 메시 --- 메시 이름이나 첫 재질 이름으로 찾는다. 새 맵의 건물 규칙이 다르면 여기에 추가
BUILDING_RE = re.compile(r"^SM_LG_|Cottage|Town_Workshop|_K_(house|hall|shack|tower|tent|watchtower|kiln|infirmary|arcade|spire|turret|cistern)"
                         r"|RowHouse|SlopeHouse|BurrowHouse|Smithy|ClosedRoof|_(Tavern|Clinic|Office|Assay)_", re.I)
# 물 메시 --- 물은 반투명이라 바탕색 캡처에 안 찍히고, 맵 전체를 덮고 땅 밑에 깔린 판(마을 RiverContact)도 있어
# 물만 따로 찍으면 가려진 곳까지 칠해진다 => 최종색을 물 있이/없이 두 번 찍어 달라진 곳을 물로 본다
WATER_RE = re.compile(r"water|river|creek|falls|foam|pond|lake", re.I)
# 나무 카드(세로 ISM Plane)의 재질 이름 --- 폭이 TREE_MIN_WIDTH 보다 좁은 카드는 덤불이라 숲에 안 넣는다
TREE_RE = re.compile(r"tree|pine|canopy|sapling|broadleaf_mid|acacia", re.I)
TREE_MIN_WIDTH = 90.0
# 캡처에서 늘 숨길 것: 포탈 상자·빛기둥·떠다니는 먼지·안개 카드 (재질 이름), 지평선·분위기 서브레벨
HIDE_MAT_RE = re.compile(r"PortalBox|Shaft|Mote|mist_bank|mist_low", re.I)
HIDE_LEVEL_RE = re.compile(r"horizon|ambient", re.I)


def _ini_bounds(package):
    text = io.open(INI, encoding="utf-8").read()
    m = re.search(r'\+Maps=\(Map="%s\.[^"]+",Texture="[^"]+",WorldMin=\(X=([-\d.]+),Y=([-\d.]+)\),WorldMax=\(X=([-\d.]+),Y=([-\d.]+)\)\)'
                  % re.escape(package), text)
    return ((float(m.group(1)), float(m.group(2))), (float(m.group(3)), float(m.group(4)))) if m else None


def _first_mat(c):
    mats = c.get_materials()
    return mats[0].get_name() if mats and mats[0] else ""


def bake(probe_dir=None):
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = ues.get_editor_world()
    level_name = world.get_name()
    package = world.get_path_name().split(".")[0]

    # 1) 분류: 숨길 것·건물·물·나무 자리. 범위가 ini 에 없을 때만 스태틱메시로 범위를 잰다
    mn = [1e12, 1e12]
    mx = [-1e12, -1e12]
    top_z = -1e12
    hide_actors, hide_comps, buildings, water, trees = [], [], [], [], []
    for a in eas.get_all_level_actors():
        label = a.get_actor_label().lower()
        cls = a.get_class().get_name().lower()
        lvl = a.get_outer().get_outer().get_name() if a.get_outer() else ""
        if "fog" in cls or "sky" in label or "sky" in cls or HIDE_LEVEL_RE.search(lvl):
            hide_actors.append(a)
            continue
        if a.get_editor_property("hidden"):
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            mesh = c.get_editor_property("static_mesh")
            if mesh is None or not c.get_editor_property("visible"):
                continue
            mat = _first_mat(c)
            key = mesh.get_name() + " " + mat
            if HIDE_MAT_RE.search(mat):
                hide_comps.append(c)
                continue
            if isinstance(c, unreal.InstancedStaticMeshComponent):
                n = c.get_instance_count()
                if n == 0 or abs(unreal.MathLibrary.get_up_vector(c.get_instance_transform(0, True).rotation.rotator()).z) > 0.5:
                    continue                      # 바닥에 누운 카드(데칼·그늘)는 땅 그림의 일부라 그대로 둔다
                hide_comps.append(c)              # 세로 카드: 위에서는 선으로만 보인다
                if TREE_RE.search(mat):
                    dead = 1 if "dead" in mat.lower() else 0    # 마른 나무 숲은 스타일화가 다른 색으로 칠한다
                    for i in range(n):
                        t = c.get_instance_transform(i, True)
                        w = 100.0 * abs(t.scale3d.x)    # Plane 은 100 x 100
                        if w >= TREE_MIN_WIDTH:
                            trees.append((t.translation.x, t.translation.y, w * 0.45, dead))
                continue
            o, e, _r = unreal.SystemLibrary.get_component_bounds(c)
            if TREE_RE.search(mesh.get_name()):   # 통짜 3D 나무: 숨기고 자리만 넘긴다
                hide_comps.append(c)
                trees.append((o.x, o.y, max(e.x, e.y) * 0.9, 0))
                continue
            if BUILDING_RE.search(key):
                buildings.append(c)
            elif WATER_RE.search(key):
                water.append(c)
            if isinstance(a, unreal.StaticMeshActor) and e.x < 20000 and e.y < 20000:
                mn[0] = min(mn[0], o.x - e.x); mn[1] = min(mn[1], o.y - e.y)
                mx[0] = max(mx[0], o.x + e.x); mx[1] = max(mx[1], o.y + e.y)
                top_z = max(top_z, o.z + e.z)

    known = _ini_bounds(package)
    if known:
        (mn[0], mn[1]), (mx[0], mx[1]) = known
        ortho = max(mx[0] - mn[0], mx[1] - mn[1])
    else:
        ortho = max(mx[0] - mn[0], mx[1] - mn[1]) * MARGIN
    cx = (mn[0] + mx[0]) * 0.5; cy = (mn[1] + mx[1]) * 0.5

    # 2) 캡처: 바탕색(땅 분류) + 건물만 + 물만. 숨길 것은 모든 캡처에서 숨긴다
    rt = unreal.RenderingLibrary.create_render_target2d(world, TEX_SIZE, TEX_SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(cx, cy, top_z + 5000.0), unreal.Rotator(0.0, -90.0, 90.0))
    cc = cap.capture_component2d
    cc.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    cc.set_editor_property("ortho_width", ortho)
    cc.set_editor_property("texture_target", rt)
    cc.set_editor_property("capture_every_frame", False)
    for h in hide_actors:
        cc.hide_actor_components(h)
    for c in hide_comps:
        cc.hide_component(c)
    stamp = int(time.time())
    out_dir = probe_dir or SAVED
    shots = {}

    def shot(tag, source, show_only=None):
        cc.set_editor_property("capture_source", source)
        cc.clear_show_only_components()
        if show_only is None:
            cc.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_RENDER_SCENE_PRIMITIVES)
        else:
            cc.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
            for c in show_only:
                cc.show_only_component(c)
        cc.capture_scene()
        name = "T_Map%s_%s_%d.png" % (tag, level_name, stamp)   # 뷰어가 이전 파일을 잠가도 새 이름이라 안전
        unreal.RenderingLibrary.export_render_target(world, rt, out_dir, name)
        shots[tag] = os.path.join(out_dir, name)

    # 그림자·색보정을 끈 최종색은 지역 맵에서 띠 모양으로 깨져서(10/15) 땅 분류는 바탕색으로 한다
    shot("Base", unreal.SceneCaptureSource.SCS_BASE_COLOR)
    shot("Mask", unreal.SceneCaptureSource.SCS_BASE_COLOR, buildings)
    shot("Lit", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    for c in water:
        cc.hide_component(c)
    shot("Dry", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    eas.destroy_actor(cap)

    # 나무 자리: 캡처 픽셀 좌표 (오른쪽 = -X, 위 = +Y)
    px = TEX_SIZE / ortho
    tree_px = [(round((0.5 + (cx - x) / ortho) * TEX_SIZE, 1), round((0.5 + (cy - y) / ortho) * TEX_SIZE, 1), round(r * px, 1), dead)
               for x, y, r, dead in trees]
    trees_json = os.path.join(out_dir, "T_MapTrees_%s_%d.json" % (level_name, stamp))
    io.open(trees_json, "w", encoding="utf-8").write(json.dumps({"size": TEX_SIZE, "trees": tree_px}))

    half = ortho * 0.5
    bounds = {"level": level_name, "package": package, "fromIni": bool(known), "buildings": len(buildings),
              "water": len(water), "trees": len(trees), "hidden": len(hide_comps),
              "coverageMin": [cx - half, cy - half], "coverageMax": [cx + half, cy + half]}
    if probe_dir:
        bounds["shots"] = shots
        unreal.log("[MapBake] %s" % json.dumps(bounds))
        return

    # 3) 시스템 파이썬으로 스타일화 (numpy/PIL 은 에디터 파이썬에 없음)
    styled_png = os.path.join(SAVED, "T_MapStyled_%s_%d.png" % (level_name, stamp))
    run = subprocess.run(["py", "-3", STYLIZER, shots["Base"], shots["Mask"], shots["Lit"], shots["Dry"], trees_json, styled_png],
                         capture_output=True, text=True)
    if run.returncode != 0:
        unreal.log_error("[MapBake] 스타일화 실패: %s" % run.stderr[-400:])
        raise RuntimeError("stylize failed")

    # 4) 임포트 + 텍스처 설정
    task = unreal.AssetImportTask()
    task.filename = styled_png
    task.destination_path = OUT_DIR
    task.destination_name = "T_Map_%s" % level_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    tex_path = "%s/T_Map_%s" % (OUT_DIR, level_name)
    tex = unreal.load_asset(tex_path)
    if tex:
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("compression_no_alpha", True)  # 캡처 알파는 쓰레기값이라 버린다
        unreal.EditorAssetLibrary.save_asset(tex_path, only_if_is_dirty=False)

    # 5) UV 변환용 바운즈 기록 --- 설정 표(NUMapImageSettings)에는 이 "커버리지"를 넣는다 (ini 에서 온 범위면 그대로)
    bounds["texture"] = tex_path
    io.open(SAVED + "map_bounds.json", "w", encoding="utf-8").write(json.dumps(bounds, indent=1))
    unreal.log("[MapBake] %s" % json.dumps(bounds))


bake(sys.argv[sys.argv.index("--probe") + 1] if "--probe" in sys.argv else None)
del bake
