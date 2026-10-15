"""Fixed V6 worker: reuses approved mesh-copy/rigid-animation primitives."""
import hashlib,json,math,sys,traceback
from pathlib import Path
import bpy
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import door_worker as core
from precision_uv import mesh_uv_audit
ROOT=Path(__file__).resolve().parents[2]
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveBuildingsV6'
STATE=ROOT/'Saved/LibregroveBuildingJobs'
core.EXCHANGE=EXCHANGE
modeling=core.modeling

def objects():
    obs=core.meshes();body=[o for o in obs if o.get('LG_role')=='body'];doors=[o for o in obs if o.get('LG_role')=='door']
    assert len(body)==1 and len(doors)<=2 and len(obs)==1+len(doors)
    return body[0],doors

def assemble(a):
    bpy.context.view_layer.update();reports=[];source=[]
    for ref in a['static_parts']+[r for d in a['doors'] for r in d['parts']]:
        ob=bpy.data.objects[ref['object_name']];report=modeling._mesh_report(ob);assert report['fingerprint']==ref['expected_fingerprint']
        source.append(ob);reports.append(report)
    body,drift=core.combine(source[:len(a['static_parts'])],a['body_name'],Vector());body['LG_role']='body'
    outputs=[body]
    for spec in a['doors']:
        parts=[bpy.data.objects[r['object_name']] for r in spec['parts']];lo,hi=core.bounds(parts[0]);pivot=Vector(spec['pivot_world_m'])
        assert abs(pivot.x-hi[0])<1e-5 and abs(pivot.z-lo[2])<1e-5
        assert abs(pivot.y-(lo[1] if spec['side']=='left' else hi[1]))<1e-5
        assert .05<hi[1]-lo[1]<2 and .1<hi[2]-lo[2]<3 and .005<hi[0]-lo[0]<.2
        door,d=core.combine(parts,spec['output_name'],pivot);drift=max(d,drift)
        door['LG_role']='door';door['LG_side']=spec['side'];door['LG_leaf_width_m']=hi[1]-lo[1]
        door['LG_opening']=json.dumps({k:spec[k] for k in ['opening_y_min','opening_y_max','opening_z_min','opening_z_max','wall_front_x']})
        outputs.append(door)
    for ob,report in zip(source,reports):assert modeling._mesh_fingerprint(ob)==report['fingerprint']
    for ob in list(bpy.data.objects):
        if ob not in outputs:bpy.data.objects.remove(ob,do_unlink=True)
    scene=bpy.context.scene;scene.name=a['body_name'];scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    scene.frame_start=1;scene.frame_end=73 if a['doors'] else 1;scene.render.fps=24;scene.render.fps_base=1;scene.frame_set(1)
    core.select(outputs)
    lo,hi=core.bounds(body);center=Vector([(x+y)/2 for x,y in zip(lo,hi)]);size=max(y-x for x,y in zip(lo,hi))
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                space=area.spaces.active;space.region_3d.view_location=center;space.region_3d.view_distance=size*1.8;space.region_3d.view_rotation=Vector((7,-5,4)).to_track_quat('Z','Y');space.region_3d.view_perspective='ORTHO';space.shading.show_cavity=True
    return {'source_reports':reports,'independent_data':True,'world_geometry_max_drift_m':drift,'after':inspect(),'output':core.save(a['output_blend'])}

def uv():
    for ob in core.meshes():
        core.select([ob]);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        try:
            bpy.ops.uv.smart_project(angle_limit=math.radians(66),margin_method='FRACTION',island_margin=.008,correct_aspect=True,scale_to_bounds=False)
            bpy.ops.uv.select_all(action='SELECT');bpy.ops.uv.average_islands_scale()
            bpy.ops.uv.pack_islands(udim_source='CLOSEST_UDIM',rotate=True,rotate_method='CARDINAL',scale=True,merge_overlap=False,margin_method='FRACTION',margin=.008,shape_method='AABB')
        finally:bpy.ops.object.mode_set(mode='OBJECT')
        assert mesh_uv_audit(ob.data)['passed']

def motion():
    body,doors=objects()
    if not doors or not any(d.animation_data for d in doors):return {'animated':False}
    scene=bpy.context.scene;initial=scene.frame_current;scene.frame_set(1);bpy.context.view_layer.update()
    pivots={d.name:d.matrix_world.translation.copy() for d in doors};closed={d.name:core.coords(d) for d in doors};bodytree=core.bvh(body)
    rays=[];samples=[];hits=[];max_hinge=0.
    opening=json.loads(doors[0]['LG_opening'])
    for iy in range(5):
        y=opening['opening_y_min']+(iy+1)/6*(opening['opening_y_max']-opening['opening_y_min'])
        for iz in range(3):
            z=opening['opening_z_min']+(iz+1)/4*(opening['opening_z_max']-opening['opening_z_min'])
            hit=bodytree.ray_cast(Vector((opening['wall_front_x']+.25,y,z)),Vector((-1,0,0)),.60)[0]
            rays.append({'y':y,'z':z,'blocked':hit is not None})
    for frame in range(1,74):
        scene.frame_set(frame);bpy.context.view_layer.update();ds=[];trees=[]
        for door in doors:
            tree=core.bvh(door);trees.append(tree);pairs=bodytree.overlap(tree)
            if pairs:hits.append({'frame':frame,'door':door.name,'body_pairs':len(pairs)})
            max_hinge=max(max_hinge,(door.matrix_world.translation-pivots[door.name]).length)
            ds.append({'name':door.name,'z_degrees':math.degrees(door.rotation_euler.z),'matrix_world':[list(row) for row in door.matrix_world]})
        if len(trees)==2 and trees[0].overlap(trees[1]):hits.append({'frame':frame,'doors_intersect':True})
        samples.append({'frame':frame,'doors':ds})
    error=max((a-b).length for d in doors for a,b in zip(closed[d.name],core.coords(d)))
    scene.frame_set(initial);bpy.context.view_layer.update()
    return {'animated':True,'fps':24,'frame_range':[1,73],'duration_seconds':3,'max_hinge_drift_m':max_hinge,'closed_return_error_m':error,'collision_samples':hits,'doorway_rays':rays,'samples':samples,'passed':max_hinge<1e-6 and error<1e-6 and not hits and not any(r['blocked'] for r in rays),'state_restored':scene.frame_current==initial}

def inspect():
    body,doors=objects();bpy.context.scene.frame_set(1);bpy.context.view_layer.update();reports=[]
    for ob in [body]+doors:
        r=modeling._mesh_report(ob)
        for field in ('loose_vertices','loose_edges','zero_length_edges','boundary_edges','wire_edges','non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices'):assert r['geometry'][field]==0,(ob.name,field)
        reports.append({'mesh':r,'uv':mesh_uv_audit(ob.data),'side':ob.get('LG_side')})
    return {'blender_version':bpy.app.version_string,'objects':reports,'body_name':body.name,'door_names':[o.name for o in doors],'total_mesh_vertices':sum(len(o.data.vertices) for o in [body]+doors),'total_triangles':sum(r['mesh']['geometry']['triangles'] for r in reports),'motion':motion()}

def preview(a):
    scene=bpy.context.scene;scene.frame_set(a['frame']);bpy.context.view_layer.update();body,doors=objects()
    for ob in [body]+doors:
        mat=bpy.data.materials.new('QA_Clay');mat.use_nodes=True;color=(.52,.55,.52,1) if ob==body else (.30,.32,.28,1)
        mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=color;mat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=.8
        ob.data.materials.clear();ob.data.materials.append(mat)
    lo,hi=core.bounds(body);target=Vector([(x+y)/2 for x,y in zip(lo,hi)]);span=max(y-x for x,y in zip(lo,hi))
    camdata=bpy.data.cameras.new('QA_Camera');cam=bpy.data.objects.new('QA_Camera',camdata);scene.collection.objects.link(cam)
    direction={'front':(8,0,2.1),'front_right':(7,-5,3.5),'back':(-7,5,3.5)}[a['view']]
    cam.location=target+Vector(direction)*span/2;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();camdata.type='ORTHO';camdata.ortho_scale=span*1.42;scene.camera=cam
    for i,(vec,power) in enumerate([((2,-2,3),75),((1,2,2),40),((-2,0,3),55)]):
        data=bpy.data.lights.new('QA_Light'+str(i),'AREA');data.energy=power*span*span;data.shape='DISK';data.size=span
        light=bpy.data.objects.new(data.name,data);scene.collection.objects.link(light);light.location=target+Vector(vec)*span;light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.new('QA_World');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.24,.26,.29,1);scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.6
    scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=6
    scene.render.resolution_x=960;scene.render.resolution_y=960;scene.render.resolution_percentage=100
    dest=core.path(a['output_path'],False);dest.parent.mkdir(parents=True,exist_ok=True);scene.render.image_settings.file_format='PNG';scene.render.filepath=str(dest);bpy.ops.render.render(write_still=True)
    return {'output_path':str(dest),'sha256':core.digest(dest),'frame':a['frame'],'view':a['view'],'clay_only':True,'input_saved':False}

def export(a):
    body,doors=objects();scene=bpy.context.scene;scene.frame_set(1);bpy.context.view_layer.update();selection=a['selection']
    if selection=='assembly':obs=[body]+doors
    elif selection=='body':obs=[body]
    else:
        side=selection.removeprefix('door_');obs=[o for o in doors if o['LG_side']==side];assert len(obs)==1
    animated=selection=='assembly' and bool(doors)
    if not animated:
        for ob in obs:ob.animation_data_clear()
        if selection.startswith('door_'):obs[0].location=(0,0,0)
    core.select(obs);dest=core.path(a['output_path'],False);dest.parent.mkdir(parents=True,exist_ok=True)
    if dest.suffix=='.glb':
        bpy.ops.export_scene.gltf(filepath=str(dest),export_format='GLB',use_selection=True,export_yup=True,export_apply=False,export_animations=animated,export_animation_mode='SCENE',export_anim_scene_split_object=False,export_frame_range=True,export_frame_step=1,export_force_sampling=True,export_extras=True,export_materials='NONE')
    else:
        assert dest.suffix=='.fbx'
        bpy.ops.export_scene.fbx(filepath=str(dest),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',mesh_smooth_type='FACE',bake_space_transform=False,add_leaf_bones=False,bake_anim=animated,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_step=1,bake_anim_simplify_factor=0,use_custom_props=True)
    assert dest.is_file();return {'path':str(dest),'sha256':core.digest(dest),'bytes':dest.stat().st_size,'animated':animated,'objects':[o.name for o in obs]}

def self_test(a):
    for ob in list(bpy.data.objects):bpy.data.objects.remove(ob,do_unlink=True)
    refs=[]
    for label,location,size in [('Base',(-2,0,.5),(.1,1,1)),('LeafL',(.98,-.265,.6),(.04,.51,1)),('LeafR',(.98,.265,.6),(.04,.51,1))]:
        bpy.ops.mesh.primitive_cube_add(size=1,location=location);ob=bpy.context.object;ob.name='TEST_'+label;ob.scale=size;bpy.context.view_layer.update();refs.append({'object_name':ob.name,'expected_fingerprint':modeling._mesh_fingerprint(ob)})
    doors=[]
    for i,side in enumerate(['left','right']):doors.append({'output_name':'TEST_Output'+side,'parts':[refs[i+1]],'pivot_world_m':[1,-.52 if i==0 else .52,.1],'side':side,'opening_y_min':-.6,'opening_y_max':.6,'opening_z_min':.08,'opening_z_max':1.2,'wall_front_x':.95})
    assembly=assemble(dict(static_parts=refs[:1],doors=doors,body_name='TEST_OutputBody',output_blend=a['output_blend']));uv()
    body,ds=objects()
    for d in ds:core.animate(d,-100 if d['LG_side']=='left' else 100)
    r=inspect();assert r['motion']['passed'] and len(r['door_names'])==2
    assert all(x['uv']['passed'] for x in r['objects'])
    return {'passed':True,'tests':['left_hinge','right_hinge','mirrored_100_degree_opening','73frame_no_collision','exact_return','source_preserving_assembly','native_AABB_UV'],'report':r,'fixture':assembly['output']}

def run(a):
    assert bpy.app.version>=(5,0,0);cmd=a['command'];assert cmd in {'self_test','assemble','uv','animate','inspect','preview','export','inspect_export'}
    source=None
    if 'blend_path' in a:source=core.path(a['blend_path']);assert core.digest(source)==a['expected_sha256']
    if cmd=='self_test':r=self_test(a)
    elif cmd=='assemble':r=assemble(a)
    elif cmd=='inspect':r=inspect()
    elif cmd=='preview':r=preview(a)
    elif cmd=='export':r=export(a)
    elif cmd=='inspect_export':r=core.run(a)
    else:
        before=inspect()
        if cmd=='uv':uv()
        else:
            body,ds=objects();assert ds
            for d in ds:core.animate(d,-100 if d['LG_side']=='left' else 100)
        after=inspect()
        if cmd=='animate':assert after['motion']['passed']
        r={'before':before,'after':after,'output':core.save(a['output_blend'])}
    if source:assert core.digest(source)==a['expected_sha256'];r['input_preserved']=True
    return r

if __name__=='__main__':
    req=Path(sys.argv[sys.argv.index('--')+1]).resolve();req.relative_to(STATE.resolve());assert req.name=='request.json'
    out=req.parent/'result.json';assert not out.exists()
    try:payload={'state':'finished','success':True,'result':run(json.loads(req.read_text(encoding='utf-8')))}
    except Exception as e:payload={'state':'failed','success':False,'error':str(e),'traceback':traceback.format_exc()}
    tmp=out.with_suffix('.tmp');tmp.write_text(json.dumps(payload,indent=2),encoding='utf-8');tmp.replace(out)
