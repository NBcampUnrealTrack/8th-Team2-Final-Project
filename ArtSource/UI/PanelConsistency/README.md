# Menu palette art — 2026-10-06

Generated with imagegen using the existing hub backplate and `SettingsReference/T_Settings_Frame.png` as references.

- `T_MenuHub_SettingsNavy.png`: existing hub geometry, transparent exterior, empty portrait ring and profile rows preserved; ivory main panel replaced with settings navy; ribbon compass replaced with the settings-style ringed eight-point star.
- `T_Settings_Compass.png`: isolated gold ringed eight-point compass with transparent spaces, for the hub title ornament. Imported at a maximum size of 256 for its small on-screen placement.

Both import into `/Game/Developers/MinkiCho/Art/UITheme/SettingsNavy`. UI compression, sRGB, no mipmaps, alpha preserved. Other menus retain their original artwork and use material/widget color changes only.

`T_MenuHub_RightCompass.png` (2026-10-06): edited with the built-in imagegen tool using `ArtSource/UI/GameMenuHub/T_GameMenuHub_GoldPattern.png`. Prompt: preserve canvas/frame/ivory/alpha; remove the ivory panel's left cropped compass and foliage; move the right compass left until approximately 90% of its circle is visible, retaining faint color and bottom-right foliage. The runtime `M_Hub_RightCompass` combines this right panel with the previously approved navy left section, preserving the new banner compass. No generated text or interactive controls are baked into the image.
