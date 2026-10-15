# HOWTO: driving Unreal Editor (ProjectM, UE 5.8) from a subagent

Project root: `C:\Users\Ndus_072\RevDevTool\RevStudio-ProjectM`
Scratchpad (write temp files here): `C:\Users\Ndus_072\AppData\Local\Temp\claude\C--Users-Ndus-072-RevDevTool-RevStudio-ProjectM\2d851b1b-c6b6-4a48-a0e8-226d39210886\scratchpad`
(bash form: `/c/Users/Ndus_072/AppData/Local/Temp/claude/C--Users-Ndus-072-RevDevTool-RevStudio-ProjectM/2d851b1b-c6b6-4a48-a0e8-226d39210886/scratchpad`)

## 1. Full-access tool calls (MUTATIONS) — use the helper script, NOT the `mcp__Revvy__*` MCP tools
The `mcp__Revvy__*` MCP tools in this session are READ-ONLY (safe tier). The `Revvy-Full` MCP server is broken (401).
Full access works by direct JSON-RPC to `http://127.0.0.1:8091/mcp` with the user's token. A helper exists:

```
SP="/c/Users/Ndus_072/AppData/Local/Temp/claude/C--Users-Ndus-072-RevDevTool-RevStudio-ProjectM/2d851b1b-c6b6-4a48-a0e8-226d39210886/scratchpad"
# run Unreal Python code stored in a file:
python - "$SP/my_script.py" "$SP/my_args.json" <<'EOF'
import json,sys
json.dump({"code":open(sys.argv[1],encoding='utf-8').read()},open(sys.argv[2],'w',encoding='utf-8'))
EOF
powershell -NoProfile -ExecutionPolicy Bypass -File "$SP/revvy-call.ps1" -Tool execute_python_code -ArgsFile "$SP/my_args.json"

# any other tool, inline JSON args:
powershell -NoProfile -ExecutionPolicy Bypass -File "$SP/revvy-call.ps1" -Tool manage_asset -ArgsJson '{"action":"import","source_file_path":"C:/path/T_X.png","destination_path":"/Game/Village/Textures/Pixel","asset_name":"T_X"}'
powershell -NoProfile -ExecutionPolicy Bypass -File "$SP/revvy-call.ps1" -Tool manage_asset -ArgsJson '{"action":"save_all"}'
powershell -NoProfile -ExecutionPolicy Bypass -File "$SP/revvy-call.ps1" -Tool manage_asset -ArgsJson '{"action":"list","path":"/Game/Village"}'
```
- Use UNIQUE file names per script (e.g. `tex_import_01.py`) so parallel agents don't clobber each other's files.
- Result JSON: `{"success":true,"output":"<stdout>","result":...,"warnings":[...]}`. On Python exception you get `success:false, error_message`.
- Results > ~24KB are truncated by the proxy and show a `revvy_result_page {"handle":..., "offset":N}` hint. Read the next page with:
  `powershell ... -File "$SP/revvy-call.ps1" -Tool revvy_result_page -ArgsJson '{"handle":"res-...","offset":23488}'`
  Better: print compact output (json.dumps without indent, only what you need).
- Python runs on the game thread inside the editor. Keep scripts < ~30 s. Never sleep/poll inside a script.
- Calls from several agents are serialized by the editor; fine to run in parallel, but never edit the SAME asset as another agent.

## 2. Unreal Python gotchas (verified in this session)
- `unreal.Rotator(...)` positional order is (roll, pitch, yaw). ALWAYS use keywords: `unreal.Rotator(roll=0, pitch=-40, yaw=90)`.
- `material.get_editor_property('expressions')` is PROTECTED → use `unreal.MaterialNodeService.export_material_graph(path)` (returns JSON string).
- NEVER call `unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(...)` / `AssetImportTask` — crashes the editor.
  - Images: `manage_asset` action=import (above) or `unreal.AssetDiscoveryService.import_asset(src, "/Game/Folder", "T_Name")` → returns (path, err).
  - FBX / meshes: Interchange, verified working & async:
    ```python
    im = unreal.InterchangeManager.get_interchange_manager_scripted()
    sd = im.create_source_data(r"C:\...\SM_House_A.fbx")
    p = unreal.ImportAssetParameters(); p.is_automated = True
    ok = im.import_asset('/Game/Village/Meshes', sd, p)   # returns immediately; asset appears ~1-3 s later
    ```
    Poll with `manage_asset list` (or `unreal.EditorAssetLibrary.does_asset_exist`) in a SEPARATE later call. Interchange also creates a MaterialInstance per FBX material slot name in the same folder — that's expected; reassign real materials afterwards with `sm.set_material(idx, mi)`.
- Deprecated but working: `unreal.EditorLevelLibrary.get_all_level_actors()`; preferred: `unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()`.
- Spawn a static mesh actor: `unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_object(static_mesh_asset, unreal.Vector(x,y,z), unreal.Rotator(roll=0,pitch=0,yaw=deg))` then `actor.set_actor_label(...)`, `actor.set_actor_scale3d(...)`, `actor.set_folder_path('Village/Houses')`.
- Save: `unreal.EditorAssetLibrary.save_asset(path)`; save everything: `manage_asset save_all`; level: `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()`.
- Texture settings: `t=unreal.load_asset('/Game/.../T_X'); t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); t.set_editor_property('srgb', False); t.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST); t.set_editor_property('max_texture_size', 512); t.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP); unreal.EditorAssetLibrary.save_asset(path)`. Height maps: `TC_GRAYSCALE`; ORM: `TC_MASKS`; BaseColor: `TC_DEFAULT` + srgb True.

## 3. Screenshots (verification) — `unreal.ScreenshotService.capture_from_pose`
```python
r = unreal.ScreenshotService.capture_from_pose(unreal.Vector(0,-13000,9500), unreal.Rotator(roll=0,pitch=-40,yaw=90), "my_shot", 60, False, False)
# r is a JSON string: {"success":true,"file_path":"...Saved/Revvy/Screenshots/my_shot.png",...}
```
File lands in `C:\Users\Ndus_072\RevDevTool\RevStudio-ProjectM\Saved\Revvy\Screenshots\<name>.png`.
**The PNG has R and B channels SWAPPED (BGR).** Before viewing, swap with PIL (local python has PIL+numpy):
```python
from PIL import Image
im=Image.open(src).convert('RGB'); r,g,b=im.split(); Image.merge('RGB',(b,g,r)).save(dst)
```
Then `Read` the swapped PNG. Do not judge colors from the raw file.
Useful poses (village center is 0,0; landscape spans x,y ∈ [-6300, 6300], ground z≈100):
- HD-2D overview: loc (0,-13000,9500) rot(pitch=-40, yaw=90), fov 60
- top-down: loc (0,0,16000) rot(pitch=-90, yaw=90)
- gameplay-ish: loc (0,-7000,2600) rot(pitch=-28, yaw=90), fov 50
- street level: loc (0,-5000,250) rot(pitch=-6, yaw=90)

## 4. Revvy service APIs (Python, inside the editor)
Full skill docs (read the relevant one before using a service):
- `$SP/skills_mat_land_shot.md` — screenshots, **materials** (MaterialService / MaterialNodeService: create_material, create_parameter, create_expression, connect_expressions, connect_to_output, batch_*, export_material_graph, get_material_diagnostics, compile_material, create_instance, set_instance_scalar/vector/texture_parameter), **landscape** (LandscapeService)
- `$SP/skills_landmat_actors_assets_foliage.md` — **landscape-materials** (LandscapeMaterialService: create_landscape_material, create_layer_blend_node, add_layer_to_blend_node, connect_to_layer_input, create_layer_coords_node, create_layer_info_object, assign_material_to_landscape, setup_layer_textures), level-actors (ActorService), asset-management, foliage (FoliageService)
- Any other skill: `mcp__Revvy__manage_skills` action=load skill_names=[...] (read-only tool, allowed).
Key LandscapeService calls (landscape label = `Landscape_Village`, 127x127 vertices, 1 component-section of 63 quads... resolution 127, scale 100 → 1 vertex = 100 cm, vertex (0,0) is world (-6300,-6300)):
- `paint_layer_in_world_rect(label, layer, min_x, min_y, max_x, max_y, strength=1.0)`
- `set_weights_in_region(label, layer, start_x, start_y, size_x, size_y, weights_flat_rowmajor)` / `get_weights_in_region(...)`
- `get_height_in_region(label, sx, sy, w, h)` / `set_height_in_region(label, sx, sy, w, h, heights)` (world Z floats)
- `get_height_at_location(label, x, y)` → LandscapeHeightSample (.height)
- `flatten_at_location(label, x, y, radius, target_z, strength, "Smooth")`, `sculpt_at_location`, `smooth_at_location`, `raise_lower_region(label, cx, cy, w, h, dz, falloff)`
- `add_layer(label, layer_info_asset_path)`, `list_layers(label)`, `set_landscape_material(...)`
Existing layers: Grass (`/Game/Village/Landscape/LI_Grass`), Dirt (`LI_Dirt`), Cobblestone (`LI_Cobblestone`). Current painted state: mostly Grass, a N-S Dirt path at x≈0, two Cobblestone circles at (0,-5200) and (0,+5000).
GeometryScriptService (procedural static meshes, no import needed): `create_dynamic_mesh(handle)`, `append_box(handle, transform, dims_vector, sx, sy, sz)`, `append_cylinder(handle, transform, radius, height, radial_steps, height_steps)`, `append_sphere(handle, transform, radius, phi, theta)`, `append_torus(...)`, `boolean_union/subtract/intersect(target, other)`, `convert_dynamic_mesh_to_static_mesh(handle, "/Game/Village/Meshes/SM_X")`, `dispose_dynamic_mesh(handle)`.

## 5. Read-only MCP tools you may call directly
`mcp__Revvy__get_editor_context`, `mcp__Revvy__manage_asset` (search/find/list only — mutations must go through the helper), `mcp__Revvy__read_logs` (errors: action=errors file=main), `mcp__Revvy__manage_skills`, `mcp__Revvy__lookup_class`, `mcp__Revvy__discover_python_*`. Load their schema with ToolSearch `select:<name>` first.

## 6. Conventions for this job
- All new assets under `/Game/Village/...`: Textures/Pixel, Materials, Meshes, Landscape, Foliage.
- Do not delete or overwrite the OLD assets (`/Game/Village/Textures/Landscape/*`, `M_VillageLandscape`, `MI_VillageLandscape`, `M_CliffRock`); create new ones next to them.
- Never touch `.mcp.json`, `Plugins/`, C++ sources.
- Save every asset you create; at the end run `manage_asset save_all`.
- Report facts only. If something failed, say exactly what and the error text.
