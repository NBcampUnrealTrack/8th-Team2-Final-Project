# Reference detail pass

Scope: shared CharacterMenu shell and character information page. The uploaded
`캐릭터_정보_통합_메뉴` is a visual reference only, never a production screenshot layer.

Run `apply.py` through trusted Unreal Python with PIE stopped. Run it **after**
the historical `UICharacterPolish` and `UICharacterDepth` passes. Re-running those
older passes alone overwrites this finish. This pass is idempotent; it preserves
existing widget names/parents, graph/binding/animation and portrait RT ownership.

- Shared rail: one selected-arrow face, consistent icon bounds, detailed helmet
  and stat bars, existing compass and live navigation colors.
- Header: live nickname/class/level hierarchy, separate metadata divider,
  restrained rule and close-button normal/hover/pressed brushes in its hit bounds.
- Information card: continuous chamfered outer panel, inset header, separate
  quieter rounded body, restrained single shadow, aligned live fields and EXP.
- Exhibit: clean concentric orbit with pointer overscan; separate fixed plinth
  face/side/rims. Existing sprite/idle/reflection and depth lifecycle stay intact.

Eleven role-specific UI materials live in `Art/CharacterMenu/Detail20261014`.
Text, buttons, icons and dividers stay separate widgets. No new lore emblem,
particle, bloom, or gameplay item is added.

`verify.py` runs after PIE stops. Runtime evidence is in `Saved/UICharacterDetail`:
original binary/identity backups, shader/preservation report, six-tab navigation,
real inventory-cell selection, header close, transient 1200×900 menu allocation,
static/moving transform comparison, and `before.png`/`final.png` screenshots.
The smaller allocation is not an OS viewport-resolution test. Artwork changes
do not make the screen the final approved project-wide design standard.
