# Required board-local ChibiOS adaptations

This directory contains **build inputs**, needed by the MY-2K Main firmware:

- `chibios.patch`: USB device resume safety guard.
- `chibios-contrib.patch`: internal USB-host safeguards, enumeration scheduling,
  MSC cached identity, write qualification and real cache-sync handling.
- `my2k_usbh_bounded.h`: finite internal-host wait helpers.
- `manifest.json`: pinned submodule commits and pristine/patched file hashes.

The two submodules under `lib/` remain pristine. Do not apply these patches to
them manually. `tools/prepare-chibios.py` creates a private tree under
`.build/my2k-chibios/`, applies the patches there and verifies the generated
files against the exact tested contents. Unchanged branches link to the
checked-out libraries; changed files are private regular files.

The copied Contrib header boundaries keep the USB driver declarations and LUN
layout consistent in every translation unit. Only MY-2K Main redirects its
Contrib sources/includes to the private tree. The existing board-local USB
generators then add the same HS2 and diagnostic hooks as before. AUX and other
keyboards continue to use their existing dependencies.

Upstream revisions, modified upstream files, and changed patch data are rejected
unless the manifest is deliberately reviewed and updated. Clearing `.build/`
is safe; the next Main build regenerates everything. A valid cache is reused
without rewriting headers. Generated symlinks are recreated if the checkout
moves. No generator writes into a submodule or accesses keyboard hardware.

The migration was verified with a fresh build after restoring both submodules.
Main and AUX UF2 bytes match the pre-migration firmware exactly. Native,
packaging and independent ELF/UF2 checks are provided under `../tests/`.

```sh
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/test_chibios.py
```
