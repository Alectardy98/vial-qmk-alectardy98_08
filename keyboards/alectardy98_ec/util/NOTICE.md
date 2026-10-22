# Attribution and scope

This is a separate application derived from selected concepts and code in
Cap-Util by Purdea Andrei (Copyright 2020, GPL-2.0-or-later). This derivative is
distributed under GPL-3.0-or-later, permitted by those original notices. Original
copyright attribution is retained in adapted source files.

- `src/widgets.cpp`: adapts Cap-Util `signal_level.cpp`'s bounded signal-color
  interpolation and current/min/max monitor concept; adds shape items, matrix
  selection, history plotting and safe handling of unsampled data.
- `src/mainwindow.{h,cpp}`: retains Cap-Util's device/monitor window organization
  and Qt signal/slot approach, with new runtime layout/model/protocol wiring.
- `src/telemetry.*`: new synthetic source; current/min/max semantics follow the
  original monitor but are independent of its Model F electrical measurement.
- `src/protocol.*`: new EC42 codec and mock transport. Cap-Util's HIDAPI
  request/reply layering informs the transport boundary; its Model F wire
  commands and low-level DAC/shift-register controls are not copied into EC42.

The source was inspected at:
`/home/alec/Documents/GitHub/vial-qmk-alectardy98_08/keyboards/reverseenginering/DUMPS/util/`.
That directory is not a build dependency and remains unchanged. Existing
AppImages, generated layout tables, icons, binaries and packaging scripts are
not incorporated. No files are symlinked back into the original utility.

KLE serialized label positions and alternative-group placement follow the KLE /
Vial public format. Behavioral reference:
https://github.com/vial-kb/vial-gui/blob/main/src/main/python/kle_serial.py
https://github.com/vial-kb/vial-gui/blob/main/src/main/python/widgets/keyboard_widget.py

The supplied Vial JSON fixture is copied unchanged from the existing keyboard.
It is example data, not an embedded or mandatory keyboard geometry.

## Verified origin and dependency notices (audit 2026-10-21)

The historical `reverseenginering/DUMPS/util/` location above is no longer the
only available evidence. All 16 source hashes in docs/PROVENANCE.json also match
`keyboards/xwhatsit/util/util/` in this checkout, including Purdea Andrei's
GPL-2.0-or-later headers. Do not discard those notices when reorganizing Qt code.
The current application source, tested fixtures and screenshot were imported
from qmk-ec-util; no third-party icon/font files or Cap-Util binaries are bundled.
The screenshot's exact original image creator is not independently established.

The executable dynamically links Qt Core/Gui/Widgets, HIDAPI-hidraw, liblzma and
system C/C++ libraries. Qt has component-specific LGPL/GPL/commercial choices;
inspect the actual installed component licenses when packaging a release.
HIDAPI's BSD option (Copyright 2010 Alan Ott, Signal 11 Software) and liblzma's
0BSD text are preserved under LICENSES/, together with GPLv2 and LGPLv3 texts.
COPYING already provides GPLv3. Dependencies are obtained from system packages,
not copied into this source distribution. Bundling libraries/plugins later
requires a separate complete dependency/license/source-compliance review.

See ../THIRD_PARTY_NOTICES.md and the repository's
`development/alectardy98_ec/licensing/REPORT.md` for the complete source audit.
This application is separate from the keyboard firmware; using HID does not
make its protocol or GUI code Cipulot-authored.
