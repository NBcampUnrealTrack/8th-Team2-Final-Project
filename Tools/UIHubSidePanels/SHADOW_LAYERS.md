# Independent hub shadows

Run `apply_shadow_layers.py` after `Tools/UILayoutContract/apply.py` and any older
hub art passes. It changes only the main hub and private sibling materials.
Backups and QA captures live in `Saved/UIHubShadows`.

| Widget | Parent / z | Purpose |
| --- | --- | --- |
| HubProfileContactShadow | HubCanvas / -2 | Soft paper silhouette behind the profile |
| HubBackplate | HubCanvas / -1 | Profile art without the extra fixed shader shade |
| HubProfileMovingShade | HubCanvas / 0 | Slowly moving diffuse shade, below live text |
| HubDeskContactShadow | HubDeskCanvas / -1 | Blurred alpha silhouette behind desk objects |
| HubDeskDecor | HubDeskCanvas / 0 | Wood desk art without the extra fixed shader shade |
| HubDeskMovingShade | HubDeskCanvas / 1 | Animated shade clipped to object alpha |
| HubDepthForeground | RootCanvas / 4 | Blurred moving shadow silhouette at screen edges |

All shadow widgets are HitTestInvisible. Original layout, native delegates,
labels, nine buttons and transparent gameplay center are preserved. No per-frame
Blueprint tick or pointer dependency: Time expressions use IgnorePause, so the
shadows continue moving while the menu pauses gameplay. Motion is periodic and
subtle; no widget geometry is animated.

Default controls: `ShadowOpacity=.16`, `MotionSpeed=.9`, `MotionPixels=8`,
`SwingDegrees=6.5`. Contact motion uses only15–30% of the pixel amplitude. Right
foreground shadows rotate about screen UV(1,0), with aspect-correct coordinates;
left-edge foreground remains stationary. Upper foreground uses `UpperShadowScale=1.3`
about the top-right pivot, swinging ±6.5 degrees in about10.6 seconds. Lower
foreground is rooted at its original bottom-right silhouette. It no longer
rotates: local x travel52 design pixels at .93*T, y travel24 at1.37*T,
phase2.1, plus mild shear .045*sin(.57*T+phase). Root weighting limits movement
at the bottom-right. Desk projection reuses this lower-only drift with x1.8/y.65
plane compression and the existing five-sample foliage alpha, clipped to the
stationary desk alpha (peak opacity .384). Upper rotation is independent.
Foreground opacity is twice the base strength. Samples reuse the existing RGBA silhouette, outputting
a cool shadow tint rather than foliage color. Existing object form/contact
shading painted into the source artwork remains; only the additional fixed
shader shadows are replaced. No bitmap repaint was required.

`qa_shadow_layers.py` checks nine entries, noninteractive decoration, close,
and captures two frames two seconds apart while gameplay is paused.
