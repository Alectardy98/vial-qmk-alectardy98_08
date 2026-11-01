# Main QMK/Vial firmware

From the repository root:

```sh
make alectardy98/matrix_my2k_100ng/main:vial
```

Application C/headers are in `src/`; peripheral transports are in `drivers/`;
filesystem/journal modules are in `storage/`. QMK's `main.c`, `matrix.c`, board
configuration, rules, keymaps and linker files retain their expected locations.
All Python helpers are shared under `../tools/`.

`rules.mk` describes the MCU, hardware drivers, shared sources and packaging.
The Vial keymap enables VIA/Vial and includes `features.mk`, which holds the
Release 1 board feature profile at its original point in QMK's build sequence.
Use that file when adding a Main module; do not put application logic into
`keymaps/vial/keymap.c`. That keymap is generated from the saved factory capture.

Read `main.c` in lifecycle order: startup hooks, Main/AUX receive dispatch,
housekeeping services, key processing, then suspend/resume. Its helpers live in
`src/`; `drivers/` contains the actual buses, protocol engines and controller
backends. Request asynchronous work from the key hooks rather than waiting for
storage or display operations there. Filesystem ownership and export barriers
must remain intact when adding a new GUI or keycode operation.

The pre-init hook completes AUX's existing finite reset before USB and RGB setup
so both processors can initialize in parallel. `aux_link.c` uses native RTOS
milliseconds for this early step because QMK rebases `timer_read32()` later.
Runtime UART servicing remains nonblocking; internal USB enumeration still waits
for reset completion and host-driver binding. Never repeat the reset on a UART
timeout or perform input callbacks from this early initialization path.

`chibios/` holds required board-local USB adaptations. The build creates private
patched copies in `.build/my2k-chibios/` while keeping both library submodules
clean. Source and output hashes are pinned so an incompatible library update
fails before compilation. The existing USB safeguards, identity gates and
driver ABI are preserved exactly.

The normal output is `alectardy98_matrix_my2k_100ng_main_vial.uf2`. `rules.mk` optionally
allows final BIN, JSON and SHA256 files; all three default to `no`. Build
intermediates remain in `.build/`.

## Saved factory settings

`defaults/capture.json` records two equal GET-only readbacks of the keyboard's
saved configuration. Supported settings are serialized in `defaults/factory.inc`
and `factory.bin`; the four-layer default keymap is in `keymaps/vial/keymap.c`.
This is a canonical reconstruction through supported GET APIs, not a raw physical
flash dump. Unexposed core fields retain QMK defaults.

Captured settings cover keymaps/macros, Vial features, GUI-exposed QMK settings,
RGB, Lock Lights, touch, sleep, game controls, animation selection/playback
policy, and the USB performance preference. OLED configuration and asset files
on the128MB storage remain separate and are untouched.

Valid existing EEPROM is preserved. Empty/invalid EEPROM and Clear EEPROM
restore the captured configuration. Initialization publishes validity only after
the complete image is durable; power-loss recovery is regression tested. The
existing32KiB logical EEPROM, paged cache and sectors10/11 are unchanged. The
factory image adds32KiB of read-only flash, with no extra global RAM mirror.

To regenerate defaults from the preserved local capture:

```sh
python3 keyboards/alectardy98/matrix_my2k_100ng/tools/generate-defaults.py
```

This command uses local files only. See the parent README for tests and output
switches. Main UF2 addresses remain`0x08020000–0x0805ffff`, excluding the ASTRO
prefix and settings sectors. Never substitute generic UF2 packaging. Build
commands do not flash hardware or change device settings.
