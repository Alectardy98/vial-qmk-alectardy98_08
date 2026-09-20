#pragma once

#include "quantum.h"
#include "ringbuf.h"
#include <stdint.h>

// ------------------------------
// Error status codes
// ------------------------------
#define IBMPC_ERR_NONE 0x00
#define IBMPC_ERR_SEND 0x01
#define IBMPC_ERR_FULL 0x02

// ------------------------------
// Protocol types
// ------------------------------
#define IBMPC_PROTOCOL_AT       0
#define IBMPC_PROTOCOL_AT_Z150  1
#define IBMPC_PROTOCOL_XT       2
#define IBMPC_PROTOCOL_PS2      3
#define IBMPC_PROTOCOL_TERMINAL 4
#define IBMPC_PROTOCOL_UNKNOWN  0xFF

// ------------------------------
// Global state
// ------------------------------
extern volatile uint8_t ibmpc_error;
extern volatile uint8_t ibmpc_isr_debug;
extern uint8_t ibmpc_protocol;

// ------------------------------
// Host interface functions
// ------------------------------
void ibmpc_host_init(void);
void ibmpc_host_enable(void);
void ibmpc_host_disable(void);
void ibmpc_host_isr_clear(void);
void ibmpc_host_set_led(uint8_t led);

uint8_t ibmpc_host_send(uint8_t data);
uint8_t ibmpc_host_recv(void);
uint8_t ibmpc_host_recv_response(uint8_t *response);

// ------------------------------
// RP2040 GPIO Definitions
// ------------------------------
#define IBMPC_CLOCK_PIN GP1
#define IBMPC_DATA_PIN  GP0
#define IBMPC_RESET_PIN GP2

#define clock_lo()       writePinLow(IBMPC_CLOCK_PIN)
#define clock_hi()       writePinHigh(IBMPC_CLOCK_PIN)
#define clock_in()       readPin(IBMPC_CLOCK_PIN)

#define data_lo()        writePinLow(IBMPC_DATA_PIN)
#define data_hi()        writePinHigh(IBMPC_DATA_PIN)
#define data_in()        readPin(IBMPC_DATA_PIN)

#define IBMPC_RST_LO()   writePinLow(IBMPC_RESET_PIN)
#define IBMPC_RST_HIZ()  setPinInputHigh(IBMPC_RESET_PIN)

