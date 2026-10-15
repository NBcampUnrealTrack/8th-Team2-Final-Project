# Character information depth preview

Only the character information exhibit opts into motion through `DepthExhibit`.
The menu shell, information card, character, floor and reflection stay fixed.
Existing transparent ornament atlas, paper texture and traveler illustration are reused.

Authoring: execute `apply.py` through the existing trusted Unreal Python bridge.
It creates three UI materials and adds clipped, non-interactive exhibit layers.
Runtime: `NUCharacterDepthMotion.cpp`; `NUCharacterInfoPage.cpp` starts/stops it.

In PIE, run `nu.CharacterMenu.Preview Character`, then compare:

```
nu.CharacterDepth.Idle 0
nu.CharacterDepth.Parallax 0
nu.CharacterDepth.Idle 1
nu.CharacterDepth.Parallax 1
```

`preview.py` starts a temporary floating PIE comparison in the test map, initially
static; stop PIE when finished. It restores launch mode, throttle and toggles,
and has a 240-second watchdog. Execute scripts through Unreal, not system Python.
`snapshot.py` captures transforms and native physical-pixel measurements.
Run `verify.py` with system Python after the recorded desktop/lifecycle sequence.

Independent tuning cvars and back-to-front layer values are in
`Docs/UI_GUIDE.md`, section "캐릭터 정보 배경 깊이 연출 (2026-10-14)".
Diagnostics, original WBP backup, screenshots, final 70-second video and build
evidence are in `Saved/UICharacterDepth/`. Saved artifacts are local, not packaged.
Keyboard was exercised; physical gamepad and a packaged build were not tested.
