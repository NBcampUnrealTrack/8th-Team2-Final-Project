"""Real Blender fixture test: capped opening, preserved source, stale ID guard."""
import json,sys,uuid
from pathlib import Path
import bpy,bmesh
from collections import Counter
sys.path.insert(0,str(Path(__file__).parent))
import blender_repair_worker as w

root=w.EXCHANGE/'RepairTests'/uuid.uuid4().hex
root.mkdir(parents=True)
source=bpy.data.objects.get('Cube');source.name='RepairFixture'
bm=bmesh.new();bm.from_mesh(source.data)
top=max(bm.faces,key=lambda f:f.calc_center_median().z)
bmesh.ops.delete(bm,geom=[top],context='FACES_ONLY')
vs=[bm.verts.new(p) for p in [(3,0,0),(3.1,0,0),(3.1,.1,0),(3,.1,0)]]
bm.faces.new(vs);bm.to_mesh(source.data);bm.free()
path=root/'fixture.blend';bpy.ops.wm.save_as_mainfile(filepath=str(path))
ins=w.run(dict(command='inspect',blend_path=str(path),object_name=source.name))
assert len(ins['topology']['components'])==2
assert len(ins['topology']['boundary_loops'])==2
fingerprint=ins['report']['fingerprint']
component=next(c for c in ins['topology']['components'] if c['face_count']==1)
args=dict(command='repair',blend_path=str(path),object_name=source.name,expected_fingerprint='0'*64,output_object_name='BadStale',output_blend=str(root/'bad.blend'),operation='delete_components',selection_ids=[component['id']],distance=.00001,max_faces=1,max_diameter=4)
try:w.run(args);raise AssertionError('Stale fingerprint accepted')
except AssertionError as err:assert str(err)=='Stale fingerprint'
assert 'BadStale' not in bpy.data.objects and not (root/'bad.blend').exists()
args.update(expected_fingerprint=fingerprint,output_object_name='CleanFixture',output_blend=str(root/'clean.blend'))
def face_signatures(obj):
    result=[]
    for face in obj.data.polygons:
        corners=tuple(tuple(obj.data.vertices[i].co) for i in face.vertices)
        # Rotate the starting corner while preserving winding.
        result.append(min(corners[i:]+corners[:i] for i in range(len(corners))))
    return Counter(result)
source_faces=face_signatures(source)
clean=w.run(args);assert clean['changes']['deleted_faces']==1
retained=face_signatures(bpy.data.objects['CleanFixture'])
assert not (retained-source_faces), 'Cleanup changed retained face winding or positions'
assert w.modeling._mesh_fingerprint(source)==fingerprint
bpy.ops.wm.open_mainfile(filepath=clean['output_blend'])
ins=w.run(dict(command='inspect',blend_path=clean['output_blend'],object_name='CleanFixture'))
assert len(ins['topology']['components'])==1
loop=ins['topology']['boundary_loops'][0]
args.update(blend_path=clean['output_blend'],object_name='CleanFixture',expected_fingerprint=ins['report']['fingerprint'],output_object_name='ClosedFixture',output_blend=str(root/'closed.blend'),operation='fill_loops',selection_ids=[loop['id']],max_diameter=4)
closed=w.run(args);assert closed['output']['geometry']['boundary_edges']==0
assert closed['output']['geometry']['non_manifold_edges']==0
assert closed['output']['geometry']['polygons']==6
print('REPAIR FIXTURE PASS: stale fingerprint rejected, selected floating component removed, selected hole capped, source preserved, six quads retained')
verts,faces,rects=w.gable_geometry(dict(center_x=0,half_width=1.5,front_y=-1.2,back_y=1.3,eave_z=1,ridge_z=2.4,thickness=.1))
testmesh=bpy.data.meshes.new('GableFixture');testmesh.from_pydata(verts,[],faces);testmesh.update()
bm=bmesh.new();bm.from_mesh(testmesh)
assert len(bm.faces)==10 and all(len(f.verts)==4 for f in bm.faces)
assert all(e.is_manifold for e in bm.edges)
bm.free()
print('GABLE FIXTURE PASS: 12 vertices, 10 quads, closed manifold')
bm=bmesh.new();bmesh.ops.create_cube(bm,size=2)
caps=[f for f in bm.faces if abs(f.normal.z)>.5]
assert len(caps)==2
bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
groups=w.topology(bm)['boundary_loops']
assert len(groups)==2 and all(g['simple_closed'] for g in groups)
assert w.fill(bm,groups)==2
assert all(e.is_manifold for e in bm.edges)
bm.free()
print('MULTI-LOOP FIXTURE PASS: both inspected openings capped without index drift')
bpy.ops.wm.open_mainfile(filepath=closed['output_blend'])
ins=w.run(dict(command='inspect',blend_path=closed['output_blend'],object_name='ClosedFixture'))
args.update(blend_path=closed['output_blend'],object_name='ClosedFixture',expected_fingerprint=ins['report']['fingerprint'],output_object_name='FaceSelectionFixture',output_blend=str(root/'faces.blend'),operation='delete_faces',selection_ids=[0],max_faces=1,max_diameter=4)
faces=w.run(args)
assert faces['changes']['deleted_faces']==1 and faces['output']['geometry']['polygons']==5
assert faces['output']['geometry']['boundary_edges']==4 and faces['source_preserved']
assert not (face_signatures(bpy.data.objects['FaceSelectionFixture'])-face_signatures(bpy.data.objects['ClosedFixture']))
print('FACE SELECTION FIXTURE PASS: only exact inspected face removed; source/winding preserved')
(root/'passed.json').write_text(json.dumps({'passed':True,'closed':closed},indent=2),encoding='utf-8')
