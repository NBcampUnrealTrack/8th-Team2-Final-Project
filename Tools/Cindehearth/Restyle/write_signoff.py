from pathlib import Path
import json,hashlib,datetime
root=Path(__file__).resolve().parent
project=root.parents[2]
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
checks={}
for key in ['rail','cemetery','ashslope','deepvein','pinehollow']:
    d=read(root/'verification'/('walk_'+key+'.json'));assert d['passed']
    checks[key]={k:v for k,v in d.items() if k!='samples'}
d=read(root/'verification/gorm_walk.json');assert d['passed'];checks['gorm']={k:v for k,v in d.items() if k!='samples'}
final=read(root/'verification/final_validation.json');assert final['saved'] and final['volumetric_tree_count']==95 and final['old_flat_tree_count']==0
stability=read(root/'verification/editor_stability.json');assert stability['passed'] and stability['elapsed_seconds']>=100
tree=read(project/'Saved/CindehearthRepair/pine_volume_cap_005/result.json')['result']['output']
assert all(tree['geometry'][k]==0 for k in ['boundary_edges','non_manifold_edges','loose_vertices','degenerate_faces'])
credit=json.loads(read(root/'verification/balance_after_volume.json')['content'][0]['text'])
assert credit['unresolved_request_count']==0 and credit['credits']['frozen']==0
log=(root/'verification/editor_render_crash_0735.log').read_text(encoding='utf-8')
runtime_lines=[s for s in log.splitlines() if 'LogPython:' in s and any(k in s for k in ['EPILOGUE END','ARCHIVE FLOOR','ARCHIVE EXIT'])]
assert any('EPILOGUE END True' in s and 'False False' in s for s in runtime_lines)
assert any('walkable_floor: True' in s for s in runtime_lines)
(root/'verification/epilogue_archive_evidence.json').write_text(json.dumps(dict(passed=True,source_log='editor_render_crash_0735.log',observed_lines=runtime_lines),indent=2),encoding='utf-8')
ex=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth/Restyle'
files=[ex/('CH_PineVolume_Closed_v005.'+ext) for ext in ['blend','glb','fbx']]
assert all(p.is_file() for p in files)
signoff=dict(date='2026-09-05',status='APPLIED_AND_VERIFIED',level=final['level'],scope='Whole-level HD2D art pass plus volumetric pine correction',
    user_visual_approval=False,positive_exemplar_registered=False,
    pine=dict(task='798ff027-dfd5-4573-ad28-beb8ef759351',model='P2-20260801',quad=True,geometry=tree['geometry'],dimensions_m=tree['object']['dimensions'],
        native_quad_faces=tree['geometry']['triangles']-tree['geometry']['polygons'],count=95,uniform_scale=True,removed_disconnected_components=1,closed_boundary_loops=13,
        front_back_top_blender_review=True,front_side_unreal_review=True,gameplay_capture='verification/pine_player.png',rejected_version='CH_Pine_Closed_v002'),
    applied=final,walk_checks=checks,state_qa='verification/state_qa.json',runtime_evidence='verification/epilogue_archive_evidence.json',
    camera='verification/player_camera.json',captures='verification/final_viewport_captures.json',
    source_files=[dict(path=str(p),sha256=digest(p)) for p in files],
    tripo_credits=dict(this_art_pass=440,tree_correction=110,balance=credit['credits']['balance'],frozen=0),
    editor=dict(restarted_after_renderer_crash=True,saved_map_reloaded=True,root_cause_fixed=False,crashes=2,probable_trigger='Revvy GetMaterialDiagnostics forces recompilation; omitted from final reload audit',stability_evidence='verification/editor_stability.json',observed_without_recurrence_seconds=stability['elapsed_seconds'],crash_logs=['verification/editor_render_crash_0735.log','verification/editor_diagnostics_crash_0737.log']),
    limitations=['House body and cart retain documented open surfaces; they are not certified globally watertight.','New pine crowns are separate closed volumes that overlap supporting branches, not one welded solid.','No shipping GPU performance certification; this is an applied level art pass.','Q7/Q8/Q9/Q11/Q12 local Director behavior is verified; external quest-ID wiring is unchanged.'])
(root/'signoff.json').write_text(json.dumps(signoff,ensure_ascii=False,indent=2),encoding='utf-8')
effect=dict(candidate='Cindehearth_PixelFire_Soot_v003',status='APPLIED_REVIEW_CANDIDATE',user_approved=False,
    system=None,niagara=False,material_paths=[p for p in final['loaded_materials'] if 'Pixel' in p],
    primary=dict(form='Upright orange fire tongue',geometry='3 crossed planes at hearth, 2 at forge',timing='10 fps UV quantization, continuous loop'),
    secondary=dict(form='Muted rising soot',geometry='56 translucent planes',timing='4 staggered phases, smooth fade at both ends of 7.14-second rise',bounds_scale=5),
    source_mesh='/Engine/BasicShapes/Plane',shared_originals_unchanged=True,
    gates=dict(structural='PASS: 5 fire / 56 smoke, materials compile',runtime='PASS: visible in level and state captures',pixel='PASS: upright silhouette and warm base in final viewport',aesthetic='ASSISTANT_REVIEWED: fire primary, subdued soot secondary',archetype='ASSISTANT_REVIEWED: persistent hearth and mining-village chimney smoke'),
    candidate_history=['Old solid flame core rejected','Inverted UV flame rejected','Upright current flame selected for user review'],
    performance=dict(static_triangles=122,gpu_timing_ms=None,shipping_profiled=False),
    captures=['verification/final_viewport/CH_Restyle_final_square_day.png','verification/final_viewport/CH_Restyle_final_square_night.png'])
(root/'effect_review.json').write_text(json.dumps(effect,indent=2),encoding='utf-8')
print('SIGNOFF WRITTEN: 95 volumetric trees; all recorded walk checks passed; balance',credit['credits']['balance'])
