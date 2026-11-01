# AUX STM32F722 firmware

From the repository root:

```sh
make alectardy98/matrix_my2k_100ng/aux:aux
```

AUX is a standalone Cortex-M7 application built by `build.mk` and the shared
`../tools/build-aux.py`. Sources are in `src/`, headers in `include/`, and private
dependencies in `vendor/`, including the Doom engine. The actual tested512KiB
flash/ITCM/game-overlay layout is `linker.ld`; shared protocol modules live in
`../common/`. CMSIS headers come from this checkout's ChibiOS tree.

`build-commands.json` contains the final 159 build operations, with obsolete
compile/link passes removed. It retains tested flags, final operation order,
softfp ABI, RAM overlays, GBC ITCM placement and storage capabilities. The builder verifies
input fingerprints and ELF/BIN hashes before reusing a cache. Failures stop the
build before packaging. Main QMK objects are not compiled into AUX.

The default output is `alectardy98_matrix_my2k_100ng_aux_aux.uf2`. `rules.mk` optionally
allows final BIN, JSON and SHA256 files; all three default to `no`. Internal
build products and cache records remain in `.build/`.

The formatting/build-manifest cleanup was verified byte-for-byte against its
baseline before the subsequent parallel-startup changes described below.
Use only the distinct AUX UF2 in a fresh ASTRO BOOT session when deliberately
flashing AUX. The build never flashes, enters BOOT, or accesses a keyboard.

## Runtime ownership

- `startup.S` initializes C memory and enters `aux_main()` in `src/main.c`.
- The foreground services USB and UART, validates frames, and queues commands.
- `io_worker()` is resumed cooperatively by `work.c`/`context.S`. It owns panel
  state, FatFs access, rendering, game lifecycle and control-command execution.
- `assets.c` decodes animations; `animation.c` handles selection/capabilities.
  `panels.c` coordinates transfers and power, and `panel_platform.c` performs
  physical display I/O. OLED configuration/content is handled separately by
  `oled.c`, `text.c`, `clock.c` and `oled_store.c`.
- `sd_card.c` is the native card transport. `diskio.c` is FatFs's adapter;
  `msc_io.c`/`msc_backend.c` coordinate USB media ownership. These current names
  replace historical `_readonly` filenames: Release 1 supports authorized
  writes, while older read-only profile gates remain in the code.
- `touch.c` reads touch samples. Main interprets those samples using the saved
  key bindings, mode, sensitivity and tap threshold.

Keep physical buses and filesystem operations in the worker; foreground/ISR
paths must not re-enter those operations across a yield. Existing bounded queues,
buffer ownership and transfer-completion boundaries are part of the design.
Normal-mode animation, test display and games share the same physical color
panel and must not present simultaneously.

Boot uses `aux_panels_begin()` and the existing color power state machine.
OLED reset waits and SDMMC/FatFs initialization overlap color's sleep-out wait;
no individual controller wait is shortened. `aux_panels_init()` retains the
blocking reference path used by older isolated panel fixtures. Only a validated
animation capability probe can queue before peripheral readiness. It is executed
by the worker after initialization; enqueue success does not claim readiness.
The saved-slot presentation gate still prevents a legacy first-frame flash.

`aux_startup` holds RAM-only boot timing. DQ26 reports the first SPI-drained color
frame time; DQ27/28 report panel/storage/selection stages. These additive pages do
not change the animation or file-transfer protocol. Their clocks start after
AUX clock setup, so optical plug-to-pixel measurements remain a separate test.

Game data overlays are intentional: NES, GBC and Doom cannot run concurrently.
Do not add unrelated buffers inside their overlaid sections or pass ITCM buffers
to a peripheral. Keep the linker assertions and independent layout checks when
changing memory use. The instruction-cache policy, card register-write fences
and `DBG_SLEEP` startup setting are hardware/runtime requirements, not obsolete
logging code.

## Extending the build

Add an AUX source's compile command before the final link in
`build-commands.json`, add its object to that link, and use the matching existing
profile's flags. Preserve any required NES/GBC section-renaming steps and the
softfp ABI; vendor engines have different build flags from the board wrappers.
Generated TinyUSB adaptations belong in `../tools/`, not edited vendor files.
Only add a shared `common/` module to both MCUs when both actually use it.

For application changes, start in the board wrappers under `src/`. Keep upstream
core attribution/licenses in `vendor/` and record deliberate core changes there.
Run the parent README's regression/layout checks, then compare actual display,
storage and input behavior on hardware. An offline FPS loop or an acknowledged
USB transfer alone does not establish visible output or data integrity.
