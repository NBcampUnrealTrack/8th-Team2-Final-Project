# -*- coding: utf-8 -*-
"""Writes the NPC size / tone table to Config/DefaultGame.ini [/Script/ProjectM.NUNPCVisualSettings] (read by
ANUNPCBase::ApplyVisualTuning). --revert removes the section.

How the numbers were chosen (Saved/HUDRenewal/npc_art, export_npc_art.py, 2026-10-14):
  every NPC idle frame is drawn at the player's dot size (1 texel = 1.04 cm) and about as tall (87-96 px vs the novice's 94),
  but the art is darker: mean sRGB luminance 0.15-0.37 against the player's 0.41. ToneGamma/ToneGain bring each NPC to ~0.33
  (4005, a black-clad wolf boy, is lifted less on purpose). The dwarf is drawn as tall as the humans and twice as wide, so it
  is scaled to 0.9 to read as a dwarf next to the player.
Run: python Tools/HUDRenewal/apply_npc_visual.py [--revert]"""
import pathlib
import sys

INI = pathlib.Path(__file__).resolve().parents[2] / 'Config' / 'DefaultGame.ini'
SECTION = '[/Script/ProjectM.NUNPCVisualSettings]'
# NpcId: (Scale, ToneGamma, ToneGain)
TABLE = {
    4001: (1.00, 0.92, 1.08),   # elf mage (Tion)          0.29 -> 0.33
    4002: (1.00, 1.00, 1.00),   # elf girl (hidden child)  0.37, already bright
    4005: (1.00, 0.72, 1.10),   # wolf boy, black clothes  0.15 -> ~0.27
    4010: (1.00, 0.82, 1.08),   # wolf farmer              0.24 -> 0.33
    4011: (1.00, 0.96, 1.06),   # young man with a staff   0.31 -> 0.33
    4017: (0.90, 0.76, 1.08),   # dwarf (Elladon)          0.21 -> 0.33, a little shorter
}
DEFAULT = (1.00, 0.90, 1.05)    # NPCs not in the table: the NPC art runs darker than the player's in general


def entry(v):
    return '(Scale=%.6f,ToneGamma=%.6f,ToneGain=%.6f)' % v


def main(revert):
    text = INI.read_text(encoding='utf-8-sig')
    newline = '\r\n' if '\r\n' in text else '\n'
    lines = text.split(newline)
    if SECTION in lines:                       # drop the old section (up to the next header)
        i = lines.index(SECTION)
        j = i + 1
        while j < len(lines) and not lines[j].startswith('['):
            j += 1
        while i > 0 and lines[i - 1] == '':
            i -= 1
        del lines[i:j]
    if not revert:
        while lines and lines[-1] == '':
            lines.pop()
        pairs = ','.join('(%d, %s)' % (k, entry(v)) for k, v in sorted(TABLE.items()))
        lines += ['', SECTION, 'PixelAA=1.000000', 'Default=' + entry(DEFAULT), 'PerNpc=(%s)' % pairs, '']
    out = newline.join(lines)
    if not out.endswith(newline):
        out += newline
    INI.write_text(out, encoding='utf-8', newline='')
    print('removed' if revert else 'written', SECTION)


if __name__ == '__main__':
    main('--revert' in sys.argv)
