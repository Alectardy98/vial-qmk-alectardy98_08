# olympia_es100_pico (RP2040 / Raspberry Pi Pico)

This is a QMK keyboard definition for an Olympia ES-100/101 typewriter keyboard PCB
(Marquardt "butterfly" switches), converted to a Raspberry Pi Pico (RP2040).

## Pin mapping (as requested)
On the top ledt of the PCB, the 18-pin connector. When looking at the pins, left → right as PCB pin 1 through PCB pin 18.

PCB pin 1 → Pico GPIO0 (GP0)
PCB pin 2 → Pico GPIO1 (GP1)
PCB pin 3 → Pico GPIO2 (GP2)
PCB pin 4 → Pico GPIO3 (GP3)
PCB pin 5 → Pico GPIO4 (GP4)
PCB pin 6 → Pico GPIO5 (GP5)
PCB pin 7 → Pico GPIO6 (GP6)
PCB pin 8 → Pico GPIO7 (GP7)
PCB pin 9 → Pico GPIO8 (GP8)
PCB pin 10 → Pico GPIO9 (GP9)
PCB pin 11 → Pico GPIO10 (GP10)
PCB pin 12 → Pico GPIO11 (GP11)
PCB pin 13 → Pico GPIO12 (GP12)
PCB pin 14 → Pico GPIO13 (GP13) (unused in the original matrix, available for extras)
PCB pin 15 → Pico GPIO14 (GP14)
PCB pin 16 → Pico GPIO15 (GP15)
PCB pin 17 → Pico GPIO16 (GP16)
PCB pin 18 → Pico GPIO17 (GP17)

## Matrix mapping 

Rows (PCB pins): 1, 2, 3, 4, 15, 16, 17, 18  
Cols (PCB pins): 5, 6, 7, 8, 9, 10, 11, 12, 13  
PCB pin 14 is unused (Pico GP13).

In QMK terms (COL2ROW):

## Build
From your qmk_firmware checkout:
```
qmk compile -kb olympia_es100_pico -km default
```

## Flash (UF2)
Put the Pico into BOOTSEL mode (hold BOOTSEL while plugging in), then:
```
qmk flash -kb olympia_es100_pico -km default
```
