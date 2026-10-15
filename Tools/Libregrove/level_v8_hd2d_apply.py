"""Build isolated material instances, then assign only current V8 actors/foliage."""
import unreal,json,hashlib,re
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908/HD2D'
BASE='/Game/Libregrove/Level_V8/HD2D';A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
STAGE=globals().get('STAGE','instances')
def save(name,value):
    with (OUT/name).open('x',encoding='utf8') as f:json.dump(value,f,indent=2)
def make(r):
    root=r['root'];kind={'M_VillagePixelLandscape2':'Landscape','M_PixelPropWA2':'WorldProps','M_HD2D_Water':'Water','M_HD2D_Sky':'Sky','M_LG8_Waterfall':'Waterfall'}.get(root.rsplit('.',1)[-1],'Surface')
    if '/Textured_V7/' in r['path']:label=r['path'].split('/Models/')[1].split('/')[0]
    elif '/Cindehearth/' in r['path']:label=r['path'].split('/Models/')[1].split('/')[0]
    else:label=r['path'].rsplit('.',1)[-1].removeprefix('MI_').removeprefix('M_')
    name='MI_HD2D_'+label+'_'+hashlib.sha1(r['path'].encode()).hexdigest()[:5]
    path=BASE+'/Instances/'+name
    if A.does_asset_exist(path):
        assert path==BASE+'/Instances/MI_HD2D_LG8_Sky_Day_17e60','Unexpected existing instance '+path
        mi=unreal.load_asset(path) # Inspected partial first instance from interrupted setup.
    else:mi=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE+'/Instances',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    parent=unreal.load_asset(BASE+'/M_LG8_HD2D_'+kind);E.set_material_instance_parent(mi,parent)
    scalars={};vectors={};textures={};normal=False
    if kind!='Surface':scalars=r['scalars'];vectors=r['vectors'];textures=r['textures']
    else:
        scalars={'PixelGrid':256.,'Roughness':.87,'Metallic':0.}
        vectors={'MaterialTint':[1,1,1,1]}
        if '/Textured_V7/' in r['path']:
            textures={'BaseColorTexture':'/Game/Libregrove/Textured_V7/Models/'+label+'/Textures/'+label+'_fbx_basecolor'}
            if 'Tree' in label:scalars['PixelGrid']=256.;scalars['Roughness']=.95
        else:
            assert r['vectors'].get('BaseColorTexture_OffsetScale',[0,0,1,1])==[0,0,1,1],r['path']
            assert abs(r['scalars'].get('BaseColorTexture_Rotation',0))<.0001
            textures={'BaseColorTexture':r['textures']['BaseColorTexture'],'NormalTexture':r['textures']['NormalTexture']}
            vectors['MaterialTint']=r['vectors'].get('BaseColorFactor',[1,1,1,1])
            normal=False # A lightweight, painterly default; users can opt into existing maps per MI.
            scalars['PixelGrid']=128.
            if label in ['Bell','Anvil','ToolRack']:scalars.update({'Roughness':.67,'Metallic':.25})
    for k,v in scalars.items():
        E.set_material_instance_scalar_parameter_value(mi,k,float(v));assert abs(E.get_material_instance_scalar_parameter_value(mi,k)-v)<.001,(path,k)
    for k,v in vectors.items():
        E.set_material_instance_vector_parameter_value(mi,k,unreal.LinearColor(*v));got=E.get_material_instance_vector_parameter_value(mi,k);assert max(abs(a-b) for a,b in zip([got.r,got.g,got.b,got.a],v))<.0001,(path,k)
    for k,v in textures.items():
        if v:
            tex=unreal.load_asset(v);assert tex,(path,k,v)
            E.set_material_instance_texture_parameter_value(mi,k,tex);assert E.get_material_instance_texture_parameter_value(mi,k)==tex,(path,k)
    if kind=='Surface':
        E.set_material_instance_static_switch_parameter_value(mi,'UseNormalMap',normal);assert E.get_material_instance_static_switch_parameter_value(mi,'UseNormalMap')==normal
    assert A.save_asset(path)
    return {'source':r['path'],'instance':mi.get_path_name(),'kind':kind,'scalars':scalars,'vectors':vectors,'textures':textures}
if STAGE=='instances':
    records=json.loads((OUT/'source_materials.json').read_text(encoding='utf8'));result=[]
    for r in records:
        rec=make(r);result.append(rec);save('instance_'+str(len(result)).zfill(2)+'.json',rec)
    save('instance_mapping.json',result);print('Created',len(result),'isolated instances')
elif STAGE=='assign':
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Libregrove'
    mapping={r['source']:r['instance'] for r in json.loads((OUT/'instance_mapping.json').read_text(encoding='utf8'))}
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();targets=[];land=[];fol=[]
    for a in actors:
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            for slot in range(comp.get_num_materials()):
                m=comp.get_material(slot)
                if m and m.get_path_name() in mapping:targets.append((a,comp,slot,m.get_path_name(),mapping[m.get_path_name()]))
        if isinstance(a,unreal.Landscape):
            src=a.get_editor_property('landscape_material').get_path_name();assert src in mapping;land.append((a,src,mapping[src]))
    for path in A.list_assets('/Game/Libregrove/Level_V8/Foliage',True,False):
        ft=unreal.load_asset(path)
        if not isinstance(ft,unreal.FoliageType_InstancedStaticMesh):continue
        mesh=ft.get_editor_property('mesh');over=list(ft.get_editor_property('override_materials'));new=[]
        for i,slot in enumerate(mesh.get_editor_property('static_materials')):
            src=(over[i] if i<len(over) and over[i] else slot.material_interface).get_path_name();assert src in mapping
            new.append(mapping[src])
        fol.append((ft,[m.get_path_name() if m else None for m in over],new))
    save('assignment_backup.json',{'components':[{'actor':a.get_path_name(),'label':a.get_actor_label(),'component':comp.get_name(),'slot':i,'old':old,'new':new} for a,comp,i,old,new in targets],'landscape':[{'actor':a.get_path_name(),'old':old,'new':new} for a,old,new in land],'foliage':[{'path':ft.get_path_name(),'old':old,'new':new} for ft,old,new in fol]})
    for a,comp,i,old,new in targets:comp.set_material(i,unreal.load_asset(new));assert comp.get_material(i).get_path_name()==new
    for ft,old,new in fol:ft.set_editor_property('override_materials',[unreal.load_asset(m) for m in new]);assert A.save_loaded_asset(ft)
    for a,old,new in land:a.set_editor_property('landscape_material',unreal.load_asset(new))
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    save('assignment_result.json',{'component_slots':len(targets),'foliage_types':len(fol),'landscapes':len(land),'original_mesh_assets_modified':False})
    print('Assigned slots',len(targets),'foliage types',len(fol),'landscapes',len(land))
else:raise RuntimeError(STAGE)
