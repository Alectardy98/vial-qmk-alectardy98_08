#include "quantum.h"


static inline void max7219_pulse(uint8_t pin) {
    writePinLow(pin);
    writePinHigh(pin);
}

void max7219_shift_byte(uint8_t byte) {
    for (int8_t i = 7; i >= 0; i--) {
        writePin(MAX7219_CLK_PIN, 0);
        writePin(MAX7219_DIN_PIN, (byte >> i) & 1);
        writePin(MAX7219_CLK_PIN, 1);
    }
}

void max7219_send(uint8_t address, uint8_t data) {
    writePinLow(MAX7219_LOAD_PIN);
    max7219_shift_byte(address);
    max7219_shift_byte(data);
    writePinHigh(MAX7219_LOAD_PIN);
}

void max7219_init(void) {
    setPinOutput(MAX7219_DIN_PIN);
    setPinOutput(MAX7219_CLK_PIN);
    setPinOutput(MAX7219_LOAD_PIN);

    writePinLow(MAX7219_DIN_PIN);
    writePinLow(MAX7219_CLK_PIN);
    writePinHigh(MAX7219_LOAD_PIN);

    wait_ms(50);

    max7219_send(0x0F, 0x00); // Display test OFF
    max7219_send(0x0C, 0x01); // Shutdown register: normal operation
    max7219_send(0x0B, 0x07); // Scan limit = 8 digits
    max7219_send(0x0A, 0x0F); // Intensity = max
    max7219_send(0x09, 0x00); // Decode mode: none

    // Clear digits
    for (uint8_t i = 1; i <= 8; i++) {
        max7219_send(i, 0x00);
    }
}
