"""Level-scoped playtest pawn/camera; original project Blueprints remain unchanged."""
import unreal,json,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908'
BASE='/Game/Libregrove/Level_V8/Gameplay';B=unreal.BlueprintService;A=unreal.EditorAssetLibrary
srcpawn='/Game/Developers/dongh/Field/BP_TestFieldCharacter';srcmode='/Game/Developers/dongh/GameMode/BP_TestGamemode'
pawn=BASE+'/BP_LG8_FieldCharacter';mode=BASE+'/BP_LG8_GameMode'
original={}
for src,dst in [(srcpawn,pawn),(srcmode,mode)]:
    assert not A.does_asset_exist(dst)
    file=ROOT/'Content'/Path(src.removeprefix('/Game/')+'.uasset')
    original[src]=hashlib.sha256(file.read_bytes()).hexdigest()
    assert A.duplicate_asset(src,dst)
for comp,key,value in [('SpringArm','RelativeRotation','(Pitch=-30,Yaw=90,Roll=0)'),('SpringArm','TargetArmLength','1900'),('SpringArm','bAbsoluteRotation','true'),('SpringArm','bDoCollisionTest','false'),('SpringArm','bUsePawnControlRotation','false'),('SpringArm','bEnableCameraLag','false'),('Camera','FieldOfView','38')]:
    assert B.set_component_property(pawn,comp,key,value),(comp,key)
assert B.compile_blueprint(pawn)
gc=A.load_blueprint_class(pawn)
assert B.set_property(mode,'DefaultPawnClass',gc.get_path_name())
assert B.compile_blueprint(mode)
assert A.save_asset(pawn);assert A.save_asset(mode)
for src,expected in original.items():
    f=ROOT/'Content'/Path(src.removeprefix('/Game/')+'.uasset')
    assert hashlib.sha256(f.read_bytes()).hexdigest()==expected
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Libregrove'
world.get_world_settings().set_editor_property('default_game_mode',A.load_blueprint_class(mode))
world.get_world_settings().set_editor_property('kill_z',-3000)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
cdo=unreal.get_default_object(gc);arm=cdo.get_component_by_class(unreal.SpringArmComponent);cam=cdo.get_component_by_class(unreal.CameraComponent)
result={'original_hashes':original,'pawn':pawn,'game_mode':mode,'arm_rotation':arm.get_editor_property('relative_rotation').to_tuple(),'arm_length':arm.target_arm_length,'fov':cam.field_of_view,'absolute_rotation':arm.absolute_rotation,'collision_test':arm.do_collision_test}
assert abs(result['arm_length']-1900)<.1 and abs(result['fov']-38)<.01
with (OUT/'gameplay_setup.json').open('x',encoding='utf8') as f:json.dump(result,f,indent=2)
print(json.dumps(result))
