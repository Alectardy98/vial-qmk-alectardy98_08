# AUX offline tests

The portable storage-write regression entry point is:

```sh
sh keyboards/alectardy98/matrix_my2k_100ng/aux/tests/run-stock-write-tests.sh
SANITIZE=1 sh keyboards/alectardy98/matrix_my2k_100ng/aux/tests/run-stock-write-tests.sh
```

Run from any directory using the script's path. It uses the TinyUSB headers
vendored with AUX, a temporary executable, and simulated sectors in RAM. It
tests asynchronous buffer ownership, partial-sector updates, bounds, failed
writes, synchronization, cancellation/requeue epochs, and format rejection.
It never opens USB, a debug probe, a mounted volume, or a real block device.

The other fixtures were retained from earlier, separately configured firmware
profiles. Some require generated FAT images, text vectors, Unicorn, or explicit
compiler flags; they are not a universal test runner for the current image.
In particular, `check_dev15_artifact.py` asserts the old D015 read-only policy
and must not be used as an acceptance check for Release 1's writable profile.

For current Main/AUX ELF/UF2 layout and factory-default checks, build both
images and run:

```sh
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/validate-artifacts.py
```

Hardware display, input, game, and storage behavior still requires physical
acceptance testing. A simulated test cannot establish visible frame rate,
electrical reliability, or real USB polling cadence.
