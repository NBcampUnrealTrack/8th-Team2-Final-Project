# -*- coding: utf-8 -*-
"""Vector icon sets for the HUD (24x24 viewBox). One geometry, three looks:
   line   - 1.7 px round strokes in the ivory text colour
   duo    - the same strokes + a soft fill of the main body + one gold accent (the recommended set)
   medal  - the line icon in gold-ivory on a small navy coin with a gold rim
Each icon: body = closed shapes (filled softly in duo), line = open strokes, accent = the gold part in duo/medal.
Used by build_refine.py (proposal page) and export_icons.py (PNG textures for the game)."""

GOLD = '#e8c46e'
IVORY = '#f1ede6'

ICONS = {
    'quest': {   # journal with an exclamation mark
        'body': ['M7 3.6h10.2a1.4 1.4 0 0 1 1.4 1.4v14.6H7.6a2.2 2.2 0 0 1-2.2-2.2V5.2A1.6 1.6 0 0 1 7 3.6z'],
        'line': ['M5.4 17.4a2.2 2.2 0 0 1 2.2-2.2h11'],
        'accent': ['M12 6.9v4.4', 'M12 13.3v.2'],
    },
    'character': {
        'body': ['M12 4.6a3.7 3.7 0 1 1 0 7.4 3.7 3.7 0 0 1 0-7.4z', 'M4.8 20c.7-3.7 3.5-5.8 7.2-5.8s6.5 2.1 7.2 5.8z'],
        'line': [],
        'accent': [],
    },
    'stats': {   # rising bars on a baseline, the top one gold
        'body': ['M5 13h3v6.5H5z', 'M10.5 9h3v10.5h-3z'],
        'line': ['M3.5 19.8h17'],
        'accent': ['M16 5h3v14.5h-3z'],
    },
    'growth': {  # sprout
        'body': ['M12 12.6c0-3.7-2.7-6.4-6.8-6.4 0 3.8 2.7 6.4 6.8 6.4z'],
        'line': ['M12 20v-7.4', 'M8.4 20h7.2'],
        'accent': ['M12 15c0-3.4 2.5-5.8 6.3-5.8 0 3.5-2.5 5.8-6.3 5.8z'],
    },
    'bag': {     # satchel with a gold clasp
        'body': ['M5 8.6h14l-1 10.8a1.6 1.6 0 0 1-1.6 1.5H7.6A1.6 1.6 0 0 1 6 19.4z'],
        'line': ['M9 8.6V7.2a3 3 0 0 1 6 0v1.4', 'M5.5 12.6h13'],
        'accent': ['M10.8 12.6v2.3h2.4v-2.3'],
    },
    'naia': {    # little spirit: a flame-drop with two eyes
        'body': ['M12 3.4c3.8 3.1 6.4 6.4 6.4 10a6.4 6.4 0 0 1-12.8 0c0-2.2 1-4.1 2.6-5.5.3 1.7 1.2 2.8 2.5 3.2-.4-2.9.2-5.4 1.3-7.7z'],
        'line': [],
        'accent': ['M9.9 14.6v.4', 'M14.1 14.6v.4'],
    },
    'menu': {'body': [], 'line': ['M5 7h14', 'M5 12h14', 'M5 17h14'], 'accent': []},
    'eye': {
        'body': ['M2.6 12S6.1 5.6 12 5.6 21.4 12 21.4 12 17.9 18.4 12 18.4 2.6 12 2.6 12z'],
        'line': [],
        'accent': ['M12 9.2a2.8 2.8 0 1 1 0 5.6 2.8 2.8 0 0 1 0-5.6z'],
    },
    'eyeoff': {
        'body': ['M2.6 12S6.1 5.6 12 5.6 21.4 12 21.4 12 17.9 18.4 12 18.4 2.6 12 2.6 12z'],
        'line': ['M4.2 4.2l15.6 15.6'],
        'accent': [],
    },
    'sword': {
        'body': ['M19.6 4.4l-.8 4.3-8.4 8.4-3.5-3.5 8.4-8.4z'],
        'line': ['M5.6 12.8l5.6 5.6', 'M8.2 15.8l-3.7 3.7'],
        'accent': ['M5.6 12.8l5.6 5.6'],
    },
    'spark': {   # skill: four-point star + a small one
        'body': ['M11 3.2c.6 4.6 1.9 6.9 6.5 7.5-4.6.6-5.9 2.9-6.5 7.5-.6-4.6-1.9-6.9-6.5-7.5 4.6-.6 5.9-2.9 6.5-7.5z'],
        'line': [],
        'accent': ['M18.4 14.6l.5 1.6 1.6.5-1.6.5-.5 1.6-.5-1.6-1.6-.5 1.6-.5z'],
    },
    'swap': {'body': [], 'line': ['M5 8.5h12.5', 'M19 15.5H6.5'], 'accent': ['M14.5 5.5l3 3-3 3', 'M9.5 12.5l-3 3 3 3']},
    'flee': {
        'body': ['M13.5 3.8H7.6A1.6 1.6 0 0 0 6 5.4v13.2a1.6 1.6 0 0 0 1.6 1.6h5.9z'],
        'line': ['M11 12h9.2'],
        'accent': ['M17 8.8l3.2 3.2-3.2 3.2'],
    },
    'chat': {
        'body': ['M5 5.2h14a1.6 1.6 0 0 1 1.6 1.6v8.1a1.6 1.6 0 0 1-1.6 1.6h-7l-4.2 3.4v-3.4H5a1.6 1.6 0 0 1-1.6-1.6V6.8A1.6 1.6 0 0 1 5 5.2z'],
        'line': [],
        'accent': ['M8.4 10.9h.2', 'M11.9 10.9h.2', 'M15.4 10.9h.2'],
    },
    'gear': {
        'body': ['M12 5.7a6.3 6.3 0 1 1 0 12.6 6.3 6.3 0 0 1 0-12.6z'],
        'line': ['M12 3v2.4M12 18.6V21M3 12h2.4M18.6 12H21M5.6 5.6l1.7 1.7M16.7 16.7l1.7 1.7M5.6 18.4l1.7-1.7M16.7 7.3l1.7-1.7'],
        'accent': ['M12 9.6a2.4 2.4 0 1 1 0 4.8 2.4 2.4 0 0 1 0-4.8z'],
    },
    'map': {
        'body': ['M9 5.2L4 7.2v12l5-2 6 2 5-2v-12l-5 2z'],
        'line': ['M9 5.2v12', 'M15 7.2v12'],
        'accent': [],
    },
    'party': {
        'body': ['M9 6a3 3 0 1 1 0 6 3 3 0 0 1 0-6z', 'M3.4 19.4c.6-3.1 2.8-4.8 5.6-4.8s5 1.7 5.6 4.8z'],
        'line': [],
        'accent': ['M16.6 6.2a2.4 2.4 0 1 1 0 4.8', 'M16 13.8c2.6-.1 4.4 1.5 4.9 4.4'],
    },
}

ATTR = 'fill="none" stroke-linecap="round" stroke-linejoin="round"'


def svg(name, size=26, style='duo', colour=IVORY, gold=GOLD, width=1.7):
    """Inline SVG markup for one icon in one style."""
    d = ICONS[name]
    parts = []
    if style == 'line':
        for p in d['body'] + d['line'] + d['accent']:
            parts.append(f'<path d="{p}" stroke="{colour}" stroke-width="{width}"/>')
    elif style == 'duo':
        for p in d['body']:
            parts.append(f'<path d="{p}" fill="{colour}" fill-opacity=".2" stroke="{colour}" stroke-width="{width}"/>')
        for p in d['line']:
            parts.append(f'<path d="{p}" stroke="{colour}" stroke-width="{width}"/>')
        for p in d['accent']:
            closed = p.rstrip().endswith('z')
            fill = f'fill="{gold}" fill-opacity=".9"' if closed else ''
            parts.append(f'<path d="{p}" {fill} stroke="{gold}" stroke-width="{width + .2:.1f}"/>')
    elif style == 'medal':
        inner = svg(name, size, 'line', '#f3dfae', gold, 1.9)
        return f'<span class="medal" style="width:{size + 10}px;height:{size + 10}px">{inner}</span>'
    return f'<svg class="ic" viewBox="0 0 24 24" width="{size}" height="{size}" {ATTR}>{"".join(parts)}</svg>'


def svg_file(name, style='duo', px=64, colour=IVORY, gold=GOLD):
    """A standalone SVG document (for exporting PNG textures)."""
    body = svg(name, px, style, colour, gold)
    return '<svg xmlns="http://www.w3.org/2000/svg"' + body[len('<svg'):]
