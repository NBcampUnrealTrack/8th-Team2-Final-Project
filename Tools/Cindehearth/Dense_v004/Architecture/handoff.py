"""Produce asset-only handoff; level acceptance remains with UE QA."""
from task import ROOT,EX,read,write,export,payload
export('SmithyRoof')
keys=['Tavern','TavernWalls','SmithyStructure','SmithyRoof','SmithyWalls']
assets={}
for key in keys:
    src=read(ROOT/'verification'/(key+'_source.json'))
    assets[key]=dict(export=read(ROOT/'verification'/(key+'_export.json')),report=src['report'])
report=dict(status='EXPORTED_FOR_UE_ASSEMBLY',date='2026-09-05',credit_cost=220,balance=1560,frozen=0,
    tripo_tasks={n:payload(read(ROOT/'verification'/(n+'_tripo_generate_p2_image.json')))['task_id'] for n in ['Tavern','Smithy']},
    assets=assets,assembly=dict(front_axis='+X',width_axis='Y',unit='meters in Blender; GLB import centimeters in Unreal',
        shared_origin='All exported parts use the original normalized building origin and identity object transforms. Copy one computed building transform to every part; never recenter or individually fit the wall or roof bounds.',
        tavern=['Tavern','TavernWalls'],smithy=['SmithyStructure','SmithyRoof','SmithyWalls'],
        roof_visibility='Apply CH.InteriorRoof only to SmithyRoof. Tavern is a closed exterior building with a solid door.',
        wall_material='Use HD2D stone world-aligned or repeated UV material. Walls carry a neutral matte placeholder only; their finite cube UVs intentionally tile outside 0..1.',
        body_material='Original P2 Color/Normal, nearest sampled PixelGrid256, normal strength0.025 or lower, roughness0.97.'),
    limitations=['P2 body meshes retain separated decorative surface patches and internal/overlapping boundaries; not globally watertight.',
       'Both repair wall meshes are closed (boundary/nonmanifold/loose/degenerate all zero).',
       'Split roof/structure seams intentionally remain open and match in shared coordinates.',
       'Typed review renderer supports one object at a time; final multi-part assembly material and gameplay camera acceptance must be checked in Unreal.',
       'Review files WallReview012 and WallReview010 are rejected intermediate UV appearances, not final art evidence.'])
fix_path=ROOT/'verification/TavernWallsInset_fix.json'
if fix_path.exists():
    report['assembly']['tavern']=['Tavern','TavernWallsFrame','TavernWallsInset']
    report['tavern_wall_correction']=read(fix_path)
    report['assets'].pop('TavernWalls',None)
    for key in ['TavernWallsFrame','TavernWallsInset']:
        src=read(ROOT/'verification'/(key+'_source.json'))
        report['assets'][key]=dict(export=read(ROOT/'verification'/(key+'_export.json')),report=src['report'])
write(ROOT/'verification/architecture_handoff.json',report)
print('HANDOFF',ROOT/'verification/architecture_handoff.json')
