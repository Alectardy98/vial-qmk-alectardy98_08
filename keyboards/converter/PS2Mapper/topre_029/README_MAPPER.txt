IBM DUAL PS/2 MAPPER (ATmega32U4)

Primary:   DATA D0, CLOCK D1
Secondary: DATA D2 (red), CLOCK D3 (blue)

Build:
  make clean && make converter/topre_029:mapper

Use hid_listen. Output identifies CH1/CH2, raw PS/2 bytes, MAKE/BREAK, and virtual row/column.
Rows 0-23 are CH1; rows 24-47 are CH2.

Secondary interface is receive-only in this diagnostic firmware.
