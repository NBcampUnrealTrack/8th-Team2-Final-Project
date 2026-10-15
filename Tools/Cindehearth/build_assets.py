import unreal
import json
from pathlib import Path

ROOT = Path(unreal.Paths.project_dir())/'Tools/Cindehearth'
DATA = json.loads((ROOT/'layout.json').read_text(encoding='utf-8'))
DEST = '/Game/Cindehearth'
STAGE = globals().get('STAGE', 'materials')
svc = unreal.MaterialService
geo = unreal.GeometryScriptService
lib = unreal.EditorAssetLibrary

def material_path(key):
    return DEST+'/Materials/MI_CH_'+key

if STAGE == 'materials':
    settings = {
        'Stone':('Stone',(.55,.57,.59),90), 'StoneLight':('StoneSmooth',(.70,.70,.66),90),
        'Roof':('Roof',(.38,.42,.46),90), 'RoofPatch':('Roof',(.58,.56,.48),90),
        'Iron':('Metal',(.42,.46,.51),60), 'Brass':('Metal',(.92,.65,.30),60),
        'Timber':('Timber',(.42,.33,.24),60), 'Soil':('Soil',(.46,.46,.43),190),
        'Path':('Cobble',(.43,.43,.39),100), 'Rock':('Rock',(.53,.56,.59),220),
        'Cloth':('Cloth',(.42,.44,.46),60), 'Paper':('Cloth',(.78,.73,.61),60),
        'Ash':('Rock',(.63,.64,.61),90), 'Pine':('Leaf',(.21,.30,.26),180),
        'Window':('Glow',(.65,.37,.12),60),
    }
    for key,(source,tint,tile) in settings.items():
        p=material_path(key)
        if lib.does_asset_exist(p):continue
        parent='/Game/Village/Materials/Props/MI_Prop_'+source
        assert lib.does_asset_exist(parent),parent
        result=svc.create_instance(parent, 'MI_CH_'+key, DEST+'/Materials')
        assert result.success, str(result)
        assert svc.set_instance_vector_parameter(p,'Tint',*tint)
        assert svc.set_instance_scalar_parameter(p,'TileCm',tile)
        if key=='Window':
            svc.set_instance_vector_parameter(p,'Emissive',1,.40,.08)
            svc.set_instance_scalar_parameter(p,'EmissiveStrength',.6)
        svc.save_instance(p)
        print('CREATED',p)
    # Small constant-material master: exactly one own graph, inherited instances.
    master=DEST+'/Materials/M_CH_Solid'
    if not lib.does_asset_exist(master):
        result=svc.create_material('M_CH_Solid',DEST+'/Materials');assert result.success
        for typ,key,value,output,y in [('Vector','Color','.1,.1,.1,1','BaseColor',0),('Vector','Glow','0,0,0,1','EmissiveColor',160),('Scalar','Roughness','.9','Roughness',320)]:
            node=unreal.MaterialNodeService.create_parameter(master,typ,key,'Surface',value,-300,y)
            assert unreal.MaterialNodeService.connect_to_output(master,node.id,'',output)
        assert svc.compile_material(master)
        svc.save_material(master)
    for key,color,glow in [('Dark',(.008,.01,.013),(0,0,0)),('Flame',(.8,.13,.008),(7,1.4,.05)),
                           ('Ember',(.3,.025,.003),(2.4,.16,.004)),('Vein',(.05,.13,.18),(.10,.50,.80)),
                           ('NaiaGlow',(.24,.29,.29),(.25,.34,.34)),('Seal',(.25,.036,.012),(0,0,0))]:
        p=material_path(key)
        if lib.does_asset_exist(p):continue
        assert svc.create_instance(master,'MI_CH_'+key,DEST+'/Materials').success
        svc.set_instance_vector_parameter(p,'Color',*color)
        svc.set_instance_vector_parameter(p,'Glow',*glow)
        svc.save_instance(p)
    sky=DEST+'/Materials/MI_CH_Sky'
    if not lib.does_asset_exist(sky):
        assert svc.create_instance('/Game/Village/Materials/MI_HD2D_Sky','MI_CH_Sky',DEST+'/Materials').success
        for key,val in {'ZenithCol':(.20,.26,.34),'MidCol':(.43,.46,.49),'HorizonCol':(.65,.61,.54),
                        'GroundCol':(.03,.04,.05),'RidgeFar':(.40,.43,.48),'RidgeNear':(.20,.23,.27),
                        'CloudCol':(.69,.67,.60),'CloudShadow':(.34,.37,.42),'Tune':(.58,.46,.001)}.items():
            svc.set_instance_vector_parameter(sky,key,*val)
        svc.save_instance(sky)
    print('MATERIALS READY')

elif STAGE.startswith('meshes'):
    _,start,end=STAGE.split(':');start,end=int(start),int(end)
    specs=[(name,mat,items) for name,parts in DATA['modules'].items() for mat,items in parts.items()]
    for index,(name,mat,items) in enumerate(specs[start:end],start):
        p=DEST+'/Meshes/SM_CH_'+name+'_'+mat
        if lib.does_asset_exist(p):
            print('EXISTS',index,p);continue
        handle=geo.create_dynamic_mesh('CH_'+name+'_'+mat)
        assert handle
        try:
            for item in items:
                kind=item['kind'];size=item['size'];pos=unreal.Vector(*[v*100 for v in item['pos']])
                pitch,yaw,roll=item['rot'];rot=unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll)
                transform=unreal.Transform(location=pos, rotation=rot)
                if kind=='box':
                    transform.translation=pos-rot.get_up_vector()*(size[2]*50)
                    ok=geo.append_box(handle,transform,unreal.Vector(*[v*100 for v in size]),1,1,1)
                elif kind=='cylinder':
                    transform.translation=pos-rot.get_up_vector()*(size[1]*50)
                    ok=geo.append_cylinder(handle,transform,size[0]*100,size[1]*100,12,1)
                elif kind=='sphere':
                    transform.scale3d=unreal.Vector(*size)
                    ok=geo.append_sphere(handle,transform,100,8,10)
                elif kind=='torus':
                    ok=geo.append_torus(handle,transform,size[0]*100,size[1]*100,12,5)
                else:raise ValueError(kind)
                assert ok,(name,mat,kind)
            assert geo.convert_dynamic_mesh_to_static_mesh(handle,p),p
            mesh=unreal.load_asset(p)
            material=unreal.load_asset(material_path(mat));assert material
            mesh.set_editor_property('static_materials',[unreal.StaticMaterial(material_interface=material,material_slot_name=mat)])
            body=mesh.get_editor_property('body_setup')
            if body:
                body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            lib.save_asset(p)
            b=mesh.get_bounds()
            assert b.box_extent.length()>0,p
            print('MESH',index,p,len(items),'primitives')
        finally:
            geo.dispose_dynamic_mesh(handle)
    print('BATCH DONE',start,end,'of',len(specs))

elif STAGE=='verify':
    missing=[]
    for name,parts in DATA['modules'].items():
        for mat in parts:
            p=DEST+'/Meshes/SM_CH_'+name+'_'+mat
            sm=unreal.load_asset(p)
            if not sm or sm.get_bounds().box_extent.length()<.001 or not len(sm.static_materials):missing.append(p)
    print('ASSET AUDIT',json.dumps({'missing_or_empty':missing,'mesh_count':sum(len(x) for x in DATA['modules'].values())}))
    assert not missing
