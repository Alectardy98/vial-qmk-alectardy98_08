# EC Prox STM32F411CEU6 — TinyUF2 variant

Independent EC42 v2.9 variant of the working EC Prox hardware adapter. The PCB
and 512 KiB F411CEU6 are confirmed by the owner. The original `ec_prox` is unchanged.

```sh
make alectardy98_ec/ec_prox_uf2:vial
make alectardy98_ec/ec_prox_uf2:vial_alec
```

Normal QMK builds produce real application-only UF2 files in `.build/` and the
repository root. They require the separately built EC Prox TinyUF2 bootloader;
selecting `tinyuf2` in QMK does not install it.

| Region | Inclusive addresses |
|---|---|
| TinyUF2, sectors0–1 | 08000000–08007FFF |
| EEPROM, sector2 | 08008000–0800BFFF |
| Reserved, sector3 | 0800C000–0800FFFF |
| Application, sectors4–5 | 08010000–0803FFFF |
| Reserved, sectors6–7 | 08040000–0807FFFF |

UF2 family57755A57; bootloader address checks reject all writes outside the
application allocation. EEPROM retains QMK's8 KiB journal/4 KiB logical storage
and EC42 guards/CRC/readback. Initial installation requires fresh configuration
and calibration; normal subsequent UF2 updates exclude persistence. The existing
single-sector journal's interrupted-consolidation data-loss limitation remains.

Both keymaps, including `vial_alec` RGB configuration/callbacks, are exact copies
of the working Prox versions. ADC/mux/scanning sources and 84 MHz/8 MHz HSE timing
are retained. A local board-header adapter preserves original startup GPIO
values while selecting the confirmed F411 MCU. Only the first64 KiB RAM is used.
Native QMK TinyUF2 entry uses mailbox2000FFFC; bootloader stack is8-byte aligned
at2000FFF8. Application identity/Vial UID/build ID are unchanged; the existing
standalone GUI needs no runtime modifications.

See `development/alectardy98_ec/tinyuf2/REPORT.md` and its `artifacts/` directory
for final files, SHA256SUMS, build/test evidence, first-install and recovery
instructions. This is software-validated experimental firmware; physical USB,
bootloader, calibration, RGB and persistence testing remain outstanding. Establish
independent ROM DFU/BOOT0-reset or SWD recovery before erasing anything.

No hardware was flashed during development. Original Cipulot/QMK notices and
TinyUF2/ChibiOS licensing are preserved. No automatic/timed bottom-out calibration,
legacy migration, RGB Matrix engine or changes to other keyboards were added.
