# onekey595 (ATmega32U4, 1x1 matrix, 3x 74HC595 LED chaser)

## Pinout (ATmega32U4)
- Row0: D0
- Col0: D1
- 74HC595 SER (data): B2
- 74HC595 SRCLK (shift clock): B1
- 74HC595 RCLK (latch): B3
- 74HC595 OE: GND (always enabled)
- 74HC595 SRCLR: VCC (no clear)

## Chain the 74HC595
- First QH' -> Second SER
- Second QH' -> Third SER
- Share SRCLK, RCLK, VCC, GND across all

## LEDs
Each LED (or each color die) needs its own current-limiting resistor.
Pattern lights one LED at a time across 24 outputs.

## Build
```
qmk compile -kb onekey595 -km default
```

## Flash
For Caterina (Pro Micro/Elite-C style):
```
qmk flash -kb onekey595 -km default
```
