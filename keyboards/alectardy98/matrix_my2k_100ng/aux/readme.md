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

`build-commands.json` retains tested compile flags, operation order, softfp ABI,
RAM overlays, GBC ITCM placement and storage capabilities. The builder verifies
input fingerprints and ELF/BIN hashes before reusing a cache. Failures stop the
build before packaging. Main QMK objects are not compiled into AUX.

The default output is `alectardy98_matrix_my2k_100ng_aux_aux.uf2`. `rules.mk` optionally
allows final BIN, JSON and SHA256 files; all three default to `no`. Internal
build products and cache records remain in `.build/`.

The layout cleanup preserves the final AUX image byte-for-byte. No AUX behavior,
protocol, display, game or storage implementation changed. Use only the distinct
AUX UF2 in a fresh ASTRO BOOT session when deliberately flashing AUX. The build
never flashes, enters BOOT, or accesses a keyboard.
