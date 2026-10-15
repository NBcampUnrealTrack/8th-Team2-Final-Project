# -*- coding: utf-8 -*-
"""Play-in-editor capture with UI, for the character creation screen.

Run:  UnrealEditor.exe <uproject> <map> -ExecCmds="py Tools/CharacterCreateGlass/pie_capture.py" -unattended -nosplash -NoSound
Env:  CC_SHOT   absolute output .png of the first state (required)
      CC_STEPS  optional JSON file: [{"shot": "name", "py": "helper.py", "wait": 1.5}, ...] - one capture per state, same folder as CC_SHOT.
                "wait" = seconds between the helper and the shot, "frames" = rendered frames at least (default 60;
                a small value with a short wait catches an animation half way). A material used for the first time compiles in the background
                and draws as a grey block until it is ready, so a state that first shows one needs a longer wait.
                Without it there is one state: CC_SHOT after the optional helper CC_GAME_PY.
      CC_WAIT   seconds to let the level settle before the first shot (default 12)
      CC_W/CC_H play window size (default 1920x1078)
Each state writes
      <shot>.png          `shot showui` (the engine's own screenshot; includes the window frame)
      <shot>.json         viewport size (crop_shot.py needs it)
      <shot>.layout.json  every live widget: name, owner, class, visibility, desired size
      <shot>.win.bmp      the play window as the desktop compositor shows it (PrintWindow)
Helpers run in the play world with GAME_WORLD, PC and log(). Opens a floating play window, stops play and quits. Saves no assets.
"""
import ctypes
import ctypes.wintypes as wt
import glob
import json
import os
import struct
import time
import unreal

SHOT = os.environ['CC_SHOT'].replace(chr(92), '/')
FOLDER = os.path.dirname(SHOT)
WAIT = float(os.environ.get('CC_WAIT', '12'))
W = int(os.environ.get('CC_W', '1920'))
H = int(os.environ.get('CC_H', '1078'))  # the floating window adds 2 px: 1078 gives a 1080 viewport
PLAY = '/Script/UnrealEd.Default__LevelEditorPlaySettings'
R = unreal.ObjectReflectionService
if os.environ.get('CC_STEPS', ''):
    HELPERS = os.path.dirname(os.path.abspath(os.environ['CC_STEPS']))      # helper paths are relative to the states file
    STEPS = [{'shot': FOLDER + '/' + s['shot'] + '.png', 'py': os.path.join(HELPERS, s['py']) if s.get('py') else '', 'wait': float(s.get('wait', 1.5)), 'frames': int(s.get('frames', 60))}
             for s in json.load(open(os.environ['CC_STEPS'], encoding='utf-8-sig'))]   # -sig: PowerShell writes a BOM
else:
    STEPS = [{'shot': SHOT, 'py': os.environ.get('CC_GAME_PY', ''), 'wait': 1.5, 'frames': 60}]
state = {'phase': 'boot', 't0': time.time(), 'ticks': 0, 'handle': None, 'old': {}, 'step': 0}


def log(message):
    unreal.log('[PROLOGUE-QA] ' + str(message))


def restore():
    for key, value in state['old'].items():
        R.set_property(PLAY, key, value)


def finish():
    restore()
    unreal.unregister_slate_post_tick_callback(state['handle'])
    unreal.SystemLibrary.quit_editor()


def dump_layout(world, path):
    """Every live widget with the size it asks for (design pixels): enough to check a stack of rows against the mock."""
    rows = []
    for widget in unreal.ObjectIterator(unreal.Widget):
        try:
            if widget.get_name().startswith('Default__') or widget.get_world() != world:
                continue
            size = widget.get_desired_size()         # FGeometry has no reflected fields, so Python cannot read positions
            tree = widget.get_outer()
            owner = tree.get_outer().get_name() if tree and tree.get_outer() else ''
            row = {'name': widget.get_name(), 'owner': owner, 'class': widget.get_class().get_name(), 'vis': str(widget.get_visibility()).split('.')[-1].split(':')[0],
                   'desired': [round(size.x, 1), round(size.y, 1)]}
            if isinstance(widget, unreal.TextBlock):
                row['text'] = str(widget.get_text())
            if isinstance(widget, unreal.BackgroundBlur):
                row['blur'] = [widget.get_editor_property('blur_strength'), widget.get_editor_property('apply_alpha_to_blur')]
            rows.append(row)
        except Exception as error:
            rows.append({'name': widget.get_name(), 'error': repr(error)})
    json.dump(rows, open(path, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    return len(rows)


def grab_window(path, width):
    """The play window as the compositor shows it. `shot showui` redraws the window for the capture and can differ (background blur)."""
    user32, gdi32 = ctypes.windll.user32, ctypes.windll.gdi32
    pid, found = os.getpid(), []

    def visit(hwnd, _):
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            rect = wt.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(rect))
            if abs(rect.right - width) <= 16 and rect.bottom >= 600:
                found.append((hwnd, rect.right, rect.bottom))
        return True

    user32.EnumWindows(ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(visit), 0)
    if not found:
        return 'no play window found'
    hwnd, w, h = found[0]
    user32.GetDC.restype = wt.HDC
    gdi32.CreateCompatibleDC.restype = wt.HDC
    gdi32.CreateCompatibleBitmap.restype = wt.HBITMAP
    screen = user32.GetDC(wt.HWND(hwnd))
    memory = gdi32.CreateCompatibleDC(wt.HDC(screen))
    bitmap = gdi32.CreateCompatibleBitmap(wt.HDC(screen), w, h)
    gdi32.SelectObject(wt.HDC(memory), wt.HBITMAP(bitmap))
    ok = user32.PrintWindow(wt.HWND(hwnd), wt.HDC(memory), 3)     # PW_CLIENTONLY | PW_RENDERFULLCONTENT
    header = struct.pack('<IiiHHIIiiII', 40, w, -h, 1, 32, 0, 0, 0, 0, 0, 0)   # top-down 32 bit
    pixels = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(wt.HDC(memory), wt.HBITMAP(bitmap), 0, h, pixels, header, 0)
    with open(path, 'wb') as handle:
        handle.write(b'BM' + struct.pack('<IHHI', 54 + len(pixels.raw), 0, 0, 54) + header + pixels.raw)
    gdi32.DeleteObject(wt.HBITMAP(bitmap)); gdi32.DeleteDC(wt.HDC(memory)); user32.ReleaseDC(wt.HWND(hwnd), wt.HDC(screen))
    return '%dx%d printwindow=%s' % (w, h, ok)


def on_tick(_dt):
    state['ticks'] += 1
    elapsed = time.time() - state['t0']
    try:
        if state['phase'] == 'boot':
            if state['ticks'] < 20:
                return
            for key, value in (('NewWindowWidth', str(W)), ('NewWindowHeight', str(H)), ('LastExecutedPlayModeType', 'PlayMode_InEditorFloating')):
                state['old'][key] = R.get_property(PLAY, key)
                R.set_property(PLAY, key, value)
            for step in STEPS:
                for old in glob.glob(os.path.splitext(step['shot'])[0] + '*.png'):
                    os.remove(old)
            unreal.WidgetService.start_pie()
            log('play requested %dx%d, %d state(s), old settings %s' % (W, H, len(STEPS), state['old']))
            state['phase'] = 'play'; state['t0'] = time.time(); state['mark'] = state['ticks']
            return
        if state['phase'] in ('end', 'quit'):
            if state['phase'] == 'end' and elapsed > 3:
                state['phase'] = 'quit'
                finish()
            return

        world = unreal.EditorLevelLibrary.get_game_world()
        frames = state['ticks'] - state['mark']
        step = STEPS[state['step']]
        base = os.path.splitext(step['shot'])[0]
        if state['phase'] == 'play':
            # seconds alone are not enough: a one-off engine ensure can freeze the first frames for 10+ seconds
            if world is None or elapsed < WAIT or frames < 150:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages', pc)
            state['phase'] = 'helper'
        if state['phase'] == 'helper':
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if step['py']:
                try:    # a broken helper must not cost the shot
                    exec(compile(open(step['py'], encoding='utf-8').read(), step['py'], 'exec'), {'GAME_WORLD': world, 'PC': pc, 'log': log})
                    log('helper ran ' + os.path.basename(step['py']))
                except Exception as error:
                    log('helper ERROR %s %r' % (os.path.basename(step['py']), error))
            state['phase'] = 'settle'; state['t0'] = time.time(); state['mark'] = state['ticks']
        elif state['phase'] == 'settle':
            if elapsed < step['wait'] or frames < step['frames']:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            size = unreal.WidgetLayoutLibrary.get_viewport_size(world)
            log('%s: viewport %.0fx%.0f scale %.3f' % (os.path.basename(base), size.x, size.y, unreal.WidgetLayoutLibrary.get_viewport_scale(world)))
            json.dump({'viewport': [size.x, size.y]}, open(base + '.json', 'w'))
            for label, call in (('layout', lambda: dump_layout(world, base + '.layout.json')), ('window', lambda: grab_window(base + '.win.bmp', int(size.x)))):
                try:
                    log('%s %s' % (label, call()))
                except Exception as error:
                    log('%s ERROR %r' % (label, error))
            unreal.SystemLibrary.execute_console_command(world, 'shot showui filename="%s"' % step['shot'], pc)
            state['phase'] = 'wait'; state['t0'] = time.time()
        elif state['phase'] == 'wait':
            made = glob.glob(base + '*.png')  # the engine appends a counter to the name
            if made or elapsed > 20:
                if made:
                    time.sleep(0.5)
                    os.replace(made[0], step['shot'])
                log('shot %s %s after %.1fs' % (os.path.basename(base), 'written' if made else 'MISSING', elapsed))
                state['step'] += 1
                if state['step'] < len(STEPS):
                    state['phase'] = 'helper'
                else:
                    unreal.WidgetService.stop_pie()
                    state['phase'] = 'end'; state['t0'] = time.time()
    except Exception as error:  # never leave an editor hanging
        log('ERROR ' + repr(error))
        state['phase'] = 'quit'
        finish()


state['handle'] = unreal.register_slate_post_tick_callback(on_tick)
log('registered shot=%s wait=%s' % (SHOT, WAIT))
