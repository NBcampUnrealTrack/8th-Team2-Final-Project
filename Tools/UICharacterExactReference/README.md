# Reference presentation — character information only

Final source: `/Game/Developers/MinkiCho/Art/캐릭터_정보_통합_메뉴`.
`T_ReferenceAtlas.png` is a byte-identical exported original. The exact left emblem,
icons, Korean static labels, selected arrow, header/card/close artwork and visible
ornamental/background pixels are sampled directly from it, not redrawn or recolored.

`T_ReferenceRepaired.png` reconstructs the area hidden by the original pixel knight
and reflection. The base material uses it **only** within two feathered replacement
regions; outside those regions it samples the untouched original. Example nickname,
class, level, row values and EXP fill are removed in the material. Existing live
fields, original gameplay portrait and its live RT reflection replace them.
Thus this is not a claim that the complete game screenshot is pixel-identical:
actual sprite/outfit, real data, live-font glyphs and reconstructed hidden pixels differ.

The source coordinates remain 1448×1086, but the runtime stage width now follows
the viewport aspect ratio at a fixed 1086 design height (minimum 4:3). Wide
viewports have no navy side margins: the rail stays at the left edge, the original
close stays at the right edge and the original exhibit moves by half the
additional width. The card now follows the right edge with its original 25-design-
pixel outer inset (about 21px in the 1604x902 viewport). Its right perimeter is a
separate original-pixel strip. These elements keep their uniform scale;
the portrait is 82% of the previous size, still 0.75 aspect, scaled about its foot
at source y795. The complete exhibit (portrait, illustration, floor, reflection,
original 12 glints) moves down 28 design pixels as one group; its foot is now y823.
Reflection width follows the portrait and opacity is 0.5. Values use a
shared 20pt body size (19pt for the longer character ID), header name30/meta19.
A restrained mist-blue-gray/blue-ivory layered gradient extends the background;
the original top/bottom perimeter pixels remain untouched. The exhibit now includes
the original floor-orbit continuation through x1180 below the card, rather than
cropping it at x906. Its two horizontal edges are feathered.
The card uses a matching chamfered alpha contour and the close artwork a diamond
alpha contour, keeping the old rectangular background pixels off the cool surface.
Independent source tiles have a two-source-pixel underlap to hide Slate rounding
seams; the selected arrow is fully removed from the exhibit base so no detached
arrow tip remains when the exhibit moves horizontally.
An additional full-width environment layer continues the pearlescent reflective
floor and airborne gold light through both sides, under the original exhibit and
all controls. This is a background-only built-in imagegen asset; no character,
platform, card, emblem or text is baked into it. Its source and exact prompt are
`ArtSource/UI/CharacterWideAtmosphere_20261014/`. Eight glints are extracted from
this layer into alpha tiles and their original locations patched locally; their
centers follow normalized backdrop coordinates while the glints retain square
aspect ratio. The header blend is reduced for legibility, and the original outer
frame remains above the new background.
Character information alone uses this stage;
`NUCharacterReferenceLayout` relocates the existing navigation/close buttons and named
slot while active, restores their exact original parent/layout/transform/style when
leaving the tab or closing, and leaves custom registered pages in the legacy layout.
No duplicate button handlers or alternate page data model are introduced.

Original widget identities/GUIDs/graphs/bindings/animations remain. Header mirrors
sync when the existing data refresh finishes. The reference's copy icon is backed by
an optional native button that copies the live numeric character ID; it is disabled
for a missing ID. Clipboard contents were not overwritten during verification.

Background/orbit parallax does not start in this mode. Twenty isolated glints
(12 original and 8 from the wide backdrop) have independent alpha materials;
their static source locations are
patched with neighboring background samples so motion does not duplicate them.
Native reference motion uses phased 12–20.93s cycles, up to 1.65px horizontal and
3px vertical drift at 1086 design height, with opacity 0.68–1. The character,
reflection, plinth and controls keep their authored positions. Motion is discarded
and transforms/opacity reset on tab exit/close; reopening starts fresh.
Console controls: `nu.CharacterReference.Particles` (0/1),
`nu.CharacterReference.ParticleStrength` (0–1), `ParticlePixels` (default 3,
clamped 0–6), and `ParticlePeriod` (default 12, clamped 8–30), all using the same
`nu.CharacterReference.` prefix. These glints are hit-test invisible.
The character retains its normal idle. Old decoration remains archived in the hidden
legacy tree. Static artwork tiles are noninteractive; original buttons own input.
Static label tiles are the original Korean raster glyphs, not newly typeset localized
labels. Dynamic text remains live; original legacy labels still serve other tabs.

Authoring order: run `apply.py` **last**, after previous Polish/Depth/Detail/FinalReference
passes, with PIE stopped. Do not overwrite this result by running those older passes
alone. The first long authoring call returned a timeout after completing successfully;
the saved assets, later authoring and verification were confirmed.

Evidence: `Saved/UICharacterExactReference`: source-region manifest, binary/identity
backups, `verified.json`, final successful DLL build, six-tab/inventory selection/close
and layout-restoration QA, 1200×900 transient allocation, `final.png`. The smaller
allocation is not an OS-window resolution change. An initial accelerated build stalled;
restarting the task build with two workers completed, followed by the final DLL build.
PIE is stopped and preview preferences restored. No commit or push.

## Current background and layout override — 2026-10-14

The earlier multi-image background description below is historical. Character information now uses one seamless static background, `ArtSource/UI/CharacterUnifiedBackground_20261014/T_CharacterUnifiedBackground.png`, generated with built-in imagegen; its exact prompt and provenance are alongside it. The 20 animated glints, atmospheric dust, live pixel portrait, live reflection, and interactive UI remain separate. Both side panels use uniform 1.1 scale with equal 24-design-pixel outer insets. The exhibit is centered between them, and the header clears the enlarged rail. Native transforms and pivots restore on exit, preventing accumulated scaling. Run apply.py LAST. Existing binding, input, data, and lifecycle requirements below remain applicable. Validation: successful editor build, six-tab/inventory/reopen checks, wide and 4:3 allocations, effect reset and bounds checks; evidence in Saved/UICharacterUnified.
2026-10-14 follow-up: left rail is flush to x0/y0 and full design height again (right card retains24 inset); menu art and original hit widgets follow the rail. Header remains24 beyond rail and exhibit recenters between panels. Built-in imagegen local edit suppresses background illustration facial features; exact prompt prompt_faceless_exact.txt. Current equal-outer-inset statement above is superseded for the left rail only.

2026-10-14 single-contour override: ReferenceCardBase replaces atlas double outer and inner inset frames with one chamfered edge stroke; CardTitle shares the card top/left geometry with one contour. Existing labels/icons/data remain separate. RailSurface no longer draws an independent rectangle underneath the viewport frame; only its right divider remains. Evidence Saved/UICharacterSingleFrame (wide and transient 4:3 captures, material diagnostics and65 preservation records).

2026-10-14 wider navigation: the left rail is now 311.52 design units wide, a 1.2 multiplier on its previous 259.6 width. Its full height and edge attachment stay fixed. Only the panel, selected face, separators, and original button hit geometry widen; emblem, icons and labels keep uniform 1.1 scale. Each icon/label pair is centered as a group with a 12-source-unit gap and shared row center. The selected arrow uses a clean independent face so lettering never stretches. Header clearance remains24 and the exhibit recenters between rail and card. Native transform restoration prevents cumulative scaling after tab switches or reopening. Evidence: Saved/UICharacterRailWide.

Wider-rail verification passed: build, six tabs/inventory/reopen/restoration, 1604x902 viewport and transient1200x900 allocation, group alignment and particle lifecycle. Rail z3, selected face/separators4 and menu artwork5 keep glints behind navigation even at4:3. See Saved/UICharacterRailWide/final.png and QA/runtime.json.
