# Hub entrance and exit

`NUGameMenuMotion` owns short, runtime-only presentation transforms. NativeConstruct starts it; NativeDestruct restores authored values and releases state. No WBP layout positions or button scales are changed.

- Left group: -20px to rest, opacity0->1, .25s cubic ease-out. Compass, title and footer share their existing group so their alignment stays together.
- Profile: +16px to rest, delay.03s, duration.25s.
- Nine generated entry widgets: -6px to rest, delay.04+.02*index, duration.18s. Animate the entry wrapper, leaving the inner .75-scale button and .97 hover animation independent.
- Desk: +10px vertically, delay.08s, duration.30s.
- Foreground shadows: opacity ramp with the desk, existing material motion continues.
- Close: .15s concurrent fade/outward movement, starting from current values even if entry was interrupted. No reverse stagger.

Slate pre-tick runs while gameplay is paused. The callback stops after the entrance settles. Exit disables the hub until complete; menu actions run only after the UI manager releases the hub's input block. CloseWindow intercepts the hub only, including Escape/close-button paths. Teardown bypasses deferred closing and NativeDestruct cancels pending actions.

Verification: `Tools/UIHubSidePanels/qa_hub_motion.py` tests a paused world, row stagger and final transforms/scales, asynchronous close, interrupted entrance/reopen, and delayed opening of settings. Does not Apply or Restore game settings. Evidence in `Saved/UIHubMotion`.
