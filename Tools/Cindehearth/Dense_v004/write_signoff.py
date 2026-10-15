"""Assemble the handoff from completed editor, movement, and visual evidence."""
import json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parent
project=root.parents[2]
v=root/'verification'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
scene=read(v/'final_scene_audit.json');saved=read(v/'final_saved.json')
assert scene['passed'] and saved['saved'] and not saved['pie'] and not saved['night'] and saved['chapter']==0
composite=read(v/'gameplay_composite.json')
fresh=root/'QA/receipts/final_refinements'
assert read(fresh/'runtime_suite.json')['passed']
for row in composite['routes']:
    if row['name'] in ('tavern_approach','cemetery','deepvein'):
        row['receipt']=str((fresh/('walk_'+row['name']+'.json')).relative_to(project))
    receipt=read(project/row['receipt']);assert receipt['passed']
    assert receipt['waypoints_reached']==row['waypoints']
composite['validation_scope']='14 routes/72 unique waypoints; rail from main_v004, 10 routes from routes_v004b, and the final tavern/cemetery/deepvein rechecks from final_refinements. State/archive/epilogue/camera tests passed in routes_v004b. Subsequent changes were visual except the separately retested tavern front wall.'
(v/'gameplay_composite.json').write_text(json.dumps(composite,ensure_ascii=False,indent=2),encoding='utf-8')
runtime=read(root/'QA/receipts/routes_v004b/runtime_suite.json');assert runtime['passed']
names=['final_village','final_square','final_homes','player_tavern','player_tavern_night','player_smithy','player_smithy_inside','player_ash_entry','player_pine_entry','player_cemetery','player_mine','player_logging','player_depot','player_cocoon']
images=[]
for name in names:
    p=v/(name+'.png');assert p.is_file() and p.stat().st_size>10000
    images.append(dict(file=str(p.relative_to(root)),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),actual_player_camera=name.startswith('player_')))
visual=dict(reviewed=True,method='Root visual inspection of actual UE captures; editor PNG R/B decode only, no art retouching.',
            findings=['Tavern door is revealed by inset wall; warm entrance is visible at night.',
                      'Smithy assembled roof/walls have no visible sky-through gaps in reviewed views; roof hides for interior camera.',
                      'Cemetery path now has continuous support underneath, with clear player silhouette.',
                      'Ash/Pine connection gaps filled; original walking floors retained.',
                      'Mine/depot player view clear after foreground ridge recess; logging approach clear of pulley frame.'],images=images)
(v/'visual_review.json').write_text(json.dumps(visual,ensure_ascii=False,indent=2),encoding='utf-8')
report=dict(status='APPLIED_AND_VERIFIED',date='2026-09-05',map='/Game/Maps/L_Cindehearth',
            user_art_approval=False,pipeline='Tripo P2 native quad -> typed Blender cleanup/part assembly -> Unreal pixel materials',
            new_p2_bases=2,credits_consumed=220,remaining_credits_verified=1560,
            footprint_area_reduction_percent=50.42,landmarks_rebuilt=['FreyaTavern','DorikSmithy','UnloadingShelter'],
            scene=scene,gameplay=composite,additional_runtime_checks=runtime['completed_stages'],
            visual=visual,saved_state=saved,
            backup='Saved/CindehearthDenseBackup/20260905_v004/L_Cindehearth.umap',
            limits=['Residual original surface and assembly boundaries remain; not a globally watertight mesh claim.',
                    'No final NPC/dialogue integration, combat, save-game, cook or performance qualification claimed.'])
(root/'signoff.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('SIGNOFF: applied/verified,14 routes72 waypoints, final scene3376 actors, Q0/day/PIE off')
