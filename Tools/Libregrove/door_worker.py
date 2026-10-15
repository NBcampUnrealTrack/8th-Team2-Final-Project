"""Fixed implementation behind the user-approved door MCP, not a script tool."""
from __future__ import annotations
import hashlib,json,math,sys,traceback
from pathlib import Path
import bpy
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveDoorV5'
STATE=ROOT/'Saved/LibregroveDoorJobs'
sys.path.insert(0,str(ROOT/'Plugins/Revvy/Content/Blender'))
sys.path.insert(0,str(Path(__file__).parent))
from revvy_blender_bridge import modeling
from precision_uv import mesh_uv_audit
VERSION='1.0.0'

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def path(p,existing=True):
    p=Path(p).resolve();p.relative_to(EXCHANGE.resolve())
    assert p.is_file() if existing else not p.exists()
    return p

def save(p):
    p=path(p,False);p.parent.mkdir(parents=True,exist_ok=True)
    result=bpy.ops.wm.save_as_mainfile(filepath=str(p),copy=True,check_existing=False)
    assert 'FINISHED' in result
    return {'path':str(p),'sha256':digest(p),'bytes':p.stat().st_size}

def select(objects):
    for ob in bpy.context.selected_objects:ob.select_set(False)
    for ob in objects:ob.hide_set(False);ob.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]

def meshes():return sorted((o for o in bpy.context.scene.objects if o.type=='MESH'),key=lambda o:o.name)

def assembly_objects():
    obs=meshes();assert len(obs)==2,'Expected exact two-mesh assembly'
    doors=[o for o in obs if o.get('LG_role')=='door'];assert len(doors)==1
    return next(o for o in obs if o!=doors[0]),doors[0]

def coords(ob):return [ob.matrix_world@v.co for v in ob.data.vertices]

def bounds(ob):
    p=coords(ob);return [[min(v[i] for v in p) for i in range(3)],[max(v[i] for v in p) for i in range(3)]]

def combine(parts,new_name,pivot):
    assert new_name not in bpy.data.objects
    vertices=[];faces=[];uvs=[];original=[]
    for ob in parts:
        assert ob.type=='MESH' and not ob.modifiers and not ob.parent and not ob.animation_data
        assert ob.data.uv_layers.active and not ob.data.materials
        local=coords(ob);offset=len(vertices);original.extend(local)
        vertices.extend([list(v-pivot) for v in local])
        layer=ob.data.uv_layers.active.data
        for poly in ob.data.polygons:
            faces.append([offset+i for i in poly.vertices]);uvs.extend([list(layer[i].uv) for i in poly.loop_indices])
    assert len(vertices)<=2048 and len(faces)<=2048
    mesh=bpy.data.meshes.new(new_name+'_Mesh');mesh.from_pydata(vertices,[],faces);mesh.update()
    layer=mesh.uv_layers.new(name='UV_Texture')
    for item,uv in zip(layer.data,uvs):item.uv=uv
    obj=bpy.data.objects.new(new_name,mesh);bpy.context.scene.collection.objects.link(obj);obj.location=pivot
    bpy.context.view_layer.update()
    drift=max((a-b).length for a,b in zip(original,coords(obj)))
    assert drift<1e-6,'Assembly changed world positions'
    return obj,drift

def assemble(a):
    bpy.context.view_layer.update();src=[];reports=[]
    for ref in a['static_parts']+a['moving_parts']:
        ob=bpy.data.objects[ref['object_name']];report=modeling._mesh_report(ob)
        assert report['fingerprint']==ref['expected_fingerprint'],'Stale part fingerprint'
        src.append(ob);reports.append(report)
    n=len(a['static_parts']);leaf=src[n];lo,hi=bounds(leaf);pivot=Vector(a['pivot_world_m'])
    assert abs(pivot.x-hi[0])<1e-5 and abs(pivot.y-lo[1])<1e-5 and abs(pivot.z-lo[2])<1e-5,'Hinge must be leaf front/left/bottom corner'
    assert .05<hi[1]-lo[1]<2 and .1<hi[2]-lo[2]<3 and .005<hi[0]-lo[0]<.2
    body,d1=combine(src[:n],a['body_name'],Vector())
    door,d2=combine(src[n:],a['door_name'],pivot)
    body['LG_role']='body';door['LG_role']='door'
    door['LG_hinge_closed_world_m']=list(pivot);door['LG_leaf_width_m']=hi[1]-lo[1]
    for ob,report in zip(src,reports):assert modeling._mesh_fingerprint(ob)==report['fingerprint']
    # Only the isolated working copy is cleaned. The hashed source .blend stays immutable.
    for ob in list(bpy.data.objects):
        if ob not in (body,door):bpy.data.objects.remove(ob,do_unlink=True)
    scene=bpy.context.scene;scene.name='LG_Cottage_V5';scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    scene.render.fps=24;scene.render.fps_base=1;scene.frame_start=1;scene.frame_end=73;scene.frame_set(1)
    select([door])
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                space=area.spaces.active;space.clip_start=.01;space.clip_end=1000
                space.region_3d.view_location=(0,0,1.1);space.region_3d.view_distance=4.8
                space.region_3d.view_rotation=Vector((6,-5,4)).to_track_quat('Z','Y');space.region_3d.view_perspective='ORTHO'
                space.shading.type='SOLID';space.shading.light='STUDIO';space.shading.show_cavity=True
    return {'source_parts':reports,'independent_data':True,'world_geometry_max_drift_m':max(d1,d2),'omitted_from_new_output_only':[o['object']['name'] for o in reports],'report':inspect(), 'output':save(a['output_blend'])}

def unwrap():
    for ob in meshes():
        select([ob]);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        try:
            bpy.ops.uv.smart_project(angle_limit=math.radians(66),margin_method='FRACTION',island_margin=.008,correct_aspect=True,scale_to_bounds=False)
            bpy.ops.uv.select_all(action='SELECT');bpy.ops.uv.average_islands_scale()
            bpy.ops.uv.pack_islands(udim_source='CLOSEST_UDIM',rotate=True,rotate_method='ANY',scale=True,merge_overlap=False,margin_method='FRACTION',margin=.008,shape_method='CONCAVE')
        finally:bpy.ops.object.mode_set(mode='OBJECT')
        assert mesh_uv_audit(ob.data)['passed'],'UV QA failed'

def curves(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                yield from bag.fcurves

def animate(door,angle):
    assert not door.animation_data,'Existing animation must not be overwritten'
    scene=bpy.context.scene;scene.render.fps=24;scene.render.fps_base=1;scene.frame_start=1;scene.frame_end=73
    door.rotation_mode='XYZ'
    for frame,degrees in [(1,0),(25,angle),(49,angle),(73,0)]:
        door.rotation_euler.z=math.radians(degrees);door.keyframe_insert(data_path='rotation_euler',index=2,frame=frame,group='Door_Hinge_Z')
    action=door.animation_data.action;action.name='LG_Door_OpenHoldClose_24fps'
    for curve in curves(action):
        for key in curve.keyframe_points:key.interpolation='BEZIER';key.handle_left_type='AUTO_CLAMPED';key.handle_right_type='AUTO_CLAMPED'
    door['LG_open_angle_degrees']=angle
    for frame,label in [(1,'CLOSED'),(25,'OPEN'),(49,'CLOSE_START'),(73,'CLOSED_END')]:scene.timeline_markers.new(label,frame=frame)
    scene.frame_set(1);bpy.context.view_layer.update()

def bvh(ob):
    ob.data.calc_loop_triangles()
    return BVHTree.FromPolygons(coords(ob),[list(t.vertices) for t in ob.data.loop_triangles],all_triangles=True,epsilon=0)

def motion(body,door):
    if not door.animation_data:return {'animated':False}
    scene=bpy.context.scene;start=scene.frame_current;scene.frame_set(1);bpy.context.view_layer.update()
    pivot=door.matrix_world.translation.copy();closed=coords(door);static=modeling._mesh_fingerprint(body);body_tree=bvh(body)
    samples=[];max_hinge=0.;hits=[];doorway=[]
    for y in [-.26,-.13,0,.13,.26]:
        for z in [.2,.6,1.0]:
            hit=body_tree.ray_cast(Vector((1.2,y,z)),Vector((-1,0,0)),.6)[0]
            doorway.append({'y':y,'z':z,'body_blocks_opening':hit is not None})
    for frame in range(1,74):
        scene.frame_set(frame);bpy.context.view_layer.update()
        max_hinge=max(max_hinge,(door.matrix_world.translation-pivot).length)
        overlap=body_tree.overlap(bvh(door))
        if overlap:hits.append({'frame':frame,'triangle_pairs':len(overlap)})
        p=door.matrix_world@Vector((0,door.get('LG_leaf_width_m',.57),.495))
        samples.append({'frame':frame,'z_degrees':math.degrees(door.rotation_euler.z),'hinge_world':list(door.matrix_world.translation),'latch_world':list(p),'door_bounds':bounds(door)})
    return_error=max((a-b).length for a,b in zip(closed,coords(door)))
    assert modeling._mesh_fingerprint(body)==static
    action=door.animation_data.action;keyframes=[{'path':c.data_path,'index':c.array_index,'keys':[[float(k.co.x),float(k.co.y)] for k in c.keyframe_points]} for c in curves(action)]
    scene.frame_set(start);bpy.context.view_layer.update()
    return {'animated':True,'action':action.name,'fps':scene.render.fps,'frame_range':[1,73],'duration_seconds':3,'samples':samples,'keyframes':keyframes,'max_hinge_drift_m':max_hinge,'closed_return_error_m':return_error,'body_door_triangle_overlap_samples':hits,'doorway_rays':doorway,'passed':max_hinge<1e-6 and return_error<1e-6 and not hits and not any(r['body_blocks_opening'] for r in doorway),'state_restored':scene.frame_current==start}

def inspect():
    body,door=assembly_objects();bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
    reports=[{'mesh':modeling._mesh_report(o),'uv':mesh_uv_audit(o.data)} for o in (body,door)]
    for item in reports:
        g=item['mesh']['geometry']
        for field in ('loose_vertices','loose_edges','zero_length_edges','boundary_edges','wire_edges','non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices'):
            assert g[field]==0,(item['mesh']['object']['name'],field,g[field])
    return {'blender_version':bpy.app.version_string,'mesh_count':2,'body_name':body.name,'door_name':door.name,'objects':reports,'total_mesh_vertices':sum(len(o.data.vertices) for o in (body,door)),'total_triangles':sum(x['mesh']['geometry']['triangles'] for x in reports),'motion':motion(body,door)}

def preview(a):
    scene=bpy.context.scene;scene.frame_set(a['frame']);bpy.context.view_layer.update()
    body,door=assembly_objects()
    for ob in (body,door):
        mat=bpy.data.materials.new('QA_Clay_'+ob.name);mat.diffuse_color=(.52,.55,.52,1) if ob==body else (.30,.32,.28,1)
        mat.use_nodes=True;bsdf=mat.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Base Color'].default_value=mat.diffuse_color;bsdf.inputs['Roughness'].default_value=.8
        ob.data.materials.clear();ob.data.materials.append(mat)
    cam_data=bpy.data.cameras.new('QA_Camera');cam=bpy.data.objects.new('QA_Camera',cam_data);scene.collection.objects.link(cam)
    target=Vector((0,0,1.12));direction={'front':(8,0,2.2),'front_right':(7,-5,3.4),'back':(-7,5,3.5)}[a['view']]
    cam.location=target+Vector(direction);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam_data.type='ORTHO';cam_data.ortho_scale=3.8;scene.camera=cam
    for index,(loc,power,size) in enumerate([((4,-4,6),550,4),((2,4,4),280,3),((-4,0,5),420,3)]):
        data=bpy.data.lights.new('QA_Light_'+str(index),'AREA');data.energy=power;data.shape='DISK';data.size=size
        light=bpy.data.objects.new(data.name,data);scene.collection.objects.link(light);light.location=loc;light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.new('QA_World');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.24,.26,.29,1);scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.6
    scene.render.engine='CYCLES';scene.cycles.samples=16;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=8
    scene.render.resolution_x=960;scene.render.resolution_y=960;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';dest=path(a['output_path'],False);dest.parent.mkdir(parents=True,exist_ok=True);scene.render.filepath=str(dest)
    bpy.ops.render.render(write_still=True)
    return {'output_path':str(dest),'sha256':digest(dest),'frame':a['frame'],'view':a['view'],'preview_only_clay_materials':True,'input_scene_saved':False}

def export(a):
    body,door=assembly_objects();scene=bpy.context.scene;scene.frame_set(1);bpy.context.view_layer.update()
    animated=a['selection']=='assembly_animated';objects=[body,door] if animated else [body] if a['selection']=='body_static' else [door]
    if not animated:
        for ob in objects:ob.animation_data_clear()
        if objects==[door]:door.location=(0,0,0)
    select(objects);dest=path(a['output_path'],False);dest.parent.mkdir(parents=True,exist_ok=True)
    if dest.suffix=='.glb':
        result=bpy.ops.export_scene.gltf(filepath=str(dest),export_format='GLB',use_selection=True,export_yup=True,export_apply=False,export_animations=animated,export_animation_mode='SCENE',export_frame_range=True,export_frame_step=1,export_force_sampling=True,export_extras=True,export_materials='NONE')
    else:
        assert dest.suffix=='.fbx'
        result=bpy.ops.export_scene.fbx(filepath=str(dest),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',use_mesh_modifiers=True,mesh_smooth_type='FACE',bake_space_transform=False,add_leaf_bones=False,bake_anim=animated,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_step=1,bake_anim_simplify_factor=0,use_custom_props=True)
    assert 'FINISHED' in result and dest.is_file()
    return {'output_path':str(dest),'sha256':digest(dest),'bytes':dest.stat().st_size,'selection':[o.name for o in objects],'animated':animated,'door_pivot_local':a['selection']=='door_static_local'}

def self_test(a):
    for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
    parts=[]
    for name,loc,scale in [('TEST_Body',(-2,0,.5),(.2,1,1)),('TEST_Leaf',(.98,.3,.6),(.04,.6,1)),('TEST_Handle',(1.02,.5,.6),(.04,.05,.03))]:
        bpy.ops.mesh.primitive_cube_add(size=1,location=loc);ob=bpy.context.object;ob.name=name;ob.scale=scale;parts.append(ob)
    bpy.context.view_layer.update()
    refs=[{'object_name':o.name,'expected_fingerprint':modeling._mesh_fingerprint(o)} for o in parts]
    intermediate=dict(static_parts=refs[:1],moving_parts=refs[1:],body_name='TEST_CombinedBody',door_name='TEST_HingedDoor',pivot_world_m=(1,0,.1),output_blend=a['output_blend'])
    result=assemble(intermediate);body,door=assembly_objects();unwrap();animate(door,-100);report=inspect()
    assert report['motion']['passed'] and abs(report['motion']['samples'][24]['z_degrees']+100)<.0001
    assert report['total_mesh_vertices']==24 and report['total_triangles']==36
    return {'tests_passed':['independent_geometry_copy','world_geometry_preserved','hinge_fixed_73_frames','open_100_degrees','closed_return_exact','handle_rigid_in_same_mesh','native_uv_no_overlap','bounded_geometry_counts'],'report':report,'fixture_geometry_saved':result['output'],'note':'Fixture file is pre-animation; tested animation ran in isolated memory.'}

def run(a):
    assert bpy.app.version>=(5,0,0)
    cmd=a['command'];assert cmd in {'assemble','uv','animate','inspect','preview','export','self_test','inspect_export'}
    source=None
    if 'blend_path' in a:
        source=path(a['blend_path']);assert digest(source)==a['expected_sha256'];assert Path(bpy.data.filepath).resolve()==source
    if cmd=='self_test':result=self_test(a)
    elif cmd=='inspect_export':
        src=path(a['export_path']);assert digest(src)==a['expected_sha256']
        for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
        bpy.context.scene.render.fps=24;bpy.context.scene.render.fps_base=1
        if src.suffix=='.glb':bpy.ops.import_scene.gltf(filepath=str(src))
        else:bpy.ops.import_scene.fbx(filepath=str(src),use_anim=True,anim_offset=0.0)
        bpy.context.view_layer.update();obs=meshes()
        result={'meshes':[{'mesh':modeling._mesh_report(o),'uv':mesh_uv_audit(o.data),'world_bounds':bounds(o)} for o in obs],'blender_version':bpy.app.version_string,'input_preserved':digest(src)==a['expected_sha256']}
        animated=[o for o in obs if o.animation_data and o.animation_data.action]
        result['animated_objects']=[{'name':o.name,'range':list(o.animation_data.action.frame_range)} for o in animated]
        result['motion_samples']=[]
        for frame in [1,13,25,49,73]:
            bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()
            result['motion_samples'].append({'frame':frame,'objects':[{'name':o.name,'matrix_world':[list(row) for row in o.matrix_world]} for o in obs]})
    elif cmd=='assemble':result=assemble(a)
    elif cmd=='inspect':result=inspect()
    elif cmd=='preview':result=preview(a)
    elif cmd=='export':result=export(a)
    else:
        before=inspect()
        if cmd=='uv':unwrap()
        else:
            body,door=assembly_objects();assert door.name==a['door_name'];animate(door,a['open_angle_degrees'])
        after=inspect()
        if cmd=='animate':assert after['motion']['passed'],'Motion or doorway clearance QA failed'
        result={'before':before,'after':after,'output':save(a['output_blend'])}
    if source:assert digest(source)==a['expected_sha256'];result.update(input_preserved=True,input_sha256=a['expected_sha256'])
    return result

if __name__=='__main__':
    req=Path(sys.argv[sys.argv.index('--')+1]).resolve();req.relative_to(STATE.resolve());assert req.name=='request.json'
    out=req.parent/'result.json';assert not out.exists()
    try:payload={'state':'finished','success':True,'result':run(json.loads(req.read_text(encoding='utf-8')))}
    except Exception as error:payload={'state':'failed','success':False,'error':str(error),'traceback':traceback.format_exc()}
    tmp=out.with_suffix('.tmp');tmp.write_text(json.dumps(payload,indent=2),encoding='utf-8');tmp.replace(out)
