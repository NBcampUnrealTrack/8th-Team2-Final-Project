"""Implementation of fixed precision MCP commands. Not a user-programmable script tool."""
from __future__ import annotations
import hashlib,json,math,sys,traceback
from pathlib import Path
import bpy,bmesh
from mathutils import Vector,Matrix,Euler,kdtree
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
EXCHANGE=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908'
OUTPUT=EXCHANGE/'precision_v2'
STATE=ROOT/'Saved/LibregrovePrecision'
sys.path.insert(0,str(ROOT/'Plugins/Revvy/Content/Blender'))
sys.path.insert(0,str(ROOT/'Tools/Cindehearth/Pipeline'))
sys.path.insert(0,str(Path(__file__).parent))
from revvy_blender_bridge import modeling
from blender_repair_worker import topology,indices
from precision_uv import mesh_uv_audit,export_precision_risks,self_test as uv_math_self_test

VERSION='2.0.0'
COMMANDS={'inspect','inspect_fbx','compare','guarded_decimate','offset_components','delete_wire_edges','dissolve','deduplicate','uv_sliver_guard','split_nonmanifold','sharp_normals','symmetry','seams','uv_native','uv_faces','panels','discs','assemble','triangulate','triangulate_repair','self_test'}

def surface_compare(obj,reference):
    def prepared(item):
        mesh=item.data;mesh.calc_loop_triangles();points=[item.matrix_world@v.co for v in mesh.vertices];triangles=[list(t.vertices) for t in mesh.loop_triangles]
        tree=BVHTree.FromPolygons(points,triangles,all_triangles=True)
        samples=points+[sum((points[i] for i in t),Vector())/3 for t in triangles]
        return tree,samples
    obj_tree,obj_points=prepared(obj);ref_tree,ref_points=prepared(reference)
    def measure(tree,points):
        values=sorted(tree.find_nearest(p)[3] for p in points)
        return {'samples':len(values),'max_distance_m':max(values),'mean_distance_m':sum(values)/len(values),'p99_distance_m':values[int((len(values)-1)*.99)]}
    return {'reference_to_output':measure(obj_tree,ref_points),'output_to_reference':measure(ref_tree,obj_points),'reference_fingerprint':modeling._mesh_fingerprint(reference),'output_fingerprint':modeling._mesh_fingerprint(obj),'method':'all vertices and triangle centroids to triangle BVH; sampled, not continuous Hausdorff'}

def select_only(obj):
    if bpy.context.object and bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
    for ob in bpy.context.selected_objects:ob.select_set(False)
    obj.hide_set(False);obj.hide_viewport=False;obj.select_set(True);bpy.context.view_layer.objects.active=obj

def with_bmesh(obj,fn):
    bm=bmesh.new();bm.from_mesh(obj.data);indices(bm)
    try:
        result=fn(bm);bm.normal_update();bm.to_mesh(obj.data);obj.data.update();return result
    finally:bm.free()

def pack_uv(obj,margin,average=True):
    select_only(obj);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    try:
        bpy.context.scene.tool_settings.use_uv_select_sync=False
        bpy.ops.uv.select_all(action='SELECT')
        if average:bpy.ops.uv.average_islands_scale()
        result=bpy.ops.uv.pack_islands(udim_source='CLOSEST_UDIM',rotate=True,rotate_method='ANY',scale=True,merge_overlap=False,margin_method='FRACTION',margin=margin,shape_method='CONCAVE')
        assert 'FINISHED' in result
    finally:bpy.ops.object.mode_set(mode='OBJECT')

def native_uv(obj,args):
    select_only(obj)
    if not obj.data.uv_layers.active:obj.data.uv_layers.new(name='UV_Game')
    if args['method']!='pack':
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        try:
            if args['method']=='smart_project':
                result=bpy.ops.uv.smart_project(angle_limit=math.radians(args['angle_degrees']),margin_method='FRACTION',island_margin=args['margin_fraction'],area_weight=.1,correct_aspect=True,scale_to_bounds=False)
            else:result=bpy.ops.uv.unwrap(method='ANGLE_BASED',margin_method='FRACTION',margin=args['margin_fraction'])
            assert 'FINISHED' in result
        finally:bpy.ops.object.mode_set(mode='OBJECT')
    pack_uv(obj,args['margin_fraction'])
    return {'operation':args['method'],'margin_method':'FRACTION','margin_fraction':args['margin_fraction']}

def project_faces(obj,args):
    mesh=obj.data;assert mesh.uv_layers.active,'Existing UV layer required'
    uv=mesh.uv_layers.active.data;faces=[mesh.polygons[i] for i in args['face_ids']]
    assert all(len(p.vertices)==3 for p in faces),'Triangulate before selected UV repair'
    # Preserve metric scale relative to the surrounding UVs. Native average-scale
    # and packing subsequently harmonize density; tiny 3D triangles are not erased.
    mesh.calc_loop_triangles();a3=0.;a2=0.
    for tri in mesh.loop_triangles:
        a3+=tri.area;p=[uv[i].uv for i in tri.loops]
        a2+=abs((p[1]-p[0]).cross(p[2]-p[0]))*.5
    factor=math.sqrt(max(a2,1e-10)/max(a3,1e-10))
    for index,poly in enumerate(faces):
        coords=[mesh.vertices[i].co for i in poly.vertices]
        axis=(coords[1]-coords[0]).normalized();normal=(coords[1]-coords[0]).cross(coords[2]-coords[0]);assert normal.length>1e-14
        second=normal.normalized().cross(axis)
        face_factor=max(factor,math.sqrt(args.get('minimum_uv_area',1e-9)/max(normal.length*.5,1e-20)))
        # Write directly near tile zero: large temporary offsets destroy the
        # altitude of thin triangles in Blender's float32 UV storage.
        shift=Vector((.127123+(index+1)*.000001,.213457+(index+1)*.000001))
        for li,co in zip(poly.loop_indices,coords):
            d=co-coords[0];uv[li].uv=(shift.x+d.dot(axis)*face_factor,shift.y+d.dot(second)*face_factor)
        for li in poly.loop_indices:mesh.edges[mesh.loops[li].edge_index].use_seam=True
    pack_uv(obj,args['margin_fraction'],average=False)
    return {'reprojected_face_ids':args['face_ids'],'native_repacked':True}

def mutate(obj,args):
    command=args['command']
    if command=='guarded_decimate':
        assert not obj.modifiers and not obj.data.shape_keys,'Decimation expects an unmodified static mesh'
        select_only(obj);modifier=obj.modifiers.new('PrecisionDecimate','DECIMATE');modifier.decimate_type='COLLAPSE';modifier.ratio=args['ratio'];modifier.use_collapse_triangulate=True;modifier.use_symmetry=False
        result=bpy.ops.object.modifier_apply(modifier=modifier.name);assert 'FINISHED' in result
        return {'native_modifier':'DECIMATE_COLLAPSE','ratio':args['ratio'],'symmetry':False,'uv_requires_validation':True}
    if command=='uv_sliver_guard':
        mesh=obj.data;uv=mesh.uv_layers.active.data;changes=[]
        risky={r['face_id'] for r in export_precision_risks(mesh)['risk_faces']}
        for fid in args['face_ids']:
            assert fid in risky,'Only inspected float32-risk faces may be changed'
            poly=mesh.polygons[fid];assert len(poly.vertices)==3
            ls=list(poly.loop_indices);pts=[uv[i].uv.copy() for i in ls]
            a,b=max(((0,1),(1,2),(2,0)),key=lambda ab:(pts[ab[1]]-pts[ab[0]]).length)
            c=3-a-b;base=pts[b]-pts[a];assert base.length>1e-8
            normal=Vector((-base.y,base.x)).normalized();height=(pts[c]-pts[a]).dot(normal)
            target=max(1e-6,2e-10/base.length)*(1 if height>=0 else -1)
            delta=target-height;assert abs(delta)<=1e-5,'UV repair exceeds bounded shift'
            uv[ls[c]].uv=pts[c]+normal*delta;changes.append({'face_id':fid,'uv_corner_shift':abs(delta)})
        assert mesh_uv_audit(mesh)['passed'],'Guarded UV edit introduced overlap/degeneracy'
        assert not any(r['glb_float32_uv_area']<=1e-12 for r in export_precision_risks(mesh)['risk_faces']),'Export still contains collapsed UV triangle'
        return {'guarded_faces':changes,'repacked':False,'geometry_changed':False}
    if command=='uv_native':return native_uv(obj,args)
    if command=='uv_faces':return project_faces(obj,args)
    if command=='sharp_normals':
        def apply(bm):
            threshold=math.radians(args['angle_degrees']);sharp=0
            for f in bm.faces:f.smooth=True
            for e in bm.edges:
                smooth=e.is_manifold and e.calc_face_angle(0)<=threshold
                e.smooth=smooth;sharp+=not smooth
            return {'sharp_edges':sharp,'angle_degrees':args['angle_degrees'],'face_winding_preserved':True}
        return with_bmesh(obj,apply)
    if command=='seams':
        for i in args['edge_ids']:obj.data.edges[i].use_seam=args['mark']
        return {'seam_edges':len(args['edge_ids']),'mark':args['mark']}
    def apply(bm):
        if command=='delete_wire_edges':
            edges=[bm.edges[i] for i in args['edge_ids']];assert all(not e.link_faces for e in edges),'Selected edge is not wire-only'
            count=len(bm.faces);verts={v for e in edges for v in e.verts}
            bmesh.ops.delete(bm,geom=edges,context='EDGES')
            isolated=[v for v in verts if v.is_valid and not v.link_edges and not v.link_faces]
            if isolated:bmesh.ops.delete(bm,geom=isolated,context='VERTS')
            assert len(bm.faces)==count
            return {'removed_wire_edge_ids':args['edge_ids'],'removed_isolated_vertices':len(isolated),'surface_faces_preserved':True}
        if command=='offset_components':
            top=topology(bm);faces={bm.faces[i] for cid in args['component_ids'] for i in top['components'][cid]['face_ids']};verts={v for f in faces for v in f.verts}
            assert all(all(f in faces for f in v.link_faces) for v in verts),'Selection shares vertices with unselected faces'
            unchanged={v:tuple(v.co) for v in bm.verts if v not in verts};delta=Vector(args['offset_m']);count=len(bm.faces)
            for v in verts:v.co+=delta
            assert len(bm.faces)==count and all(tuple(v.co)==co for v,co in unchanged.items())
            return {'offset_component_ids':args['component_ids'],'offset_m':args['offset_m'],'moved_vertices':len(verts),'unselected_vertices_unchanged':True,'face_count_preserved':True}
        if command=='deduplicate':
            removed=[]
            for pair in args['pairs']:
                keep=bm.faces[pair['keep_face']];drop=bm.faces[pair['remove_face']]
                assert len(keep.verts)==len(drop.verts)==3 and keep.material_index==drop.material_index
                assert sorted(tuple(v.co) for v in keep.verts)==sorted(tuple(v.co) for v in drop.verts),'Faces are not exact coordinate duplicates'
                if keep.normal.dot(drop.normal)<0:
                    assert len(obj.data.materials)>keep.material_index and obj.data.materials[keep.material_index] and not obj.data.materials[keep.material_index].use_backface_culling,'Opposite duplicate needs existing two-sided material'
                removed.append(drop)
            bmesh.ops.delete(bm,geom=removed,context='FACES')
            loose=[v for v in bm.verts if not v.link_faces]
            if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
            return {'removed_duplicate_faces':args['pairs'],'surviving_coordinates_and_uv_preserved':True}
        if command in ('triangulate','triangulate_repair'):
            source_ids=bm.faces.layers.int.new('precision_original_face')
            for f in bm.faces:f[source_ids]=f.index
            result=bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY')
            removed=[]
            if command=='triangulate_repair':
                allowed=set(args['degenerate_source_face_ids'])
                tiny=[f for f in bm.faces if f[source_ids] in allowed and f.calc_area()<=args['max_removed_triangle_area']]
                assert 0<len(tiny)<=100,'Expected a bounded set of tiny triangles'
                removed=[{'source_face_id':f[source_ids],'area':f.calc_area()} for f in tiny]
                bmesh.ops.delete(bm,geom=tiny,context='FACES')
                loose=[v for v in bm.verts if not v.link_faces]
                if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
            bm.faces.layers.int.remove(source_ids)
            return {'new_triangles':len(result['faces']),'removed_tiny_triangles':removed}
        if command=='dissolve':
            selected={bm.faces[i] for i in args['face_ids']}
            edges=[e for e in bm.edges if e.is_manifold and all(f in selected for f in e.link_faces) and not e.seam and len({f.material_index for f in e.link_faces})==1]
            verts=[v for v in bm.verts if v.link_faces and all(f in selected for f in v.link_faces) and all(e.is_manifold for e in v.link_edges)]
            before=len(bm.faces)
            bmesh.ops.dissolve_limit(bm,angle_limit=math.radians(args['angle_degrees']),use_dissolve_boundaries=False,verts=verts,edges=edges,delimit={'MATERIAL','SEAM','UV'})
            return {'dissolved_faces':before-len(bm.faces),'angle_degrees':args['angle_degrees']}
        if command=='split_nonmanifold':
            edges=[bm.edges[i] for i in args['edge_ids']];assert all(len(e.link_faces)>2 for e in edges),'Selection contains an edge that is not nonmanifold'
            before_faces=len(bm.faces);bmesh.ops.split_edges(bm,edges=edges,use_verts=False)
            assert len(bm.faces)==before_faces
            return {'split_edges':len(edges),'faces_preserved':True}
        if command=='symmetry':
            top=topology(bm);fs={bm.faces[i] for c in args['component_ids'] for i in top['components'][c]['face_ids']}
            verts={v for f in fs for v in f.verts};edges={e for f in fs for e in f.edges};tree=kdtree.KDTree(len(verts))
            for i,v in enumerate(verts):tree.insert(v.co,i)
            tree.balance();axis=0 if 'X' in args['direction'] else 1;worst=0.
            for v in verts:
                p=v.co.copy();p[axis]*=-1;worst=max(worst,tree.find(p)[2])
            assert worst<=args['max_reflection_error'],'Selected parts are not sufficiently symmetric'
            bmesh.ops.symmetrize(bm,input=list(fs|verts|edges),direction=args['direction'],dist=args['tolerance'])
            return {'components':args['component_ids'],'reflection_nearest_vertex_error':worst,'direction':args['direction']}
        if command=='panels':
            prior_min=[min(v.co[i] for v in bm.verts) for i in range(3)];prior_max=[max(v.co[i] for v in bm.verts) for i in range(3)];created=[]
            for spec in args['panels']:
                transform=Matrix.Translation(Vector(spec['center']))@Euler([math.radians(v) for v in spec['rotation_degrees']],'XYZ').to_matrix().to_4x4()@Matrix.Diagonal(Vector((*[v/2 for v in spec['dimensions']],1)))
                points=[transform@Vector((x,y,z)) for x in (-1,1) for y in (-1,1) for z in (-1,1)]
                assert all(prior_min[i]-args['max_bounds_growth']<=p[i]<=prior_max[i]+args['max_bounds_growth'] for p in points for i in range(3)),'Backing panel exceeds inspected source bounds'
                result=bmesh.ops.create_cube(bm,size=2,matrix=transform)
                for v in result['verts']:
                    for f in v.link_faces:f.smooth=False;f.material_index=0
                created.append(spec)
            return {'closed_box_panels':len(created),'added_triangles':12*len(created),'panels':created}
        if command=='discs':
            lo=[min(v.co[k] for v in bm.verts) for k in range(3)];hi=[max(v.co[k] for v in bm.verts) for k in range(3)]
            for spec in args['discs']:
                ext=[spec['radius'],spec['radius'],spec['depth']/2]
                assert all(lo[k]<=spec['center'][k]-ext[k] and spec['center'][k]+ext[k]<=hi[k] for k in range(3)),'Disc outside source bounds'
                result=bmesh.ops.create_cone(bm,cap_ends=True,cap_tris=False,segments=spec['segments'],radius1=spec['radius'],radius2=spec['radius'],depth=spec['depth'],matrix=Matrix.Translation(Vector(spec['center'])))
                for v in result['verts']:
                    for f in v.link_faces:f.smooth=False;f.material_index=0
            return {'closed_backing_discs':len(args['discs']),'discs':args['discs']}
        raise ValueError('Unknown fixed mutation')
    return with_bmesh(obj,apply)

def detailed(obj,args):
    bm=bmesh.new();bm.from_mesh(obj.data);indices(bm)
    try:
        top=topology(bm)
        top['edges']=[{'id':e.index,'vertices':[v.index for v in e.verts],'face_ids':[f.index for f in e.link_faces],'seam':e.seam,'sharp':not e.smooth} for e in bm.edges]
        top['face_normals']=[list(f.normal) for f in bm.faces]
        top['face_areas']=[f.calc_area() for f in bm.faces]
        preview=bm.copy();indices(preview);layer=preview.faces.layers.int.new('precision_preview_source')
        for f in preview.faces:f[layer]=f.index
        bmesh.ops.triangulate(preview,faces=list(preview.faces),quad_method='BEAUTY',ngon_method='BEAUTY')
        tiny=[{'source_face_id':f[layer],'triangle_area':f.calc_area()} for f in preview.faces if f.calc_area()<=1e-8]
        preview.free()
        return {'report':modeling._mesh_report(obj),'topology':top,'triangulation_preview':{'tiny_triangles_below_1e_8':tiny},'uv_audit':mesh_uv_audit(obj.data,args.get('include_uv_overlap',True)),'runtime':{'blender':bpy.app.version_string,'runtime_supported':bpy.app.version>=(5,0,0),'precision_mcp_version':VERSION,'contract_compatible':True}}
    finally:bm.free()

def fixture(name,verts,faces):
    mesh=bpy.data.meshes.new(name+'_Mesh');mesh.from_pydata(verts,[],faces);mesh.update();obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj);return obj

def self_test(args):
    tests=uv_math_self_test()
    plane=fixture('TEST_Plane',[(-1,-1,0),(1,-1,0),(1,1,0),(-1,1,0)],[(0,1,2),(0,2,3)])
    source_fp=modeling._mesh_fingerprint(plane);copy=modeling._duplicate_mesh(plane,'TEST_Dissolved')
    mutate(copy,{'command':'dissolve','face_ids':[0,1],'angle_degrees':.1});assert len(copy.data.polygons)==1 and modeling._mesh_fingerprint(plane)==source_fp;tests['planar_dissolve_preserves_source']=True
    fan=fixture('TEST_Fan',[(0,0,0),(1,0,0),(0,1,0),(0,-1,0),(0,0,1)],[(0,1,2),(1,0,3),(0,1,4)])
    info=detailed(fan,{'include_uv_overlap':False});edge_ids=[e['id'] for e in info['topology']['edges'] if len(e['face_ids'])>2]
    mutate(fan,{'command':'split_nonmanifold','edge_ids':edge_ids});assert modeling._mesh_report(fan)['geometry']['non_manifold_edges']==0;tests['nonmanifold_split']=True
    mutate(copy,{'command':'seams','edge_ids':[0],'mark':True});assert copy.data.edges[0].use_seam;tests['exact_seam']=True
    mutate(copy,{'command':'sharp_normals','angle_degrees':45});assert all(p.use_smooth for p in copy.data.polygons);tests['sharp_normals']=True
    mutate(copy,{'command':'triangulate'});mutate(copy,{'command':'uv_native','method':'smart_project','angle_degrees':60,'margin_fraction':.002})
    assert mesh_uv_audit(copy.data)['passed'];tests['native_fraction_uv_pack']=True
    for li in copy.data.polygons[0].loop_indices:copy.data.uv_layers.active.data[li].uv=(0,0)
    mutate(copy,{'command':'uv_faces','face_ids':[0],'margin_fraction':.002});assert mesh_uv_audit(copy.data)['passed'];tests['exact_uv_face_repair']=True
    symmetric=fixture('TEST_Symmetry',[(-1,-1,0),(1,-1,0),(1,1,0),(-1,1,0)],[(0,1,2),(0,2,3)])
    mutate(symmetric,{'command':'symmetry','component_ids':[0],'direction':'-Y','tolerance':.0001,'max_reflection_error':.0001})
    tests['guarded_partial_symmetry']=True
    degenerate=fixture('TEST_TriClean',[(0,0,0),(1,0,0),(2,0,0),(0,1,0)],[(0,1,2),(0,2,3)])
    mutate(degenerate,{'command':'triangulate_repair','degenerate_source_face_ids':[0],'max_removed_triangle_area':1e-10})
    assert len(degenerate.data.polygons)==1;tests['bounded_degenerate_triangulation']=True
    panel_source=fixture('TEST_Panels',[(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])
    mutate(panel_source,{'command':'panels','panels':[{'center':[0,0,0],'dimensions':[1,1,.1],'rotation_degrees':[0,0,0]}],'max_bounds_growth':0})
    assert modeling._mesh_report(panel_source)['geometry']['non_manifold_edges']==0;tests['closed_backing_panel']=True
    mutate(panel_source,{'command':'discs','discs':[{'center':[0,0,0],'radius':.5,'depth':.05,'segments':32}]})
    assert modeling._mesh_report(panel_source)['geometry']['non_manifold_edges']==0;tests['closed_backing_disc']=True
    guard=fixture('TEST_UVFloat32',[(0,0,0),(1,0,0),(0,1,0)],[(0,1,2)]);layer=guard.data.uv_layers.new(name='UV_Game')
    for loop,value in zip(layer.data,[(.1,.1),(.5,.1),(.1,.10000001)]):loop.uv=value
    mutate(guard,{'command':'uv_sliver_guard','face_ids':[0]});assert mesh_uv_audit(guard.data)['passed'];tests['float32_uv_guard']=True
    duplicate=fixture('TEST_Duplicate',[(0,0,0),(1,0,0),(0,1,0),(0,0,0),(1,0,0),(0,1,0)],[(0,1,2),(3,4,5)])
    mutate(duplicate,{'command':'deduplicate','pairs':[{'keep_face':0,'remove_face':1}]});assert len(duplicate.data.polygons)==1 and len(duplicate.data.vertices)==3;tests['exact_duplicate_removal']=True
    grid=fixture('TEST_DecimateGrid',[(x/8,y/8,0) for y in range(9) for x in range(9)],[(y*9+x,y*9+x+1,(y+1)*9+x+1) for y in range(8) for x in range(8)]+[(y*9+x,(y+1)*9+x+1,(y+1)*9+x) for y in range(8) for x in range(8)])
    grid_copy=modeling._duplicate_mesh(grid,'TEST_DecimatedGrid');fp=modeling._mesh_fingerprint(grid)
    mutate(grid_copy,{'command':'guarded_decimate','ratio':.5});comparison=surface_compare(grid_copy,grid)
    assert len(grid_copy.data.polygons)<len(grid.data.polygons) and max(comparison[d]['max_distance_m'] for d in ('reference_to_output','output_to_reference'))<1e-5
    assert modeling._mesh_fingerprint(grid)==fp;tests['native_decimation_and_surface_comparison']=True
    offset=fixture('TEST_Offset',[(0,0,0),(1,0,0),(0,1,0),(2,0,0),(3,0,0),(2,1,0)],[(0,1,2),(3,4,5)])
    mutate(offset,{'command':'offset_components','component_ids':[0],'offset_m':[0,0,.002]})
    assert abs(offset.data.vertices[0].co.z-.002)<1e-7 and offset.data.vertices[3].co.z==0;tests['bounded_component_offset']=True
    wire=fixture('TEST_Wire',[(0,0,0),(1,0,0),(0,1,0),(3,0,0),(4,0,0)],[(0,1,2)])
    def add_wire(bm):bm.edges.new((bm.verts[3],bm.verts[4]))
    with_bmesh(wire,add_wire);top=detailed(wire,{'include_uv_overlap':False})['topology'];wire_ids=[e['id'] for e in top['edges'] if not e['face_ids']]
    mutate(wire,{'command':'delete_wire_edges','edge_ids':wire_ids});assert len(wire.data.vertices)==3 and len(wire.data.polygons)==1;tests['exact_wire_edge_removal']=True
    dest=Path(args['output_blend']).resolve();dest.relative_to(OUTPUT.resolve());assert not dest.exists()
    bpy.ops.wm.save_as_mainfile(filepath=str(dest),copy=True,check_existing=False)
    return {'tests':tests,'blender_version':bpy.app.version_string,'runtime_supported':bpy.app.version>=(5,0,0),'precision_mcp_version':VERSION,'output_blend':str(dest),'user_models_loaded':False}

def run(args):
    assert args['version']==VERSION and args['command'] in COMMANDS
    if args['command']=='inspect_fbx':
        assert not bpy.data.filepath,'FBX inspection requires a fresh isolated process'
        source=Path(args['fbx_path']).resolve();source.relative_to((OUTPUT/'exports').resolve());assert source.suffix.lower()=='.fbx'
        digest=hashlib.sha256(source.read_bytes()).hexdigest();assert digest==args['expected_sha256'],'FBX hash mismatch'
        for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
        result=bpy.ops.import_scene.fbx(filepath=str(source),use_anim=False);assert 'FINISHED' in result
        bpy.context.view_layer.update()
        meshes=[obj for obj in bpy.context.scene.objects if obj.type=='MESH'];assert len(meshes)==1,'Expected exactly one exported asset'
        obj=meshes[0];report=modeling._mesh_report(obj);uv=mesh_uv_audit(obj.data)
        assert hashlib.sha256(source.read_bytes()).hexdigest()==digest,'FBX changed during inspection'
        return {'input_path':str(source),'input_sha256':digest,'input_preserved':True,'mesh_count':1,'report':report,'uv_audit':uv,'blender_version':bpy.app.version_string,'live_scene_modified':False}
    if args['command']=='self_test':return self_test(args)
    if args['command']=='assemble':
        assert not bpy.data.filepath,'Assembly must run in a fresh empty process'
        for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
        collection=bpy.data.collections.new('LG_MODELS_V2');bpy.context.scene.collection.children.link(collection)
        outputs=[];hashes=[]
        for i,spec in enumerate(args['assets']):
            src=Path(spec['blend_path']).resolve();src.relative_to(EXCHANGE.resolve());digest=hashlib.sha256(src.read_bytes()).hexdigest()
            with bpy.data.libraries.load(str(src),link=False) as (available,requested):
                assert spec['object_name'] in available.objects;requested.objects=[spec['object_name']]
            obj=requested.objects[0];collection.objects.link(obj);assert modeling._mesh_fingerprint(obj)==spec['expected_fingerprint'],'Stale assembly source'
            obj.data=obj.data.copy();obj.name=spec['display_name'];obj.hide_viewport=False;obj.hide_render=False;obj.hide_set(False)
            obj.location=((i%3)*8,(i//3)*8,0) if args['layout']=='grid' else (0,0,0)
            outputs.append({'name':obj.name,'data_name':obj.data.name,'source_object':spec['object_name'],'independent_data':True,'location':list(obj.location),'dimensions':list(obj.dimensions)})
            assert hashlib.sha256(src.read_bytes()).hexdigest()==digest;hashes.append({'path':str(src),'sha256':digest})
        bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
        dest=Path(args['output_blend']).resolve();dest.relative_to(OUTPUT.resolve());assert not dest.exists()
        select_only(requested.objects[0]);bpy.ops.wm.save_as_mainfile(filepath=str(dest),copy=True,check_existing=False)
        return {'output_blend':str(dest),'meshes':outputs,'source_files_preserved':hashes,'mesh_count':len(outputs),'layout':args['layout'],'collection':collection.name}
    path=Path(args['blend_path']).resolve();path.relative_to(EXCHANGE.resolve());assert path==Path(bpy.data.filepath).resolve()
    input_sha=hashlib.sha256(path.read_bytes()).hexdigest()
    source=modeling._exact_object(args['object_name'],'MESH');before=modeling._mesh_report(source);fingerprint=before['fingerprint']
    if args['command']=='inspect':return {**detailed(source,args),'input_sha256':input_sha,'uv_export_precision':export_precision_risks(source.data)}
    if args['command']=='compare':return {**surface_compare(source,modeling._exact_object(args['reference_object_name'],'MESH')),'input_sha256':input_sha}
    assert args['expected_fingerprint']==fingerprint,'Stale fingerprint'
    dest=Path(args['output_blend']).resolve();dest.relative_to(OUTPUT.resolve());assert not dest.exists(),'Output exists';assert args['output_object_name'] not in bpy.data.objects,'Output object exists'
    output=modeling._duplicate_mesh(source,args['output_object_name']);changes=mutate(output,args)
    after=modeling._mesh_report(output)
    if args['command']=='guarded_decimate':
        comparison=surface_compare(output,source)
        assert max(comparison[d]['max_distance_m'] for d in ('reference_to_output','output_to_reference'))<=args['max_surface_error_m'],('Surface-error budget exceeded',comparison)
        drift=max(abs(after['bounds'][kind][k]-before['bounds'][kind][k]) for kind in ('world_min','world_max') for k in range(3))
        assert drift<=args['max_bounds_drift_m'],('Bounds drift exceeded',drift)
        assert after['geometry']['triangles']<before['geometry']['triangles'],'No reduction'
        changes.update(surface_comparison=comparison,bounds_drift_m=drift)
    assert after['geometry']['non_finite_vertices']==0 and after['geometry']['invalid_face_normals']==0,'Invalid output coordinates or normals'
    assert after['geometry']['degenerate_faces']<=before['geometry']['degenerate_faces'],'Introduced degenerate geometry'
    assert modeling._mesh_fingerprint(source)==fingerprint,'Source object changed'
    assert hashlib.sha256(path.read_bytes()).hexdigest()==input_sha,'Input file changed'
    select_only(output)
    for ob in bpy.context.scene.objects:
        if ob.type=='MESH':ob.hide_set(ob!=output);ob.hide_render=ob!=output
    dest.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(dest),copy=True,check_existing=False)
    return {'source_preserved':True,'input_sha256':input_sha,'source':before,'output':after,'changes':changes,'output_blend':str(dest),'blender_version':bpy.app.version_string,'precision_mcp_version':VERSION,'status':'CANDIDATE_REQUIRES_REINSPECTION'}

if __name__=='__main__':
    request=Path(sys.argv[sys.argv.index('--')+1]).resolve();request.relative_to(STATE.resolve());assert request.name=='request.json'
    result_path=request.parent/'result.json';assert not result_path.exists()
    try:payload={'state':'finished','success':True,'result':run(json.loads(request.read_text(encoding='utf-8')))}
    except Exception as error:payload={'state':'failed','success':False,'error':str(error),'traceback':traceback.format_exc()}
    temp=result_path.with_suffix('.tmp');temp.write_text(json.dumps(payload,indent=2),encoding='utf-8');temp.replace(result_path)
