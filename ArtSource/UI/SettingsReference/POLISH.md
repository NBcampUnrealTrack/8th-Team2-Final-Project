# Settings detail polish, 2026-10-07

Source: exported original T_Settings_Frame, edited with built-in imagegen into T_Settings_FramePolished.png (1536x1024 RGBA). Original source and key-settings texture preserved.

Prompt: preserve exact panel/banner composition, one top compass only, faint lower-right watermark and leaves; repair chipped double borders, fragmented banner outlines and messy compass rings into clean continuous lines. Restrained illustrated navy #16293D and champagne #C8B691, soft shading/grain, no distress, no text/controls/new motifs, transparent exterior.

Runtime uses M_OptionsPolishedFrame with smooth palette remapping that retains neutral metal highlight pixels and the source alpha. The source's RGB outside alpha is not rendered.

Final authoring: Tools/UISettingsReference/apply_detail_polish.py after apply_palette_pilot.py. Popup buttons reuse the real MI_Alert_Button / M_HUDCutFrame parent via private normal/hover/pressed instances in SettingsPalettePilot. Footer and key-settings launcher remove stacked style/slot vertical padding, center the slot, and apply -1 design pixel optical text offset. Compact radio cells use the same frame behind existing clickable controls; selected overlay is beneath text. No shared popup/button/key-settings assets are edited.

Evidence and original asset backups: Saved/UISettingsPolish.

The popup parent is privately duplicated as M_OptionsPopupFrame; its secondary inner line fades out below 42px height to protect compact option labels. Small idle choices have a quieter rim, selected choices have brighter fill and full rim. Gold primary buttons use dark ink outlines for visible framing.

2026-10-07 outer-frame correction: T_KeySettings_FrameRounded.png is a built-in imagegen edit of T_Settings_FramePolished.png. Prompt: preserve canvas/bounds/banner/compass/watermark and alpha; replace irregular vine-like scalloped corner contours with smooth quarter-circle arcs tangent to straight uniform gold rails, retain sharp four-point stars. Generated source: exec-4a42265b-e11c-47e4-97e5-72067358b504.png. Imported only through private M_KeySettingsRoundedFrame for the key window. Exterior alpha checked at corners/top; no raster postprocessing.

2026-10-07 banner revision: T_Settings_BannerLaurel.png generated with built-in imagegen from T_KeySettings_FrameRounded.png (exec-b2549871-4dd0-49eb-b1f2-22c4e600b56e.png). Prompt replaces scattered tiny leaves with a broad lower-banner champagne laurel arrangement; simplifies bottom to a single navy folded tail with gold piping, removes solid gold flap and dangling star. Full image retained as source, but M_KeySettingsGeometricFrame samples the new image ONLY within x<192,y186..760 with feathered boundaries. The geometric frame, compass and lower-right art keep their existing sources. Both options and keys share this material.

2026-10-07 celestial banner: T_Settings_BannerCelestial.png is an imagegen edit of BannerLaurel (exec-7013cd44-e058-47fd-b395-05f74facd901.png). Only banner botanical motif replaced by connected champagne stars, fine orbit arcs and a diagonal illustrated Milky Way star stream; navy single folded tail preserved. Runtime samples only the existing local banner mask, retaining analytic frame, compass and original panel/watermark. No raster postprocessing.
