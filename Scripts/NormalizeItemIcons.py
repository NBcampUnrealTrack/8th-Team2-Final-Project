# 아이템 아이콘 PNG 정규화 --- 생성기에서 받은 큰 이미지를 게임 아이콘 규격으로 맞춘다.
#   투명 배경 그림(9/11부터 이렇게 뽑는다):
#     1) 생성기가 남기는 아주 옅은 알파 잡티를 지운다
#     2) 알파를 살린 채 128x128로 축소 (곱한 알파로 줄여 투명한 곳의 검은 색이 가장자리로 번지지 않게)
#   배경이 꽉 찬 옛 그림:
#     1) 배경을 기존 아이콘과 같은 단색(#101925)으로 평탄화 (생성기는 비네트/그라데이션을 넣고 지정 색도 잘 안 지킨다)
#     2) 128x128로 축소
#   칸은 72px이라 128보다 크면 용량만 늘어난다 (1254px 원본 기준 애셋 1.5MB => 20KB).
# 원본은 ArtSource/ItemIcons_Pixel/_src_hires/ 에 남긴다 (커밋 제외, .git/info/exclude).
#
# 사용법:
#   python Scripts/NormalizeItemIcons.py                    # ArtSource/ItemIcons_Pixel 안의 128보다 큰 PNG 전부
#   python Scripts/NormalizeItemIcons.py T_Item_C_VIT_03    # 이름 지정 (여러 개 가능)
#
# 처리 후 에디터에서 다시 임포트해야 애셋에 반영된다 (기존 애셋은 삭제 후 임포트 --- 덮어쓰기는 막혀 있다).
import os
import shutil
import sys
from collections import Counter

from PIL import Image, ImageChops, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICON_DIR = os.path.join(ROOT, "ArtSource", "ItemIcons_Pixel")
BACKUP_DIR = os.path.join(ICON_DIR, "_src_hires")
BG = (0x10, 0x19, 0x25)   # 기존 아이콘 배경색
SIZE = 128
FILL_THRESHOLD = 42       # 배경 그라데이션은 먹고 아이템 외곽선은 넘지 않는 값
MARKER = (255, 0, 255)    # 배경을 표시할 임시 색 --- 아이콘에 나올 일 없는 값이면 된다
ALPHA_NOISE = 24          # 투명 그림에서 이보다 옅은 알파는 잡티로 본다 (말풍선 이미지에서 1~32짜리가 넓게 흩어져 있었다)
MIN_CLEAR_RATIO = 0.2     # 가운데 놓인 아이콘이면 칸의 이만큼은 비어 있어야 한다 --- 모자라면 배경이 덜 빠진 그림


def flatten_background(image):
    """가장자리에서 흘려 채워 배경을 BG 단색으로. 어디까지 배경으로 봤는지 마스크로 함께 돌려준다.

    BG로 바로 칠하면 안 된다 --- PIL의 floodfill은 시작 픽셀이 채울 색과 thresh 안쪽이면
    "이미 칠해져 있다"고 보고 그냥 돌아간다. 배경이 이미 BG에 가까운 그림(#111D28 등)은
    그래서 손도 못 대고 통과한다. 원본에 없을 마커 색으로 칠한 뒤 BG로 바꾼다.
    """
    width, height = image.size
    seeds = ((0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1),
             (width // 2, 0), (width // 2, height - 1), (0, height // 2), (width - 1, height // 2))
    for seed in seeds:
        ImageDraw.floodfill(image, seed, MARKER, thresh=FILL_THRESHOLD)
    mask = ImageChops.difference(image, Image.new("RGB", image.size, MARKER))         .convert("L").point(lambda v: 255 if v == 0 else 0)
    return Image.composite(Image.new("RGB", image.size, BG), image, mask), mask


def has_transparency(image):
    """배경을 투명으로 뽑은 그림인지 --- 절반 넘게 비치는 픽셀이 하나라도 있으면 투명 그림으로 본다.
    체크무늬로 투명을 흉내 낸 그림은 알파가 없어 여기서 걸러지고, 옛 경로에서 "배경 확인 필요"로 드러난다."""
    if image.mode not in ("RGBA", "LA", "P"):
        return False
    return image.convert("RGBA").getchannel("A").getextrema()[0] < 128


def normalize(path):
    """정규화하고 (원본 크기, 배경 종류, 확인 값, 문제없음 여부, 용량)을 돌려준다."""
    name = os.path.splitext(os.path.basename(path))[0]
    backup = os.path.join(BACKUP_DIR, name + ".png")
    if not os.path.exists(backup):
        os.makedirs(BACKUP_DIR, exist_ok=True)
        shutil.copy2(path, backup)

    source = Image.open(backup)
    source_size = source.size[0]

    if has_transparency(source):
        image = source.convert("RGBA")
        image.putalpha(image.getchannel("A").point(lambda v: 0 if v < ALPHA_NOISE else v))
        if image.width != image.height:
            # 정사각형이 아니면(K_FAS_03이 1246x1262였다) 투명 여백을 붙여 가운데 둔다 --- 그대로 줄이면 한쪽으로 눌린다
            side = max(image.size)
            square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
            square.paste(image, ((side - image.width) // 2, (side - image.height) // 2))
            image = square
        small = image.convert("RGBa").resize((SIZE, SIZE), Image.LANCZOS).convert("RGBA")
        small.save(path, optimize=True)

        alpha = small.getchannel("A")
        corner_alpha = alpha.getpixel((0, 0))
        clear_ratio = alpha.histogram()[0] / (SIZE * SIZE)
        detail = f"모서리 알파 {corner_alpha}, 빈 곳 {clear_ratio:.0%}"
        return source_size, "투명", detail, corner_alpha == 0 and clear_ratio >= MIN_CLEAR_RATIO, os.path.getsize(path)

    image, mask = flatten_background(source.convert("RGB"))
    small = image.resize((SIZE, SIZE), Image.LANCZOS)
    # 줄이면 LANCZOS가 이웃 픽셀을 섞어 배경이 미세하게 흔들린다.
    # 배경 마스크도 같이 줄여 "확실히 배경인 곳"만 정확한 BG로 되돌린다(경계는 그대로 둬 계단이 생기지 않게).
    small_mask = mask.resize((SIZE, SIZE), Image.LANCZOS).point(lambda v: 255 if v >= 250 else 0)
    small = Image.composite(Image.new("RGB", small.size, BG), small, small_mask)
    small.save(path, optimize=True)

    corner = small.load()[0, 0]
    common = Counter(small.getdata()).most_common(1)[0][0]
    detail = "모서리 #%02X%02X%02X, 최빈색 #%02X%02X%02X" % (*corner, *common)
    return source_size, "남색", detail, corner == BG and common == BG, os.path.getsize(path)


def main(argv):
    if argv:
        targets = [os.path.join(ICON_DIR, n if n.endswith(".png") else n + ".png") for n in argv]
    else:
        targets = []
        for entry in sorted(os.listdir(ICON_DIR)):
            if not entry.endswith(".png"):
                continue
            full = os.path.join(ICON_DIR, entry)
            if Image.open(full).size[0] > SIZE:
                targets.append(full)

    if not targets:
        print("정규화할 PNG가 없습니다 (모두 128px 이하)")
        return 0

    print(f"{'파일':<28} {'원본':<8} {'배경':<4} 확인 값")
    problems = 0
    for path in targets:
        if not os.path.exists(path):
            print(f"{os.path.basename(path):<28} (없음)")
            problems += 1
            continue
        source_size, kind, detail, ok, size = normalize(path)
        mark = "" if ok else "   <= 배경 확인 필요"
        if mark:
            problems += 1
        print("%-28s %-8s %-4s %s   %dKB%s" % (os.path.basename(path), f"{source_size}px", kind, detail, size // 1024, mark))
    print(f"\n{len(targets)}장 처리, 확인 필요 {problems}장. 원본은 {os.path.relpath(BACKUP_DIR, ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
