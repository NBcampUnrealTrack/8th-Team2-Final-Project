# -*- coding: utf-8 -*-
"""Writes one prompt per item icon for the HD-2D icon remake (Tools/ItemIconsHD2D/prompts/<T_Item_...>.txt).

Every icon that exists today (Content/Developers/dbals/UI/ItemIcons) is redrawn. The subject of each item comes from
Scripts/BuildIconPrompts.py (dbals's item-by-item wording: INDIVIDUAL, equipment family + tier) and, for items that file
leaves to the old icon, from the short lines below. The style is style.txt for all of them, with the player's field
sprite attached as the style reference and the old icon attached only to say WHAT the item is (gen_all.sh).
Run: python Tools/ItemIconsHD2D/make_prompts.py
"""
import importlib.util
import os
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
spec = importlib.util.spec_from_file_location('bip', ROOT / 'Scripts' / 'BuildIconPrompts.py')
bip = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bip)

# items BuildIconPrompts leaves to the old picture
EXTRA = {
    'C_VIT_01': 'a small round glass potion flask with a cork stopper, filled with red healing liquid (a minor health potion)',
    'C_VIT_02': 'a medium round glass potion bottle with a cork stopper, bright red healing liquid, a thin bronze band around the neck',
    'C_SP_01': 'a small round glass potion flask with a cork stopper, filled with blue spirit liquid (a minor SP potion)',
    'C_SP_02': 'a medium round glass potion bottle with a cork stopper, bright blue liquid, a thin silver band around the neck',
    'C_MP_01': 'a small round glass potion flask with a cork stopper, filled with pale violet-blue mana liquid',
    'C_MIX_01': 'a round glass potion bottle with a cork stopper, red and blue liquid swirling together in two halves',
    'C_REV_01': 'a small teardrop-shaped crystal, pale golden white, soft holy glow, a few tiny sparkles (a spirit tear that revives)',
    'C_BOND_01': 'a smooth rounded flat river stone, warm pale grey, a small carved spirit mark on its face, soft warm glow',
    'C_BOND_02': 'a rounded flat raw gemstone, rose quartz pink with crystal veins, a small carved spirit mark, warm glow',
    'C_Commune_01': 'an upright egg-shaped smooth grey stone, one engraved rune glowing faint pale teal',
    'C_State_01': 'a small medicine vial with a dark screw cap and a paper label with a flame mark, orange-red liquid (burn cure)',
    'C_State_02': 'a small medicine vial with a dark screw cap and a paper label with a snail-shell mark, pale lavender liquid (slow cure)',
    'C_State_03': 'a small medicine vial with a dark screw cap and a paper label with a cross mark, green liquid (poison cure)',
    'C_State_04': 'a small medicine vial with a dark screw cap and a paper label with a spiral mark, pink-violet liquid (hallucination cure)',
    'C_State_05': 'a small medicine vial with a dark screw cap and a paper label with a lightning mark, yellow liquid (paralysis cure)',
    'C_State_06': 'a small medicine vial with a dark screw cap and a paper label with a crack mark, pale grey-blue liquid (fracture cure)',
    'C_State_07': 'a larger medicine bottle with a gold screw cap and a paper label with a star mark, shining golden liquid (cures everything)',
    'E_DEV_01': 'an old worn short sword standing upright, chipped steel blade, plain crossguard, leather grip',
    'E_DEV_02': 'a pair of plain brown leather gloves laid down at a diagonal, three-quarter view from above, fingers held together',
    'E_DEV_03': 'a rusty iron helmet seen from the front, open face, dents and rust patches',
    'E_DEV_04': 'a plain brown leather chest armor seen from the front, laced front, simple stitching',
    'E_DEV_05': 'a plain copper ring standing upright, three-quarter view, no gem',
    'E_DEV_06': 'a plain silver ring standing upright, three-quarter view, small clear gem',
    'E_DEV_07': 'a gold ring standing upright, three-quarter view, small red gem',
    'E_DEV_08': 'a single dangling earring with a hook and a faceted clear crystal',
    'E_DEV_09': 'a single dangling earring with a hook and a round white pearl',
}


def subject(row_name):
    if row_name in EXTRA:
        return EXTRA[row_name]
    if row_name in bip.INDIVIDUAL:
        return bip.INDIVIDUAL[row_name]
    text = bip.equip_prompt(row_name)
    if text:
        return text.replace(bip.COMMON + ', ', '')
    raise KeyError(row_name)


def main():
    style = (HERE / 'style.txt').read_text(encoding='utf-8').strip()
    icons = sorted(n[:-7] for n in os.listdir(ROOT / 'Content/Developers/dbals/UI/ItemIcons') if n.endswith('.uasset'))
    out = HERE / 'prompts'
    out.mkdir(exist_ok=True)
    for name in icons:
        (out / (name + '.txt')).write_text(style + '\n\nItem: ' + subject(name[len('T_Item_'):]) + '.\n', encoding='utf-8')
    print(len(icons), 'prompts ->', out)


if __name__ == '__main__':
    main()
