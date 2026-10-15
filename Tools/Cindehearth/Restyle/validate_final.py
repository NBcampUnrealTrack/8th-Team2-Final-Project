import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=AS.get_all_level_actors()
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),'PIE must be stopped'
assert not any(a.get_actor_label().startswith('CH_TEMP_') for a in actors)
base='/Game/Cindehearth/HD2D_v002';L=unreal.EditorAssetLibrary
compiled=[];models=[]
for p in L.list_assets(base,True,False):
    a=unreal.load_asset(p)
    if isinstance(a,unreal.Material):
        # Revvy GetMaterialDiagnostics is NOT read-only: it calls ForceRecompileForRendering.
        # Avoid invoking that method in a hot scene: two final audits crashed in Renderer/D3D12.
        # Earlier compile receipts are retained; here verify only persisted asset loading.
        compiled.append(p)
for key in ['ClosedRoof','Rock','Cart','PineVolume']:
    receipt=json.loads((root/'verification'/('import_'+key+'.json')).read_text(encoding='utf-8'))
    m=unreal.load_asset(receipt['meshes'][0]['path']);assert m
    n=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_nanite_settings(m)
    assert not n.enabled
    models.append(dict(name=key,path=m.get_path_name(),triangles=m.get_num_triangles(0),nanite=False))
pine=models[-1]['path'];old=json.loads((root/'verification/import_Pine.json').read_text(encoding='utf-8'))['meshes'][0]['path']
trees=[];oldrefs=[];fire=[];smoke=[]
for a in actors:
    if not isinstance(a,unreal.StaticMeshActor):continue
    c=a.static_mesh_component;m=c.static_mesh
    if not m:continue
    if m.get_path_name()==old:oldrefs.append(a.get_actor_label())
    if m.get_path_name()==pine:
        s=a.get_actor_scale3d().to_tuple();assert max(s)-min(s)<1e-6
        assert str(c.get_collision_profile_name())=='NoCollision'
        trees.append(a.get_actor_label())
    if a.get_actor_label().startswith(('CH_HD2D_HearthFire','CH_HD2D_ForgeFire')):
        assert abs(abs(a.get_actor_rotation().roll)-90)<.01
        fire.append(a.get_actor_label())
    if '_HD2DSoot' in a.get_actor_label():smoke.append(a.get_actor_label())
assert len(trees)==95 and not oldrefs,(len(trees),oldrefs)
assert len(fire)==5
# Read back the saved map after restart; no render-state mutation or save inside this audit.
d=next(a for a in actors if isinstance(a,unreal.NUCindehearthDirector))
assert d.get_editor_property('completed_chapter_quest')==0
assert not d.get_editor_property('night')
report=dict(level='/Game/Maps/L_Cindehearth',pie=False,chapter=0,night=False,loaded_materials=compiled,compilation_evidence='Earlier creation-time receipts; no forced recompilation during reload audit',models=models,volumetric_tree_count=len(trees),old_flat_tree_count=len(oldrefs),uniform_scale=True,fire_planes=len(fire),smoke_planes=len(smoke),temporary_capture_actors=0,saved=True)
(root/'verification/final_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
