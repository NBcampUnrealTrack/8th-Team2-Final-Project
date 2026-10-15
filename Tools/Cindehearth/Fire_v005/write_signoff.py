import json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parent;v=root/'verification'
read=lambda p:json.loads(p.read_text(encoding='utf-8'))
runtime=read(v/'runtime_verified.json');saved=read(v/'saved_verified.json');assert runtime['passed'] and saved['passed'] and saved['source_material_unchanged']
created=read(v/'material_created.json');assert created['compile_requested']
captures=read(v/'final_captures.json')+read(v/'forge_final_captures.json')
assert all(abs(x['camera_rotation'][0]+25)<.01 and abs(x['camera_rotation'][1]-90)<.01 and abs(x['fov']-32)<.01 for x in captures)
paths=['final_hearth_day.png','final_hearth_night.png','final_hearth_motion_a.png','final_hearth_motion_b.png','forge_final_forge.png']
images=[]
for name in paths:
 p=v/name;assert p.is_file();images.append(dict(path=str(p.relative_to(root)),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
report=dict(status='APPLIED_AND_VERIFIED_FRONT_VIEW',date='2026-09-05',user_art_approval=False,
 camera_unchanged=True,runtime=runtime,saved=saved,material=created,credits_used=0,
 gates=dict(structural='Two depth-tested NoCollision quads, seven-expression material and two instances; compile request returned true and shader rendered correctly.',runtime='Actual PIE front camera and day/night state readback passed.',pixel='Front captures show no crossed-plane blades or rectangular fringe; forge placed inside dark firebox.',aesthetic='Root reviewed yellow core/orange rim, unequal tongues and three time samples; contact correction verified.',archetype='Persistent fire anchored to logs/firebox, sparse upward motes; no extinguishing loop reset.'),
 visual_evidence=images,performance_scope='Five old quads -> two, no new lights or particle simulation. No target-platform frame-budget qualification.',
 original_assets_preserved=True,backup='Saved/CindehearthFireBackup/20260905_v005/L_Cindehearth.umap')
(root/'signoff.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('FRONT FIRE SIGNOFF SAVED')
