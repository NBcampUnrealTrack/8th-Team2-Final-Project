"""Implementation of the fixed Blender repair MCP commands, not a script tool.

Only loads requests created by blender_repair_mcp in its private job directory.
All changes remain in a new object/new file. No eval, exec or dynamic operators.
"""
from __future__ import annotations
import hashlib
import json
import math
from pathlib import Path
import sys
import traceback
import bpy
import bmesh
from mathutils import Vector,geometry

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Plugins/Revvy/Content/Blender'))
from revvy_blender_bridge import modeling
from revvy_blender_bridge.common import BridgeConfig
EXCHANGE=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth'

def safe_path(value,suffixes,exists):
    p=Path(value).resolve();p.relative_to(EXCHANGE.resolve())
    assert p.suffix.lower() in suffixes
    assert p.is_file() if exists else not p.exists()
    return p

def indices(bm):
    for seq in (bm.verts,bm.edges,bm.faces):seq.ensure_lookup_table();seq.index_update()

def box(verts):
    points=[v.co for v in verts]
    lo=[min(p[k] for p in points) for k in range(3)];hi=[max(p[k] for p in points) for k in range(3)]
    return dict(min=lo,max=hi,center=[(a+b)/2 for a,b in zip(lo,hi)],diameter=math.dist(lo,hi))

def topology(bm):
    indices(bm)
    pending=set(bm.faces);components=[]
    while pending:
        first=min(pending,key=lambda f:f.index);pending.remove(first);stack=[first];faces=[]
        while stack:
            f=stack.pop();faces.append(f)
            for e in f.edges:
                for other in e.link_faces:
                    if other in pending:pending.remove(other);stack.append(other)
        components.append(dict(id=len(components),face_ids=sorted(f.index for f in faces),face_count=len(faces),area=sum(f.calc_area() for f in faces),bounds=box({v for f in faces for v in f.verts})))
    pending={e for e in bm.edges if e.is_boundary};loops=[]
    while pending:
        first=min(pending,key=lambda e:e.index);pending.remove(first);stack=[first];edges=[]
        while stack:
            e=stack.pop();edges.append(e)
            for v in e.verts:
                for other in v.link_edges:
                    if other in pending:pending.remove(other);stack.append(other)
        verts={v for e in edges for v in e.verts}
        simple=all(sum(e in edges for e in v.link_edges)==2 for v in verts)
        loops.append(dict(id=len(loops),edge_ids=sorted(e.index for e in edges),vertex_ids=sorted(v.index for v in verts),edge_count=len(edges),simple_closed=simple,bounds=box(verts)))
    return dict(components=components,boundary_loops=loops,vertices=[list(v.co) for v in bm.verts],faces=[[v.index for v in f.verts] for f in bm.faces])

def fill(bm,groups):
    uv=bm.loops.layers.uv.active
    added=[]
    # Cache exact inspected edges before adding new ones invalidates the index table.
    edge_groups=[[bm.edges[i] for i in group['edge_ids']] for group in groups]
    for group,edges in zip(groups,edge_groups):
        neighbors={f for e in edges for f in e.link_faces}
        # Store adjacent UV triangles before filling. Preserve every original loop.
        refs=[]
        for f in neighbors:
            ls=list(f.loops)
            for i in range(1,len(ls)-1):
                tri=[ls[0],ls[i],ls[i+1]]
                refs.append((sum((l.vert.co for l in tri),Vector())/3,[l.vert.co.copy() for l in tri],[Vector((*l[uv].uv,0)) for l in tri] if uv else None,f.material_index))
        result=bmesh.ops.holes_fill(bm,edges=edges,sides=0)
        fresh=result['faces']
        assert fresh,'No faces created for boundary '+str(group['id'])
        for f in fresh:
            center=f.calc_center_median();ref=min(refs,key=lambda r:(r[0]-center).length)
            f.material_index=ref[3]
            if uv:
                f.normal_update()
                edge=max(f.edges,key=lambda e:e.calc_length())
                axis=(edge.verts[1].co-edge.verts[0].co).normalized();other=f.normal.cross(axis).normalized()
                coords=[(l.vert.co.dot(axis),l.vert.co.dot(other)) for l in f.loops]
                mins=[min(c[k] for c in coords) for k in (0,1)];maxs=[max(c[k] for c in coords) for k in (0,1)]
                umins=[min(p[k] for p in ref[2]) for k in (0,1)];umaxs=[max(p[k] for p in ref[2]) for k in (0,1)]
                for l,p in zip(f.loops,coords):
                    l[uv].uv=tuple(umins[k]+(p[k]-mins[k])/max(maxs[k]-mins[k],1e-8)*max(umaxs[k]-umins[k],1e-5) for k in (0,1))
        # Explicitly triangulate only new n-gons; preserve all source quads.
        ngons=[f for f in fresh if len(f.verts)>4]
        if ngons:
            fresh=[f for f in fresh if f not in ngons]+bmesh.ops.triangulate(bm,faces=ngons)['faces']
        # A non-planar cap can produce a triangle edge-on to the n-gon's UV
        # projection. Reproject only such NEW faces in their own local plane.
        if uv:
            for f in fresh:
                ls=list(f.loops);pts=[l[uv].uv.copy() for l in ls]
                area=abs(sum(pts[i].x*pts[(i+1)%len(pts)].y-pts[(i+1)%len(pts)].x*pts[i].y for i in range(len(pts))))*.5
                if area<1e-10:
                    f.normal_update();edge=max(f.edges,key=lambda e:e.calc_length())
                    axis=(edge.verts[1].co-edge.verts[0].co).normalized();other=f.normal.cross(axis).normalized()
                    coords=[(l.vert.co.dot(axis),l.vert.co.dot(other)) for l in ls]
                    mins=[min(c[k] for c in coords) for k in (0,1)];spans=[max(c[k] for c in coords)-mins[k] for k in (0,1)]
                    center=sum(pts,Vector((0,0)))/len(pts)
                    for l,p in zip(ls,coords):l[uv].uv=tuple(max(.002,min(.998,center[k]))+((p[k]-mins[k])/max(spans[k],1e-10)-.5)*.002 for k in (0,1))
        added.extend(fresh)
    return len(added)

def render(obj,args):
    dest=safe_path(args['output_path'],('.png',),False);dest.parent.mkdir(parents=True,exist_ok=True)
    scene=bpy.context.scene
    for item in scene.objects:item.hide_render=item!=obj
    obj.hide_render=False
    center=sum((obj.matrix_world@Vector(corner) for corner in obj.bound_box),Vector())/8
    radius=max(obj.dimensions)*1.55
    directions={'front_left':(-1,-1,.68),'front_right':(1,-1,.68),'back_left':(-1,1,.68),'back_right':(1,1,.68),'top':(.01,-.02,1)}
    camera_data=bpy.data.cameras.new('RepairReviewCamera');camera=bpy.data.objects.new('RepairReviewCamera',camera_data);scene.collection.objects.link(camera)
    camera.location=center+Vector(directions[args['view']]).normalized()*radius
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.type='ORTHO';camera_data.ortho_scale=max(obj.dimensions)*1.32
    scene.camera=camera
    scene.render.engine='CYCLES';scene.cycles.samples=8;scene.cycles.use_denoising=True
    scene.render.threads_mode='FIXED';scene.render.threads=6
    scene.world=bpy.data.worlds.new('RepairReviewWorld');scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.32,.32,.32,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.7
    for i,offset in enumerate([(3,-4,6),(-4,-1,3),(0,4,5)]):
        ld=bpy.data.lights.new('ReviewLight'+str(i),'AREA');lo=bpy.data.objects.new(ld.name,ld);scene.collection.objects.link(lo)
        lo.location=center+Vector(offset);lo.rotation_euler=(center-lo.location).to_track_quat('-Z','Y').to_euler();ld.energy=[550,350,500][i];ld.size=4
    if args['mode']=='clay':
        mat=bpy.data.materials.new('RepairReviewClay');mat.diffuse_color=(.48,.53,.56,1);mat.use_nodes=True
        mat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.48,.53,.56,1)
        mat.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.8
        obj.data.materials.clear();obj.data.materials.append(mat)
        for p in obj.data.polygons:p.material_index=0
    scene.render.resolution_x=768;scene.render.resolution_y=768;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.filepath=str(dest)
    scene.view_settings.view_transform='AgX'
    bpy.ops.render.render(write_still=True)
    return dict(path=str(dest),bytes=dest.stat().st_size,view=args['view'],mode=args['mode'])

def gable_geometry(args):
    x=args['center_x'];w=args['half_width'];lo=args['eave_z'];hi=args['ridge_z'];d=args['thickness']
    profile=[(x-w,lo),(x,hi),(x+w,lo),(x-w,lo-d),(x,hi-d),(x+w,lo-d)]
    verts=[(px,y,z) for y in (args['front_y'],args['back_y']) for px,z in profile]
    faces=[(0,1,7,6),(1,2,8,7),(9,10,4,3),(10,11,5,4),(6,9,3,0),(2,5,11,8),(0,3,4,1),(1,4,5,2),(6,7,10,9),(7,8,11,10)]
    # Two large slope islands; eight distinct smaller islands for soffit/fascia.
    rects=[(.02,.55,.48,.98),(.52,.55,.98,.98)]+[(.02+(i%4)*.245,.03+(i//4)*.245,.23+(i%4)*.245,.23+(i//4)*.245) for i in range(8)]
    return verts,faces,rects

def replace_roof(source,output,args):
    path=safe_path(args['texture_path'],('.png',),False);path.parent.mkdir(parents=True,exist_ok=True)
    bm=bmesh.new();bm.from_mesh(source.data);top=topology(bm)
    roof_ids=set(top['components'][args['roof_component_id']]['face_ids'])
    assert 10<=len(roof_ids)<=5000
    stray_ids={i for cid in args['delete_component_ids'] for i in top['components'][cid]['face_ids']}
    assert len(stray_ids)<=20 and not roof_ids.intersection(stray_ids)
    bm.free()
    donor=modeling._duplicate_mesh(source,args['output_object_name']+'_RoofBakeREF')
    bm=bmesh.new();bm.from_mesh(donor.data);indices(bm)
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.index not in roof_ids],context='FACES')
    bm.to_mesh(donor.data);bm.free()
    mesh=bpy.data.meshes.new(args['output_object_name']+'_RoofMesh')
    verts,faces,rects=gable_geometry(args);mesh.from_pydata(verts,[],faces);mesh.update()
    roof=bpy.data.objects.new(args['output_object_name']+'_Roof',mesh);bpy.context.scene.collection.objects.link(roof)
    roof.matrix_world=source.matrix_world.copy()
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
    uv=mesh.uv_layers.new(name=source.data.uv_layers.active.name)
    for poly,rect in zip(mesh.polygons,rects):
        u0,v0,u1,v1=rect
        for li,coord in zip(poly.loop_indices,[(u0,v0),(u1,v0),(u1,v1),(u0,v1)]):uv.data[li].uv=coord
    mat=bpy.data.materials.new(args['output_object_name']+'_RoofMaterial');mat.use_nodes=True
    image=bpy.data.images.new(args['output_object_name']+'_RoofColor',args['texture_size'],args['texture_size'],alpha=False)
    image.generated_color=(.19,.18,.16,1)
    node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=image
    mat.node_tree.nodes.active=node;node.select=True
    mesh.materials.append(mat)
    for ob in bpy.context.scene.objects:
        ob.select_set(False);ob.hide_render=ob not in (roof,donor)
    for ob in (roof,donor):ob.hide_set(False);ob.hide_render=False;ob.select_set(True)
    bpy.context.view_layer.objects.active=roof
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
    scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.25;scene.render.bake.max_ray_distance=.5
    scene.render.bake.margin=12;scene.render.bake.use_clear=False
    # Diffuse COLOR only: transfer surface paint without light/shadow baking.
    bpy.ops.object.bake(type='DIFFUSE',pass_filter={'COLOR'},use_selected_to_active=True,cage_extrusion=.25,max_ray_distance=.5,margin=12,use_clear=False)
    image.filepath_raw=str(path);image.file_format='PNG';image.save();image.pack()
    bsdf=mat.node_tree.nodes.get('Principled BSDF');mat.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color']);bsdf.inputs['Roughness'].default_value=.92
    roof_report=modeling._mesh_report(roof,{'require_manifold':True})
    assert roof_report['geometry']['polygons']==10 and roof_report['geometry']['boundary_edges']==0 and roof_report['geometry']['non_manifold_edges']==0
    # Remove exact selected components in the copied house, including orphan vertices.
    bm=bmesh.new();bm.from_mesh(output.data);indices(bm)
    bmesh.ops.delete(bm,geom=[bm.faces[i] for i in roof_ids|stray_ids],context='FACES')
    loose=[v for v in bm.verts if not v.link_faces]
    if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
    bm.to_mesh(output.data);bm.free()
    for ob in bpy.context.selected_objects:ob.select_set(False)
    output.hide_set(False);roof.hide_set(False);output.select_set(True);roof.select_set(True)
    bpy.context.view_layer.objects.active=output
    bpy.ops.object.join()
    donor.hide_set(True);donor.hide_render=True
    return dict(roof=roof_report,deleted_roof_faces=len(roof_ids),deleted_stray_faces=len(stray_ids),texture_path=str(path),texture_size=args['texture_size'])

def run(args):
    command=args['command'];assert command in ('inspect','repair','gable_roof','export','render')
    source_path=safe_path(args['blend_path'],('.blend',),True)
    assert Path(bpy.data.filepath).resolve()==source_path
    input_hash=hashlib.sha256(source_path.read_bytes()).hexdigest()
    source=modeling._exact_object(args['object_name'],'MESH')
    before=modeling._mesh_report(source);fingerprint=before['fingerprint']
    if command in ('repair','gable_roof','export'):assert args['expected_fingerprint']==fingerprint,'Stale fingerprint'
    config=BridgeConfig(path=ROOT/'Saved/CindehearthRepair/config.json',token='',exchange_root=EXCHANGE.resolve(),port=0)
    if command=='inspect':
        bm=bmesh.new();bm.from_mesh(source.data)
        try:return dict(report=before,topology=topology(bm),input_sha256=input_hash)
        finally:bm.free()
    if command=='export':return modeling._model_export(config,dict(path=args['output_path'],object_names=[source.name],overwrite=False,apply_modifiers=True))
    if command=='render':return render(source,args)
    dest=safe_path(args['output_blend'],('.blend',),False);dest.parent.mkdir(parents=True,exist_ok=True)
    assert args['output_object_name'] not in bpy.data.objects
    output=modeling._duplicate_mesh(source,args['output_object_name'])
    if command=='gable_roof':
        changes=replace_roof(source,output,args)
        after=modeling._mesh_report(output)
        assert modeling._mesh_fingerprint(source)==fingerprint
        assert hashlib.sha256(source_path.read_bytes()).hexdigest()==input_hash
        for item in bpy.context.scene.objects:
            if item.type=='MESH':item.hide_set(item!=output);item.hide_render=item!=output
        output.hide_set(False);output.select_set(True);bpy.context.view_layer.objects.active=output
        bpy.ops.wm.save_as_mainfile(filepath=str(dest),copy=True,check_existing=False)
        return dict(source_preserved=True,input_sha256=input_hash,source=before,output=after,changes=changes,output_blend=str(dest))
    bm=bmesh.new();bm.from_mesh(output.data);before_top=topology(bm)
    operation=args['operation'];selection=args['selection_ids'];changes={}
    try:
        if operation=='weld':
            assert not selection and 0<float(args['distance'])<=.02
            bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=args['distance'])
        elif operation=='cap_polygon':
            assert 3<=len(selection)<=4
            vs=[bm.verts[i] for i in selection]
            assert box(vs)['diameter']<=args['max_diameter']
            normal=(vs[1].co-vs[0].co).cross(vs[2].co-vs[0].co).normalized()
            neighbors={f for v in vs for f in v.link_faces if abs(f.normal.dot(normal))>.95}
            ref=max(neighbors,key=lambda f:f.calc_area())
            uv=bm.loops.layers.uv.active
            tri=next((list(ref.loops)[0],list(ref.loops)[i],list(ref.loops)[i+1]) for i in range(1,len(ref.loops)-1) if (list(ref.loops)[i].vert.co-list(ref.loops)[0].vert.co).cross(list(ref.loops)[i+1].vert.co-list(ref.loops)[0].vert.co).length>1e-8)
            coords=[geometry.barycentric_transform(v.co,*(l.vert.co for l in tri),*(Vector((*l[uv].uv,0)) for l in tri)) for v in vs] if uv else []
            f=bm.faces.new(vs);f.material_index=ref.material_index
            if uv:
                for l,p in zip(f.loops,coords):l[uv].uv=(max(0,min(1,p.x)),max(0,min(1,p.y)))
            changes['added_faces']=1
        elif operation in ('delete_components','delete_faces'):
            if operation=='delete_components':
                groups=[before_top['components'][i] for i in selection]
                ids={i for c in groups for i in c['face_ids']}
            else:
                ids=set(selection)
                assert all(0<=i<len(bm.faces) for i in ids)
                assert all(box(bm.faces[i].verts)['diameter']<=args['max_diameter'] for i in ids)
            assert len(ids)<=args['max_faces']<=5000
            verts={v for i in ids for v in bm.faces[i].verts}
            bmesh.ops.delete(bm,geom=[bm.faces[i] for i in ids],context='FACES')
            orphan=[v for v in verts if v.is_valid and not v.link_faces]
            if orphan:bmesh.ops.delete(bm,geom=orphan,context='VERTS')
            changes['deleted_faces']=len(ids)
        elif operation in ('fill_loops','fill_networks'):
            groups=[before_top['boundary_loops'][i] for i in selection]
            assert all((c['simple_closed'] or operation=='fill_networks') and c['bounds']['diameter']<=args['max_diameter']<=5 for c in groups)
            changes['added_faces']=fill(bm,groups)
            assert changes['added_faces']<=args['max_faces'],'Cap face budget exceeded'
        else:raise ValueError('Unsupported fixed operation')
        # Preserve winding of untouched open sheets. Global recalc can flip a
        # one-face gable and invalidate its existing tangent-space normal map.
        bm.normal_update();bm.to_mesh(output.data);output.data.update()
    finally:bm.free()
    after=modeling._mesh_report(output)
    for name in ['non_finite_vertices','degenerate_faces','invalid_face_normals','non_manifold_edges']:
        assert after['geometry'][name]<=before['geometry'][name],(name,after['geometry'])
    assert all(layer['degenerate_faces']==0 and layer['non_finite']==0 for layer in after['uv_layers'])
    assert modeling._mesh_fingerprint(source)==fingerprint
    assert hashlib.sha256(source_path.read_bytes()).hexdigest()==input_hash
    # Isolate the accepted working copy when this saved file is opened interactively.
    for item in bpy.context.scene.objects:
        if item.type=='MESH':item.hide_set(item!=output);item.hide_render=item!=output
    output.select_set(True);bpy.context.view_layer.objects.active=output
    result=bpy.ops.wm.save_as_mainfile(filepath=str(dest),copy=True,check_existing=False)
    assert 'FINISHED' in result and dest.is_file()
    return dict(source_preserved=True,input_sha256=input_hash,source=before,output=after,changes=changes,output_blend=str(dest))

if __name__=='__main__':
    req=Path(sys.argv[sys.argv.index('--')+1]).resolve();req.relative_to((ROOT/'Saved/CindehearthRepair').resolve())
    result_file=req.parent/'result.json'
    assert not result_file.exists(),'Job result already exists'
    try:
        result=run(json.loads(req.read_text(encoding='utf-8')))
        payload=dict(state='finished',success=True,result=result)
    except Exception as error:
        payload=dict(state='failed',success=False,error=str(error),traceback=traceback.format_exc())
    temp=result_file.with_suffix('.tmp');temp.write_text(json.dumps(payload,indent=2),encoding='utf-8');temp.replace(result_file)
