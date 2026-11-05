# Virgo EC — EC42 TinyUF2 variant

```sh
make alectardy98_ec/ec_virgo_uf2:vial
make alectardy98_ec/ec_virgo_uf2:vial_alec
```

These commands create **application-only UF2s** in the repository root. They
require the Virgo TinyUF2 bootloader. They do not install a bootloader on an
original STM32-DFU Virgo.

Cipulot sources, EC42 common files, QMK and ChibiOS remain unchanged. The two
new EC42 Virgo adapters now share the corrected 256 KiB flash contract. This adapter compiles the original Virgo acquisition and matrix files
directly and includes its keymap sources. Protocol, calibration policy, pin
mapping, 96 MHz application clock, Vial UID, custom macros and logical EEPROM
addresses are retained. VialAstro 1.56 already recognizes the same UID; no new GUI
or EC42 command is required.

## Memory contract

Addresses below are inclusive. The owner identified the chip as
**STM32F411CCU6: 256 KiB flash and 128 KiB SRAM**. The initial TinyUF2 build
incorrectly required 512 KiB and consequently rejected a CC device during both
application validation and UF2 writes. This build corrects that gate and all
linker/package/write bounds. The factory density must report 256 KiB; the check
remains fail-closed. The common EC42 storage validator also checks the flash
descriptor, erase geometry and protection state.

| Region | Address range |
|---|---|
| TinyUF2 sectors 0–1 | `0x08000000–0x08007fff` |
| Settings sector 2 | `0x08008000–0x0800bfff` |
| Reserved sector 3 | `0x0800c000–0x0800ffff` |
| Application sectors 4–5 | `0x08010000–0x0803ffff` |
| SRAM available to application | `0x20000000–0x2001fffb` |
| Shared reset mailbox, excluded from heap | `0x2001fffc–0x2001ffff` |

The existing 4 KiB logical EEPROM and 8 KiB wear-leveling journal occupy sector 2.
The journal owns that entire 16 KiB erase unit. The EC record is still 341 bytes;
keymaps start at byte 382, macros at 2438, and macros have 1658 bytes available.
The single-sector journal's existing interrupted-consolidation limitation remains.

The bootloader's allocated space is 32 KiB (31 KiB code plus a reserved 1 KiB
configuration area). The application starts at 64 KiB to align with the existing
QMK TinyUF2 layout. Both linkers use mailbox `0x2001fffc`, retaining 128 KiB RAM
rather than reducing the application to 64 KiB. The bootloader stack is aligned
at `0x2001fff8`. Application vectors, stack bounds and the Thumb reset address
are validated by the board-local validator.

## Bootloader and first installation

Bootloader source, pinned dependencies, build script and an MIT license are in
`bootloader/`. Its identity is **Virgo TinyUF2**; the mounted volume is
**VIRGOBOOT**, with Board-ID `EC-Virgo-STM32F411CC`. It uses family `0x57755a57`, which is shared with other STM32F4
boards: always choose the Virgo UF2, not a different board's UF2.

Use the separately packaged **`EC-Virgo-TinyUF2-vial_alec-FIRST-INSTALL.bin`** for
an intentional initial installation at **`0x08000000`**. It contains TinyUF2,
erased FF gaps/settings, and the matching application. The package also includes
a `vial` carrier and bootloader-only BIN/HEX. Selecting `tinyuf2` in QMK does not
install TinyUF2 by itself.

**First installation discards the old flash settings and requires calibration.**
If the previous 512 KiB TinyUF2 build is already installed, copying a corrected
application UF2 will not fix its bootloader gate. Replace it using the corrected
combined first-install BIN through the same ROM-DFU/SWD route used initially.
The old loader does not support self-update. Afterwards, normal application UF2
updates work without replacing TinyUF2 or touching settings.

The non-UF2 target stores settings in sector 1, which this layout uses for the
bootloader. Do not attempt to preserve or migrate that physical region in place.
Back up keymaps/calibration and full flash before converting; establish the
board's physical ROM-DFU or SWD recovery procedure first.

With only this Virgo connected in its known STM32 ROM DFU mode, a user-controlled
first install is:

```sh
sudo dfu-util -l
sudo dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave \
  -D EC-Virgo-TinyUF2-vial_alec-FIRST-INSTALL.bin
```

The carrier is not a UF2 and must not be copied to a TinyUF2 volume. It writes
from address zero of user flash; it must not be used on an unrelated keyboard or
an unverified third-party flash layout. No option-byte unlock, write-protection
change, mass erase or automatic flashing is part of the build/package scripts.

After installation:

1. Open VialAstro 1.56 or newer. Perform manual calibration and **Save and apply**.
2. Verify every installed key, Vial remapping, macros and settings after reconnect.
3. Enter TinyUF2 with QK_BOOT/the supported GUI action, or double-reset where the
   physical reset connection supports it. Confirm `VIRGOBOOT` and INFO_UF2 identity.
4. Copy the appropriate `alectardy98_ec_ec_virgo_uf2_*.uf2` to that volume.
   Subsequent UF2s contain application addresses only and exclude settings.
5. Verify calibration and Vial settings survive that update and a power cycle.

If an update is interrupted, TinyUF2 remains outside the application's erase
range. Reset/re-enter it and repeat the correct UF2. This is **not** an atomic
A/B update or a whole-image signature check; vector validation alone does not
prove a complete application was transferred. Independent ROM-DFU/SWD recovery
is still required for first acceptance. Automatic option-byte protection and
bootloader self-update are intentionally disabled.

To return to the original non-UF2 layout, use ROM DFU/SWD to restore a matching
full-flash backup, or intentionally install the `ec_virgo` clean-install carrier
at `0x08000000` and recalibrate. Do not copy a non-UF2 image into `VIRGOBOOT`.

## Rebuilding TinyUF2

From the QMK root, with a separate TinyUF2 checkout containing the dependency
revisions listed in `bootloader/upstream.json`:

```sh
python3 keyboards/alectardy98_ec/ec_virgo_uf2/bootloader/build.py \
  --tinyuf2 /path/to/tinyuf2 --output /tmp/virgo-boot-build
python3 keyboards/alectardy98_ec/ec_virgo_uf2/tests/test_bootloader.py \
  /tmp/virgo-boot-build
python3 keyboards/alectardy98_ec/ec_virgo_uf2/bootloader/package.py \
  --boot-build /tmp/virgo-boot-build --output /tmp/virgo-firmware
```

The builder creates a disposable checkout at the pinned TinyUF2 commit, applies
the recorded bounds/error-handling patch, installs only this board's configuration,
and invokes TinyUF2's build contract. Dependencies are checked against their pins
and must be clean. It does not modify the owner checkout's sources or submodule
revisions. TinyUF2 embeds its compilation date/time, so independent builds can
have different hashes. Normal QMK application builds do not need that checkout.

The recorded hardening patch comes from the earlier Pro X TinyUF2 port: packet
bounds, flash-write failure/readback checks and application-range checks. Virgo's
local source substitutions replace that port's 64 KiB stack check and add a
256 KiB factory-density gate; they do not alter shared source files.
`INFO_UF2.TXT` also reports the factory size, application vectors, reset request
and raw reset flags whenever the loader remains active, without changing them. No protocol or
calibration engine is copied into this variant. See source licenses/notices.

## Validation and limits

```sh
python3 keyboards/alectardy98_ec/ec_virgo_uf2/tests/test_port.py
VIRGO_HARNESS=/tmp/ec-virgo-host-harness \
  python3 keyboards/alectardy98_ec/ec_virgo/tests/test_port.py
```

Both QMK keymaps and TinyUF2 compile. Tests check real compiled EEPROM offsets,
Vial definitions, mailbox/heap boundaries, UF2/ELF byte agreement, clean-install
FF gaps, and original/common source hashes. The intentional board-local flash-limit
changes are recorded separately in `tests/flash-correction.json`. The native bootloader harness tests
the built parser/write bodies with mocked HAL/flash, including failed/corrupt
writes, invalid packets, flash density, boot/settings exclusion and reset checks.
It does not establish physical USB, flash endurance or power-loss reliability.

The TinyUF2 pre-commit formatting/spelling/YAML checks pass. Its repository-wide
agent-script test hook ran 472 tests but had six errors because unrelated i.MX RT
and STM32F3 SDK files were absent in the isolated checkout (one test skipped).
The actual Virgo build contract passes; the broad hook is not reported as passing.

The local manufacturer-document/errata lookup remains unavailable because the
Calibre `metadata.db` is missing (lookups `8004dfdd0802`, `7967fc94b49b`). ST's
[STM32F411CC product page](https://www.st.com/en/microcontrollers-microprocessors/stm32f411cc.html)
and DS10314 Rev 8 support the owner's part identification/capacity. Physical
post-fix boot, factory-register readback and other hardware acceptance remain pending. No hardware
was flashed. Test USB enumeration, cold boot, bootloader entry/re-entry, calibration,
Global/per-key/RT settings, all keys/layout options, persistence and rollback on
this specific keyboard before relying on the new layout.

References: [QMK flashing documentation](https://docs.qmk.fm/flashing) and
[TinyUF2 source](https://github.com/adafruit/tinyuf2).
