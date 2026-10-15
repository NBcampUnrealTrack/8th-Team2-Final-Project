# Rich final-reference character information finish

This pass follows the user's latest explicit art direction: preserve the rich
JRPG reference atmosphere and ornament density. It supersedes the earlier quiet
ivory background / plain concentric-orbit finish. Run `apply.py` through Unreal
Python with PIE stopped, **after** the Polish, Depth and Detail passes.

Reference: `/Game/Developers/MinkiCho/Art/캐릭터_정보_통합_메뉴`.
Production sources: `ArtSource/UI/CharacterFinalReference_20261014/`.
Assets: `Art/CharacterMenu/FinalReference20261014` (2 textures, 10 UI materials).

Independent composition:

1. Cool global Blue Mist background, fixed; no UI or world objects baked in.
2. Far translucent blue haze, existing independently tuned depth controller.
3. Existing low-contrast traveler illustration, retained as original artwork.
4. Transparent rich rear orbit above the illustration, below the pixel portrait.
   Fine circular/broken/tilted elliptical curves, vertical guides and star knots.
   Generated texture alpha is palette-remapped to Muted Gold / Cream in its material.
5. Twenty-eight individual named `ReferenceLight_XX` widgets. Eight star glints,
   twenty light dots; each supports its own opacity/transform for later animation.
6. Fixed mirror plinth face/side/rims and original live portrait reflection RT.
   Reflection starts at the visible foot line, page y430. Character is untouched.
7. Independent crossing front ellipses through the existing depth motion owner.
8. Existing UI foreground, cream navigation arrow, navy/ivory floating card with
   continuous gold outer/inset chamfer strokes, corner cuts and separate live text.

All added decoration is HitTestInvisible and clipped to the exhibit. No buildings,
plants, flags, slogans, fake gameplay values or invented representative symbols.
Native input, bindings, page lifecycle and motion logic are unchanged.

The separately supplied ProjectM representative emblem could not be identified
in available assets. Its exact asset path was requested; the existing compass is
retained unchanged pending that source. This required detail remains unfinished.

Evidence: `Saved/UICharacterFinalReference/` contains original WBP backups,
preservation/material compile report, six-tab PIE / real inventory selection /
header close / transient 1200x900 allocation, fixed transform and motion-cap
checks, and `final00000.png`. Outside-pointer return was not confirmed in the last
desktop retest because the requested cursor route was not reached; previous
native depth lifecycle checks remain recorded in UICharacterDepth. No claim of
a fresh outside-return pass is made. PIE preview has a 240-second cleanup watchdog.
