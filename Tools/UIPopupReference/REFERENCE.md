# Popup reference — 2026-10-12

Status: implemented in WBP_ConfirmDialog; runtime verification follows below.
Editor asset search verified `/Game/Developers/MinkiCho/Art/팝업창_디자인_개선.팝업창_디자인_개선` (Texture2D).
Source image inspected: `C:/Users/user/Downloads/팝업창 디자인 개선.png`.

## Scope

Restyle `/Game/Developers/dbals/UI/WBP_ConfirmDialog` to the central confirmation dialog in the supplied reference. Preserve native confirm/cancel callbacks, dynamic message and button labels, intent colors, focus navigation, and notice-only behavior. Do not propagate to count/reward/settings/shop windows without a matching layout requirement.

## Reference measurements

Coordinates are approximate initial measurements in the 1672×941 reference; refine against runtime captures before claiming a match.

| Element | Reference bounds / position |
| --- | --- |
| Outer frame | x514..1158, y264..600; 644×336 |
| Center stars | centers x836, y266 / y597; extend beyond frame |
| Header emblem | x542..584, y290..334 |
| Heading | x598, y298; gold `확인` |
| Header X | x1101..1122, y300..320 |
| Header rule | x547..1126, y342; center diamond x836 |
| Message | centered around x836, y432; ivory `게임을 종료할까요?` |
| Watermark | right body, approximately x976..1150, y349..522 |
| Primary button | x550..820, y507..567; 270×60 |
| Secondary button | x850..1122, y507..567; 272×60 |
| Button gap | approximately 30 |

Use one centered panel origin and uniform aspect-preserving scaling. Reference placement is vertically above screen center; do not silently recenter it. Prefer original reference artwork for ornament fidelity. Keep labels live and keep interactive geometry coincident with visible controls. Source screenshot includes the world, HUD and baked labels: it must not be used as one full-screen replacement image.

## Existing structure and required changes

Historical backup `Saved/UIPopupPalette/20261008_182403/WBP_ConfirmDialog.json` records PanelBG 560×306 and ConfirmLayout; query the live editor before mutation because the backup is not authoritative.
Preserve MessageText, ConfirmButton, CancelButton and label bindings. Existing AlertSeal/AlertHeading/AlertHeaderRule can host corresponding new visuals. Remove or hide the existing footer separator because the reference has no equivalent. Header X is absent in that backup and needs a real cancel action binding. Check NUConfirmDialog NativeConstruct intent tint before choosing primary material colors to avoid multiplying red twice.

## Completion checks

Back up current binary assets before editing. Compile/save changed WBP and private materials. Capture the actual quit-confirmation path without accepting quit. Compare dialog crop, frame corners, ornaments, text baselines, watermark and button geometry against the reference at matching scale. Verify cancel, header X, normal/hover/pressed geometry and a second aspect ratio. Record any remaining mismatch honestly; do not claim zero error from approximate measurements alone.

## Final implementation (2026-10-12)

Run `apply.py` through `revvy_project.ps1 -Full`, followed by runtime `qa.py`.
Only WBP_ConfirmDialog and private materials in Art/UITheme/PopupReference change.
The uploaded texture is sampled directly for the original frame, stars, seal,
header rule, watermark, button silhouettes and close icon. Shader UV remapping
removes baked text; localized runtime labels remain separate. No source texture
or unrelated popup is modified. The stage is 1672x941 with uniform ScaleToFit;
panel artwork bounds are x510,y248,width652,height366, including star overflow.
Button bounds are 270x60 and 272x60 with a 30-unit gap.

PanelIntro's margin defaults and height keys are updated together with the
layout; otherwise the animation restores the old 560x306 geometry at runtime.
Header X binds OnClicked through a Blueprint delegate and calls native Cancel.
WidgetService.bind_event is a logging-only stub in this installed bridge and
must not be used as evidence of an actual event connection.

Backups are under Saved/UIPopupReference/<timestamp>; the earliest backup is
the original widget, before any authoring mutations. Final compile has zero
errors/warnings. Runtime evidence is under Saved/UIPopupReference/QA.
Live font uses the existing MaruBuri-SemiBold font. The screenshot does not
provide an editable font specification or clean art layers, so raster text
removal and live font rendering are not a claim of pixel-perfect identity.
The second ratio check is a transient 1280x960 widget layout area, not an
actual operating-system viewport resize. Pointer hover is not proven by a
forced hover style preview.

Final QA: native quit path, cancel, header X, sample confirmation without a
quit callback and notice-only collapse passed (runtime.json 13:16:38).
Reviewed quit_normal00005.png, hover_style00001.png and area_4x300001.png.
Saved widget reloaded and compiled/saved without errors/warnings after editor
restart. The earlier teardown crash came from a transactional property edit
on a PIE-only widget; QA now avoids that service for live objects. The final
run ended PIE normally, and the editor remained connected (PIE_STOPPED True).

Border fill correction (15:17): the first repair selected a disconnected stale
Custom expression. fill_panel.py now traces the actual Opacity output through
its mask to the connected Custom expression and verifies that connection after
saving. quit_normal00007.png shows the navy fill at x499/500 where the previous
capture showed gameplay pixels; existing interior pixels remain unchanged.
The final runtime QA passed at15:18:25. Do not select nodes by ObjectIterator
order; removed or disconnected expressions can share the same material outer.

User resize (15:21): PanelBG uniformly scales .75 around its center, retaining
the reference stage's resolution scaling. This restores approximately460px
visible frame width at1604x900, matching the preceding popup's footprint.
All child visuals and hit regions scale together. resize_panel.py modifies
only the WBP; canonical apply.py retains the scale. Runtime cancel/X/confirm
and notice checks passed. WBP_QuestJournal_C was also visible in this test-world
run; its background is outside the confirmation-popup change.

2026-10-13: Production artwork is now T_ConfirmPopupAtlas in PopupReference/Textures. The atlas crops source rectangle (502,240)-(1170,622), retaining 8px sampling padding. Eight materials remap original pixel coordinates into this 668x382 atlas. Original concept Texture2D deleted after confirming zero referencers. Source PNG: ArtSource/UI/ConfirmPopup/T_ConfirmPopupAtlas.png.
