IBM M50 / unusual PS/2 keyboard discovery firmware
===================================================

Purpose
-------
This is a deliberately permissive 32U4 PS/2 mapping build derived from the
provided M50 converter. It is intended to discover an unknown keyboard's raw
PS/2 Set-2 scan codes before creating the final keyboard layout.

Important behavior
------------------
* Full 24 x 32 virtual matrix (768 possible normal/E0/E1 positions).
* Every keymap position is KC_NO. Unknown scan codes therefore cannot type
  letters or accidentally invoke QK_BOOT.
* Console is enabled.
* Vial/VIA are intentionally disabled in this discovery build so the enormous
  24x32 virtual matrix does not consume dynamic-keymap EEPROM.
* PS/2 pins remain D1 clock / D0 data, interrupt driver, ATmega32U4/Caterina.
* All changes are local to this keyboard folder.

Console output
--------------
Every received PS/2 byte is printed as PS2:XX. Completed make/break events also
show the synthetic code and virtual matrix position, for example:

  PS2:1C | MAKE  CODE:1C -> R00 C28
  PS2:F0 PS2:1C | BREAK CODE:1C -> R00 C28

  PS2:E0 PS2:75 | MAKE  CODE:E0 75 -> R11 C21
  PS2:E0 PS2:F0 PS2:75 | BREAK CODE:E0 75 -> R11 C21

Build
-----
Place this directory under your QMK/Vial keyboards tree using whatever keyboard
path/name you want. If installed as:

  keyboards/converter/ibm_m50_mapper/

build with:

  make clean && make converter/ibm_m50_mapper:mapper

Flash the resulting Caterina/ATmega32U4 firmware and run hid_listen.

Mapping workflow
----------------
Press and release ONE physical key at a time. Record:
  physical key | raw PS/2 sequence | MAKE row/col | BREAK row/col

Once the whole board is mapped, use those discovered matrix positions to build
a normal keyboard.json/keymap and remove the discovery-only console behavior.
