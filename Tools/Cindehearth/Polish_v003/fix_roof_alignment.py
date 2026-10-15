import unreal,json,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003'
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in AS.get_all_level_actors()}
dims={'Tavern':(14,10,6.6),'Smithy':(12,9,4.2),'Assay':(8,6,3.5),'Clinic':(6,5,3.3),'Office':(5,4,3.2)}
layout=json.loads((root.parent/'layout.json').read_text());changes=[]
for p in layout['placements']:
 if p['module'] not in dims:continue
 prefix='CH_'+p['name'];src=actors[prefix+'_Stone'];roof=actors[prefix+'_HD2DRoof'];w,d,h=dims[p['module']]
 b=roof.static_mesh_component.static_mesh.get_bounding_box();lo=b.min.to_tuple();hi=b.max.to_tuple()
 size=((w+1.3)*45,(d+1.3)*45,(w*.4+.2)*45);s=[size[i]/(hi[i]-lo[i]) for i in range(3)]
 yaw=src.get_actor_rotation().yaw;a=math.radians(yaw);o=src.get_actor_location().to_tuple();cx=(lo[0]+hi[0])*.5*s[0];cy=(lo[1]+hi[1])*.5*s[1]
 roof.set_actor_rotation(unreal.Rotator(yaw=yaw),False);roof.set_actor_scale3d(unreal.Vector(*s));roof.set_actor_location(unreal.Vector(o[0]-cx*math.cos(a)+cy*math.sin(a),o[1]-cx*math.sin(a)-cy*math.cos(a),o[2]+(h-.1)*45-lo[2]*s[2]),False,False)
 # Keep the correctly proportioned structural gable behind the P2 wall cladding.
 # It shares the old building's footprint and ensures no gaps below the roof.
 src.static_mesh_component.set_visibility(True,True);src.static_mesh_component.set_hidden_in_game(False,True);src.set_actor_hidden_in_game(False);src.set_is_temporarily_hidden_in_editor(False)
 nside=2*math.ceil(d/3.8)*math.ceil(h/2);nback=math.ceil(w/3.8)*math.ceil(h/2)
 nfront=0 if p['module']=='Smithy' else 2*math.ceil(((w-1.6)/2)/3.8)*math.ceil(h/2)+1
 start=nside+nback+nfront
 for i in range(start,start+16):
  g=actors[prefix+'_P3Wall_'+str(i)];g.static_mesh_component.set_visibility(False,True);g.static_mesh_component.set_hidden_in_game(True,True);g.set_actor_hidden_in_game(True);g.set_is_temporarily_hidden_in_editor(True)
 changes.append(dict(building=prefix,roof_yaw=yaw,hidden_course_range=[start,start+15]))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root/'verification/roof_alignment.json').write_text(json.dumps(changes,indent=2));print('ALIGNED',len(changes),'roofs with structural gables')
