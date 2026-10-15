# -*- coding: utf-8 -*-
"""Google login screen: /Game/Art/UI/Login/WBP_Login_Google (parent UNULoginWidget, C++ Core/NULoginWidget).

The title art and its places are the current login screen's (Developers/dongh/UI/Login/WBP_Login, read with dump_login.py):
background, golden tree, logo, under-bar and the framed menu box at the bottom. Inside the box: the Google sign-in button
(Google light theme: white pill, 4-colour G, dark label), the status line, and the developer name login (hidden in shipping).
The old WBP_Login is not touched; the game uses this one through DefaultGame.ini
[/Script/ProjectM.NUGoogleAuthSettings] LoginWidgetOverride (run.ps1 -Steps apply / revert).

Headless: Tools/GoogleLogin/run.ps1 -Steps login   (the widget is removed first and built from nothing)
"""
import importlib
import os
import sys
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(ROOT, 'Tools/CharacterCreateGlass'))
import ccg_kit
importlib.reload(ccg_kit)
from ccg_kit import (E, TOOLS, VIS, HA, VA, slate, vec2, nodraw, rbox, font, typeface, box, canvas, text, image, sized,  # noqa: F401
                     button, widget_blueprint, FONT_BOLD, FONT_SEMI)

LOGIN = '/Game/Art/UI/Login'
WIDGET = LOGIN + '/WBP_Login_Google'
ART = os.path.join(ROOT, 'ArtSource/UI/GoogleLogin')
TITLE = '/Game/Developers/MinkiCho/Art/Login/'
IVORY, GOLD, MUTED = 'F1EDE6', 'D6BE8C', 'AAB6BF'


def log(message):
    unreal.log('[LOGIN] ' + str(message))


def import_art():
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ART, 'T_Login_GoogleG.png')
    task.destination_path = LOGIN; task.destination_name = 'T_Login_GoogleG'
    task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(LOGIN + '/T_Login_GoogleG')
    assert texture
    texture.set_editor_properties({'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                                   'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE,
                                   'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI, 'never_stream': True, 'srgb': True})
    assert E.save_asset(LOGIN + '/T_Login_GoogleG', False)
    log('google G imported')


def picture(path, size=None, draw=unreal.SlateBrushDrawType.IMAGE, tint=('FFFFFF', 1.0)):
    asset = unreal.load_asset(path)
    assert asset, path
    brush = unreal.SlateBrush()
    brush.set_editor_property('resource_object', asset)
    brush.draw_as = draw
    if size:
        brush.image_size = vec2(*size)
    else:
        brush.image_size = vec2(asset.blueprint_get_size_x(), asset.blueprint_get_size_y())
    brush.tint_color = slate(*tint)
    return brush


def build():
    t = widget_blueprint(WIDGET, '/Script/ProjectM.NULoginWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)

    # ---- the title art, at the old screen's places (1080p canvas) ----
    bg = canvas(image(t, 'Background', 'Root', picture(TITLE + 'T_Title_Background')), (0, 0, 1, 1), (0, 0, 0, 0))
    canvas(image(t, 'IntroTree', 'Root', picture(TITLE + 'T_Title_Image', (1120, 698))), (0.5, 0, 0.5, 0), (0, 114, 1120, 698), (0.5, 0), z=5)
    canvas(image(t, 'IntroBar', 'Root', picture(TITLE + 'T_Title_Bar', (1368, 164))), (0.5, 0, 0.5, 0), (0, 580, 1368, 164), (0.5, 0), z=6)
    canvas(image(t, 'IntroTitle', 'Root', picture(TITLE + 'T_TitleText', (1480, 312))), (0.5, 0, 0.5, 0), (0, 420, 1480, 312), (0.5, 0), z=6)

    # ---- the framed menu box, bottom centre ----
    menu = canvas(t.add('Border', 'IntroMenu', 'Root', VIS.SELF_HIT_TEST_INVISIBLE), (0.5, 1, 0.5, 1), (0, 0, 506, 380), (0.5, 1), z=7)
    menu.set_brush(picture('/Game/Art/UI/T_Title_MenuBox', (506, 380)))
    body = t.add('VerticalBox', 'MenuBody', 'IntroMenu', VIS.SELF_HIT_TEST_INVISIBLE)
    # a Border's padding IS its child's slot padding: set it on the slot (Border.set_padding then box() would zero it)
    box(body, pad=(48, 78, 48, 50))

    # Google sign-in (light theme: white, #747775 hairline, #1F1F1F label, the 4-colour G)
    size = box(sized(t, 'GoogleLoginSize', 'MenuBody', None, 52))
    google = box(button(t, 'GoogleLoginButton', 'GoogleLoginSize',
                        rbox(('FFFFFF', 1.0), None, ('747775', 1.0), 1.0), rbox(('F2F2F2', 1.0), None, ('747775', 1.0), 1.0),
                        rbox(('E6E6E6', 1.0), None, ('747775', 1.0), 1.0), rbox(('FFFFFF', 0.55), None, ('747775', 0.55), 1.0)))
    row = box(t.add('HorizontalBox', 'GoogleRow', 'GoogleLoginButton', VIS.HIT_TEST_INVISIBLE), h='center', v='center')
    box(image(t, 'GoogleG', 'GoogleRow', picture(LOGIN + '/T_Login_GoogleG', (22, 22))), pad=(0, 0, 12, 0), v='center')
    box(text(t, 'GoogleLabel', 'GoogleRow', 'Google 계정으로 로그인', 17, True, ('1F1F1F', 1.0)), v='center')

    # status line + "cancel" while the browser is open
    status = box(t.add('Overlay', 'StatusRow', 'MenuBody', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 12, 0, 0))
    box(sized(t, 'StatusHeight', 'StatusRow', None, 26))
    box(text(t, 'StatusText', 'StatusRow', '구글 계정으로 로그인하세요', 15, False, (IVORY, 0.92)), h='center', v='center')
    cancel = box(button(t, 'GoogleCancelButton', 'StatusRow', rbox(('FFFFFF', 0.06), None, ('FFFFFF', 0.30), 1.0),
                        rbox(('FFFFFF', 0.14), None, ('FFFFFF', 0.45), 1.0), rbox(('FFFFFF', 0.2), None, ('FFFFFF', 0.5), 1.0), pad=(12, 1, 12, 2)),
                 h='right', v='center')
    box(text(t, 'GoogleCancelLabel', 'GoogleCancelButton', '취소', 13, True, (IVORY, 0.9)), h='center', v='center')
    cancel.set_visibility(VIS.COLLAPSED)
    hint = box(text(t, 'GoogleSetupHint', 'MenuBody', '구글 로그인 설정 전 (프로젝트 세팅 > NU Google Login > Client Id)', 12, False, (MUTED, 0.9)), pad=(0, 2, 0, 0), h='center')
    hint.set_visibility(VIS.COLLAPSED)

    # developer name login (C++ hides it in shipping builds)
    dev = box(t.add('VerticalBox', 'DevLoginBox', 'MenuBody', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 12, 0, 0))
    box(image(t, 'DevRule', 'DevLoginBox', rbox((GOLD, 0.30), 0, None, 0.0, (8, 1))), pad=(0, 0, 0, 8))
    box(text(t, 'DevCaption', 'DevLoginBox', '개발용 이름 접속', 12.5, True, (GOLD, 0.95), 80), pad=(2, 0, 0, 6), h='left')
    devrow = box(t.add('HorizontalBox', 'DevRow', 'DevLoginBox', VIS.SELF_HIT_TEST_INVISIBLE))
    inputbox = box(sized(t, 'UserIdSize', 'DevRow', None, 40), fill=1.0)
    field = box(t.add('Border', 'UserIdField', 'UserIdSize', VIS.SELF_HIT_TEST_INVISIBLE))
    field.set_brush(rbox(('0B1118', 0.85), 8, ('F1EDE6', 0.22), 1.0))
    field.set_padding(unreal.Margin(12, 0, 8, 0))
    field.set_vertical_alignment(VA['center'])
    user = t.add('EditableTextBox', 'UserIdInput', 'UserIdField')
    style = user.get_editor_property('widget_style').copy()
    for state in ('background_image_normal', 'background_image_hovered', 'background_image_focused', 'background_image_read_only'):
        style.set_editor_property(state, nodraw())
    info = unreal.SlateFontInfo()
    info.font_object, info.typeface_font_name = typeface(FONT_SEMI)
    info.size = 16 * 0.75
    text_style = style.get_editor_property('text_style')
    text_style.set_editor_property('font', info)
    text_style.set_editor_property('color_and_opacity', slate(IVORY))
    style.set_editor_property('text_style', text_style)
    for field_name, value in (('foreground_color', slate(IVORY)), ('focused_foreground_color', slate(IVORY)), ('padding', unreal.Margin(0, 0, 0, 0))):
        try:
            style.set_editor_property(field_name, value)
        except Exception as error:
            log('text box style.%s not set: %s' % (field_name, error))
    user.set_editor_property('widget_style', style)
    user.set_hint_text('영문·숫자·_ 2~64자')
    go_size = box(sized(t, 'DevLoginSize', 'DevRow', 104, 40), pad=(10, 0, 0, 0))
    go = box(button(t, 'DevLoginButton', 'DevLoginSize', picture('/Game/Developers/MinkiCho/Art/Button/T_Title_Button', (104, 40), unreal.SlateBrushDrawType.IMAGE),
                    picture('/Game/Developers/MinkiCho/Art/Button/T_Title_Button', (104, 40), unreal.SlateBrushDrawType.IMAGE, ('FFF6E0', 1.0)),
                    picture('/Game/Developers/MinkiCho/Art/Button/T_Title_Button', (104, 40), unreal.SlateBrushDrawType.IMAGE, ('D8C8A8', 1.0))))
    box(text(t, 'DevLoginLabel', 'DevLoginButton', '접속', 15, True, ('2A2014', 1.0)), h='center', v='center')
    t.finish()


def main():
    import_art()
    build()
    log('DONE')


main()
