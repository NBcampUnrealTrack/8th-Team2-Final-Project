# -*- coding: utf-8 -*-
"""Pictures for the new in-game HUD (concept A "glass edge", Docs/HUD_Renewal_Proposal_20261014).

Panels themselves are Slate rounded boxes (crisp at any size); this kit only holds what Slate cannot draw:
soft shadows, the light scrims behind floating text, the area-banner rules and glow, bar fills, the minimap vignette,
markers, and the pixel icons (one art pixel = 2 screen px, like the field sprites; imported with nearest filtering).

Output: ArtSource/UI/HUD2026/kit/*.png + kit.json {name: {"size": [w, h], "margin": [l, t, r, b] | null, "pixel": bool}}
Run: python Tools/HUDRenewal/make_hud_kit.py
"""
import io
import json
import math
import pathlib

from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'ArtSource' / 'UI' / 'HUD2026' / 'kit'
ICONS = ROOT / 'Docs' / 'HUD_Renewal_Proposal_20261014' / 'assets' / 'icons'
SS = 4      # supersampling for smooth edges

GOLD = (214, 190, 140)
IVORY = (241, 237, 230)
NAVY = (22, 41, 61)
META = {}


def save(name, im, margin=None, pixel=False):
    OUT.mkdir(parents=True, exist_ok=True)
    im.save(OUT / (name + '.png'))
    META[name] = {'size': list(im.size), 'margin': margin, 'pixel': pixel}


def shadow(name, box=64, radius=16, blur=18, alpha=0.55, pad=32):
    """Soft drop shadow of a rounded panel. 9-slice: corners = pad + radius."""
    size = box + pad * 2
    im = Image.new('L', (size * SS, size * SS), 0)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([pad * SS, pad * SS, (pad + box) * SS, (pad + box) * SS], radius * SS, fill=int(255 * alpha))
    im = im.resize((size, size), Image.LANCZOS).filter(ImageFilter.GaussianBlur(blur / 2.0))
    rgba = Image.new('RGBA', (size, size), (3, 8, 14, 0))
    rgba.putalpha(im)
    m = (pad + radius) / size
    save(name, rgba, [m, m, m, m])


def hgrad(name, w, h, stops, colour=(8, 14, 22), feather=0):
    """Horizontal alpha ramp. stops = [(x 0..1, alpha 0..1), ...]. feather > 0 also fades the top and bottom
    `feather` pixels (smoothstep) and saves them as 9-slice margins, so a panel of any height keeps soft edges."""
    im = Image.new('RGBA', (w, h))
    for x in range(w):
        t = (x + 0.5) / w
        for (x0, a0), (x1, a1) in zip(stops, stops[1:]):
            if x0 <= t <= x1:
                a = a0 + (a1 - a0) * ((t - x0) / max(x1 - x0, 1e-6))
                break
        else:
            a = stops[-1][1]
        for y in range(h):
            v = 1.0
            if feather:
                e = min(y + 0.5, h - y - 0.5) / feather
                e = max(0.0, min(1.0, e))
                v = e * e * (3 - 2 * e)
            im.putpixel((x, y), colour + (int(255 * a * v),))
    save(name, im, [0, feather / h, 0, feather / h] if feather else None)


def vgrad(name, w, h, stops, colour=(8, 14, 22)):
    """Vertical alpha ramp, top to bottom, eased between stops. stops = [(y 0..1, alpha 0..1), ...]."""
    im = Image.new('RGBA', (w, h))
    for y in range(h):
        t = (y + 0.5) / h
        a = stops[-1][1]
        for (y0, a0), (y1, a1) in zip(stops, stops[1:]):
            if y0 <= t <= y1:
                u = (t - y0) / max(y1 - y0, 1e-6)
                a = a0 + (a1 - a0) * u * u * (3 - 2 * u)
                break
        for x in range(w):
            im.putpixel((x, y), colour + (int(round(255 * a)),))
    save(name, im)


def radial(name, size, colour, alpha, falloff=1.6, squash=1.0):
    im = Image.new('RGBA', (size, int(size * squash)))
    w, h = im.size
    for y in range(h):
        for x in range(w):
            dx = (x + 0.5 - w / 2) / (w / 2)
            dy = (y + 0.5 - h / 2) / (h / 2)
            r = math.sqrt(dx * dx + dy * dy)
            a = max(0.0, 1.0 - r) ** falloff * alpha
            im.putpixel((x, y), colour + (int(255 * a),))
    save(name, im)


def bar(name, left, right, h=16):
    """Bar fill: left->right colour ramp plus a soft top light."""
    w = 256
    im = Image.new('RGBA', (w, h))
    for x in range(w):
        t = x / (w - 1)
        c = [int(left[i] + (right[i] - left[i]) * t) for i in range(3)]
        for y in range(h):
            light = 1.0 + 0.18 * max(0.0, 1.0 - y / (h * 0.45))       # top light
            shade = 1.0 - 0.12 * max(0.0, (y - h * 0.6) / (h * 0.4))     # bottom shade
            im.putpixel((x, y), tuple(min(255, int(v * light * shade)) for v in c) + (255,))
    save(name, im)


def vignette(name, size=96, radius=17, depth=18, alpha=0.38):
    """Inner shadow of a rounded rectangle (over the minimap picture). 9-slice."""
    im = Image.new('L', (size * SS, size * SS), int(255 * alpha))
    d = ImageDraw.Draw(im)
    inset = depth * 0.55
    d.rounded_rectangle([inset * SS, inset * SS, (size - inset) * SS, (size - inset) * SS], radius * SS, fill=0)
    im = im.resize((size, size), Image.LANCZOS).filter(ImageFilter.GaussianBlur(depth / 3.0))
    # keep the outside of the rounded corner fully dark: it sits under the glass frame anyway
    rgba = Image.new('RGBA', (size, size), (8, 14, 22, 0))
    rgba.putalpha(im)
    m = (radius + depth) / size
    save(name, rgba, [m, m, m, m])


def corner_mask(name, size=64, radius=17, colour=(14, 25, 38)):
    """The four rounded corners filled with the glass colour: laid over a square picture, it rounds it off. 9-slice."""
    im = Image.new('RGBA', (size * SS, size * SS), colour + (255,))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, size * SS - 1, size * SS - 1], radius * SS, fill=(0, 0, 0, 0))
    im = im.resize((size, size), Image.LANCZOS)
    m = (radius + 2) / size
    save(name, im, [m, m, m, m])


def diamond(name, size=12, hollow=False, colour=GOLD):
    im = Image.new('RGBA', (size * SS, size * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = size * SS / 2
    pts = [(c, 0), (size * SS, c), (c, size * SS), (0, c)]
    if hollow:
        d.polygon(pts, outline=colour + (255,), width=int(1.6 * SS))
    else:
        d.polygon(pts, fill=colour + (255,))
    save(name, im.resize((size, size), Image.LANCZOS))


def option_dot(name, canvas=48, size=17, hollow=False, colour=IVORY):
    """A small diamond centred on a badge-sized canvas, so it can share the badges' icon slot (same brush size)."""
    im = Image.new('RGBA', (canvas * SS, canvas * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c, r = canvas * SS / 2, size * SS / 2
    pts = [(c, c - r), (c + r, c), (c, c + r), (c - r, c)]
    if hollow:
        d.polygon(pts, outline=colour + (255,), width=int(2.6 * SS))
    else:
        d.polygon(pts, fill=colour + (255,))
    save(name, im.resize((canvas, canvas), Image.LANCZOS))


def coin(name, size=40):
    """Gold coin for prices and the purse (shop). Drawn at 2x, shown at ~18 px."""
    S = size * SS
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    p = 2 * SS
    d.ellipse([p, p, S - p, S - p], fill=(120, 86, 30, 255))
    d.ellipse([p + 2.5 * SS, p + 1.5 * SS, S - p - 2.5 * SS, S - p - 3.5 * SS], fill=(214, 166, 64, 255))
    d.ellipse([S * .30, S * .26, S * .70, S * .66], outline=(160, 116, 40, 255), width=int(2 * SS))
    d.ellipse([S * .24, S * .18, S * .46, S * .36], fill=(255, 228, 150, 220))
    save(name, im.resize((size, size), Image.LANCZOS))


def close_glyph(name, size=32, colour=(241, 237, 230), width=3.2):
    S = size * SS
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    k = S * 0.26
    for a, b in (((k, k), (S - k, S - k)), ((k, S - k), (S - k, k))):
        d.line([a, b], fill=colour + (255,), width=int(width * SS))
    save(name, im.resize((size, size), Image.LANCZOS))


def arrow(name, w=20, h=22, fill=(244, 215, 143), line=NAVY):
    im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pts = [(w / 2 * SS, 1 * SS), ((w - 1) * SS, (h - 2) * SS), (w / 2 * SS, (h - 7) * SS), (1 * SS, (h - 2) * SS)]
    d.polygon(pts, fill=fill + (255,), outline=line + (255,), width=int(2 * SS))
    save(name, im.resize((w, h), Image.LANCZOS))


def chevron(name, w=18, h=12, fill=(244, 215, 143)):
    im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (w * SS, 0), (w / 2 * SS, h * SS)], fill=fill + (255,))
    save(name, im.resize((w, h), Image.LANCZOS))


def bubble_tail(name, w=18, h=10):
    """Speech bubble tail: a white triangle pointing down (the bubble tints it to its own fill)."""
    S = 8
    im = Image.new('RGBA', (w * S, h * S), (0, 0, 0, 0))
    ImageDraw.Draw(im).polygon([(0, 0), (w * S, 0), (w * S / 2, h * S)], fill=(255, 255, 255, 255))
    save(name, im.resize((w, h), Image.LANCZOS))


def ring(name, size=64, width=3, colour=(244, 215, 143), glow=True):
    im = Image.new('RGBA', (size * SS, size * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    p = 6 * SS
    d.ellipse([p, p, size * SS - p, size * SS - p], outline=colour + (255,), width=int(width * SS))
    im = im.resize((size, size), Image.LANCZOS)
    if glow:
        g = im.filter(ImageFilter.GaussianBlur(3))
        im = Image.alpha_composite(g, im)
    save(name, im)


def target_cursor(name, w=40, h=32):
    """Battle target cursor: a fat gold arrow pointing down with a navy outline, a light top facet and a soft glow,
    so it reads over any battle set."""
    W, H = w * SS, h * SS
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    o = 2.8 * SS                                   # outline thickness
    tip = (W / 2, H - 2 * SS)
    outer = [(2 * SS, 3 * SS), (W - 2 * SS, 3 * SS), tip]
    d.polygon(outer, fill=(11, 21, 32, 255))
    inner = [(2 * SS + o * 1.7, 3 * SS + o), (W - 2 * SS - o * 1.7, 3 * SS + o), (W / 2, H - 2 * SS - o * 2.0)]
    d.polygon(inner, fill=(236, 184, 72, 255))
    facet = [(2 * SS + o * 1.7, 3 * SS + o), (W - 2 * SS - o * 1.7, 3 * SS + o), (W / 2, 3 * SS + o + (H - 5 * SS) * 0.34)]
    d.polygon(facet, fill=(255, 232, 160, 255))
    im = im.resize((w, h), Image.LANCZOS)
    glow = Image.new('RGBA', im.size, (240, 215, 157, 0))
    glow.putalpha(im.split()[3].filter(ImageFilter.GaussianBlur(3.0)).point(lambda a: int(a * 0.6)))
    save(name, Image.alpha_composite(glow, im))


def target_ring(name, w=132, h=42, width=3.6, colour=(250, 206, 104)):
    """Ground ring under the chosen target: a thin gold ellipse with a soft inner glow and two side ticks."""
    W, H = w * SS, h * SS
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    p = 5 * SS
    d.ellipse([p, p, W - p, H - p], outline=colour + (255,), width=int(width * SS))
    for x in (p - 2 * SS, W - p - 6 * SS):        # little ticks at the ends of the long axis
        d.rectangle([x, H / 2 - 1 * SS, x + 8 * SS, H / 2 + 1 * SS], fill=colour + (255,))
    im = im.resize((w, h), Image.LANCZOS)
    fill = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(fill).ellipse([p, p, W - p, H - p], fill=colour + (105,))
    fill = fill.resize((w, h), Image.LANCZOS).filter(ImageFilter.GaussianBlur(4))
    glow = im.filter(ImageFilter.GaussianBlur(2.5))
    save(name, Image.alpha_composite(Image.alpha_composite(fill, glow), im))


FONT = ROOT / 'Saved' / 'HUDRenewal' / 'fonts' / 'MaruBuri-Bold.ttf'      # pulled out of the game's font asset (build_proposal pipeline)


def marker(name, fill, glyph, size=44, ring=None):
    """Map / quest marker: a filled disc with a navy rim and a glyph, or a glowing ring (portal)."""
    from PIL import ImageFont
    im = Image.new('RGBA', (size * SS, size * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    p = 3 * SS
    if ring:
        d.ellipse([p * 2, p * 2, size * SS - p * 2, size * SS - p * 2], outline=ring + (255,), width=int(5 * SS))
        im = im.resize((size, size), Image.LANCZOS)
        glow = im.filter(ImageFilter.GaussianBlur(3))
        save(name, Image.alpha_composite(glow, im))
        return
    d.ellipse([p, p, size * SS - p, size * SS - p], fill=NAVY + (255,))
    d.ellipse([p + 3 * SS, p + 3 * SS, size * SS - p - 3 * SS, size * SS - p - 3 * SS], fill=fill + (255,))
    if glyph:
        font = ImageFont.truetype(str(FONT), int(size * SS * 0.62))
        box = d.textbbox((0, 0), glyph, font=font)
        w, h = box[2] - box[0], box[3] - box[1]
        d.text(((size * SS - w) / 2 - box[0], (size * SS - h) / 2 - box[1]), glyph, font=font, fill=NAVY + (255,))
    save(name, im.resize((size, size), Image.LANCZOS))


def _glow(im, blur, alpha, colour=(4, 10, 18)):
    """A soft dark halo under a shape so it reads over any part of the map."""
    halo = Image.new('RGBA', im.size, colour + (0,))
    halo.putalpha(im.split()[3].filter(ImageFilter.GaussianBlur(blur)).point(lambda a: int(min(255, a * alpha))))
    return Image.alpha_composite(halo, im)


def player_arrow(name, size=40):
    """Minimap player: a faceted arrowhead pointing up, centred on the canvas (the widget rotates about the centre).
    Left facet ivory, right facet gold, navy outline, dark halo --- reads on grass, roofs and water alike."""
    S = size * SS
    c = S / 2
    tip, left, right, notch = (c, c - 0.40 * S), (c - 0.29 * S, c + 0.33 * S), (c + 0.29 * S, c + 0.33 * S), (c, c + 0.15 * S)
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.polygon([tip, right, notch, left], fill=NAVY + (255,), outline=NAVY + (255,), width=int(5.0 * SS))
    shrink = lambda p, k=0.80: (c + (p[0] - c) * k, c - 0.035 * S + (p[1] - c) * k)
    t, l, r, n = shrink(tip), shrink(left), shrink(right), shrink(notch, 0.55)
    d.polygon([t, n, l], fill=(255, 248, 228, 255))
    d.polygon([t, r, n], fill=(238, 192, 92, 255))
    im = im.resize((size, size), Image.LANCZOS)
    save(name, _glow(im, 2.2, 0.85))


def player_cone(name, size=128, angle=78, alpha=0.80, colour=(255, 244, 206)):
    """The facing cone behind the player arrow: a soft wedge opening upward from the centre."""
    im = Image.new('RGBA', (size, size), colour + (0,))
    for y in range(size):
        for x in range(size):
            dx = (x + 0.5 - size / 2) / (size / 2)
            dy = (y + 0.5 - size / 2) / (size / 2)
            r = math.hypot(dx, dy)
            if r > 1.0 or r < 1e-3:
                continue
            off = abs(math.degrees(math.atan2(dx, -dy)))           # 0 = straight up
            edge = max(0.0, min(1.0, (angle / 2 - off) / 9.0))       # feathered sides
            fall = (1.0 - r) ** 0.55 * min(1.0, r / 0.15)
            im.putpixel((x, y), colour + (int(255 * alpha * edge * fall),))
    save(name, im)


def _diamond_shape(S, inset, radius):
    """A rounded square turned 45 degrees, as an L mask the size of the canvas."""
    big = Image.new('L', (S, S), 0)
    side = (S - 2 * inset) / math.sqrt(2)
    o = (S - side) / 2
    ImageDraw.Draw(big).rounded_rectangle([o, o, o + side, o + side], radius, fill=255)
    return big.rotate(45, resample=Image.BICUBIC)


def badge(name, fill, glyph=None, glyph_colour=NAVY, rim=NAVY, size=48, hollow=False, dots=False, light=True):
    """Map quest marker: a rounded gold diamond (the HUD's diamond motif) with a navy rim, a top light and a glyph."""
    from PIL import ImageFont
    S = size * SS
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    outer = _diamond_shape(S, 2 * SS, 7 * SS)
    inner = _diamond_shape(S, 5.2 * SS, 5 * SS)
    im.paste(Image.new('RGBA', (S, S), rim + (255,)), (0, 0), outer)
    if hollow:
        core = _diamond_shape(S, 10 * SS, 3 * SS)
        ring = Image.new('L', (S, S), 0)
        ring.paste(inner, (0, 0))
        ring.paste(0, (0, 0), core)
        im.paste(Image.new('RGBA', (S, S), fill + (255,)), (0, 0), ring)
        dot = Image.new('L', (S, S), 0)
        ImageDraw.Draw(dot).ellipse([S / 2 - 4.2 * SS, S / 2 - 4.2 * SS, S / 2 + 4.2 * SS, S / 2 + 4.2 * SS], fill=255)
        im.paste(Image.new('RGBA', (S, S), fill + (255,)), (0, 0), dot)
    else:
        im.paste(Image.new('RGBA', (S, S), fill + (255,)), (0, 0), inner)
        if light:   # the upper half a touch lighter: a lit facet
            top = Image.new('L', (S, S), 0)
            ImageDraw.Draw(top).rectangle([0, 0, S, S * 0.47], fill=70)
            mask = Image.composite(top, Image.new('L', (S, S), 0), inner)
            im.paste(Image.new('RGBA', (S, S), (255, 255, 255, 255)), (0, 0), mask)
    d = ImageDraw.Draw(im)
    if dots:
        for k in (-1, 0, 1):
            x = S / 2 + k * 7 * SS
            d.ellipse([x - 2.6 * SS, S / 2 - 2.6 * SS, x + 2.6 * SS, S / 2 + 2.6 * SS], fill=glyph_colour + (255,))
    elif glyph:
        font = ImageFont.truetype(str(FONT), int(S * 0.50))
        box = d.textbbox((0, 0), glyph, font=font)
        w, h = box[2] - box[0], box[3] - box[1]
        d.text(((S - w) / 2 - box[0], (S - h) / 2 - box[1] + 0.5 * SS), glyph, font=font, fill=glyph_colour + (255,))
    save(name, _glow(im.resize((size, size), Image.LANCZOS), 2.0, 0.75))


def head_badge(name, fill, glyph=None, glyph_colour=NAVY, rim=NAVY, line=(255, 246, 222), size=128, hollow=False, light=True):
    """NPC head quest marker (Docs/HUD_NpcMarker_Proposal_20261014, option A): the minimap badge drawn big for the world.
    Thick navy rim with a thin ivory line inside it (reads on dark stone as well as bright grass), a lit upper facet, a big
    glyph. Centred on the canvas, a soft dark halo baked around it."""
    from PIL import ImageFont
    S = size * SS
    D = int(S * 0.90)                         # the diamond's square canvas (the rest is room for the halo)
    ox = oy = (S - D) // 2
    k = D / (48.0 * SS)                       # the minimap badge was drawn on 48 px: same proportions, thicker rim
    rim_mask = _diamond_shape(D, 2.0 * k * SS, 7.0 * k * SS)
    line_mask = _diamond_shape(D, 5.4 * k * SS, 5.6 * k * SS)
    fill_mask = _diamond_shape(D, 7.0 * k * SS, 4.6 * k * SS)
    dia = Image.new('RGBA', (D, D), (0, 0, 0, 0))
    dia.paste(Image.new('RGBA', (D, D), rim + (255,)), (0, 0), rim_mask)
    dia.paste(Image.new('RGBA', (D, D), line + (255,)), (0, 0), line_mask)
    if hollow:
        core = _diamond_shape(D, 13.0 * k * SS, 3.0 * k * SS)
        ring = Image.new('L', (D, D), 0)
        ring.paste(fill_mask, (0, 0))
        ring.paste(0, (0, 0), core)
        dia.paste(Image.new('RGBA', (D, D), rim + (255,)), (0, 0), fill_mask)
        dia.paste(Image.new('RGBA', (D, D), fill + (255,)), (0, 0), ring)
        dot = Image.new('L', (D, D), 0)
        r = 4.6 * k * SS
        ImageDraw.Draw(dot).ellipse([D / 2 - r, D / 2 - r, D / 2 + r, D / 2 + r], fill=255)
        dia.paste(Image.new('RGBA', (D, D), line + (255,)), (0, 0), dot)
    else:
        dia.paste(Image.new('RGBA', (D, D), fill + (255,)), (0, 0), fill_mask)
        if light:
            top = Image.new('L', (D, D), 0)
            ImageDraw.Draw(top).rectangle([0, 0, D, D * 0.47], fill=78)
            dia.paste(Image.new('RGBA', (D, D), (255, 255, 255, 255)), (0, 0), Image.composite(top, Image.new('L', (D, D), 0), fill_mask))
    d = ImageDraw.Draw(dia)
    if glyph:
        font = ImageFont.truetype(str(FONT), int(D * 0.50))
        box = d.textbbox((0, 0), glyph, font=font)
        w, h = box[2] - box[0], box[3] - box[1]
        d.text(((D - w) / 2 - box[0], (D - h) / 2 - box[1] + 0.6 * k * SS), glyph, font=font, fill=glyph_colour + (255,))
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    im.paste(dia, (ox, oy), dia)
    im = im.resize((size, size), Image.LANCZOS)
    save(name, _glow(im, 4.0, 0.80))
    META[name]['mips'] = True


def portal_badge(name, size=48, teal=(127, 224, 214)):
    """Map portal: a navy disc with a teal arch (a gate) and a small light inside."""
    S = size * SS
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    p = 4 * SS
    d.ellipse([p, p, S - p, S - p], fill=NAVY + (255,), outline=teal + (255,), width=int(2.4 * SS))
    w, top, bottom = 7.5 * SS, S * 0.30, S * 0.71
    d.rounded_rectangle([S / 2 - w, top, S / 2 + w, bottom + w], w, outline=teal + (255,), width=int(2.6 * SS))
    d.rectangle([S / 2 - w - 2 * SS, bottom, S / 2 + w + 2 * SS, bottom + w + 3 * SS], fill=NAVY + (255,))
    d.line([S / 2 - w - 2.5 * SS, bottom, S / 2 + w + 2.5 * SS, bottom], fill=teal + (255,), width=int(2.6 * SS))
    d.ellipse([S / 2 - 2.6 * SS, S * 0.50 - 2.6 * SS, S / 2 + 2.6 * SS, S * 0.50 + 2.6 * SS], fill=(214, 255, 248, 255))
    im = im.resize((size, size), Image.LANCZOS)
    glow = Image.new('RGBA', im.size, teal + (0,))
    glow.putalpha(im.split()[3].filter(ImageFilter.GaussianBlur(3)).point(lambda a: int(a * 0.45)))
    save(name, Image.alpha_composite(glow, im))


def edge_pointer(name, w=18, h=12, fill=(244, 215, 143)):
    """Small triangle pointing up: shown beside a marker held at the minimap edge, turned toward the real spot."""
    W, H = w * SS, h * SS
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.polygon([(W / 2, 1 * SS), (W - 1 * SS, H - 1 * SS), (1 * SS, H - 1 * SS)], fill=fill + (255,), outline=NAVY + (255,), width=int(2 * SS))
    save(name, _glow(im.resize((w, h), Image.LANCZOS), 1.5, 0.7))


def sparkle(name, size=40, colour=(255, 238, 190)):
    """A four-point star with a soft glow (quest banner bursts)."""
    S = size * SS
    c = S / 2
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    a, b = 0.48 * S, 0.075 * S
    d.polygon([(c, c - a), (c + b, c - b), (c + a, c), (c + b, c + b), (c, c + a), (c - b, c + b), (c - a, c), (c - b, c - b)], fill=colour + (255,))
    im = im.resize((size, size), Image.LANCZOS)
    glow = Image.new('RGBA', im.size, colour + (0,))
    glow.putalpha(im.split()[3].filter(ImageFilter.GaussianBlur(2.6)).point(lambda v: int(v * 0.8)))
    save(name, Image.alpha_composite(glow, im))


def rays(name, size=384, count=14, colour=(255, 222, 150), alpha=0.82):
    """Soft light rays out of the centre (quest complete burst). Alternating wide and thin rays, fading outward."""
    im = Image.new('RGBA', (size, size), colour + (0,))
    for y in range(size):
        for x in range(size):
            dx = (x + 0.5 - size / 2) / (size / 2)
            dy = (y + 0.5 - size / 2) / (size / 2)
            r = math.hypot(dx, dy)
            if r > 1.0:
                continue
            ang = math.atan2(dy, dx) * count / 2.0
            k = (math.cos(ang * 2) + 1) / 2                       # 0..1 per ray
            wide = (math.cos(ang) + 1) / 2                         # every other ray wider
            ray = k ** (3.2 - 1.8 * wide)
            fall = max(0.0, 1.0 - r) ** 1.6 * min(1.0, r / 0.12)
            core = max(0.0, 1.0 - r / 0.32) ** 2 * 0.55
            im.putpixel((x, y), colour + (int(255 * min(1.0, alpha * (ray * fall + core))),))
    save(name, im)


def circle_mask(name, size=72, colour=(14, 25, 38)):
    """Navy outside a circle: laid over a square portrait it reads as a round one."""
    im = Image.new('RGBA', (size * SS, size * SS), colour + (255,))
    d = ImageDraw.Draw(im)
    d.ellipse([0, 0, size * SS - 1, size * SS - 1], fill=(0, 0, 0, 0))
    save(name, im.resize((size, size), Image.LANCZOS))


ICON_PX = 52       # 2x the 26 px the dock / command panel draw them at; imported with mips for the smaller uses


def icons(style='duo'):
    """The HUD icons from the vector source (hud_icons.py, recommended duotone set), rasterised by Chromium at ICON_PX
    on a transparent background. Replaces the old 2px pixel set (make_icons.py), which read rough next to the glass."""
    import sys
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
    import hud_icons
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page(viewport={'width': ICON_PX, 'height': ICON_PX})
        for name in hud_icons.ICONS:
            doc = ('<html><body style="margin:0;background:transparent">%s</body></html>'
                   % hud_icons.svg_file(name, style, ICON_PX))
            page.set_content(doc)
            raw = page.screenshot(omit_background=True, clip={'x': 0, 'y': 0, 'width': ICON_PX, 'height': ICON_PX})
            im = Image.open(io.BytesIO(raw)).convert('RGBA')
            save('T_HUD_Icon_' + name.capitalize(), im)
            META['T_HUD_Icon_' + name.capitalize()]['mips'] = True
        browser.close()


def hover_frame(name, size=48, radius=13, fill=(241, 237, 230, 28), line=(214, 190, 140, 140)):
    """The dock / button hover square as a 9-slice picture. A ROUNDED_BOX brush's outline ignores the widget's render
    opacity in UE 5.8, so an outlined brush that fades with opacity leaves its outline behind --- a texture fades whole."""
    S = size * SS
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, S - 1, S - 1], radius * SS, fill=fill, outline=line, width=SS)
    m = (radius + 2) / size
    save(name, im.resize((size, size), Image.LANCZOS), [m, m, m, m])


def line_frame(name, radius, size=None, width=1.5):
    """A rounded outline alone, white, 9-slice (tinted in the widget). Same reason as hover_frame: outlines that fade
    with the widget (result window reveal, speech bubble fade) must be pictures, not ROUNDED_BOX outlines."""
    size = size or radius * 2 + 8
    S = size * 4
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle([1, 1, S - 2, S - 2], radius * 4, outline=(255, 255, 255, 255), width=int(round(width * 4)))
    m = (radius + 2) / size
    save(name, im.resize((size, size), Image.LANCZOS), [m, m, m, m])


def circle_line(name, size, width=1.6):
    """A circle outline alone, white (portrait rings that fade with their row)."""
    S = size * 4
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(im).ellipse([2, 2, S - 3, S - 3], outline=(255, 255, 255, 255), width=int(round(width * 4)))
    save(name, im.resize((size, size), Image.LANCZOS))


def beam(name, w=64, h=256):
    """Light column for the selection stage: white, brightest at the bottom, soft at both sides (tinted in the widget)."""
    im = Image.new('RGBA', (w, h))
    for y in range(h):
        down = ((y + 0.5) / h) ** 0.8
        for x in range(w):
            across = max(0.0, 1.0 - abs((x + 0.5) / w * 2 - 1)) ** 1.5
            im.putpixel((x, y), (255, 255, 255, int(255 * across * down)))
    save(name, im)


def veil(name, size=256, centre=0.62, edge=0.92, cy=0.45, colour=(7, 13, 22)):
    """Full-screen dim with a vignette: a little lighter in the middle (stretched over the screen)."""
    im = Image.new('RGBA', (size, size))
    for y in range(size):
        for x in range(size):
            dx = ((x + 0.5) / size - 0.5) / 0.62
            dy = ((y + 0.5) / size - cy) / 0.62
            r = math.sqrt(dx * dx + dy * dy)
            a = min(1.0, centre + (edge - centre) * r)
            im.putpixel((x, y), colour + (int(255 * a),))
    save(name, im)


def main():
    shadow('T_HUD_Shadow')                                            # panels
    shadow('T_HUD_ShadowSoft', box=48, radius=24, blur=14, alpha=0.4, pad=24)   # pills, chips
    hgrad('T_HUD_ScrimL', 256, 64, [(0, .62), (.62, .40), (1, 0)], feather=22)      # chat lines (dark at the screen edge)
    # quest tracker (10/15: back to text on this scrim, a little darker than the first version .40/.66 so the title reads over roofs)
    hgrad('T_HUD_ScrimR', 256, 64, [(0, 0), (.34, .52), (1, .78)], feather=22)
    radial('T_HUD_BannerGlow', 256, (8, 14, 22), 0.62, 1.4, squash=0.3)
    hgrad('T_HUD_RuleL', 256, 4, [(0, 0), (1, 1)], GOLD)               # area banner rules
    hgrad('T_HUD_RuleR', 256, 4, [(0, 1), (1, 0)], GOLD)
    bar('T_HUD_BarHP', (111, 191, 99), (169, 224, 138))
    bar('T_HUD_BarSP', (93, 159, 219), (159, 208, 245))
    bar('T_HUD_BarFoe', (217, 96, 77), (240, 138, 110))
    bar('T_HUD_BarGold', (196, 160, 92), (240, 215, 157))
    vignette('T_HUD_MapVignette')
    corner_mask('T_HUD_MapCorners')
    diamond('T_HUD_Diamond')
    diamond('T_HUD_DiamondHollow', hollow=True)
    diamond('T_HUD_Pip', 12, colour=(232, 196, 110))
    diamond('T_HUD_PipEmpty', 12, hollow=True, colour=(241, 237, 230))
    diamond('T_HUD_PipSP', 12, colour=(143, 199, 242))
    arrow('T_HUD_PlayerArrow')
    chevron('T_HUD_Chevron')
    ring('T_HUD_Ring', 64, 3)
    target_cursor('T_HUD_TargetCursor')
    target_ring('T_HUD_TargetRing')
    radial('T_HUD_Glow', 128, (244, 215, 143), 0.55, 2.0)
    marker('T_HUD_Marker_Request', (232, 196, 110), '!')
    marker('T_HUD_Marker_TurnIn', (232, 196, 110), '?')
    marker('T_HUD_Marker_InProgress', (170, 182, 191), '…')
    marker('T_HUD_Marker_Portal', None, None, ring=(127, 224, 214))
    # minimap v2 (Docs/HUD_QuestMap_Proposal_20261014): facing arrow + cone, diamond quest badges, edge pointer
    player_arrow('T_HUD_MapPlayer')
    player_cone('T_HUD_MapPlayerCone')
    radial('T_HUD_MapPlayerHalo', 64, (6, 12, 20), 0.55, 1.2)
    badge('T_HUD_Badge_Offer', (236, 196, 104), '!')
    badge('T_HUD_Badge_TurnIn', (250, 214, 120), '?')
    badge('T_HUD_Badge_InProgress', (128, 144, 158), dots=True, glyph_colour=(241, 237, 230))
    badge('T_HUD_Badge_Locked', (44, 62, 80), '!', glyph_colour=(150, 162, 172), rim=(110, 122, 134), light=False)
    badge('T_HUD_Badge_Target', (127, 224, 214), hollow=True)
    portal_badge('T_HUD_Badge_Portal')
    edge_pointer('T_HUD_EdgePointer')
    # NPC head quest marker (Docs/HUD_NpcMarker_Proposal_20261014, option A, drawn bigger and bolder than the map badges)
    head_badge('T_HUD_Head_Offer', (240, 196, 92), '!')
    head_badge('T_HUD_Head_TurnIn', (255, 220, 112), '?')
    head_badge('T_HUD_Head_Target', (112, 222, 210), hollow=True)
    head_badge('T_HUD_Head_Locked', (52, 70, 88), '!', glyph_colour=(170, 182, 192), rim=(30, 42, 56), line=(126, 138, 150), light=False)
    # quest banners
    sparkle('T_HUD_Sparkle')
    radial('T_HUD_QuestGlow', 256, (8, 14, 22), 0.80, 1.05, squash=0.36)   # denser than the area banner glow: the banner sits over the bright road
    rays('T_HUD_Rays')
    circle_mask('T_HUD_PortraitMask72', 72)
    circle_mask('T_HUD_PortraitMask36', 36)
    hover_frame('T_HUD_HoverFrame')
    # NPC dialogue, subtitle style (Docs/HUD_Dialogue_Proposal_20261014, option C): no box, the screen bottom darkens.
    # Clear at the top, ~.6 where the speaker name sits, ~.85 behind the lines --- readable over the bright road.
    vgrad('T_HUD_DialogueScrim', 8, 256, [(0, 0), (.16, .06), (.36, .58), (.52, .84), (1, .90)], colour=(9, 17, 27))
    option_dot('T_HUD_OptionDot', colour=(214, 222, 228))                  # talk / shop / other choices (quests use the badges)
    option_dot('T_HUD_OptionExit', hollow=True, colour=(170, 182, 191))     # leave
    # shop side sheet (Docs/HUD_Shop_Proposal_20261014, option B)
    coin('T_HUD_Coin')
    close_glyph('T_HUD_Close')
    # job / starter Naia selection stage (Docs/HUD_Selection_Proposal_20261015, option A): light column and ground light
    # (white, tinted by the job / element colour in the widget), the foot shadow and the full-screen vignette
    beam('T_HUD_SelBeam')
    radial('T_HUD_SelGlow', 256, (255, 255, 255), 1.0, 1.6, squash=0.24)
    radial('T_HUD_SelShadow', 128, (0, 0, 0), 0.62, 1.0, squash=0.24)
    veil('T_HUD_SelVeil')
    # chat speech bubble (Docs/HUD_ResultBubble_Proposal_20261015, bubble A): the tail under the glass body
    bubble_tail('T_HUD_BubbleTail')
    # battle result window (same proposal, result A): outlines as pictures so they fade with the reveal
    line_frame('T_HUD_Line8', 8)
    line_frame('T_HUD_Line10', 10)
    line_frame('T_HUD_Line14', 14)
    line_frame('T_HUD_Line22', 22, width=1.2)
    circle_line('T_HUD_CircleLine52', 52)
    icons()
    (OUT / 'kit.json').write_text(json.dumps(META, indent=1), encoding='utf-8')
    print('kit', len(META), '->', OUT)


if __name__ == '__main__':
    main()
