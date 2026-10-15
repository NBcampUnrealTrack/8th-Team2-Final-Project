"""Recess two decorative ridge pieces below the depot sightline.

Default STAGE='review' records the proposal. Root executes STAGE='apply' only
after stopping PIE and reviewing the two affected labels. No assets are created.
"""
import json
from pathlib import Path
import unreal

STAGE = globals().get('STAGE', 'review')
ROOT = Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
assert not unreal.WidgetService.is_pie_running(), 'Stop PIE before reviewing/applying saved editor positions'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/Game/Maps/L_Cindehearth.')
actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
targets = ('CH_MainMineRockBrow_P3', 'CH_RidgeMass_15_HD2D_P3')
protected = ('CH_DeepveinTunnel_Path', 'CH_RingDepotFloor_Path', 'CH_P3_DepotFloor',
             'CH_DeepveinTunnel_Rock', 'CH_RidgeMass_15', 'CH_RidgeMass_15_HD2D')


def state(actor):
    rotation = actor.get_actor_rotation()
    return dict(location=list(actor.get_actor_location().to_tuple()),
                scale=list(actor.get_actor_scale3d().to_tuple()),
                rotation=[rotation.roll,rotation.pitch,rotation.yaw],
                hidden=actor.get_editor_property('hidden'),
                collision=actor.get_actor_enable_collision(),
                mesh=actor.static_mesh_component.static_mesh.get_path_name(),
                component_visible=actor.static_mesh_component.is_visible())


unchanged = {label: state(actors[label]) for label in protected}
plan = []
for label in targets:
    actor = actors[label]
    component = actor.static_mesh_component
    assert str(component.get_collision_profile_name()) == 'NoCollision', label
    assert component.is_visible() and not actor.get_editor_property('hidden'), 'Do not alter an already-hidden source: '+label
    assert '/Polish_v003/Models/Ridge/' in component.static_mesh.get_path_name(), label
    center, extent = actor.get_actor_bounds(False)
    before = actor.get_actor_location()
    # Lowest nearby tunnel/depot walking surface is135cm. Keep the decorative
    # ridge top60cm below it; the existing backfill still closes terrain gaps.
    old_top = center.z+extent.z
    assert old_top > 200, 'Already recessed or unexpected bounds: '+label
    new_z = before.z + 75-old_top
    plan.append(dict(label=label, before=list(before.to_tuple()),
                     after=[before.x,before.y,new_z], old_bounds_top=old_top,
                     proposed_bounds_top=75, collision_profile='NoCollision',
                     reason='Foreground decorative ridge crosses depot camera line; place below walking surface'))

report = dict(status='proposal', camera_source='verification/player_captures_all.json:depot',
              target_count=2, placements=plan, protected_labels=list(protected),
              no_collision_floors_changed=True, no_hidden_source_reenabled=True)
review = ROOT/'verification/depot_view_proposal.json'
review.write_text(json.dumps(report,indent=2),encoding='utf-8')
if STAGE == 'apply':
    receipt = ROOT/'verification/depot_view_fixed.json'
    assert not receipt.exists(), 'Inspect the prior receipt before reapplying'
    with unreal.ScopedEditorTransaction('Recess decorative foreground mine ridges for depot camera'):
        for row in plan:
            actor = actors[row['label']]
            actor.modify()
            actor.set_actor_location(unreal.Vector(*row['after']),False,True)
            center,extent = actor.get_actor_bounds(False)
            assert abs(center.z+extent.z-75) < .1, row['label']
        assert {label:state(actors[label]) for label in protected} == unchanged
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    report['status'] = 'applied_pending_visual_retake'
    receipt.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
