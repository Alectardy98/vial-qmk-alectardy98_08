# Build helpers

This folder is required by the Main/AUX build commands.

- `qmk`: host-only Python3.14 compatibility for this checkout's QMK CLI.
- `build-aux.py`: standalone AUX compiler/verified build-cache integration.
- `package.py`, `astro_uf2.py`: ASTRO UF2 packaging and layout validation.
- `prepare-debug-host.py`, `prepare-dev16-usb-lld.py`: board-local USB driver adaptations.
- `prepare-chibios.py`: private, hash-verified ChibiOS copies from pristine submodules.
- `prepare_tinyusb*.py`: pinned AUX TinyUSB adaptations.
- `generate-defaults.py`: regenerate Main's factory image/keymap from its saved capture.

Helpers do not flash hardware. Generated driver copies and intermediate binaries
live in `.build/`; Python's normal `__pycache__/` behavior is unchanged.
