"""Apply the three requested menu-hub visual changes in the Unreal editor.

Import the four ArtSource/UI/GameMenuHub PNGs before running this script.
The menu registry, profile widgets, five-column TileView and close control are retained.
"""
import json
from pathlib import Path
import unreal

MAIN = '/Game/Developers/dbals/UI/WBP_GameMenu'
TILE = '/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuTileEntry'
SYSTEM = '/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuSystem'
TEXTURES = '/Game/Developers/dbals/UI/GameMenuHub/Textures/'
OUT = Path(unreal.Paths.project_saved_dir()) / 'UIMenuHubReference'
OUT.mkdir(parents=True, exist_ok=True)
W, B, E = unreal.WidgetService, unreal.BlueprintService, unreal.EditorAssetLibrary


def tree(path):
    bp = unreal.load_asset(path)
    bp.modify()
    result = unreal.find_object(bp, 'WidgetTree')
    result.modify()
    return bp, result


def widget(wtree, name):
    result = unreal.find_object(wtree, name)
    assert result, name
    result.modify()
    return result


def rect(control, x, y, width, height):
    control.slot.modify()
    control.slot.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(0, 0)))
    control.slot.set_alignment(unreal.Vector2D(0, 0))
    control.slot.set_position(unreal.Vector2D(x, y))
    control.slot.set_size(unreal.Vector2D(width, height))


def apply_layout(wt):
    """Center both button groups in the ivory panel and reserve footer padding.

    Coordinates use the existing 1672x941 design canvas. Entry dimensions include
    spacing: 208x208 leaves a 200x200 button, with identical 8px gaps on both axes.
    CenterAligned also centers a partially filled final row.
    """
    center_x = 1012
    tiles = widget(wt, 'MainMenuTileView')
    rect(tiles, center_x - 520, 271, 1040, 416)
    tiles.set_editor_property('tile_alignment', unreal.ListItemAlignment.CENTER_ALIGNED)
    tiles.set_editor_property('bEntrySizeIncludesEntrySpacing', True)
    tiles.set_editor_property('entry_width', 208.0)
    tiles.set_editor_property('entry_height', 208.0)
    tiles.set_editor_property('horizontal_entry_spacing', 8.0)
    tiles.set_editor_property('vertical_entry_spacing', 8.0)

    # Balance the footer band: ~22px above and below the 56px button row.
    rect(widget(wt, 'HubFooterRule'), center_x - 548, 692, 1096, 1)
    for index, name in enumerate(['OptionsButton', 'CharacterSelectButton', 'QuitGameButton']):
        rect(widget(wt, name), center_x + (index - 1) * 120 - 36, 714, 72, 56)
    for index, name in enumerate(['HubFooterSeparator0', 'HubFooterSeparator1']):
        rect(widget(wt, name), center_x + (index - 0.5) * 120 - 0.5, 729, 1, 26)


def foreground():
    return unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND)


def icon_button(button):
    style = button.get_editor_property('widget_style')
    colors = {
        'normal': (.012, .031, .057, 1),
        'hovered': (.52, .31, .085, 1),
        'pressed': (.24, .12, .025, 1),
        'disabled': (.12, .14, .16, .4),
    }
    for state, color in colors.items():
        brush = unreal.SlateBrush()
        brush.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
        setattr(style, state, brush)
        setattr(style, state + '_foreground', unreal.SlateColor(specified_color=unreal.LinearColor(*color)))
    style.normal_padding = unreal.Margin(0, 0, 0, 0)
    style.pressed_padding = unreal.Margin(0, 1, 0, -1)
    button.set_style(style)


def inherit_icon_color(image):
    brush = image.get_editor_property('brush')
    brush.tint_color = foreground()
    image.set_brush(brush)
    image.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))


def configure_texture(name, background=False):
    texture = unreal.load_asset(TEXTURES + name)
    assert texture, name
    texture.modify()
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('filter', unreal.TextureFilter.TF_TRILINEAR)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property('max_texture_size', 2048 if background else 256)
    if not background:
        texture.set_editor_property('resize_during_build_x', 256)
        texture.set_editor_property('resize_during_build_y', 256)
        # Keep the clean source alpha; UMG supplies the actual navy/gold foreground.
        texture.set_editor_property('adjust_saturation', 0.0)
        texture.set_editor_property('adjust_brightness', 100.0)
    E.save_asset(texture.get_path_name(), False)
    return texture


def apply():
    before_graphs = {p: B.describe_graph(p, 'EventGraph')[0].export_text() for p in [MAIN, TILE, SYSTEM]}
    textures = {n: configure_texture(n, n.endswith('GoldPattern')) for n in [
        'T_GameMenuHub_GoldPattern', 'T_GameMenu_Settings', 'T_GameMenu_CharacterChange', 'T_GameMenu_Quit']}

    # The entry's existing icon, stat glyph and label retain their current placement.
    bp, wt = tree(TILE)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.modify()
    cdo.set_editor_property('bIconOnly', True)
    icon_button(widget(wt, 'Btn_Common'))
    for name in ['Img_Icon', 'HubStatBar1', 'HubStatBar2', 'HubStatBar3', 'HubStatBaseline']:
        inherit_icon_color(widget(wt, name))
    # The native tile restores this after the common button applies LabelColor.
    widget(wt, 'Txt_Label').set_color_and_opacity(foreground())
    # With no enclosing card, keep each live shortcut alongside its label.
    rect(widget(wt, 'ShortcutKeycap'), 84, 179, 32, 22)
    result = B.compile_blueprint(TILE)
    assert result.success, result.export_text()
    E.save_asset(TILE, False)

    # System controls already use the common button's optional IconTexture contract.
    if not any(w.widget_name == 'Img_Icon' for w in W.get_hierarchy(SYSTEM)):
        assert W.add_component(SYSTEM, 'Image', 'Img_Icon', 'ButtonCanvas', False)
    bp, wt = tree(SYSTEM)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.modify()
    cdo.set_editor_property('bIconOnly', True)
    cdo.set_editor_property('IconSize', unreal.Vector2D(56, 56))
    icon_button(widget(wt, 'Btn_Common'))
    icon = widget(wt, 'Img_Icon')
    rect(icon, 8, 0, 56, 56)
    icon.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    inherit_icon_color(icon)
    # Keep the existing label/shortcut bindings; the footer communicates via icons and tooltips.
    widget(wt, 'Txt_Label').set_render_opacity(0)
    widget(wt, 'ShortcutKeycap').set_render_opacity(0)
    result = B.compile_blueprint(SYSTEM)
    assert result.success, result.export_text()
    E.save_asset(SYSTEM, False)

    bp, wt = tree(MAIN)
    widget(wt, 'HubBackplate').set_brush_from_texture(textures['T_GameMenuHub_GoldPattern'], False)
    systems = [
        ('OptionsButton', 'T_GameMenu_Settings', '설정'),
        ('CharacterSelectButton', 'T_GameMenu_CharacterChange', '캐릭터 변경'),
        ('QuitGameButton', 'T_GameMenu_Quit', '게임 종료'),
    ]
    for name, texture_name, tooltip in systems:
        button = widget(wt, name)
        button.set_editor_property('bIconOnly', True)
        button.set_editor_property('IconTexture', textures[texture_name])
        button.set_editor_property('IconSize', unreal.Vector2D(56, 56))
        button.set_editor_property('bOverrideButtonSize', False)
        button.set_tool_tip_text(tooltip)
    apply_layout(wt)
    result = B.compile_blueprint(MAIN)
    assert result.success, result.export_text()
    E.save_asset(MAIN, False)

    results = {}
    for path in [MAIN, TILE, SYSTEM]:
        assert B.describe_graph(path, 'EventGraph')[0].export_text() == before_graphs[path], path + ' graph changed'
        result = B.compile_blueprint(path)
        assert result.success, result.export_text()
        results[path] = result.export_text()
        snapshot = {s.widget_name: s.export_text() for s in W.get_widget_snapshot(path)}
        (OUT / (path.split('/')[-1] + '.after.json')).write_text(json.dumps(snapshot, ensure_ascii=False, indent=2), encoding='utf-8')
    (OUT / 'compile.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    print('Menu reference applied:', results)


if __name__ == '__main__':
    apply()
