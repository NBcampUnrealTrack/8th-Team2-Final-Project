# -*- coding: utf-8 -*-
"""Confirm dialogs in the glass-sheet style, for the creation and select screens. Run headless through run.ps1 -Steps dialog.

  /Game/Art/UI/CharacterCreate/Glass/Dialog/WBP_CCG_ConfirmDialog          ivory confirm button (create, keep going)
  /Game/Art/UI/CharacterCreate/Glass/Dialog/WBP_CCG_ConfirmDialog_Danger   red confirm button (delete)
  /Game/Art/UI/CharacterCreate/Glass/Dialog/WBP_CCG_DialogPrimary, WBP_CCG_DialogDanger   their confirm buttons

Both are children of UNUConfirmDialog, so callers do not change. The creation game modes' ConfirmDialogClass points at the ivory
one; the select screen (apply_select.py) uses the red one for deleting. The game-wide dialog (project settings
NU UI > ConfirmDialogClass, WBP_ConfirmDialog) is not touched.

The confirm button is a UNU_CommonButtonWidget on purpose: the C++ tints a plain UButton confirm button with the palette's
positive / destructive colour, which would turn the ivory button green.
"""
import importlib
import os
import shutil
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/CharacterCreateGlass'))
import ccg_kit
importlib.reload(ccg_kit)       # an editor session keeps modules between runs
from ccg_kit import *           # noqa: F401,F403  (paths, colours, brush and tree helpers)

DIALOG = BASE + '/Dialog'
PRIMARY = DIALOG + '/WBP_CCG_DialogPrimary'
DANGER = DIALOG + '/WBP_CCG_DialogDanger'
CONFIRM = DIALOG + '/WBP_CCG_ConfirmDialog'
CONFIRM_DANGER = DIALOG + '/WBP_CCG_ConfirmDialog_Danger'
CREATE_GAME_MODES = ['/Game/GameMode/BP_CharCreateGameMode', '/Game/Developers/dbals/BP_dbals_CharCreateGameMode']
CARD_W = 520
BUTTON_H = 56
RED = 'B4453C'


def cta(value, alpha=1.0):
    return tex('T_CCG_Cta', (96, 64), (1 / 3.0, 0, 1 / 3.0, 0), ('%02X%02X%02X' % (value, value, value), alpha))


def build_button(path, normal, hovered, pressed):
    """Only the click area and its look. The label is drawn by the dialog (ConfirmLabelText), over the button."""
    t = widget_blueprint(path, '/Script/ProjectM.NU_CommonButtonWidget')
    button(t, 'Btn_Common', '', normal, hovered, pressed, normal)
    t.finish()


def build_dialog(path, confirm_class, confirm_label_colour):
    t = widget_blueprint(path, '/Script/ProjectM.NUConfirmDialog')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    # dim everything behind (Visible: clicks must not reach the sheet under a modal)
    canvas(image(t, 'Dim', 'Root', flat('060A12', 0.55), VIS.VISIBLE), (0, 0, 1, 1), (0, 0, 0, 0))

    card = t.add('Overlay', 'Card', 'Root', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(card, (0.5, 0.5, 0.5, 0.5), (0, 0, CARD_W, 0), align=(0.5, 0.5), auto=True, z=1)
    t.add('SizeBox', 'CardWidth', 'Card')
    box(t.get('CardWidth')).set_width_override(CARD_W)
    t.add('Overlay', 'CardStack', 'CardWidth', VIS.SELF_HIT_TEST_INVISIBLE)
    box(image(t, 'CardShadow', 'CardStack', tex('T_CCG_SheetShadow', (520, 520), (190 / 520, 160 / 520, 190 / 520, 220 / 520))), pad=(-160, -130, -160, -190))
    blur = box(t.add('BackgroundBlur', 'CardBlur', 'CardStack', VIS.HIT_TEST_INVISIBLE), pad=(10, 10, 10, 10))   # square: see apply_ui
    blur.set_editor_property('blur_strength', 26.0)
    blur.set_editor_property('apply_alpha_to_blur', True)       # fades in with the card
    # a touch more opaque than the side sheet: it sits over the bright stage, and its text must read first
    box(image(t, 'CardGlass', 'CardStack', tex('T_CCG_Sheet', (128, 256), (0.25, 0.125, 0.25, 0.125), tint=('FFFFFF', 1.0))))
    box(image(t, 'CardTint', 'CardStack', rbox(('101724', 0.35), 30)))
    body = box(t.add('VerticalBox', 'CardBody', 'CardStack', VIS.SELF_HIT_TEST_INVISIBLE), pad=(33, 31, 33, 29))

    box(sized(t, 'CapLine', body.get_name(), height=20))
    box(text(t, 'Cap', 'CapLine', '확인', 15, True, (GOLD, 1.0), 160), h='left', v='center')
    message = box(text(t, 'MessageText', body.get_name(), '이대로 생성하시겠습니까?\n\n이름 : 레브\n종족 : 인간\n성별 : 남성\n\n이름과 종족은 생성 후 변경할 수 없습니다.',
                       18, colour=(IVORY, 0.92), wrap=True), pad=(0, 16, 0, 0), h='fill')
    message.set_editor_property('line_height_percentage', 1.25)
    box(image(t, 'Rule', body.get_name(), flat('FFFFFF', 0.10, (32, 1))), pad=(0, 24, 0, 22))

    row = box(t.add('HorizontalBox', 'Buttons', body.get_name(), VIS.SELF_HIT_TEST_INVISIBLE))
    # cancel / modify: a quiet ghost pill
    cancel_cell = box(sized(t, 'CancelBox', row.get_name(), height=BUTTON_H), pad=(0, 0, 6, 0), fill=1.0)
    cancel = box(button(t, 'CancelButton', cancel_cell.get_name(), ghost(0.06, 0.26), ghost(0.12, 0.36), ghost(0.18, 0.42)))
    box(text(t, 'CancelLabelText', cancel.get_name(), '취소', 19, colour=(IVORY, 0.9)), h='center', v='center')
    # confirm: the screen's primary look, label drawn over it
    confirm_cell = box(sized(t, 'ConfirmBox', row.get_name(), height=BUTTON_H), pad=(6, 0, 0, 0), fill=1.0)
    t.add('Overlay', 'ConfirmStack', confirm_cell.get_name(), VIS.SELF_HIT_TEST_INVISIBLE)
    box(t.add(confirm_class, 'ConfirmButton', 'ConfirmStack'))
    box(text(t, 'ConfirmLabelText', 'ConfirmStack', '생성하기', 19, True, confirm_label_colour), h='center', v='center')
    t.get('ConfirmLabelText').set_visibility(VIS.HIT_TEST_INVISIBLE)
    t.finish()
    unreal.get_default_object(t.bp.generated_class()).set_editor_property('play_open_motion', True)   # Dim fades, Card grows in
    t.finish()


def point_game_modes():
    target = unreal.load_class(None, CONFIRM + '.WBP_CCG_ConfirmDialog_C')
    assert target, 'dialog class missing'
    for path in CREATE_GAME_MODES:
        keep = os.path.join(BACKUP, path.strip('/').replace('/', '_') + '.uasset')
        if not os.path.exists(keep):
            os.makedirs(BACKUP, exist_ok=True)
            shutil.copy2(os.path.join(ROOT, path.replace('/Game/', 'Content/') + '.uasset'), keep)
        blueprint = unreal.load_asset(path)
        defaults = unreal.get_default_object(blueprint.generated_class())
        before = defaults.get_editor_property('confirm_dialog_class')
        blueprint.modify()
        defaults.set_editor_property('confirm_dialog_class', target)
        assert B.compile_blueprint(path).success, path
        assert E.save_asset(path, False), path
        log('%s confirm dialog: %s -> %s' % (path, before.get_name() if before else None, target.get_name()))


import_kit()
build_button(PRIMARY, cta(244), cta(255), cta(216))
build_button(DANGER, rbox((RED, 0.92)), rbox(('C4524A', 1.0)), rbox(('9C3A33', 1.0)))
build_dialog(CONFIRM, 'WBP_CCG_DialogPrimary', (NAVY, 1.0))
build_dialog(CONFIRM_DANGER, 'WBP_CCG_DialogDanger', (IVORY, 1.0))
if os.environ.get('CC_SKIP_GAMEMODES', '') != '1':
    point_game_modes()
log('DONE')
