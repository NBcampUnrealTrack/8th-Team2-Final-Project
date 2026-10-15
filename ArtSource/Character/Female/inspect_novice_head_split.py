from PIL import Image
import numpy as np

base = "ArtSource/Character/Female"
orig = "Saved/FemaleSpriteBackup_20261014"

# Let's check in Saved/FemaleSpriteBackup_20261014 how Novice f0..f7 was drawn in Side and Back!
for anim in ["Idle_Front", "Walk_Front", "Idle_Side", "Walk_Side", "Idle_Back", "Walk_Back"]:
    nov = np.array(Image.open(f"{base}/WCh_Body_{anim}_Sheet.png").convert("RGBA"))
    o_nov = np.array(Image.open(f"{orig}/WCh_Body_{anim}_Sheet.png").convert("RGBA"))
    print(f"=== {anim} ===")
    for f in range(8):
        nf = nov[:, f*60:(f+1)*60]
        onf = o_nov[:, f*60:(f+1)*60]
        # Where did improve_female_sprites.py modify pixels in y=30..50 compared to o_nov?
        # Wait: improve_female_sprites.py shifted idle frames by 1px vertically for breathing AND swayed ponytail by dx=+1/-1!
        # Let's find the vertical shift dy_orig between nf and onf for the upper torso (y=32..48)!
        best_dy = 0
        best_match = -1
        for d in range(-2, 3):
            s_onf = np.zeros_like(onf)
            y0, y1 = max(0, -d), min(100, 100-d)
            s_onf[y0+d:y1+d] = onf[y0:y1]
            m = int((np.all(nf[32:48] == s_onf[32:48], axis=2) & (nf[32:48, :, 3] > 0)).sum())
            if m > best_match:
                best_match = m
                best_dy = d
        print(f"  f{f}: best_dy_vs_orig={best_dy:+d}, torso_exact={best_match}")
