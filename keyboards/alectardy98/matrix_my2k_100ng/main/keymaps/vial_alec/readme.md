# Vial Alec

This Main-only variant keeps the existing MY-2K firmware features and captured
four-layer defaults, and adds 39 personal macros to Vial's **User** picker.
The additional macros are initially unassigned; map them to keys or layers in
VialAstro. The existing AUX firmware is compatible and needs no update.

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

This variant shares the supported Main feature rules, EEPROM layout and factory
settings. Existing saved keymaps/settings remain valid. A distinct Vial UID
makes the GUI load the variant's expanded definition. Host-saved Vial profiles
may distinguish that UID from regular `vial`.

Returning to regular `vial` preserves EEPROM, but any assigned personal-macro
codes will have no action until remapped or the `vial_alec` image is restored.
`generate-defaults.py` updates the baseline `vial` keymap only; this variant is
maintained separately.

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
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/validate-artifacts.py --mcu main --keymap vial_alec
```

The macro test compiles the real keymap against a fake output sink, using QMK's
actual keycode/send-string definitions. It checks all 39 sequences, releases,
modifier balance, console on/off and pass-through of every other 16-bit code.
It also checks the original controls/layout metadata. Add `--gui /path/to/vial-gui_astro`
when running with the GUI's Python environment to test its actual keycode model.
No test sends shortcuts to the host or accesses the keyboard.
