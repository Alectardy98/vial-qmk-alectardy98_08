IBM TOPRE 029 - STAGE 1 MAPPER (ATmega32U4)

Primary PS/2: DATA D0, CLOCK D1

Build:
  make clean && make converter/topre_029:mapper

Stage 1 implements ordinary Set-2 and E0-prefixed keys only.
Rows 0-7 are ordinary bytes; rows 8-15 are E0 bytes; rows 16-23 are reserved.

Not decoded yet: green multi-byte keys, yellow mystery modifiers, red Shift behavior, REP, Shift Lock.
Do not use those special keys to judge Stage 1 behavior.
