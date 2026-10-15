"""Rail hover/pressed states for the motion pass (2026-10-15). Safe to re-run.
Normal stays transparent (the selected item draws its own highlight); hover lightly fills the row."""
import importlib, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

PATH = "/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterMenuBase"
NAV = ("Character", "Equipment", "Inventory", "Stats", "Traits", "Naia")


def run():
    b = Builder(PATH)
    clear = rounded(TEXT, 0.0, 10)
    style = (f"(Normal={clear},Hovered={rounded(TEXT, 0.06, 10, TEXT, 0.10, 1.0)},Pressed={rounded(GOLD, 0.12, 10, GOLD, 0.35, 1.0)},"
             f"Disabled={clear},NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),PressedPadding=(Left=0,Top=0,Right=0,Bottom=0))")
    for key in NAV:
        b.prop(f"Nav{key}Button", "WidgetStyle", style)
        # The travelling NavSlider replaces each item's own highlight.
        for part in ("SelectedBody", "SelectedBar"):
            if b.exists(f"Nav{key}{part}"):
                b.vis(f"Nav{key}{part}", "Collapsed")

    # One selection pill behind the rail buttons; C++ slides it to the selected item (NUCharacterMenuNav).
    x, y = 52, 112  # NavCharacterButton position; C++ offsets by render translation
    b.canvas("NavSlider", "ShellCanvas", x, y, 256, 58, vis="HitTestInvisible")
    b.slot("NavSlider").set_z_order(8)
    b.image("NavSliderBody", "NavSlider", 0, 0, 256, 58, rounded(TEXT, 0.09, 10, TEXT, 0.10, 1.0), z=0)
    b.image("NavSliderBar", "NavSlider", 0, 15, 4, 28, rounded(GOLD, 1.0, 2), z=1)
    b.compile_save()
    print("failed:", b.failed)


run()
