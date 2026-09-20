# Keyboard converter for ATT WALTII / 430 ACZ

The protocol used is 4800 BAUD, with 8 bit scan codes with one stop bit. There are 5 active lines coming from the PCB, each of which is labeled with a letter on the silk screen.

W - The keyboard input line, tie to TX on the pro micro

P - 5v

B - Gnd

Y - The keyboard output line, tie to RX on the pro micro

R - This isnt tied to anything 

G - This line must be grounded for the keyboard set active (tie to GND) 

There are a few custom keycodes, and some custom lock light functions
The caps LED will sync with caps LED state
The LED next to the clear key will light up when in the FN layer

There is keycodes for clicker and beeper
The clicker when activated will send 2X byte on every key press, where X indacates the LED State
The beeper when activated will send 4X byte on every key press, where X indacates the LED State

    X = 0, No LEDs are on
    X = 1, Caps LEDs is on
    X = 2, FN LEDs is on
    X = 3, Both Caps and FN LEDs are on


## Build

```
make converter/56k_430:vial
or 
make converter/56k_430:vial_alec

```

