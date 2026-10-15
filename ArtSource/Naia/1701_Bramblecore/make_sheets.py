"""브램블코어(1701) 프레임 PNG 를 동작별 가로 시트로 묶는다.

원본: Frames/Moss_<동작><1~8>.png  (256x256, 8장씩, 팔레트 PNG 섞임)
출력: Sheets/T_1701_Bramblecore_<동작>.png  (2048x256, 칸 256, RGBA)
      Sheets/sheets.json  (동작별 칸 크기 · 프레임 수 · 피벗)

피벗은 칸 안 좌표 (128, 발 바닥 한 줄 아래). 원본이 동작마다 바닥 높이가 달라서
(1페이즈 Idle · Stomp 만 32px 아래에 그려짐) 동작마다 따로 잰 값을 쓴다.
가로는 캔버스 가운데 — 발밑 소품(나무 공 · 신발)이 아래 줄에 걸려 발 중심을 재면 왼쪽으로 쏠린다.

    python ArtSource/Naia/1701_Bramblecore/make_sheets.py
"""

import json
from pathlib import Path

from PIL import Image

HERE = Path(__file__).parent
CELL = 256
FRAMES = 8

# 원본 이름 -> 시트 이름 (원본의 Evolutiont 오타는 원본 그대로 읽는다)
ANIMS = [
    ("Prewake", "Prewake", "평시 — 잠든 언덕, 등에 생활 흔적"),
    ("Rise", "Rise", "평시 -> 전투, 일어남"),
    ("Idle", "Idle", "1페이즈 전투 대기 (Lv10 대련)"),
    ("Stomp", "Stomp", "1페이즈 공격 (Lv10 대련)"),
    ("Evolutiont", "Evolution", "1 -> 2페이즈 전환, 등의 소품이 떨어져 나감"),
    ("Evolved_Idle", "EvolvedIdle", "2페이즈 전투 대기 (레이드)"),
    ("Double_Stomp", "DoubleStomp", "2페이즈 공격 (레이드)"),
]


def main():
    out_dir = HERE / "Sheets"
    out_dir.mkdir(exist_ok=True)
    meta = {}

    for source, name, note in ANIMS:
        sheet = Image.new("RGBA", (CELL * FRAMES, CELL), (0, 0, 0, 0))
        bottom = 0
        for index in range(FRAMES):
            frame = Image.open(HERE / "Frames" / f"Moss_{source}{index + 1}.png").convert("RGBA")
            assert frame.size == (CELL, CELL), (source, index, frame.size)
            sheet.alpha_composite(frame, (index * CELL, 0))
            if index == 0:
                bottom = frame.getbbox()[3]  # 마지막 불투명 줄 + 1

        file_name = f"T_1701_Bramblecore_{name}.png"
        sheet.save(out_dir / file_name)
        meta[name] = {
            "texture": file_name,
            "source": f"Moss_{source}1~{FRAMES}.png",
            "note": note,
            "cell": [CELL, CELL],
            "frames": FRAMES,
            "pivot_in_cell": [CELL // 2, bottom],
            "pivot_uv": [0.5, round(bottom / CELL, 4)],
        }
        print(f"{file_name}  pivot (128, {bottom})  {note}")

    (out_dir / "sheets.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
