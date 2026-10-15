"""Top-right profile anchoring and localized fades; center stays transparent."""
import importlib.util, json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
spec = importlib.util.spec_from_file_location('hub_interaction', ROOT/'Tools/UIHubSidePanels/apply_interaction.py')
i = importlib.util.module_from_spec(spec)
spec.loader.exec_module(i)
h, M = i.h, i.M
OUT = ROOT/'Saved/UIHubEdgeFade'
OUT.mkdir(exist_ok=True)

def fade(name, parent, rect, code):
    mat = h.mat('M_'+name)
    uv = M.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -400, 0)
    alpha = h.custom(mat, code, unreal.CustomMaterialOutputType.CMOT_FLOAT1, [('UV', uv, '')])
    rgb = h.custom(mat, 'return float3(0,0,0);', unreal.CustomMaterialOutputType.CMOT_FLOAT3, [])
    M.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    M.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    h.finishmat(mat)
    image = i.add('Image', name, parent)
    h.rect(image, *rect)
    image.set_brush_from_material(mat)
    image.slot.set_z_order(-5)
    image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

def apply():
    assert not h.W.is_pie_running()
    i.apply()
    bp, tree = h.tree(h.MAIN)
    size = h.obj(tree, 'HubDesignSize')
    size.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
    size.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
    # The reference's paper starts at source y=139. Move art and live values together.
    h.rect(h.obj(tree, 'HubBackplate'), 0, -139, 1672, 941)
    h.obj(tree, 'HubBackplate').slot.set_z_order(-1)
    for name in ['HubLevel', 'HubName', 'HubExperienceBar', 'HubExperience',
                 'HubLocation', 'HubJob', 'HubGold', 'HubCharacterId', 'HubMottoInput']:
        widget = h.obj(tree, name)
        pos = widget.slot.get_position()
        widget.slot.set_position(unreal.Vector2D(pos.x, pos.y-139))
    # Keep X on the paper, above the live value column.
    h.rect(h.obj(tree, 'HubCloseButton'), 1620, 10, 40, 40)
    h.obj(tree, 'HubCloseX').set_color_and_opacity(h.slate((.025,.022,.018)))
    h.obj(tree, 'HubCloseButton').slot.set_z_order(5)
    # Finite support: alpha reaches exactly zero before entering the clear center.
    fade('HubLeftEdgeFade', 'HubLeftCanvas', (0,0,620,899),
         'float edge=lerp(490,620,smoothstep(.79,.92,UV.y));'
         'return .55*(1-smoothstep(0,edge,UV.x*620));')
    fade('HubTopRightFade', 'HubCanvas', (900,0,772,385),
         'return .55*smoothstep(0,1,UV.x)*(1-smoothstep(0,1,UV.y));')
    fade('HubDeskEdgeFade', 'HubDeskCanvas', (920,390,752,551),
         'return .50*smoothstep(0,1,UV.x)*smoothstep(0,1,UV.y);')
    result = h.B.compile_blueprint(h.MAIN)
    assert result.success and result.num_errors == 0, result.export_text()
    assert h.E.save_asset(h.MAIN, False)
    (OUT/'applied.json').write_text(json.dumps({'errors':result.num_errors, 'warnings':result.num_warnings,
        'profile_offset_y':-139, 'profile_alignment':'right/top', 'center_fade_alpha':0}, indent=2))

if __name__ == '__main__':
    apply()
