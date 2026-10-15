# 트래커 전투대기 머리·얼굴 시트 만들기 --- 원본(몸 한 장)에는 머리가 없어서 노비스 전투대기 머리를 얹는다.
# 경환 님 확인(9/23): 트래커 전투대기 머리는 노비스와 같은 그림이다. 실제로 트래커 공격 0프레임 머리와
# 노비스 전투대기 머리가 634픽셀 중 624가 같고, 트래커 전투대기 몸 0프레임은 공격 0프레임 몸과 같은 자리다.
#
# 자리: 노비스 칸(57x94) 좌표 + (28, 13) = 트래커 칸(128x112)의 공격 0프레임 머리 자리.
# 흔들림: 노비스 머리 자체의 위아래(0,0,1,2,2,1,0,0 아래로)를 빼고, 트래커 몸 목 부분의 움직임
#         (0,-1,-1,-1,0,+1,+1,+1, 음수 = 위)을 더한다. 4프레임 눈 깜빡임은 노비스 그림 그대로 살린다.
# 실행: py -3 make_combat_idle_head.py  (ArtSource/Character/Tracker 에서)
from PIL import Image

NOVICE = "../Novice/"
CELL_N = (57, 94)
CELL_T = (128, 112)
FRAMES = 8
OFFSET = (28, 13)
NOVICE_BOB = [0, 0, 1, 2, 2, 1, 0, 0]
TRACKER_NECK = [0, -1, -1, -1, 0, 1, 1, 1]

LAYERS = (("MCh_Hair1_Combat_Idle_Sheet.png", "MCh_Tracker_Hair_Combat_Idle_Sheet.png"),
          ("MCh_FaceEye_Combat_Idle_Sheet.png", "MCh_Tracker_FaceEye_Combat_Idle_Sheet.png"))

for src_name, out_name in LAYERS:
    src = Image.open(NOVICE + src_name).convert("RGBA")
    out = Image.new("RGBA", (CELL_T[0] * FRAMES, CELL_T[1]), (0, 0, 0, 0))
    for k in range(FRAMES):
        cell = src.crop((k * CELL_N[0], 0, (k + 1) * CELL_N[0], CELL_N[1]))
        x = k * CELL_T[0] + OFFSET[0]
        y = OFFSET[1] - NOVICE_BOB[k] + TRACKER_NECK[k]
        out.alpha_composite(cell, (x, y))
    out.save(out_name)
    print("saved", out_name, out.getbbox())
