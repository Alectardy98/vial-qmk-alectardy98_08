# Vial Alec

This Main-only variant keeps the existing MY-2K firmware features and adds 39
personal macros to Vial's **User** picker. Its personal factory configuration
comes from two verified keyboard readbacks on 2026-11-03, including the saved
macro assignments. The existing AUX firmware is compatible and needs no update.

Build from the QMK repository root:

```sh
make alectardy98/matrix_my2k_100ng/main:vial_alec
```

Output:

```text
alectardy98_matrix_my2k_100ng_main_vial_alec.uf2
```

The build only compiles/packages; it does not flash hardware. The regular
`main:vial` target and its output remain available for rollback.

## Keycode numbering and settings

The first 18 custom entries retain the existing storage, GIF and game actions.
`TEST` starts at `USER18` (`QK_KB_18`), followed by the other personal macros
through `THUM` at `USER56`. Starting `TEST` at `QK_KB_0` would instead invoke the
existing storage toggle. Keep the enum and `vial.json` in matching order.
VialAstro supports 64 entries; this definition uses 57.

This variant shares the supported Main feature rules and EEPROM layout, with
its own factory image under `defaults/`. Existing saved keymaps/settings remain
valid and are never overwritten by a normal flash. A distinct Vial UID
makes the GUI load the variant's expanded definition. Host-saved Vial profiles
may distinguish that UID from regular `vial`.

Returning to regular `vial` preserves EEPROM, but any assigned personal-macro
codes will have no action until remapped or the `vial_alec` image is restored.
The regular `vial` factory image is unchanged. Clear EEPROM while running
`vial_alec`, or first initialization of empty EEPROM, restores the personal
image: keymaps/macros, QMK/Vial settings, RGB groups and per-LED colors, Lock
Lights, touch, sleep, game controls, USB preference and saved GIF selection/
playback policy. Files and OLED configuration on the 128 MB storage are
deliberately excluded. PC-only GUI preferences, such as clock sync on connect,
are not Main EEPROM and are not embedded in firmware.

The capture records supported GET APIs, not an opaque flash clone. Unexposed
core/reserved fields keep the existing QMK factory policy. No extra RAM mirror
or EEPROM-layout change is introduced.

To capture new saved settings, close VialAstro and use a fresh output filename:

```sh
python3 keyboards/alectardy98/matrix_my2k_100ng/tools/capture-defaults.py --output /tmp/my2k-saved-settings.json
python3 keyboards/alectardy98/matrix_my2k_100ng/tools/generate-defaults.py --keymap vial_alec --capture /tmp/my2k-saved-settings.json
```

The capture script sends GETs only. Generation retains the personal macro enum
and implementation, replacing only its keymap array. To regenerate from the
checked-in personal capture, omit `--capture`.

## Macro behavior

The shortcut/string sequences match the supplied macro list. Each macro runs
once on press and consumes both press and release; other keys keep normal
processing. Main's existing game/storage gates run before these macros.

- `TEST` types `TEST`; `DZRO` types `00`.
- Desktop/app shortcuts preserve the supplied modifiers and key order.
- `TASK` and `QUIT` each emit both supplied Windows and Mac sequences. There is
  no automatic OS detection. Their host behavior depends on those shortcuts.
- `GOGL` and `CMD` emit Ctrl+Alt+G and Ctrl+Alt+T; opening applications requires
  corresponding host-side bindings.
- `NUM` taps the host Num Lock key; it does not add or switch keyboard layers.
- Discord macros type their colon-delimited text and press Enter. The matching
  emoji must exist on the server for Discord to resolve it.

## Offline validation

```sh
SANITIZE=1 python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/test_vial_alec.py
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/test_defaults.py
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/validate-artifacts.py --mcu main --keymap vial_alec
```

The macro test compiles the real keymap against a fake output sink, using QMK's
actual keycode/send-string definitions. It checks all 39 sequences, releases,
modifier balance, console on/off and pass-through of every other 16-bit code.
It also checks the original controls/layout metadata. Add `--gui /path/to/vial-gui_astro`
when running with the GUI's Python environment to test its actual keycode model.
No test sends shortcuts to the host or accesses the keyboard.
