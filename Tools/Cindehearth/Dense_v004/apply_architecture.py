import unreal,json,math,sys,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';out=root/'verification'
sys.path.insert(0,str(root.parent/'Polish_v003'));import ue_common as C;importlib.reload(C)
assert not unreal.WidgetService.is_pie_running();assert not (out/'architecture_applied.json').exists()
S=unreal.MaterialService;L=unreal.EditorAssetLibrary;meshes={};mats={}
for key in ['Tavern','TavernWalls','Smithy','SmithyWalls','SmithyStructure','SmithyRoof']:
 r=json.loads((out/('import_'+key+'.json')).read_text());assert r['status']=='finished';meshes[key]=unreal.load_asset(r['meshes'][0]['path'])
 if key not in ('Tavern','Smithy'):continue
 color=next(p for p in r['assets'] if p.endswith('/Color.Color'));normal=next(p for p in r['assets'] if p.endswith('/Normal.Normal'))
 tex=unreal.load_asset(color);tex.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST);L.save_loaded_asset(tex)
 dest='/Game/Cindehearth/Dense_v004/Materials';mi=dest+'/MI_CH_'+key
 if not L.does_asset_exist(mi):assert S.create_instance('/Game/Cindehearth/Tripo/GormHouse_P2_v001/MI_CH_GormP2_Pixel','MI_CH_'+key,dest).success
 assert S.set_instance_texture_parameter(mi,'Albedo',color);assert S.set_instance_texture_parameter(mi,'NormalMap',normal)
 for p,v in [('PixelGrid',256),('NormalStrength',.015),('Roughness',.97),('AmbientLift',.03)]:assert S.set_instance_scalar_parameter(mi,p,v)
 assert S.set_instance_vector_parameter(mi,'Tint',1.13,1.12,1.06);assert S.save_instance(mi);mats[key]=unreal.load_asset(mi)
stone=unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/MI_CH_Stone')

def fit(key,p,size,yaw):
 b=meshes[key].get_bounding_box();lo=b.min.to_tuple();hi=b.max.to_tuple();s=[size[i]/(hi[i]-lo[i]) for i in range(3)]
 c=[(lo[i]+hi[i])*.5*s[i] for i in range(2)];a=math.radians(yaw)
 loc=[p[0]-c[0]*math.cos(a)+c[1]*math.sin(a),p[1]-c[0]*math.sin(a)-c[1]*math.cos(a),p[2]-lo[2]*s[2]]
 return loc,s
def put(label,key,loc,scale,mat,roof=False):
 assert label not in C.ACTORS
 a=C.AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(yaw=-90));a.set_actor_label(label);a.set_folder_path('Cindehearth/Dense_v004/Architecture')
 a.tags=['Cindehearth','CH.HD2D','CH.Dense_v004']+(['CH.InteriorRoof'] if roof else [])
 c=a.static_mesh_component;c.set_static_mesh(meshes[key]);a.set_actor_scale3d(unreal.Vector(*scale))
 for i in range(c.get_num_materials()):c.set_material(i,mat)
 c.set_collision_profile_name('NoCollision' if roof else 'BlockAll');C.ACTORS[label]=a
 return dict(label=label,key=key,location=loc,scale=scale,bounds=[list(v.to_tuple()) for v in a.get_actor_bounds(False)])
hidden=[];placed=[]
with unreal.ScopedEditorTransaction('Replace TavernStone and smithy with complete repaired P2 buildings'):
 for name,key,p,size in [('FreyaTavern','Tavern',[-126,-1718.55,0],[400,560,440]),('DorikSmithy','Smithy',[-92.7,715,157.5],[350,470,330]),('UnloadingShelter','Smithy',[73.8,1869.3,315],[350,470,330])]:
  for n,a in list(C.ACTORS.items()):
   if n.startswith('CH_'+name+'_') and isinstance(a,unreal.StaticMeshActor):C.hide(a);hidden.append(n)
  loc,s=fit(key,p,size,-90)
  if key=='Tavern':placed.append(put('CH_D4_'+name,key,loc,s,mats[key]))
  else:
   placed.append(put('CH_D4_'+name,'SmithyStructure',loc,s,mats[key]));placed.append(put('CH_D4_'+name+'_Roof','SmithyRoof',loc,s,mats[key],True))
  placed.append(put('CH_D4_'+name+'_Walls',key+'Walls',loc,s,stone))
 # Old nameplates sit in the replacement facade; keep them as small legible outside signs.
 for label,p in [('CH_Lettering_Tavern',[-126,-1935,123]),('CH_Lettering_Smithy',[-92.7,527,282])]:
  a=C.ACTORS[label];a.modify();a.set_actor_location(unreal.Vector(*p),False,True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'architecture_applied.json').write_text(json.dumps(dict(hidden=hidden,placed=placed),indent=2),encoding='utf-8')
print('FULL BUILDINGS REPLACED3; hidden',len(hidden),'newparts',len(placed))
