"""Shared helpers for the character-menu card dashboard pass (concept C, 2026-10-14).

Palette follows the current HUD2026 / glass character select (sRGB):
glass #0E1A28, card #0C1622, text #F1EDE6, muted #AAB6BF, gold #D6BE8C,
cream #E1D8C3, navy #16293D, SP blue #A6DCF2, HP green #7FCB8F.
"""
import unreal

WS = unreal.WidgetService
FONT_BOLD = "/Game/Art/Font/MaruBuri-Bold_Font.MaruBuri-Bold_Font"
FONT_SEMI = "/Game/Art/Font/MaruBuri-SemiBold_Font.MaruBuri-SemiBold_Font"
RADAR_MATERIAL = "/Game/Art/UI/CharacterMenuCard/M_CharMenuCard_StatRadar.M_CharMenuCard_StatRadar"


def _lin(v):
    v = v / 255.0
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def color(hex_rgb, a=1.0):
    h = hex_rgb.lstrip("#")
    r, g, b = (_lin(int(h[i:i + 2], 16)) for i in (0, 2, 4))
    return f"(R={r:.6f},G={g:.6f},B={b:.6f},A={a:.6f})"


GLASS, CARD, TEXT, MUTED, GOLD = "0E1A28", "0C1622", "F1EDE6", "AAB6BF", "D6BE8C"
CREAM, NAVY, BLUE, GREEN = "E1D8C3", "16293D", "A6DCF2", "7FCB8F"


def rounded(hex_rgb, a, radius, outline_hex=None, outline_a=0.0, outline_w=0.0, half_height=False):
    rounding = "HalfHeightRadius" if half_height else "FixedRadius"
    oc = color(outline_hex or hex_rgb, outline_a)
    return (f"(ResourceObject=None,TintColor=(SpecifiedColor={color(hex_rgb, a)}),DrawAs=RoundedBox,ImageSize=(X=32.000000,Y=32.000000),"
            f"OutlineSettings=(CornerRadii=(X={radius},Y={radius},Z={radius},W={radius}),Color=(SpecifiedColor={oc}),"
            f"Width={outline_w},RoundingType={rounding}))")


class Builder:
    def __init__(self, path):
        self.path = path
        self.failed = []

    def ok(self, result, what):
        if result is False or (hasattr(result, "success") and not result.success):
            self.failed.append(what)

    def exists(self, name):
        return WS.widget_exists(self.path, name)

    def add(self, kind, name, parent):
        if self.exists(name):
            return name
        self.ok(WS.add_component(self.path, kind, name, parent, True), f"add {name}")
        return name

    def prop(self, name, key, value):
        self.ok(WS.set_property(self.path, name, key, str(value)), f"{name}.{key}")

    def rect(self, name, x, y, w, h, z=None):
        for k, v in (("Position X", x), ("Position Y", y), ("Size X", w), ("Size Y", h)):
            self.prop(name, k, v)
        if z is not None:
            self.slot(name).set_z_order(z)

    def slot(self, name):
        """Canvas slot of a designer widget (WidgetService has no z-order/alignment alias)."""
        asset = self.path.rsplit("/", 1)[1]
        w = unreal.find_object(None, f"{self.path}.{asset}:WidgetTree.{name}")
        if w is None or w.slot is None:
            raise RuntimeError(f"no slot for {name}")
        return w.slot

    def vis(self, name, value):
        self.prop(name, "Visibility", value)

    def image(self, name, parent, x, y, w, h, brush, z=None, vis="HitTestInvisible"):
        self.add("Image", name, parent)
        self.prop(name, "Brush", brush)
        self.rect(name, x, y, w, h, z)
        self.vis(name, vis)
        return name

    def text(self, name, parent, x, y, w, h, value, size, hex_rgb=TEXT, a=1.0, bold=False, justify="Left",
             spacing=0, z=None):
        self.add("TextBlock", name, parent)
        if value is not None:
            self.prop(name, "Text", value)
        f = unreal.WidgetFontInfo()
        f.font_family = FONT_BOLD if bold else FONT_SEMI
        f.typeface = "Default"
        f.size = size
        f.letter_spacing = spacing
        f.color = color(hex_rgb, a)
        self.ok(WS.set_font(self.path, name, f), f"font {name}")
        self.prop(name, "ColorAndOpacity", f"(SpecifiedColor={color(hex_rgb, a)})")
        self.prop(name, "Justification", justify)
        self.rect(name, x, y, w, h, z)
        self.vis(name, "HitTestInvisible")
        return name

    def bar(self, name, parent, x, y, w, h, fill_hex, z=None):
        self.add("ProgressBar", name, parent)
        style = (f"(BackgroundImage={rounded(TEXT, 0.10, h / 2, half_height=True)},"
                 f"FillImage={rounded(fill_hex, 1.0, h / 2, half_height=True)},"
                 f"MarqueeImage={rounded(fill_hex, 1.0, h / 2, half_height=True)})")
        self.prop(name, "WidgetStyle", style)
        self.prop(name, "FillColorAndOpacity", "(R=1.000000,G=1.000000,B=1.000000,A=1.000000)")
        self.rect(name, x, y, w, h, z)
        self.vis(name, "HitTestInvisible")
        return name

    def canvas(self, name, parent, x, y, w, h, z=None, vis="SelfHitTestInvisible"):
        self.add("CanvasPanel", name, parent)
        self.rect(name, x, y, w, h, z)
        self.vis(name, vis)
        return name

    def card(self, name, parent, x, y, w, h, caption, z=1, fill_a=0.86):
        """Glass card: 1px ivory hairline outline around a deep navy fill, gold caption."""
        c = self.canvas(name, parent, x, y, w, h, z)
        self.image(f"{name}Fill", c, 0, 0, w, h, rounded(CARD, fill_a, 14, TEXT, 0.11, 1.0), z=0)
        if caption:
            self.text(f"{name}Caption", c, 24, 18, w - 48, 24, caption, 13, GOLD, bold=True, spacing=40, z=2)
        return c

    def compile_save(self):
        bp = unreal.load_asset(self.path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset(self.path, only_if_is_dirty=False)
