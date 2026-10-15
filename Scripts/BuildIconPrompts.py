# 아직 아이콘이 없는 아이템의 "완성 프롬프트"를 뽑아 Docs/아이콘프롬프트_대기목록.md로 쓴다.
# 표(ItemTable/EquipTable)와 아이콘 폴더를 대조하므로, 아이콘을 채운 뒤 다시 돌리면 남은 것만 나온다.
#
#   python Scripts/BuildIconPrompts.py          # 상점 품목표에 깔린 것만 (기본)
#   python Scripts/BuildIconPrompts.py --all    # 표에 있는 것 전부
#
# 장비는 계열 문구 + 티어 문구를 이어 붙인다. 그 밖은 아래 INDIVIDUAL의 문구를 쓴다.
# 장비가 아닌 것의 모양은 리버브 「아이템 기획서」(기획안 > 상점 시스템, 민서 님 9/8)의 "아이콘 컨셉" 열을 따른다.
# 제작 레시피(CraftRecipeTable)에 쓰이는 재료·도면·결과물은 맨 앞 절에 모아 먼저 뽑게 한다.
# 문구를 고칠 일이 생기면 이 파일만 고친다 (Docs/아이콘프롬프트_*.md는 사람이 읽는 설명용).
# 받은 PNG는 Scripts/NormalizeItemIcons.py로 128px 축소를 거친다 (투명 배경은 알파를 살리고, 배경이 꽉 찬 옛 그림만 #101925로 평탄화).
import csv
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICON_ASSET_DIR = os.path.join(ROOT, "Content", "Developers", "dbals", "UI", "ItemIcons")
OUT_PATH = os.path.join(ROOT, "Docs", "아이콘프롬프트_대기목록.md")

# 배경은 투명으로 뽑는다(9/11 유민). 투명을 체크무늬로 그려 넣는 생성기가 있어 체크무늬 금지도 같이 넣는다
COMMON = ("16-bit pixel art RPG item icon, single item centered with margin, "
          "transparent background (PNG with alpha channel), no background color, no checkerboard pattern, "
          "no vignette, soft top-left light source, gentle shading and highlights, crisp readable silhouette, "
          "no text, no border, no drop shadow, no ground shadow, game inventory style")

# 장비 계열 --- 부위 실루엣. 마법 계열(EQ_M*)은 천·비단이라 일반 방어구와 한눈에 갈린다.
# 장갑은 기존 아이콘처럼 "비스듬히 놓인 3/4 구도 + 손가락 모음"으로 --- 손가락을 펴서 정면으로 세우면
# 물건이 아니라 사람 손처럼 보이고, fingerless는 구멍이 여러 개라 개수가 틀어진다(9/8 유민 지적 2회)
EQUIP_FAMILY = {
    "EQ_Sword": "a straight one-handed sword standing upright, steel blade pointing up, leather-wrapped grip and simple crossguard",
    "EQ_Bow": "a short recurve bow standing upright, taut bowstring, curved wooden limbs",
    "EQ_Staff": "a wooden magic staff standing upright, a round crystal orb fixed at the top",
    "EQ_Card": "a fan of three playing cards, arcane suit symbol on the face card",
    "EQ_Totem": "a carved wooden totem idol standing upright, stylized animal face on the front",
    "EQ_Armor": "a sleeveless chest armor piece seen from the front, laced front closure",
    "EQ_Helmet": "a helmet seen from the front, open face",
    "EQ_Gloves": "a pair of leather gauntlet gloves laid down at a diagonal and seen from a three-quarter angle above, fingers relaxed and held together, one glove resting slightly over the other, wrist cuffs toward the lower edge",
    "EQ_Pants": "a pair of leg armor trousers seen from the front",
    "EQ_Boots": "a pair of boots side by side, three-quarter view",
    "EQ_Earring": "a single dangling earring with a hook and a small gemstone",
    "EQ_Ring1": "a copper ring standing upright, three-quarter view, small round gemstone set on top",
    "EQ_Ring2": "a silver ring standing upright, three-quarter view, small round gemstone set on top",
    "EQ_MArmor": "a mage robe chest piece with soft cloth folds, gemstone brooch at the collar",
    "EQ_MHelmet": "a pointed cloth mage hood seen from the front, small gemstone on the brow",
    "EQ_MGloves": "a pair of cloth mage gloves laid down at a diagonal and seen from a three-quarter angle above, fingers relaxed and held together, one glove resting slightly over the other, embroidered wrist cuffs toward the lower edge",
    "EQ_MPants": "a pair of flowing cloth mage trousers seen from the front",
    "EQ_MBoots": "a pair of soft cloth mage shoes with curled toes, side by side",
    "EQ_MEarring": "a single dangling earring with a hook and a faceted magic crystal",
    "EQ_MRing1": "a copper ring standing upright, three-quarter view, faceted magic crystal set on top",
    "EQ_MRing2": "a silver ring standing upright, three-quarter view, faceted magic crystal set on top",
}

# 티어 --- 표의 이름이 낡은/단련된/정교한/각인된/나이아가 깃든 5단계로 통일돼 있다.
# 재질은 계열 문구가 정한다(활·지팡이·토템은 나무, 카드는 종이) --- 그래서 티어 문구에는 재질을 넣지 않는다
TIER_GEAR = {
    "01": "worn and weathered, dull and unpolished, chipped edges, faded color, no ornament",
    "02": "clean and well made, polished surface, neat reinforced details, a small sky-blue gemstone accent",
    "03": "finely crafted, engraved filigree patterns with silver trim, bright blue gemstones, delicate detailing",
    "04": "ornate with gold trim, covered with engraved runes glowing faint amber, richly detailed",
    "05": "spirit-infused masterwork, glowing teal-cyan runes along the edges, faint floating light particles, ethereal aura",
}
# 옷 부위 전용 --- 등급을 옷감 색으로 올린다. 마법 반지·귀걸이는 옷이 아니라 TIER_GEAR를 쓴다
TIER_CLOTH = {
    "01": "worn and faded coarse beige cloth, frayed hem, no ornament",
    "02": "clean woven cloth in muted blue, neat stitching, a small sky-blue gemstone accent",
    "03": "finely woven deep blue silk with silver embroidery, bright blue gemstones",
    "04": "ornate violet silk with gold embroidered runes glowing faint amber, richly detailed",
    "05": "spirit-infused robes, glowing teal-cyan embroidery, faint floating light particles, ethereal aura",
}

# 장비가 아닌 것 --- 시리즈가 아니라 하나씩
INDIVIDUAL = {
    # 포션: 기존 병 모양(둥근 유리병 + 코르크)과 통일, 급수는 장식·발광 차등
    "C_VIT_03": "a large round glass potion bottle with cork, glowing crimson red liquid, thin gold trim band on the bottle",
    "C_VIT_04": "an ornate round glass potion bottle with cork, radiant ruby red liquid with sparkle highlights, gold trim and tiny wing ornament",
    # 05(특제·전설)와 혼합 03은 10/7에 01~04 규칙(장식·발광을 한 단계 더)을 이어 붙인 초안 --- 기획서에 컨셉 없음
    "C_VIT_05": ("a grand ornate round glass potion bottle with a gem-topped stopper, brilliant ruby red liquid with a golden inner glow, "
                 "elaborate gold filigree and small wing ornaments, strong radiant aura with floating sparkles"),
    "C_SP_03": "a large round glass potion bottle with cork, glowing sapphire blue liquid, thin silver trim band",
    "C_SP_04": "an ornate round glass potion bottle with cork, radiant azure liquid with sparkle highlights, silver trim and tiny star ornament",
    "C_SP_05": ("a grand ornate round glass potion bottle with a gem-topped stopper, brilliant azure liquid with a silver-white inner glow, "
                "elaborate silver filigree and small star ornaments, strong radiant aura with floating sparkles"),
    "C_MIX_02": "a large ornate glass potion bottle with cork, red and blue liquid swirling in a yin-yang pattern, glowing softly, gold trim",
    "C_MIX_03": ("a grand ornate glass potion bottle with cork, red and blue liquid swirling in a yin-yang pattern, radiant glow with sparkle "
                 "highlights, gold and silver trim with a tiny wing and star ornament"),
    # 부활: 눈물방울 크리스탈
    "C_REV_02": "a teardrop-shaped crystal, radiant golden white, strong holy glow with light rays, sparkles around it",
    # 교감석: 세워진 알 모양 + 룬 (유대의 돌은 둥글넓적한 자연석이라 실루엣으로 갈린다).
    # 등급은 색이 아니라 룬 밀도·발광 세기가 함께 올라가야 보인다. 01~03은 같은 돌로 두고
    # 재질이 바뀌는 건 04(일부 결정)와 05(온통 결정)뿐 --- 03을 결정으로 만들었더니 04보다 화려해 보였다(9/8 유민 지적).
    # 테나 받침 같은 금속 장식도 넣지 않는다(자연석 콘셉트에서 겉돈다).
    # 눈에 띄는 정도: 01 옅은 청록 < 02 초록 < 03 보라 < 04 금빛(+결정화·아우라·입자) < 05 무지갯빛 결정(+후광)
    "C_Commune_02": ("an upright egg-shaped smooth polished grey stone, two engraved runes glowing soft emerald green, "
                     "a few small sparkles around it"),
    "C_Commune_03": ("an upright egg-shaped polished stone of deep violet, densely engraved runes glowing bright violet "
                     "purple, a few tiny crystal facets embedded in the surface, steady glow"),
    "C_Commune_04": ("an ancient upright egg-shaped golden relic, its surface partly turned to crystal, covered with "
                     "elaborate engraved runes glowing radiant gold, glowing cracks pouring light across the weathered "
                     "surface, strong mystical aura, many floating light particles"),
    # 05(전설)는 10/7 초안 --- 04(금빛 유물)보다 한 단계 위로 보이게 온통 결정 + 무지갯빛
    "C_Commune_05": ("an ancient upright egg-shaped relic made entirely of clear prismatic crystal with a white-gold core, intricate "
                     "engraved runes glowing with shifting rainbow light, a halo ring of light around it, intense mystical aura, "
                     "many floating light particles"),
    # 카브라 부족 음식 (기획서: 불룩한 가죽 물주머니 / 김 나는 나무 그릇)
    "C_FAS_01": "a leather waterskin pouch with a wooden stopper and a braided carrying strap, plump with water, worn tan hide",
    "C_FAS_02": "a wooden bowl of thick hearty stew with meat and root vegetables, a carved wooden spoon resting in it, faint steam rising",
    # 제작 재료 --- 9/9 표에서 이름이 바뀐 것(샘물·숯·억새실)은 기획서 컨셉으로 다시 씀
    "M_ALCH_01": "a small wooden water pail with a rope handle, filled to the brim with clear spring water, a faint wisp of mist rising from the surface, a few soft blue water sparkles",
    "M_WEAP_01": "a few chunks of black charcoal piled together, small glowing red-orange embers in the cracks, faint warm heat glow",
    "M_CLOTH_01": "a neat skein of pale golden thread wound in loops, a loose thread end trailing out, soft straw-gold sheen",
    "M_ORE_01": "a few small rough grey ore fragments piled together, sharp broken edges, dull metallic specks",
    "M_ORE_02": "a single larger raw ore chunk of dark rock with visible crystalline grain, bright silver-blue mineral veins running through it",
    "M_HIDE_01": "a folded piece of tough grey-brown animal hide, neatly creased, coarse fur along one edge",
    "M_TOOL_01": "a small pile of worn iron gear cogs and a wooden pulley wheel with a few rivets, rusty edges",
    "M_WOOD_01": "a short cut log section of dense pale wood, tight close grain rings visible on the sawn end, rough bark on the side",
    "M_RING_01": "a broken dark iron chain link fragment snapped open at one side, a faint engraved double ring symbol on its surface, dim residual glow in the carving",
    "M_ROPE_01": "a coiled brown rope of braided natural fiber, one decorative knot tied on the coil",
    # 도면
    "B_WOOD_01": "a rolled parchment scroll partly unrolled, an ink sketch of a timber joint drawn on it, tied with a leather cord",
    # 리베르그로브 (진행용 증표·기념품·미래 페이오프 재료)
    "K_LIB_01": "a single slender living twig with a few small green leaves, fresh and unwithered, a soft pale green glow at its tip",
    "K_LIB_02": "a small wooden spinning top toy, painted with faded red and blue stripes, worn from play",
    "K_LIB_03": "a folded parchment map partly open, a hand-drawn village layout in brown ink with a few tiny roof shapes and a winding road",
    "K_LIB_04": "a soft clump of bright green moss, a plump cushion-like tuft, a few tiny fern sprigs",
    "K_LIB_05": "a small round glass marble, clear with a swirl of colored bubbles trapped inside, glossy highlight",
    "K_LIB_06": "a faded pink and cream ribbon tied in a neat bow, frayed ends, slightly worn fabric",
    "K_LIB_07": "three thin booklets stacked in a slight fan, dark blue covers each stamped with a small glowing spirit emblem",
    "K_LIB_08": "a round wooden tag on a cord carved from pale branch wood, a ring of six tiny elemental symbols engraved around its rim",
    # 신데하스
    "K_CIN_01": "a small rectangular wooden tag on a short cord, carved lettering on its face, a stamped mark below",
    "K_CIN_02": "a round bronze token medal on a short cord, engraved with a stylized forge furnace emblem",
    # 파스트라이드 (카브라 부족 흐름)
    "K_FAS_01": "a thin leather cord wristband with a small decorative knot, laid in a loose circle",
    "K_FAS_02": "a single curved grey wolf claw with a small paper evidence tag tied to it with string",
    "K_FAS_03": "a pendant necklace on a dark cord, a black smooth stone disc with a faint engraved pattern barely visible on its back, unsettling cold sheen",
    "K_FAS_04": "a folded sheet of paper showing a charcoal-rubbed double ring pattern, smudged edges",
    "K_FAS_05": "a bundled set of ceremonial hunting tools, a short spear, a bone knife and a small snare tied together with a leather cord",
    "K_FAS_06": "a short carved bone ornament wound with red thread, hung on a thin leather thong",
    "K_FAS_07": "a necklace on a leather cord with a single claw-shaped pendant, pale bone color darkening to a black tip",
}

# 표시 순서 --- 제작 레시피에 쓰이는 것, 상점에 깔린 것, 낮은 티어를 앞에 둔다
CATEGORY_ORDER = ["제작 레시피 재료", "소모품", "재료", "도면", "열쇠", "기념품", "퀘스트", "장비"]


def read_rows(rel_path):
    with io.open(os.path.join(ROOT, rel_path), encoding="utf-8-sig", newline="") as fh:
        return list(csv.DictReader(fh))


def existing_icons():
    return {name[len("T_Item_"):] for name in
            (os.path.splitext(f)[0] for f in os.listdir(ICON_ASSET_DIR) if f.endswith(".uasset"))
            if name.startswith("T_Item_")}


CLOTH_PARTS = {"EQ_MArmor", "EQ_MHelmet", "EQ_MGloves", "EQ_MPants", "EQ_MBoots"}


def equip_prompt(row_name):
    family = re.sub(r"_\d+$", "", row_name)
    tier = row_name.split("_")[-1]
    family_text = EQUIP_FAMILY.get(family)
    tier_text = (TIER_CLOTH if family in CLOTH_PARTS else TIER_GEAR).get(tier)
    if family_text is None or tier_text is None:
        return None
    return f"{COMMON}, {family_text}, {tier_text}"


def category_of(row):
    item_type = row.get("ItemType", "")
    if item_type == "Equipment":
        return "장비"
    if item_type == "Material":
        return "재료"
    if item_type == "Blueprint":
        return "도면"
    if item_type == "KeyItem":
        return "열쇠"
    if item_type == "Keepsake":
        return "기념품"
    if item_type == "Quest":
        return "퀘스트"
    return "소모품"


def main(argv):
    shop_only = "--all" not in argv
    icons = existing_icons()
    items = read_rows("Scripts/Data/ItemTable.csv")
    equips = read_rows("Scripts/Data/EquipTable.csv")
    shop_refs = {r.get("RefId", "").strip() for r in read_rows("Scripts/Data/ShopItemTable.csv")}
    # 제작 레시피가 가리키는 재료·도면·결과물 (RefType은 무시 --- 아이템 표와 장비 표의 번호가 겹치지 않는다)
    craft_refs = set()
    for r in read_rows("Scripts/Data/CraftRecipeTable.csv"):
        craft_refs.update(re.findall(r"RefId=(\d+)", ",".join(r.get(k, "") for k in ("Material1", "Material2", "Material3", "Material4"))))
        craft_refs.update(v for v in (r.get("ResultId", "").strip(), r.get("RequiredBlueprintId", "").strip()) if v)
    craft_refs.discard("0")

    pending, missing_text = [], []
    for row in items + equips:
        name = row["Name"].strip()
        if name in icons or name.startswith("E_DEV"):
            continue
        numeric_id = row.get("NumericId", "").strip()
        if shop_only and numeric_id not in shop_refs and numeric_id not in craft_refs:
            continue
        prompt = equip_prompt(name) if name.startswith("EQ_") else (
            f"{COMMON}, {INDIVIDUAL[name]}" if name in INDIVIDUAL else None)
        if prompt is None:
            missing_text.append(name)
            continue
        pending.append({
            "name": name,
            "display": row.get("DisplayName", name),
            "category": CATEGORY_ORDER[0] if numeric_id in craft_refs else category_of(row),
            "in_shop": numeric_id in shop_refs,
            "in_craft": numeric_id in craft_refs,
            "tier": name.split("_")[-1] if name.startswith("EQ_") else "",
            "prompt": prompt,
        })

    pending.sort(key=lambda e: (CATEGORY_ORDER.index(e["category"]), e["tier"],
                                not e["in_shop"], e["name"]))

    scope_note = ("상점 품목표(ShopItemTable)에 깔린 것과 제작 레시피 재료만" if shop_only
                  else "표에 있는 것 전부(상점에 없는 퀘스트·기념품·장비 상위 티어 포함)")
    lines = ["# 아이콘 대기 목록 (완성 프롬프트)", "",
             f"> 범위: **{scope_note}**. `python Scripts/BuildIconPrompts.py`(전체는 `--all`)가 표와 아이콘 폴더를 대조해 만든 파일. 아이콘을 채우고 다시 돌리면 줄어든다.",
             "> 프롬프트는 그대로 복사해 쓰면 된다(공통 스타일이 이미 앞에 붙어 있다). 배경은 투명(PNG 알파)으로 뽑는다 --- 체크무늬 같은 가짜 투명이 나오면 다시 뽑을 것.",
             "> 받은 PNG는 `T_Item_{행이름}.png`로 `ArtSource/ItemIcons_Pixel/`에 넣고 `python Scripts/NormalizeItemIcons.py`를 돌린 뒤 임포트한다.",
             "> 장비가 아닌 것의 모양은 리버브 「아이템 기획서」(기획안 > 상점 시스템)의 아이콘 컨셉 열을 따랐다. 제작 레시피에 쓰이는 것(◆)이 맨 앞 절이고 ★는 상점 품목이다.", ""]
    total = len(pending)
    counts = {}
    for entry in pending:
        counts[entry["category"]] = counts.get(entry["category"], 0) + 1
    lines.append(f"남은 것 **{total}종** --- " + " · ".join(f"{k} {counts[k]}" for k in CATEGORY_ORDER if k in counts))
    lines.append("")

    current = None
    for entry in pending:
        if entry["category"] != current:
            current = entry["category"]
            lines += ["", f"## {current} ({counts[current]}종)", ""]
        star = (" ◆" if entry["in_craft"] else "") + (" ★" if entry["in_shop"] else "")
        lines += [f"**T_Item_{entry['name']}** {entry['display']}{star}", "", "```", entry["prompt"], "```", ""]

    with io.open(OUT_PATH, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))

    print(f"{total}종을 {os.path.relpath(OUT_PATH, ROOT)}에 씀")
    for key in CATEGORY_ORDER:
        if key in counts:
            print(f"  {key:<5} {counts[key]:>3}종")
    shop_count = sum(1 for e in pending if e["in_shop"])
    craft_count = sum(1 for e in pending if e["in_craft"])
    print(f"  그중 상점에 깔린 것 {shop_count}종, 제작 레시피에 쓰이는 것 {craft_count}종")
    if missing_text:
        print("\n프롬프트 문구가 없는 행(이 파일의 INDIVIDUAL에 추가 필요):")
        for name in missing_text:
            print("  " + name)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
