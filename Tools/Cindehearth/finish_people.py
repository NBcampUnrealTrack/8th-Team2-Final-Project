import unreal
import math
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
residents=[a for a in AS.get_all_level_actors() if a.actor_has_tag('CH.Resident')]
for i,a in enumerate(residents):
    c=a.get_component_by_class(unreal.PaperSpriteComponent)
    assert c
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    if not c.get_sprite():
        sprite=unreal.load_asset('/Game/Art/Sprites/NPC/NPC'+str(1+i%8)+'_Sprite')
        assert isinstance(sprite,unreal.PaperSprite),sprite
        assert c.set_sprite(sprite),a.get_actor_label()
    assert c.get_material(0),a.get_actor_label()
    center,extent=a.get_actor_bounds(False)
    assert extent.z>0,a.get_actor_label()
    # Match the existing 88 cm playable character with a compact resident silhouette.
    a.set_actor_scale3d(unreal.Vector(80/(extent.z*2),80/(extent.z*2),80/(extent.z*2))*a.get_actor_scale3d())
    name=a.get_actor_label()
    if a.actor_has_tag('CH.Crowd'):
        n=int(name.rsplit('_',1)[1]);ang=math.radians([20,75,140,205,270][n//5])
        radius=8.6+(1.9 if n%5<3 else -1.9)
        along=[-1.6,0,1.6,-.8,.8][n%5]
        a.set_actor_location(unreal.Vector((-6+radius*math.cos(ang)-along*math.sin(ang))*45,(radius*math.sin(ang)+along*math.cos(ang))*45,a.get_actor_location().z),False,False)
        c.set_sprite_color(unreal.LinearColor(.85,.88,.90,1))
    floor=166.5 if a.actor_has_tag('CH.Crowd') else 315 if name=='CH_Gorm' else 157.5 if name in ('CH_Dorik','CH_YoungAssayer') else 0
    center,extent=a.get_actor_bounds(False)
    a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,floor-center.z+extent.z),False,False)
    a.set_actor_rotation(unreal.Rotator(yaw=0),False)
    c.set_collision_profile_name('NoCollision')
    print(name,c.get_sprite().get_name(),'HEIGHT',a.get_actor_bounds(False)[1].z*2)
LS.save_current_level()
print('VISIBLE SPRITES VALIDATED',len(residents))
