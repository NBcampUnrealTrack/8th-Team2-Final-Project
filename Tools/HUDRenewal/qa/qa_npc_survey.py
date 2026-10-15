# -*- coding: utf-8 -*-
"""Read-only NPC survey in play: for the player and every NPC, the sprite's scale, art size, world texel size, height on screen,
screen pixels per art pixel, material, and the head marker place. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal


def sprite_info(comp):
    fb = comp.get_flipbook()
    if fb is None:
        return None
    key = fb.get_sprite_at_frame(0) if fb.get_num_frames() > 0 else None
    ppu = key.get_editor_property('pixels_per_unreal_unit') if key else 0.0
    dim = key.get_editor_property('source_dimension') if key else None
    return fb, key, ppu, dim


def screen_scale(loc):
    """Screen pixels per world cm at this place (vertical), from projecting two points 50 cm apart."""
    a = unreal.GameplayStatics.project_world_to_screen(PC, loc, False)
    b = unreal.GameplayStatics.project_world_to_screen(PC, loc + unreal.Vector(0, 0, 50), False)
    if isinstance(a, tuple):
        a, b = a[-1], b[-1]
    if a is None or b is None:
        return 0.0
    return abs(a.y - b.y) / 50.0


def report(tag, actor, comp):
    info = sprite_info(comp)
    if info is None:
        log('NPC %s %s: no flipbook' % (tag, actor.get_name()))
        return
    fb, key, ppu, dim = info
    scale = comp.get_world_scale()
    texel_cm = (1.0 / ppu) * scale.z if ppu else 0.0
    height = (dim.y / ppu * scale.z) if (dim and ppu) else 0.0
    px_per_cm = screen_scale(actor.get_actor_location())
    mat = comp.get_material(0)
    extra = ''
    if isinstance(mat, unreal.MaterialInstanceDynamic):
        extra = ' gamma=%.2f gain=%.2f aa=%.2f' % (mat.get_scalar_parameter_value('ToneGamma'), mat.get_scalar_parameter_value('ToneGain'),
                                                    mat.get_scalar_parameter_value('NUA_PixelAA'))
    log('NPC %s %-34s fb=%-34s ppu=%.3f dim=%s scale=(%.3f,%.3f,%.3f) texel=%.2fcm h=%.0fcm px/texel=%.2f mat=%s cls=%s' % (
        tag, actor.get_name(), fb.get_name(), ppu, '%dx%d' % (dim.x, dim.y) if dim else '-', scale.x, scale.y, scale.z,
        texel_cm, height, px_per_cm * texel_cm, (mat.get_name() if mat else 'None') + extra, actor.get_class().get_name()))


pawn = PC.get_controlled_pawn()
for comp in pawn.get_components_by_class(unreal.PaperFlipbookComponent):
    report('PLAYER', pawn, comp)
npc_class = unreal.load_class(None, '/Script/ProjectM.NUNPCBase')
n = 0
for actor in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.Actor):
    if actor == pawn:
        continue
    comps = [c for c in actor.get_components_by_class(unreal.PaperFlipbookComponent) if c.is_visible()]
    if comps:
        report('npc' if isinstance(actor, unreal.NUNPCBase) else 'other', actor, comps[0])
        for c in comps[:4]:
            fb = c.get_flipbook()
            key = fb.get_sprite_at_frame(0) if fb and fb.get_num_frames() > 0 else None
            tex = key.get_editor_property('source_texture') if key else None
            log('   comp %s stencil=%s cd=%s tex=%s' % (c.get_name(), c.get_editor_property('custom_depth_stencil_value'),
                c.get_editor_property('render_custom_depth'), tex.get_path_name() if tex else None))
        n += 1
log('NPC survey %d' % n)
