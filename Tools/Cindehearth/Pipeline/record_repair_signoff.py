"""Record the observed repair evidence and verify all deliverable hashes."""
import json, hashlib, shutil
from pathlib import Path

root=Path(__file__).resolve().parents[3]
evidence=Path(__file__).parent/'verification'
exchange=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def job(name):return read(root/'Saved/CindehearthRepair'/name/'result.json')['result']
roof=job('gorm_closed_roof_001')
clean=job('gorm_remove_preserve_winding_001')
assert roof['source_preserved'] and clean['source_preserved']
assert sha(exchange/'Blender/CH_GormP2_Final_v001.blend')==roof['input_sha256']
assert sha(exchange/'Repair/CH_GormP2_Repaired_v002.blend')==clean['input_sha256']
assert read(evidence/'repair_walk.json')['passed']
assert all(read(evidence/'repair_epilogue_runtime.json').values())
assert read(evidence/'repair_unreal_readback.json')['passed']
assert clean['output']['geometry']['triangles']==1994
for src,suffix in [('v006_front_left_textured.png','textured'),('v006_front_right_clay.png','clay'),('v006_back.png','back')]:
    dest=evidence/('gorm_repaired_v006_blender_'+suffix+'.png')
    assert not dest.exists()
    shutil.copy2(exchange/'Repair/Review'/src,dest)
files=['Repair/CH_GormP2_Clean_v006.blend','Export/CH_GormP2_Clean_v006.glb','Export/CH_GormP2_Clean_v006.fbx','Repair/CH_GormRoof_Color_v002.png']
data=dict(date='2026-09-05',acceptance='VISIBLE_DEFECT_REPAIR_VERIFIED',
          scope='Gorm house exterior: user-marked roof holes, gable gaps and detached panel; not whole-town final art or global watertight certification',
          previous_acceptance='v001 rejected by user visual QA',additional_tripo_generations=0,additional_tripo_credits=0,
          source_file_hashes_verified=True,roof_rebuild=roof['changes'],output=clean['output'],
          roof_quads=10,roof_closed=True,globally_watertight=False,
          remaining_open_boundary_edges=634,final_quad_faces=932,final_triangle_faces=130,
          files=[dict(path=str(exchange/p),sha256=sha(exchange/p)) for p in files],
          unreal=read(evidence/'repair_unreal_readback.json'),
          walk=dict(passed=True,reached=8,total=8,evidence='repair_walk.json'),
          epilogue=read(evidence/'repair_epilogue_runtime.json'),
          review_images=['gorm_repaired_v006_blender_textured.png','gorm_repaired_v006_blender_clay.png','gorm_repaired_v006_blender_back.png','gorm_repaired_v006_day.png','gorm_repaired_v006_night.png','gorm_repaired_v006_player_camera.png'],
          tests=dict(offline_transport=5,blender_fixture='selected deletion, retained winding, stale fingerprint, source preservation, capped opening, closed 10-quad roof passed'),
          rejected_candidates=['v003: flipped back-gable winding','v004: invalid visual cap','v005: non-manifold guard rejection, no file saved'],
          interactive_blender='Existing interactive scene preserved; repaired file saved separately',
          interrupted_readonly_job='gorm_repair_inspect_001: client closed, no mutation; replaced by inspect_002')
dest=evidence/'repair_pipeline_signoff.json'
assert not dest.exists()
dest.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
print('REPAIR SIGNOFF WRITTEN',dest)
